#ifndef VM_OPENAI_H
#define VM_OPENAI_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Header-only OpenAI connector for the VM.
 *
 * Usage in exactly one C file:
 *   #define VM_OPENAI_IMPLEMENTATION
 *   #include "vm_openai.h"
 *
 * Link with libcurl, for example:
 *   cc -std=c11 vm.c -lcurl
 */

typedef struct {
    const char *model;
    const char *api_key;
    int max_output_tokens;
    float temperature;
} VMOpenAIConfig;

typedef struct {
    char *text;
    int ok;
    char error[512];
} VMOpenAIResult;

VMOpenAIResult vm_openai_generate(
    VMOpenAIConfig config,
    const char *prompt
);

void vm_openai_free_result(VMOpenAIResult *result);

#ifdef __cplusplus
}
#endif

#ifdef VM_OPENAI_IMPLEMENTATION

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VM_OPENAI_DEFAULT_MODEL
#define VM_OPENAI_DEFAULT_MODEL "gpt-4.1-mini"
#endif

#ifndef VM_OPENAI_ENDPOINT
#define VM_OPENAI_ENDPOINT "https://api.openai.com/v1/responses"
#endif

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} VMOpenAIBuffer;

static void vm_openai_set_error(VMOpenAIResult *result, const char *message) {
    if (!result) {
        return;
    }
    result->ok = 0;
    result->text = NULL;
    snprintf(result->error, sizeof(result->error), "%s", message ? message : "unknown OpenAI connector error");
}

static char *vm_openai_strdup(const char *text) {
    size_t len;
    char *copy;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static int vm_openai_buffer_append(VMOpenAIBuffer *buffer, const char *data, size_t len) {
    char *next;
    size_t needed;

    if (!buffer || !data) {
        return 0;
    }
    needed = buffer->len + len + 1;
    if (needed > buffer->cap) {
        size_t next_cap = buffer->cap ? buffer->cap : 1024;
        while (next_cap < needed) {
            next_cap *= 2;
        }
        next = (char *)realloc(buffer->data, next_cap);
        if (!next) {
            return 0;
        }
        buffer->data = next;
        buffer->cap = next_cap;
    }
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
    buffer->data[buffer->len] = '\0';
    return 1;
}

static size_t vm_openai_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    VMOpenAIBuffer *buffer = (VMOpenAIBuffer *)userdata;

    if (!vm_openai_buffer_append(buffer, ptr, total)) {
        return 0;
    }
    return total;
}

static char *vm_openai_json_escape(const char *text) {
    size_t len = 0;
    char *out;
    char *p;
    const unsigned char *s;

    if (!text) {
        text = "";
    }

    for (s = (const unsigned char *)text; *s; s++) {
        switch (*s) {
            case '"':
            case '\\':
                len += 2;
                break;
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                len += 2;
                break;
            default:
                if (*s < 0x20) {
                    len += 6;
                } else {
                    len += 1;
                }
                break;
        }
    }

    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }

    p = out;
    for (s = (const unsigned char *)text; *s; s++) {
        switch (*s) {
            case '"':
                *p++ = '\\';
                *p++ = '"';
                break;
            case '\\':
                *p++ = '\\';
                *p++ = '\\';
                break;
            case '\b':
                *p++ = '\\';
                *p++ = 'b';
                break;
            case '\f':
                *p++ = '\\';
                *p++ = 'f';
                break;
            case '\n':
                *p++ = '\\';
                *p++ = 'n';
                break;
            case '\r':
                *p++ = '\\';
                *p++ = 'r';
                break;
            case '\t':
                *p++ = '\\';
                *p++ = 't';
                break;
            default:
                if (*s < 0x20) {
                    sprintf(p, "\\u%04x", *s);
                    p += 6;
                } else {
                    *p++ = (char)*s;
                }
                break;
        }
    }
    *p = '\0';
    return out;
}

static char *vm_openai_build_payload(VMOpenAIConfig config, const char *prompt) {
    const char *model = config.model ? config.model : VM_OPENAI_DEFAULT_MODEL;
    int max_tokens = config.max_output_tokens > 0 ? config.max_output_tokens : 512;
    float temperature = config.temperature >= 0.0f ? config.temperature : 0.7f;
    char *model_json = vm_openai_json_escape(model);
    char *prompt_json = vm_openai_json_escape(prompt);
    char *payload = NULL;
    int needed;

    if (!model_json || !prompt_json) {
        free(model_json);
        free(prompt_json);
        return NULL;
    }

    needed = snprintf(
        NULL,
        0,
        "{\"model\":\"%s\",\"input\":\"%s\",\"max_output_tokens\":%d,\"temperature\":%.3f}",
        model_json,
        prompt_json,
        max_tokens,
        temperature
    );
    if (needed < 0) {
        free(model_json);
        free(prompt_json);
        return NULL;
    }

    payload = (char *)malloc((size_t)needed + 1);
    if (payload) {
        snprintf(
            payload,
            (size_t)needed + 1,
            "{\"model\":\"%s\",\"input\":\"%s\",\"max_output_tokens\":%d,\"temperature\":%.3f}",
            model_json,
            prompt_json,
            max_tokens,
            temperature
        );
    }

    free(model_json);
    free(prompt_json);
    return payload;
}

