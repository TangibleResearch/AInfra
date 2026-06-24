#include "infravm.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "optimizer.h"

#define VM_OPENAI_IMPLEMENTATION
#include "vm_openai.h"
#include "vm_ollama.h"

#define AIF_VERSION 1
#define AIF_MAX_FILE_BYTES (64u * 1024u * 1024u)
#define AIF_MAX_OBJECTS 4096u
#define AIF_MAX_PROPERTIES 4096u
#define AIF_MAX_POINTERS 4096u
#define AIF_MAX_LIST_ITEMS 8192u
#define AIF_MAX_STRING_BYTES (1024u * 1024u)

typedef enum {
    VAL_STRING = 1,
    VAL_NUMBER = 2,
    VAL_BOOL = 3,
    VAL_REF = 4,
    VAL_LIST = 5
} ValueTag;

typedef struct Value Value;

struct Value {
    ValueTag tag;
    char *text;
    int boolean;
    Value *items;
    uint32_t item_count;
};

typedef struct {
    char *key;
    Value value;
} AIFProperty;

typedef struct {
    char *pointer_type;
    char *target_object_id;
} AIFPointer;

typedef struct {
    char *object_id;
    char *name;
    char *type;
    int start_flag;
    AIFProperty *properties;
    uint32_t property_count;
    AIFPointer *pointers;
    uint32_t pointer_count;
} AIFObject;

typedef struct {
    unsigned char *bytes;
    size_t len;
    size_t pos;
} Reader;

struct InfraVM {
    AIFObject *objects;
    uint32_t object_count;
    char last_error[512];
};

static void vm_set_error(InfraVM *vm, const char *message) {
    if (!vm) return;
    snprintf(vm->last_error, sizeof(vm->last_error), "%s", message ? message : "unknown InfraVM error");
}

static void vm_set_errorf(InfraVM *vm, const char *prefix, const char *detail) {
    if (!vm) return;
    snprintf(vm->last_error, sizeof(vm->last_error), "%s%s%s", prefix ? prefix : "", detail ? ": " : "", detail ? detail : "");
}

static void *xcalloc(InfraVM *vm, size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr) vm_set_error(vm, "out of memory");
    return ptr;
}

static int ensure_bytes(InfraVM *vm, Reader *reader, size_t count) {
    if (count > reader->len || reader->pos > reader->len - count) {
        vm_set_error(vm, "truncated AIF file");
        return 0;
    }
    return 1;
}

static uint8_t read_u8(InfraVM *vm, Reader *reader, int *ok) {
    if (!ensure_bytes(vm, reader, 1)) {
        *ok = 0;
        return 0;
    }
    return reader->bytes[reader->pos++];
}

static uint16_t read_u16(InfraVM *vm, Reader *reader, int *ok) {
    uint16_t value;
    if (!ensure_bytes(vm, reader, 2)) {
        *ok = 0;
        return 0;
    }
    value = (uint16_t)reader->bytes[reader->pos] | ((uint16_t)reader->bytes[reader->pos + 1] << 8);
    reader->pos += 2;
    return value;
}

static uint32_t read_u32(InfraVM *vm, Reader *reader, int *ok) {
    uint32_t value;
    if (!ensure_bytes(vm, reader, 4)) {
        *ok = 0;
        return 0;
    }
    value = (uint32_t)reader->bytes[reader->pos] |
            ((uint32_t)reader->bytes[reader->pos + 1] << 8) |
            ((uint32_t)reader->bytes[reader->pos + 2] << 16) |
            ((uint32_t)reader->bytes[reader->pos + 3] << 24);
    reader->pos += 4;
    return value;
}

