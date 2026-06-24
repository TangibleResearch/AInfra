#ifndef VM_JSON_H
#define VM_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Small header-only JSON tokenizer/parser for the VM.
 *
 * This is intentionally not a full DOM parser. It walks valid JSON tokens and
 * extracts string values by key without substring searching inside model text.
 */

char *vm_json_find_string_value(const char *json, const char *key);
char *vm_json_find_string_in_object_with_string(
    const char *json,
    const char *match_key,
    const char *match_value,
    const char *result_key
);

#ifdef __cplusplus
}
#endif

#endif

#if defined(VM_JSON_IMPLEMENTATION) && !defined(VM_JSON_IMPLEMENTED)
#define VM_JSON_IMPLEMENTED

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VM_JSON_EOF = 0,
    VM_JSON_LBRACE,
    VM_JSON_RBRACE,
    VM_JSON_LBRACKET,
    VM_JSON_RBRACKET,
    VM_JSON_COLON,
    VM_JSON_COMMA,
    VM_JSON_STRING,
    VM_JSON_NUMBER,
    VM_JSON_TRUE,
    VM_JSON_FALSE,
    VM_JSON_NULL,
    VM_JSON_ERROR
} VMJsonTokenKind;

typedef struct {
    VMJsonTokenKind kind;
    char *text;
} VMJsonToken;

typedef struct {
    const char *src;
    size_t pos;
} VMJsonParser;

