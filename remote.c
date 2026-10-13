#include <elf.h>
#include <linux/ptrace.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include "remote.h"

#define LOG_TAG "RemoteProcess"
#include "logger.h"

#ifdef __aarch64__
typedef struct user_pt_regs arm_regs_t;
static size_t arm_param_regs_len = 8;
#else
typedef struct pt_regs arm_regs_t;
static size_t arm_param_regs_len = 4;
#endif
static size_t cpsr_t_mask = (1u << 5);

static inline long ptrace2(int request, pid_t pid, void *addr, void *data) {
  long ret = ptrace(request, pid, addr, data);
  LOGD("ptrace with request: %d, pid: %d, addr: %p and data: %p returned %lX ",
       request, pid, addr, data, ret);
  return ret;
}

int remoteattach(pid_t pid) {
  return ptrace2(PTRACE_ATTACH, pid, NULL, NULL) != 0 ||
         waitpid(pid, 0, WUNTRACED) != pid;
}

int remotedetach(pid_t pid) {
  return ptrace2(PTRACE_DETACH, pid, NULL, NULL) != 0;
}

int remotewrite(pid_t pid, void *addr, uint8_t *buf, size_t size) {
  int chunks = size / sizeof(long);
  for (int i = 0; i < chunks; i++) {
    long word = 0;
    long offby = i * sizeof(long);
    memcpy(&word, buf + offby, sizeof(word));
    if (ptrace2(PTRACE_POKETEXT, pid, (void *)((uintptr_t)addr + offby),
                (void *)word))
      return 1;
  }
  int nudge = size % sizeof(long);
  if (!nudge)
    return 0;
  long offby = chunks * sizeof(long);
  long word = ptrace2(PTRACE_PEEKTEXT, pid, (void *)(addr + offby), 0);
  memcpy(&word, buf + offby, nudge);
  if (ptrace2(PTRACE_POKETEXT, pid, (void *)((uintptr_t)addr + offby),
              (void *)word) != 0)
    return 1;
  return 0;
}

int remoteread(pid_t pid, void *addr, uint8_t *buf, size_t size) {
  int chunks = size / sizeof(long);
  for (int i = 0; i < chunks; i++) {
    long offby = i * sizeof(long);
    long word =
        ptrace2(PTRACE_PEEKTEXT, pid, (void *)((uintptr_t)addr + offby), 0);
    memcpy(buf + offby, &word, sizeof(word));
  }
  int nudge = size % sizeof(long);
  if (!nudge)
    return 0;
  long offby = chunks * sizeof(long);
  long word =
      ptrace2(PTRACE_PEEKTEXT, pid, (void *)((uintptr_t)addr + offby), 0);
  memcpy(buf + offby, (uint8_t *)&word, nudge);
  return 0;
}

static inline int getregs(pid_t pid, arm_regs_t *regs) {
#ifdef __aarch64__
  struct iovec iov;
  iov.iov_base = regs;
  iov.iov_len = sizeof(*regs);
  return ptrace2(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &iov);
#else
  return ptrace2(PTRACE_GETREGS, pid, NULL, regs);
#endif
}

static inline int setregs(pid_t pid, arm_regs_t *regs) {
#ifdef __aarch64__
  struct iovec iov;
  iov.iov_base = regs;
  iov.iov_len = sizeof(*regs);
  return ptrace2(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &iov);
#else
  return ptrace2(PTRACE_SETREGS, pid, NULL, regs);
#endif
}

static inline uintptr_t getmodulebase(pid_t pid, const char *module) {
  char path[64];
  if (pid > 0) {
    snprintf(path, sizeof(path), "/proc/%d/maps", (int)pid);
  } else {
    snprintf(path, sizeof(path), "/proc/self/maps");
  }
  FILE *file = fopen(path, "r");
  if (!file)
    return 0;
  char line[PATH_MAX];
  uintptr_t base = 0;
  while (fgets(line, sizeof line, file)) {
    if (strstr(line, module)) {
      base = (uintptr_t)strtoull(line, NULL, 16);
      break;
    }
  }
  fclose(file);
  return base;
}

long remotecall(pid_t pid, const char *module, void *fun, long *argv,
                size_t argc) {
  arm_regs_t curr, back;
  long ret;
  if (getregs(pid, &curr)) {
    LOGE("failed to get registers");
    return 0xdeadbeef;
  }
  memcpy(&back, &curr, sizeof(back));
  for (size_t i = 0; i < argc && i < arm_param_regs_len; i++) {
#ifdef __aarch64__
    curr.regs[i] = argv[i];
#else
    curr.uregs[i] = argv[i];
#endif
  }
  if (argc > arm_param_regs_len) {
    size_t offby = (argc - arm_param_regs_len) * sizeof(*argv);
#ifdef __aarch64__
    // SP mod 16 = 0
    // The stack must be quad-word aligned
    curr.sp = (curr.sp - offby) & ~(arm_param_regs_len * 2 - 1);
    ret = remotewrite(pid, (void *)curr.sp,
                      (uint8_t *)&argv[arm_param_regs_len], offby);
    if (ret) {
      LOGE("failed to write stack registers");
      return 0xdeadbeef;
    }
#else
    curr.ARM_sp = (curr.ARM_sp - offby) & ~(arm_param_regs_len * 2 - 1);
    ret = remotewrite(pid, (void *)curr.ARM_sp,
                      (uint8_t *)&argv[arm_param_regs_len], offby);
    if (ret) {
      LOGE("failed to write stack registers");
      return 0xdeadbeef;
    }
#endif
  }
  uintptr_t offby = (uintptr_t)fun - getmodulebase(-1, module);
  uintptr_t func = getmodulebase(pid, module) + offby;
#ifdef __aarch64__
  curr.pc = func;
#else
  curr.ARM_pc = func;
#endif

#ifdef __aarch64__
  if (curr.pc & 1) {
    // Thumb
    curr.pc &= (~1u);
    curr.pstate |= cpsr_t_mask;
  } else {
    // ARM
    curr.pstate &= ~cpsr_t_mask;
  }
#else
  if (curr.ARM_pc & 1) {
    // Thumb
    curr.ARM_pc &= (~1u);
    curr.ARM_cpsr |= cpsr_t_mask;
  } else {
    // ARM
    curr.ARM_cpsr &= ~cpsr_t_mask;
  }
#endif
#ifdef __aarch64__
  curr.regs[30] = getmodulebase(pid, "libc.so");
#else
  curr.ARM_lr = getmodulebase(pid, "libc.so");
#endif
  if (setregs(pid, &curr) || ptrace2(PTRACE_CONT, pid, 0, 0) != 0) {
    LOGE("failed to set registers or continue target process");
    return 0xdeadbeef;
  }
  int status = 0;
  do {
    waitpid(pid, &status, WUNTRACED);
  } while (!WIFSTOPPED(status));
  if (getregs(pid, &curr)) {
    LOGE("failed to get registers after call");
    return 0xdeadbeef;
  }
#ifdef __aarch64__
  ret = curr.regs[0];
#else
  ret = curr.ARM_r0;
#endif
  if (setregs(pid, &back)) {
    LOGE("failed to set backup registers after call");
    return 0xdeadbeef;
  }
  if (ptrace2(PTRACE_CONT, pid, 0, 0) != 0) {
    LOGE("failed to continue target process");
    return 0xdeadbeef;
  }
  return ret;
}