static char *read_string(InfraVM *vm, Reader *reader, int *ok) {
    uint32_t len = read_u32(vm, reader, ok);
    char *text;
    if (*ok && len > AIF_MAX_STRING_BYTES) {
        vm_set_error(vm, "AIF string exceeds size limit");
        *ok = 0;
        return NULL;
    }
    if (!*ok || !ensure_bytes(vm, reader, len)) {
        *ok = 0;
        return NULL;
    }
    text = malloc((size_t)len + 1);
    if (!text) {
        vm_set_error(vm, "out of memory");
        *ok = 0;
        return NULL;
    }
    memcpy(text, reader->bytes + reader->pos, len);
    text[len] = '\0';
    reader->pos += len;
    return text;
}

static Value read_value(InfraVM *vm, Reader *reader, int *ok) {
    Value value;
    uint32_t i;
    memset(&value, 0, sizeof(value));
    value.tag = (ValueTag)read_u8(vm, reader, ok);
    if (!*ok) return value;

    switch (value.tag) {
        case VAL_STRING:
        case VAL_NUMBER:
        case VAL_REF:
            value.text = read_string(vm, reader, ok);
            break;
        case VAL_BOOL:
            value.boolean = read_u8(vm, reader, ok) != 0;
            break;
        case VAL_LIST:
            value.item_count = read_u32(vm, reader, ok);
            if (!*ok) break;
            if (value.item_count > AIF_MAX_LIST_ITEMS) {
                vm_set_error(vm, "AIF list exceeds item limit");
                *ok = 0;
                break;
            }
            if (value.item_count > 0) {
                value.items = xcalloc(vm, value.item_count, sizeof(Value));
                if (!value.items) {
                    *ok = 0;
                    break;
                }
                for (i = 0; i < value.item_count; i++) {
                    value.items[i] = read_value(vm, reader, ok);
                    if (!*ok) break;
                }
            }
            break;
        default:
            vm_set_error(vm, "unknown AIF value tag");
            *ok = 0;
            break;
    }
    return value;
}

static void free_value(Value *value) {
    uint32_t i;
    if (!value) return;
    free(value->text);
    for (i = 0; i < value->item_count; i++) free_value(&value->items[i]);
    free(value->items);
}

static void clear_vm(InfraVM *vm) {
    uint32_t i;
    uint32_t p;
    if (!vm) return;
    for (i = 0; i < vm->object_count; i++) {
        AIFObject *object = &vm->objects[i];
        free(object->object_id);
        free(object->name);
        free(object->type);
        for (p = 0; p < object->property_count; p++) {
            free(object->properties[p].key);
            free_value(&object->properties[p].value);
        }
        for (p = 0; p < object->pointer_count; p++) {
            free(object->pointers[p].pointer_type);
            free(object->pointers[p].target_object_id);
        }
        free(object->properties);
        free(object->pointers);
    }
    free(vm->objects);
    vm->objects = NULL;
    vm->object_count = 0;
}

static unsigned char *read_file_bytes(InfraVM *vm, const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    long size;
    unsigned char *bytes;
    if (!file) {
        vm_set_errorf(vm, "failed to open AIF file", path);
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        vm_set_error(vm, "failed to seek AIF file");
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        vm_set_error(vm, "failed to size AIF file");
        return NULL;
    }
    if ((unsigned long)size > AIF_MAX_FILE_BYTES) {
        fclose(file);
        vm_set_error(vm, "AIF file exceeds size limit");
        return NULL;
    }
    rewind(file);
    bytes = malloc((size_t)size);
    if (size > 0 && !bytes) {
        fclose(file);
        vm_set_error(vm, "out of memory");
        return NULL;
    }
    if (size > 0 && fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        vm_set_error(vm, "failed to read AIF file");
        return NULL;
    }
    fclose(file);
    *len = (size_t)size;
    return bytes;
}

static AIFProperty *find_property(AIFObject *object, const char *key) {
    uint32_t i;
    if (!object || !key) return NULL;
    for (i = 0; i < object->property_count; i++) {
        if (strcmp(object->properties[i].key, key) == 0) return &object->properties[i];
    }
    return NULL;
}

