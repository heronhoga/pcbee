CC ?= cc
APP := build/pcbee
RAYLIB_DIR := .deps/raylib
RAYLIB := $(RAYLIB_DIR)/src/libraylib.a
CPPFLAGS += -Isrc -I$(RAYLIB_DIR)/src
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
LDLIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
LDLIBS := -lGL -lm -lpthread -ldl -lrt -lX11
endif

.PHONY: all run test sanitize clean
all: $(APP)

$(RAYLIB_DIR)/src/Makefile:
	mkdir -p .deps
	git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git $(RAYLIB_DIR)

$(RAYLIB): $(RAYLIB_DIR)/src/Makefile
	$(MAKE) -C $(RAYLIB_DIR)/src PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC

build:
	mkdir -p build

$(APP): src/main.c src/pcb.c src/pcb.h $(RAYLIB) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/pcb.c $(RAYLIB) $(LDFLAGS) $(LDLIBS) -o $@

run: $(APP)
	./$(APP)

build/test_pcb: tests/test_pcb.c src/pcb.c src/pcb.h | build
	$(CC) -Isrc $(CFLAGS) tests/test_pcb.c src/pcb.c -lm -o $@

test: build/test_pcb
	./build/test_pcb

sanitize: | build
	$(CC) -Isrc -std=c99 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined tests/test_pcb.c src/pcb.c -lm -o build/test_pcb_sanitize
	./build/test_pcb_sanitize

clean:
	rm -rf build
