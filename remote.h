#ifndef REMOTE_H
#define REMOTE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define remotecall2(pid, module, fun, ...)                                     \
  remotecall(pid, module, fun, (long[]){__VA_ARGS__},                          \
             sizeof((long[]){__VA_ARGS__}) / sizeof(long))

#define remotecall3(pid, module, fun) remotecall(pid, module, fun, NULL, 0)

int remoteattach(pid_t pid);
int remotedetach(pid_t pid);

int remoteread(pid_t pid, void *addr, uint8_t *buf, size_t size);
int remotewrite(pid_t pid, void *addr, uint8_t *buf, size_t size);

long remotecall(pid_t pid, const char *module, void *fun, long *argv,
                size_t argc);

#ifdef __cplusplus
}
#endif

#endif /* REMOTE_H */