static const char *value_text(Value *value) {
    if (!value) return NULL;
    switch (value->tag) {
        case VAL_STRING:
        case VAL_NUMBER:
        case VAL_REF:
            return value->text;
        case VAL_BOOL:
            return value->boolean ? "true" : "false";
        case VAL_LIST:
            return NULL;
    }
    return NULL;
}

static const char *prop_text(AIFObject *object, const char *key) {
    AIFProperty *prop = find_property(object, key);
    return prop ? value_text(&prop->value) : NULL;
}

static int prop_int(AIFObject *object, const char *key, int fallback) {
    const char *text = prop_text(object, key);
    return text ? atoi(text) : fallback;
}

static float prop_float(AIFObject *object, const char *key, float fallback) {
    const char *text = prop_text(object, key);
    return text ? (float)atof(text) : fallback;
}

static AIFObject *find_object(InfraVM *vm, const char *object_id) {
    uint32_t i;
    AIFObject *match = NULL;
    size_t id_len;
    if (!vm || !object_id) return NULL;
    for (i = 0; i < vm->object_count; i++) {
        if (strcmp(vm->objects[i].object_id, object_id) == 0) return &vm->objects[i];
    }
    id_len = strlen(object_id);
    for (i = 0; i < vm->object_count; i++) {
        char *sep = strstr(vm->objects[i].object_id, "::");
        const char *local_id = sep ? sep + 2 : vm->objects[i].object_id;
        if (strcmp(local_id, object_id) == 0) {
            if (match) return NULL;
            match = &vm->objects[i];
        } else {
            size_t full_len = strlen(vm->objects[i].object_id);
            if (full_len > id_len + 2 &&
                strcmp(vm->objects[i].object_id + full_len - id_len, object_id) == 0 &&
                vm->objects[i].object_id[full_len - id_len - 1] == ':') {
                if (match) return NULL;
                match = &vm->objects[i];
            }
        }
    }
    if (match) return match;
    return NULL;
}

static char *object_ref_for_local(AIFObject *context, const char *kind, const char *name) {
    const char *sep;
    size_t ns_len = 0;
    size_t needed;
    char *out;
    if (!kind || !name) return NULL;
    if (strstr(name, "::")) {
        needed = strlen(name) + 1;
        out = malloc(needed);
        if (out) snprintf(out, needed, "%s", name);
        return out;
    }
    sep = context && context->object_id ? strstr(context->object_id, "::") : NULL;
    if (sep) ns_len = (size_t)(sep - context->object_id);
    needed = ns_len + (ns_len ? 2 : 0) + strlen(kind) + 1 + strlen(name) + 1;
    out = malloc(needed);
    if (!out) return NULL;
    if (ns_len) {
        snprintf(out, needed, "%.*s::%s:%s", (int)ns_len, context->object_id, kind, name);
    } else {
        snprintf(out, needed, "%s:%s", kind, name);
    }
    return out;
}

static AIFObject *find_start(InfraVM *vm) {
    uint32_t i;
    if (!vm) return NULL;
    for (i = 0; i < vm->object_count; i++) {
        if (vm->objects[i].start_flag) return &vm->objects[i];
    }
    return vm->object_count ? &vm->objects[0] : NULL;
}

static char *replace_input(const char *template_text, const char *input) {
    const char *needle = "{input}";
    size_t needle_len = strlen(needle);
    size_t input_len = input ? strlen(input) : 0;
    const char *p = template_text ? template_text : "";
    size_t out_len = 0;
    char *out;
    char *w;

    while (*p) {
        const char *hit = strstr(p, needle);
        if (!hit) {
            out_len += strlen(p);
            break;
        }
        out_len += (size_t)(hit - p) + input_len;
        p = hit + needle_len;
    }
    out = malloc(out_len + 1);
    if (!out) return NULL;
    p = template_text ? template_text : "";
    w = out;
    while (*p) {
        const char *hit = strstr(p, needle);
        if (!hit) {
            size_t tail = strlen(p);
            memcpy(w, p, tail);
            w += tail;
            break;
        }
        memcpy(w, p, (size_t)(hit - p));
        w += (size_t)(hit - p);
        if (input_len) {
            memcpy(w, input, input_len);
            w += input_len;
        }
        p = hit + needle_len;
    }
    *w = '\0';
    return out;
}

