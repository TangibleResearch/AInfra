#ifndef VM_OLLAMA_H
#define VM_OLLAMA_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *text;
    int ok;
    char error[256];
} VMOllamaResult;

static VMOllamaResult vm_ollama_generate(const char *model, const char *prompt) {
    VMOllamaResult result = {0};
    (void)model;
    (void)prompt;
    result.ok = 0;
    snprintf(result.error, sizeof(result.error), "Ollama connector is a VM v0.1 stub");
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