static const char *vm_openai_find_json_string_value(const char *json, const char *key) {
    char pattern[64];
    const char *p;
    const char *colon;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    while (p) {
        colon = p + strlen(pattern);
        while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
            colon++;
        }
        if (*colon == ':') {
            colon++;
            while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
                colon++;
            }
            if (*colon == '"') {
                return colon + 1;
            }
        }
        p = strstr(p + 1, pattern);
    }
    return NULL;
}

static char *vm_openai_parse_json_string(const char *start) {
    VMOpenAIBuffer out;
    const char *p;
    char ch;

    memset(&out, 0, sizeof(out));
    p = start;
    while (*p) {
        ch = *p++;
        if (ch == '"') {
            return out.data ? out.data : vm_openai_strdup("");
        }
        if (ch == '\\') {
            ch = *p++;
            switch (ch) {
                case '"':
                case '\\':
                case '/':
                    if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
                case 'b':
                    ch = '\b';
                    if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
                case 'f':
                    ch = '\f';
                    if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
                case 'n':
                    ch = '\n';
                    if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
                case 'r':
                    ch = '\r';
                    if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
                case 't':
                    ch = '\t';
                    if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
                case 'u':
                    /* Keep unicode escapes readable enough for the VM stub path. */
                    if (!vm_openai_buffer_append(&out, "\\u", 2)) goto oom;
                    if (!vm_openai_buffer_append(&out, p, 4)) goto oom;
                    p += 4;
                    break;
                default:
                    if (!ch || !vm_openai_buffer_append(&out, &ch, 1)) goto oom;
                    break;
            }
        } else {
            if (!vm_openai_buffer_append(&out, &ch, 1)) goto oom;
        }
    }

    free(out.data);
    return NULL;

oom:
    free(out.data);
    return NULL;
}

static char *vm_openai_extract_text(const char *json) {
    const char *start;

    if (!json) {
        return NULL;
    }

    start = vm_openai_find_json_string_value(json, "output_text");
    if (start) {
        return vm_openai_parse_json_string(start);
    }

    start = vm_openai_find_json_string_value(json, "text");
    if (start) {
        return vm_openai_parse_json_string(start);
    }

    return NULL;
}

static char *vm_openai_extract_error(const char *json) {
    const char *start;

    if (!json) {
        return NULL;
    }
    start = vm_openai_find_json_string_value(json, "message");
    if (start) {
        return vm_openai_parse_json_string(start);
    }
    start = vm_openai_find_json_string_value(json, "error");
    if (start) {
        return vm_openai_parse_json_string(start);
    }
    return NULL;
}

VMOpenAIResult vm_openai_generate(VMOpenAIConfig config, const char *prompt) {
    VMOpenAIResult result;
    const char *api_key = config.api_key ? config.api_key : getenv("OPENAI_API_KEY");
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    VMOpenAIBuffer response;
    char auth_header[1024];
    char curl_error[CURL_ERROR_SIZE];
    char *payload = NULL;
    char *text = NULL;
    char *api_error = NULL;
    long status = 0;
    CURLcode code;

    memset(&result, 0, sizeof(result));
    memset(&response, 0, sizeof(response));
    memset(curl_error, 0, sizeof(curl_error));

    if (!prompt || prompt[0] == '\0') {
        vm_openai_set_error(&result, "OpenAI prompt is empty");
        return result;
    }

    if (!api_key || api_key[0] == '\0') {
        vm_openai_set_error(&result, "OPENAI_API_KEY is not set");
        return result;
    }

    if (strlen(api_key) > sizeof(auth_header) - 32) {
        vm_openai_set_error(&result, "OPENAI_API_KEY is too long for connector buffer");
        return result;
    }

    payload = vm_openai_build_payload(config, prompt);
    if (!payload) {
        vm_openai_set_error(&result, "failed to build OpenAI request payload");
        return result;
    }

    curl = curl_easy_init();
    if (!curl) {
        free(payload);
        vm_openai_set_error(&result, "failed to initialize libcurl");
        return result;
    }

    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);
    if (!headers) {
        curl_easy_cleanup(curl);
        free(payload);
        vm_openai_set_error(&result, "failed to allocate OpenAI request headers");
        return result;
    }

    curl_easy_setopt(curl, CURLOPT_URL, VM_OPENAI_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, vm_openai_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "A2VM/0.1 VM OpenAI Connector");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(payload);

    if (code != CURLE_OK) {
        snprintf(result.error, sizeof(result.error), "OpenAI HTTPS request failed: %s", curl_error[0] ? curl_error : curl_easy_strerror(code));
        result.ok = 0;
        free(response.data);
        return result;
    }

    if (status < 200 || status >= 300) {
        api_error = vm_openai_extract_error(response.data);
        snprintf(result.error, sizeof(result.error), "OpenAI API returned HTTP %ld%s%s", status, api_error ? ": " : "", api_error ? api_error : "");
        result.ok = 0;
        free(api_error);
        free(response.data);
        return result;
    }

    text = vm_openai_extract_text(response.data);
    free(response.data);
    if (!text) {
        vm_openai_set_error(&result, "OpenAI response did not contain generated text");
        return result;
    }

    result.text = text;
    result.ok = 1;
    result.error[0] = '\0';
    return result;
}

void vm_openai_free_result(VMOpenAIResult *result) {
    if (!result) {
        return;
    }
    free(result->text);
    result->text = NULL;
    result->ok = 0;
    result->error[0] = '\0';
}

#endif /* VM_OPENAI_IMPLEMENTATION */

#endif /* VM_OPENAI_H */