InfraVM *infravm_create(void) {
    InfraVM *vm = calloc(1, sizeof(InfraVM));
    if (!vm) return NULL;
    return vm;
}

void infravm_destroy(InfraVM *vm) {
    if (!vm) return;
    clear_vm(vm);
    free(vm);
}

int infravm_load_file(InfraVM *vm, const char *path) {
    Reader reader;
    unsigned char *bytes;
    size_t len = 0;
    uint32_t object_count;
    uint32_t i;
    uint32_t j;
    int ok = 1;

    if (!vm || !path) return 0;
    clear_vm(vm);
    vm->last_error[0] = '\0';

    bytes = read_file_bytes(vm, path, &len);
    if (!bytes && len == 0) return 0;
    reader.bytes = bytes;
    reader.len = len;
    reader.pos = 0;

    if (len < 10 || memcmp(bytes, "AIF0", 4) != 0) {
        free(bytes);
        vm_set_error(vm, "not an AIF file");
        return 0;
    }
    reader.pos = 4;
    if (read_u16(vm, &reader, &ok) != AIF_VERSION || !ok) {
        free(bytes);
        vm_set_error(vm, "unsupported AIF version");
        return 0;
    }
    object_count = read_u32(vm, &reader, &ok);
    if (!ok) {
        free(bytes);
        return 0;
    }
    if (object_count > AIF_MAX_OBJECTS) {
        free(bytes);
        vm_set_error(vm, "AIF object count exceeds limit");
        return 0;
    }
    vm->objects = xcalloc(vm, object_count, sizeof(AIFObject));
    if (!vm->objects) {
        free(bytes);
        return 0;
    }
    vm->object_count = object_count;

    for (i = 0; i < object_count && ok; i++) {
        AIFObject *object = &vm->objects[i];
        object->object_id = read_string(vm, &reader, &ok);
        object->name = read_string(vm, &reader, &ok);
        object->type = read_string(vm, &reader, &ok);
        object->start_flag = read_u8(vm, &reader, &ok) != 0;
        object->property_count = read_u32(vm, &reader, &ok);
        if (ok && object->property_count > AIF_MAX_PROPERTIES) {
            vm_set_error(vm, "AIF property count exceeds limit");
            ok = 0;
        }
        if (object->property_count > 0) {
            object->properties = xcalloc(vm, object->property_count, sizeof(AIFProperty));
            if (!object->properties) ok = 0;
        }
        for (j = 0; j < object->property_count && ok; j++) {
            object->properties[j].key = read_string(vm, &reader, &ok);
            object->properties[j].value = read_value(vm, &reader, &ok);
        }
        object->pointer_count = read_u32(vm, &reader, &ok);
        if (ok && object->pointer_count > AIF_MAX_POINTERS) {
            vm_set_error(vm, "AIF pointer count exceeds limit");
            ok = 0;
        }
        if (object->pointer_count > 0) {
            object->pointers = xcalloc(vm, object->pointer_count, sizeof(AIFPointer));
            if (!object->pointers) ok = 0;
        }
        for (j = 0; j < object->pointer_count && ok; j++) {
            object->pointers[j].pointer_type = read_string(vm, &reader, &ok);
            object->pointers[j].target_object_id = read_string(vm, &reader, &ok);
        }
        if (read_u32(vm, &reader, &ok) != 0 && ok) {
            vm_set_error(vm, "AIF instructions are reserved but not executable in v1");
            ok = 0;
        }
    }

    free(bytes);
    if (!ok) {
        clear_vm(vm);
        return 0;
    }
    return 1;
}