static char *vm_json_strdup_range(const char *start, size_t len) {
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static int vm_json_append_char(char **out, size_t *len, size_t *cap, char ch) {
    char *next;
    if (*len + 2 > *cap) {
        size_t next_cap = *cap ? *cap * 2 : 32;
        next = (char *)realloc(*out, next_cap);
        if (!next) return 0;
        *out = next;
        *cap = next_cap;
    }
    (*out)[(*len)++] = ch;
    (*out)[*len] = '\0';
    return 1;
}

static void vm_json_skip_ws(VMJsonParser *parser) {
    while (parser->src[parser->pos] && isspace((unsigned char)parser->src[parser->pos])) {
        parser->pos++;
    }
}

static char *vm_json_read_string(VMJsonParser *parser) {
    char *out = NULL;
    size_t len = 0;
    size_t cap = 0;
    char ch;

    if (parser->src[parser->pos] != '"') return NULL;
    parser->pos++;
    while ((ch = parser->src[parser->pos++]) != '\0') {
        if (ch == '"') {
            return out ? out : vm_json_strdup_range("", 0);
        }
        if (ch == '\\') {
            ch = parser->src[parser->pos++];
            switch (ch) {
                case '"':
                case '\\':
                case '/':
                    if (!vm_json_append_char(&out, &len, &cap, ch)) goto oom;
                    break;
                case 'b':
                    if (!vm_json_append_char(&out, &len, &cap, '\b')) goto oom;
                    break;
                case 'f':
                    if (!vm_json_append_char(&out, &len, &cap, '\f')) goto oom;
                    break;
                case 'n':
                    if (!vm_json_append_char(&out, &len, &cap, '\n')) goto oom;
                    break;
                case 'r':
                    if (!vm_json_append_char(&out, &len, &cap, '\r')) goto oom;
                    break;
                case 't':
                    if (!vm_json_append_char(&out, &len, &cap, '\t')) goto oom;
                    break;
                case 'u':
                    /* Keep unicode escapes stable without pretending to decode UTF-16. */
                    if (!vm_json_append_char(&out, &len, &cap, '\\')) goto oom;
                    if (!vm_json_append_char(&out, &len, &cap, 'u')) goto oom;
                    for (int i = 0; i < 4; i++) {
                        ch = parser->src[parser->pos++];
                        if (!isxdigit((unsigned char)ch)) goto oom;
                        if (!vm_json_append_char(&out, &len, &cap, ch)) goto oom;
                    }
                    break;
                default:
                    goto oom;
            }
        } else {
            if ((unsigned char)ch < 0x20) goto oom;
            if (!vm_json_append_char(&out, &len, &cap, ch)) goto oom;
        }
    }

oom:
    free(out);
    return NULL;
}

static int vm_json_match_literal(VMJsonParser *parser, const char *literal) {
    size_t len = strlen(literal);
    if (strncmp(parser->src + parser->pos, literal, len) != 0) return 0;
    parser->pos += len;
    return 1;
}

static VMJsonToken vm_json_next(VMJsonParser *parser) {
    VMJsonToken token;
    const char *start;
    char ch;
    memset(&token, 0, sizeof(token));
    vm_json_skip_ws(parser);
    ch = parser->src[parser->pos];
    if (!ch) {
        token.kind = VM_JSON_EOF;
        return token;
    }
    parser->pos++;
    switch (ch) {
        case '{': token.kind = VM_JSON_LBRACE; return token;
        case '}': token.kind = VM_JSON_RBRACE; return token;
        case '[': token.kind = VM_JSON_LBRACKET; return token;
        case ']': token.kind = VM_JSON_RBRACKET; return token;
        case ':': token.kind = VM_JSON_COLON; return token;
        case ',': token.kind = VM_JSON_COMMA; return token;
        case '"':
            parser->pos--;
            token.kind = VM_JSON_STRING;
            token.text = vm_json_read_string(parser);
            if (!token.text) token.kind = VM_JSON_ERROR;
            return token;
        case 't':
            parser->pos--;
            token.kind = vm_json_match_literal(parser, "true") ? VM_JSON_TRUE : VM_JSON_ERROR;
            return token;
        case 'f':
            parser->pos--;
            token.kind = vm_json_match_literal(parser, "false") ? VM_JSON_FALSE : VM_JSON_ERROR;
            return token;
        case 'n':
            parser->pos--;
            token.kind = vm_json_match_literal(parser, "null") ? VM_JSON_NULL : VM_JSON_ERROR;
            return token;
        default:
            if (ch == '-' || isdigit((unsigned char)ch)) {
                parser->pos--;
                start = parser->src + parser->pos;
                if (parser->src[parser->pos] == '-') parser->pos++;
                while (isdigit((unsigned char)parser->src[parser->pos])) parser->pos++;
                if (parser->src[parser->pos] == '.') {
                    parser->pos++;
                    while (isdigit((unsigned char)parser->src[parser->pos])) parser->pos++;
                }
                if (parser->src[parser->pos] == 'e' || parser->src[parser->pos] == 'E') {
                    parser->pos++;
                    if (parser->src[parser->pos] == '+' || parser->src[parser->pos] == '-') parser->pos++;
                    while (isdigit((unsigned char)parser->src[parser->pos])) parser->pos++;
                }
                token.kind = VM_JSON_NUMBER;
                token.text = vm_json_strdup_range(start, (size_t)(parser->src + parser->pos - start));
                if (!token.text) token.kind = VM_JSON_ERROR;
                return token;
            }
            token.kind = VM_JSON_ERROR;
            return token;
    }
}

static void vm_json_free_token(VMJsonToken *token) {
    if (!token) return;
    free(token->text);
    token->text = NULL;
}

static int vm_json_search_from_token(VMJsonParser *parser, VMJsonToken token, const char *key, char **out);
static int vm_json_search_match_from_token(
    VMJsonParser *parser,
    VMJsonToken token,
    const char *match_key,
    const char *match_value,
    const char *result_key,
    char **out
);

static int vm_json_search_object(VMJsonParser *parser, const char *key, char **out) {
    for (;;) {
        VMJsonToken name = vm_json_next(parser);
        VMJsonToken colon;
        VMJsonToken value;
        int matched;

        if (name.kind == VM_JSON_RBRACE) return 0;
        if (name.kind != VM_JSON_STRING) {
            vm_json_free_token(&name);
            return 0;
        }
        colon = vm_json_next(parser);
        if (colon.kind != VM_JSON_COLON) {
            vm_json_free_token(&name);
            return 0;
        }
        value = vm_json_next(parser);
        matched = strcmp(name.text, key) == 0;
        vm_json_free_token(&name);
        if (matched && value.kind == VM_JSON_STRING) {
            *out = value.text;
            value.text = NULL;
            return 1;
        }
        if (vm_json_search_from_token(parser, value, key, out)) return 1;
        value = vm_json_next(parser);
        if (value.kind == VM_JSON_RBRACE) return 0;
        if (value.kind != VM_JSON_COMMA) {
            vm_json_free_token(&value);
            return 0;
        }
    }
}

static int vm_json_search_array(VMJsonParser *parser, const char *key, char **out) {
    for (;;) {
        VMJsonToken value = vm_json_next(parser);
        if (value.kind == VM_JSON_RBRACKET) return 0;
        if (vm_json_search_from_token(parser, value, key, out)) return 1;
        value = vm_json_next(parser);
        if (value.kind == VM_JSON_RBRACKET) return 0;
        if (value.kind != VM_JSON_COMMA) {
            vm_json_free_token(&value);
            return 0;
        }
    }
}

static int vm_json_search_from_token(VMJsonParser *parser, VMJsonToken token, const char *key, char **out) {
    switch (token.kind) {
        case VM_JSON_LBRACE:
            return vm_json_search_object(parser, key, out);
        case VM_JSON_LBRACKET:
            return vm_json_search_array(parser, key, out);
        case VM_JSON_STRING:
        case VM_JSON_NUMBER:
            vm_json_free_token(&token);
            return 0;
        default:
            return 0;
    }
}

char *vm_json_find_string_value(const char *json, const char *key) {
    VMJsonParser parser;
    VMJsonToken token;
    char *out = NULL;

    if (!json || !key || !*key) return NULL;
    parser.src = json;
    parser.pos = 0;
    token = vm_json_next(&parser);
    if (vm_json_search_from_token(&parser, token, key, &out)) return out;
    return NULL;
}

static int vm_json_search_match_object(
    VMJsonParser *parser,
    const char *match_key,
    const char *match_value,
    const char *result_key,
    char **out
) {
    int matched = 0;
    char *local_result = NULL;

    for (;;) {
        VMJsonToken name = vm_json_next(parser);
        VMJsonToken colon;
        VMJsonToken value;

        if (name.kind == VM_JSON_RBRACE) {
            if (matched && local_result) {
                *out = local_result;
                return 1;
            }
            free(local_result);
            return 0;
        }
        if (name.kind != VM_JSON_STRING) {
            vm_json_free_token(&name);
            free(local_result);
            return 0;
        }
        colon = vm_json_next(parser);
        if (colon.kind != VM_JSON_COLON) {
            vm_json_free_token(&name);
            free(local_result);
            return 0;
        }
        value = vm_json_next(parser);
        if (value.kind == VM_JSON_STRING) {
            if (strcmp(name.text, match_key) == 0 && strcmp(value.text, match_value) == 0) {
                matched = 1;
            }
            if (strcmp(name.text, result_key) == 0 && !local_result) {
                local_result = value.text;
                value.text = NULL;
            }
        } else if (vm_json_search_match_from_token(parser, value, match_key, match_value, result_key, out)) {
            vm_json_free_token(&name);
            free(local_result);
            return 1;
        }
        vm_json_free_token(&name);
        vm_json_free_token(&value);

        value = vm_json_next(parser);
        if (value.kind == VM_JSON_RBRACE) {
            if (matched && local_result) {
                *out = local_result;
                return 1;
            }
            free(local_result);
            return 0;
        }
        if (value.kind != VM_JSON_COMMA) {
            vm_json_free_token(&value);
            free(local_result);
            return 0;
        }
    }
}

static int vm_json_search_match_array(
    VMJsonParser *parser,
    const char *match_key,
    const char *match_value,
    const char *result_key,
    char **out
) {
    for (;;) {
        VMJsonToken value = vm_json_next(parser);
        if (value.kind == VM_JSON_RBRACKET) return 0;
        if (vm_json_search_match_from_token(parser, value, match_key, match_value, result_key, out)) return 1;
        value = vm_json_next(parser);
        if (value.kind == VM_JSON_RBRACKET) return 0;
        if (value.kind != VM_JSON_COMMA) {
            vm_json_free_token(&value);
            return 0;
        }
    }
}

static int vm_json_search_match_from_token(
    VMJsonParser *parser,
    VMJsonToken token,
    const char *match_key,
    const char *match_value,
    const char *result_key,
    char **out
) {
    switch (token.kind) {
        case VM_JSON_LBRACE:
            return vm_json_search_match_object(parser, match_key, match_value, result_key, out);
        case VM_JSON_LBRACKET:
            return vm_json_search_match_array(parser, match_key, match_value, result_key, out);
        case VM_JSON_STRING:
        case VM_JSON_NUMBER:
            vm_json_free_token(&token);
            return 0;
        default:
            return 0;
    }
}

char *vm_json_find_string_in_object_with_string(
    const char *json,
    const char *match_key,
    const char *match_value,
    const char *result_key
) {
    VMJsonParser parser;
    VMJsonToken token;
    char *out = NULL;

    if (!json || !match_key || !match_value || !result_key) return NULL;
    parser.src = json;
    parser.pos = 0;
    token = vm_json_next(&parser);
    if (vm_json_search_match_from_token(&parser, token, match_key, match_value, result_key, &out)) return out;
    return NULL;
}

#endif
