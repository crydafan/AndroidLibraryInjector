#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "Injector"
#include "logger.h"

#include "injector.h"
#include "remote.h"

int inject_library(pid_t pid, const char *libpath) {
  remoteattach(pid);
  void *remotebuf = (void *)remotecall2(pid, "libc.so", malloc, 1024);
  remotewrite(pid, remotebuf, (uint8_t *)libpath, strlen(libpath) + 1);
  void *libhandle = (void *)remotecall2(
      pid, "libdl.so", dlopen, (uintptr_t)remotebuf, RTLD_NOW | RTLD_GLOBAL);
  LOGI("remote dlopen returned %p", libhandle);
  if (libhandle) {
    remotedetach(pid);
    return 0;
  }
  char *remoteerr = (char *)remotecall3(pid, "libdl.so", dlerror);
  char err[1024] = {0};
  remoteread(pid, remoteerr, (uint8_t *)err, sizeof(err));
  LOGE("remote dlerror returned '%s'", err);
  remotedetach(pid);
  return 1;
}