static int pointrun_agent(InfraVM *vm, AIFObject *agent, const char *input) {
    const char *model_ref = prop_text(agent, "model");
    const char *prompt_ref = prop_text(agent, "prompt");
    char *model_id;
    char *prompt_id;
    AIFObject *model;
    AIFObject *prompt_object;
    const char *template_text;
    char *final_prompt;
    const char *engine;

    model_id = object_ref_for_local(agent, "model", model_ref ? model_ref : "");
    prompt_id = object_ref_for_local(agent, "prompt", prompt_ref ? prompt_ref : "");
    if (!model_id || !prompt_id) {
        free(model_id);
        free(prompt_id);
        vm_set_error(vm, "failed to allocate object reference");
        return 0;
    }
    model = find_object(vm, model_id);
    prompt_object = find_object(vm, prompt_id);
    free(model_id);
    free(prompt_id);
    if (!model) {
        vm_set_error(vm, "agent model pointer could not be resolved");
        return 0;
    }
    template_text = prop_text(prompt_object, "text");
    final_prompt = replace_input(template_text ? template_text : (input ? input : ""), input ? input : "");
    if (!final_prompt) {
        vm_set_error(vm, "failed to build final prompt");
        return 0;
    }

    optimizer_compact_prompt(final_prompt);
    printf("PointRun final prompt: %s\n", final_prompt);
    engine = prop_text(model, "engine");
    switch (optimizer_provider_from_engine(engine)) {
    case OPT_PROVIDER_OPENAI: {
        VMOpenAIConfig config;
        VMOpenAIResult result;
        memset(&config, 0, sizeof(config));
        config.model = prop_text(model, "name");
        config.max_output_tokens = prop_int(model, "max_output_tokens", 512);
        config.temperature = prop_float(model, "temperature", 0.7f);
        printf("InfraVM: calling OpenAI model %s\n", config.model ? config.model : VM_OPENAI_DEFAULT_MODEL);
        result = vm_openai_generate(config, final_prompt);
        free(final_prompt);
        if (!result.ok) {
            vm_set_errorf(vm, "OpenAI connector failed", result.error);
            printf("InfraVM error: %s\n", infravm_last_error(vm));
            vm_openai_free_result(&result);
            return 0;
        }
        printf("InfraVM output: %s\n", result.text ? result.text : "");
        vm_openai_free_result(&result);
        return 1;
    }
    case OPT_PROVIDER_OLLAMA: {
        VMOllamaResult result = vm_ollama_generate(prop_text(model, "name"), final_prompt);
        free(final_prompt);
        if (!result.ok) {
            vm_set_errorf(vm, "Ollama connector failed", result.error);
            printf("InfraVM error: %s\n", infravm_last_error(vm));
            vm_ollama_free_result(&result);
            return 0;
        }
        if (result.error[0]) {
            printf("InfraVM notice: %s\n", result.error);
        }
        printf("InfraVM output: %s\n", result.text ? result.text : "");
        vm_ollama_free_result(&result);
        return 1;
    }
    case OPT_PROVIDER_ANTHROPIC:
    case OPT_PROVIDER_GEMINI:
    case OPT_PROVIDER_MICROSOFT:
    case OPT_PROVIDER_DEEPSEEK:
    case OPT_PROVIDER_HUGGINGFACE:
        printf("InfraVM stub: %s connector is registered but not live in v0.1\n",
               optimizer_provider_name(optimizer_provider_from_engine(engine)));
        free(final_prompt);
        return 1;
    case OPT_PROVIDER_UNKNOWN:
    default:
        printf("InfraVM stub: unsupported model engine `%s`\n", engine ? engine : "(missing)");
        break;
    }
    free(final_prompt);
    return 1;
}

