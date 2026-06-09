#ifndef INFRAVM_H
#define INFRAVM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct InfraVM InfraVM;

InfraVM *infravm_create(void);
void infravm_destroy(InfraVM *vm);

int infravm_load_file(InfraVM *vm, const char *path);
int infravm_run_start(InfraVM *vm);
int infravm_pointrun(InfraVM *vm, const char *object_id);

const char *infravm_last_error(InfraVM *vm);

#ifdef __cplusplus
}
#endif

#endif
