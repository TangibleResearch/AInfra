#ifndef AINFRA_OPTIMIZER_H
#define AINFRA_OPTIMIZER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    OPT_PROVIDER_UNKNOWN = 0,
    OPT_PROVIDER_OPENAI,
    OPT_PROVIDER_ANTHROPIC,
    OPT_PROVIDER_GEMINI,
    OPT_PROVIDER_MICROSOFT,
    OPT_PROVIDER_DEEPSEEK,
    OPT_PROVIDER_HUGGINGFACE,
    OPT_PROVIDER_OLLAMA
} OptimizerProvider;

uint64_t optimizer_fnv1a64(const char *text);
OptimizerProvider optimizer_provider_from_engine(const char *engine);
const char *optimizer_provider_name(OptimizerProvider provider);
size_t optimizer_compact_prompt(char *text);

#endif
