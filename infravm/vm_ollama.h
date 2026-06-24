#ifndef VM_OLLAMA_H
#define VM_OLLAMA_H

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    int ok;
    char error[256];
} VMOllamaResult;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} VMOllamaBuffer;

static int vm_ollama_append(VMOllamaBuffer *buffer, const char *data, size_t len) {
    char *next;
    size_t needed;
    if (!buffer || !data) return 0;
    needed = buffer->len + len + 1;
    if (needed > buffer->cap) {
        size_t next_cap = buffer->cap ? buffer->cap : 1024;
        while (next_cap < needed) next_cap *= 2;
        next = (char *)realloc(buffer->data, next_cap);
        if (!next) return 0;
        buffer->data = next;
        buffer->cap = next_cap;
    }
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
    buffer->data[buffer->len] = '\0';
    return 1;
}

static size_t vm_ollama_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    return vm_ollama_append((VMOllamaBuffer *)userdata, ptr, total) ? total : 0;
}

static char *vm_ollama_escape(const char *text) {
    size_t len = 0;
    char *out;
    char *p;
    const unsigned char *s;
    if (!text) text = "";
    for (s = (const unsigned char *)text; *s; s++) {
        switch (*s) {
            case '"':
            case '\\':
            case '\n':
            case '\r':
            case '\t':
                len += 2;
                break;
            default:
                len += *s < 0x20 ? 6 : 1;
                break;
        }
    }
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    p = out;
    for (s = (const unsigned char *)text; *s; s++) {
        switch (*s) {
            case '"': *p++ = '\\'; *p++ = '"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
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

static char *vm_ollama_fallback_text(const char *model, const char *prompt) {
    const char *prefix = "Local VM output";
    int needed = snprintf(NULL, 0, "%s (%s): %s", prefix, model ? model : "ollama", prompt ? prompt : "");
    char *out;
    if (needed < 0) return NULL;
    out = (char *)malloc((size_t)needed + 1);
    if (!out) return NULL;
    snprintf(out, (size_t)needed + 1, "%s (%s): %s", prefix, model ? model : "ollama", prompt ? prompt : "");
    return out;
}

static VMOllamaResult vm_ollama_generate(const char *model, const char *prompt) {
    VMOllamaResult result = {0};
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    VMOllamaBuffer response = {0};
    char *model_json = vm_ollama_escape(model ? model : "llama3.2");
    char *prompt_json = vm_ollama_escape(prompt ? prompt : "");
    char *payload = NULL;
    char *parsed = NULL;
    long status = 0;
    int needed;

    if (!model_json || !prompt_json) {
        snprintf(result.error, sizeof(result.error), "failed to allocate Ollama request");
        goto fallback;
    }
    needed = snprintf(NULL, 0, "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}", model_json, prompt_json);
    if (needed < 0) {
        snprintf(result.error, sizeof(result.error), "failed to build Ollama request");
        goto fallback;
    }
    payload = (char *)malloc((size_t)needed + 1);
    if (!payload) {
        snprintf(result.error, sizeof(result.error), "failed to allocate Ollama payload");
        goto fallback;
    }
    snprintf(payload, (size_t)needed + 1, "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}", model_json, prompt_json);

    curl = curl_easy_init();
    if (!curl) {
        snprintf(result.error, sizeof(result.error), "libcurl init failed");
        goto fallback;
    }
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:11434/api/generate");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, vm_ollama_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    if (curl_easy_perform(curl) == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if (status >= 200 && status < 300) {
            parsed = vm_json_find_string_value(response.data, "response");
            if (parsed) {
                result.text = parsed;
                result.ok = 1;
                goto done;
            }
        }
    }
    snprintf(result.error, sizeof(result.error), "Ollama is not reachable, using deterministic local VM output");

fallback:
    result.text = vm_ollama_fallback_text(model, prompt);
    result.ok = result.text != NULL;
    if (!result.ok && result.error[0] == '\0') {
        snprintf(result.error, sizeof(result.error), "failed to produce local VM output");
    }

done:
    free(model_json);
    free(prompt_json);
    free(payload);
    free(response.data);
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    return result;
}

static void vm_ollama_free_result(VMOllamaResult *result) {
    if (!result) return;
    free(result->text);
    result->text = NULL;
    result->ok = 0;
    result->error[0] = '\0';
}

#endif