static int pointrun_object(InfraVM *vm, const char *object_id, const char *input) {
    AIFObject *object;
    if (!vm || !object_id) return 0;
    object = find_object(vm, object_id);
    if (!object) {
        vm_set_errorf(vm, "object not found", object_id);
        return 0;
    }

    printf("PointRun start: %s (%s)\n", object->object_id, object->type);
    if (strcmp(object->type, "run") == 0) {
        const char *run_input = prop_text(object, "input");
        if (object->pointer_count == 0) {
            vm_set_error(vm, "run object has no runs pointer");
            return 0;
        }
        return pointrun_object(vm, object->pointers[0].target_object_id, run_input ? run_input : input);
    }
    if (strcmp(object->type, "agent") == 0) {
        return pointrun_agent(vm, object, input ? input : "");
    }
    if (strcmp(object->type, "port") == 0 && object->pointer_count > 0) {
        return pointrun_object(vm, object->pointers[0].target_object_id, input);
    }
    if (strcmp(object->type, "model") == 0) {
        printf("InfraVM stub: model objects execute through agents in v0.1\n");
        return 1;
    }
    printf("InfraVM stub: object type `%s` is not executable yet\n", object->type);
    return 1;
}

int infravm_pointrun(InfraVM *vm, const char *object_id) {
    if (!vm) return 0;
    vm->last_error[0] = '\0';
    return pointrun_object(vm, object_id, "");
}

int infravm_run_start(InfraVM *vm) {
    AIFObject *start = find_start(vm);
    if (!start) {
        vm_set_error(vm, "no start object found");
        return 0;
    }
    return infravm_pointrun(vm, start->object_id);
}

const char *infravm_last_error(InfraVM *vm) {
    return vm ? vm->last_error : "InfraVM is null";
}

static void debug_print_value(Value *value) {
    uint32_t i;
    if (!value) return;
    switch (value->tag) {
        case VAL_STRING: printf("\"%s\"", value->text); break;
        case VAL_NUMBER: printf("%s", value->text); break;
        case VAL_BOOL: printf("%s", value->boolean ? "true" : "false"); break;
        case VAL_REF: printf("%s", value->text); break;
        case VAL_LIST:
            printf("[");
            for (i = 0; i < value->item_count; i++) {
                if (i) printf(", ");
                debug_print_value(&value->items[i]);
            }
            printf("]");
            break;
    }
}

static void debug_print_registry(InfraVM *vm) {
    uint32_t i;
    uint32_t j;
    printf("InfraVM loaded %u AIF objects\n", vm->object_count);
    for (i = 0; i < vm->object_count; i++) {
        AIFObject *object = &vm->objects[i];
        printf("%s %s%s\n", object->type, object->object_id, object->start_flag ? " [start]" : "");
        for (j = 0; j < object->property_count; j++) {
            printf("  %s = ", object->properties[j].key);
            debug_print_value(&object->properties[j].value);
            printf("\n");
        }
        for (j = 0; j < object->pointer_count; j++) {
            printf("  -> %s %s\n", object->pointers[j].pointer_type, object->pointers[j].target_object_id);
        }
    }
}

int main(int argc, char **argv) {
    InfraVM *vm;
    int ok;
    int debug = 0;
    const char *file_path;
    const char *object_id = NULL;
    int argi = 1;
    if (argc >= 2 && strcmp(argv[1], "--debug") == 0) {
        debug = 1;
        argi++;
    }
    if (argc <= argi || argc > argi + 2 || strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
        fprintf(stderr, "Usage: %s [--debug] program.aif [object_id]\n", argv[0]);
        return argc == 2 ? 0 : 2;
    }
    file_path = argv[argi];
    if (argc == argi + 2) object_id = argv[argi + 1];

    vm = infravm_create();
    if (!vm) {
        fprintf(stderr, "InfraVM error: out of memory\n");
        return 1;
    }
    if (!infravm_load_file(vm, file_path)) {
        fprintf(stderr, "InfraVM error: %s\n", infravm_last_error(vm));
        infravm_destroy(vm);
        return 1;
    }
    if (debug) debug_print_registry(vm);
    ok = object_id ? infravm_pointrun(vm, object_id) : infravm_run_start(vm);
    if (!ok) {
        fprintf(stderr, "InfraVM error: %s\n", infravm_last_error(vm));
        infravm_destroy(vm);
        return 1;
    }
    infravm_destroy(vm);
    return 0;
}
