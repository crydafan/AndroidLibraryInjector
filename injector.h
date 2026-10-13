#ifndef INJECTOR_H
#define INJECTOR_H

#include <unistd.h>

int inject_library(pid_t pid, const char *libpath);

#endif /* INJECTOR_H */