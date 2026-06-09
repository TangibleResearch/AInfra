#include "optimizer.h"

#include <ctype.h>
#include <string.h>

uint64_t optimizer_fnv1a64(const char *text) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    while (*p) {
        hash ^= (uint64_t)(*p++);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

static int engine_eq(const char *engine, const char *name) {
    size_t i = 0;
    if (!engine || !name) return 0;
    while (engine[i] && name[i]) {
        char a = (char)tolower((unsigned char)engine[i]);
        char b = (char)tolower((unsigned char)name[i]);
        if (a == '_') a = '-';
        if (b == '_') b = '-';
        if (a != b) return 0;
        i++;
    }
    return engine[i] == '\0' && name[i] == '\0';
}

OptimizerProvider optimizer_provider_from_engine(const char *engine) {
    while (engine && isspace((unsigned char)*engine)) engine++;
    if (engine_eq(engine, "openai")) return OPT_PROVIDER_OPENAI;
    if (engine_eq(engine, "anthropic") || engine_eq(engine, "claude")) return OPT_PROVIDER_ANTHROPIC;
    if (engine_eq(engine, "gemini") || engine_eq(engine, "google") || engine_eq(engine, "google-gemini")) return OPT_PROVIDER_GEMINI;
    if (engine_eq(engine, "microsoft") || engine_eq(engine, "azure") || engine_eq(engine, "azure-openai")) return OPT_PROVIDER_MICROSOFT;
    if (engine_eq(engine, "deepseek")) return OPT_PROVIDER_DEEPSEEK;
    if (engine_eq(engine, "huggingface") || engine_eq(engine, "hf")) return OPT_PROVIDER_HUGGINGFACE;
    if (engine_eq(engine, "ollama")) return OPT_PROVIDER_OLLAMA;
    return OPT_PROVIDER_UNKNOWN;
}

const char *optimizer_provider_name(OptimizerProvider provider) {
    switch (provider) {
        case OPT_PROVIDER_OPENAI: return "openai";
        case OPT_PROVIDER_ANTHROPIC: return "anthropic";
        case OPT_PROVIDER_GEMINI: return "gemini";
        case OPT_PROVIDER_MICROSOFT: return "microsoft";
        case OPT_PROVIDER_DEEPSEEK: return "deepseek";
        case OPT_PROVIDER_HUGGINGFACE: return "huggingface";
        case OPT_PROVIDER_OLLAMA: return "ollama";
        case OPT_PROVIDER_UNKNOWN:
        default: return "unknown";
    }
}

size_t optimizer_compact_prompt(char *text) {
    char *read = text;
    char *write = text;
    int in_space = 1;
    if (!text) return 0;
    while (*read) {
        if (isspace((unsigned char)*read)) {
            if (!in_space) {
                *write++ = ' ';
                in_space = 1;
            }
        } else {
            *write++ = *read;
            in_space = 0;
        }
        read++;
    }
    if (write > text && write[-1] == ' ') write--;
    *write = '\0';
    return (size_t)(write - text);
}
