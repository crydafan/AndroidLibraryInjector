NDK_BUILD_MAKEFILE = $(ANDROID_NDK)/build/core/build-local.mk
NDK_BUILD_ARGS = -f "$(NDK_BUILD_MAKEFILE)" \
	NDK_PROJECT_PATH=. \
	APP_BUILD_SCRIPT=Android.mk \
	NDK_APPLICATION_MK=Application.mk \
	NDK_DEBUG=0

.PHONY: all build compile_commands

all: build

build:
	@$(MAKE) $(NDK_BUILD_ARGS)

compile_commands:
	@compiledb -o compile_commands.json make -n $(NDK_BUILD_ARGS) 2>&1>/dev/null
	@printf '%s/compile_commands.json\n' "$(CURDIR)"
