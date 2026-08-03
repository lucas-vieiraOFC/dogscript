CC      ?= gcc
AS      ?= as
LD      ?= ld
AR      ?= ar
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pedantic -I./src
PREFIX  ?= $(HOME)/.local
BINDIR  ?= $(PREFIX)/bin

SRC_DIR := src
RUNTIME_DIR := src/runtime

RUNTIME_CFLAGS := -D_GNU_SOURCE -D_XOPEN_SOURCE=700
RUNTIME_LDFLAGS :=

ifneq ($(shell pkg-config --exists sdl2 && echo yes),)
RUNTIME_CFLAGS += $(shell pkg-config --cflags sdl2 2>/dev/null)
ifneq ($(shell pkg-config --exists SDL2_ttf && echo yes),)
RUNTIME_CFLAGS += $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
RUNTIME_LDFLAGS += $(shell pkg-config --libs SDL2_ttf 2>/dev/null)
endif
RUNTIME_LDFLAGS += $(shell pkg-config --libs sdl2 2>/dev/null)
else
RUNTIME_CFLAGS += -DDOG_GUI_FALLBACK=1
endif

ifneq ($(shell pkg-config --exists openssl && echo yes),)
RUNTIME_CFLAGS += -DDOG_HAVE_OPENSSL=1
RUNTIME_LDFLAGS += $(shell pkg-config --libs openssl 2>/dev/null)
endif

COMPILER_SRCS = $(SRC_DIR)/dogscript.c \
                $(SRC_DIR)/cli.c \
                $(SRC_DIR)/lexer.c \
                $(SRC_DIR)/parser.c \
                $(SRC_DIR)/analyzer.c \
                $(SRC_DIR)/ir.c \
                $(SRC_DIR)/irgen.c \
                $(SRC_DIR)/asmemit.c \
                $(SRC_DIR)/dogsig.c \
                $(SRC_DIR)/utils.c

RUNTIME_SRCS = $(wildcard $(RUNTIME_DIR)/dog_*.c)
RUNTIME_OBJS = $(RUNTIME_SRCS:.c=.o)

.PHONY: all clean install uninstall

all: dog libdogruntime.a

dog: $(COMPILER_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

libdogruntime.a: $(RUNTIME_OBJS)
	$(AR) rcs $@ $^

$(RUNTIME_DIR)/%.o: $(RUNTIME_DIR)/%.c
	$(CC) $(CFLAGS) $(RUNTIME_CFLAGS) -c -o $@ $<

clean:
	rm -f dog libdogruntime.a $(RUNTIME_OBJS) /tmp/dog_*.s /tmp/dog_*.o /tmp/dog_*_err.log

install: all
	mkdir -p "$(BINDIR)"
	cp dog "$(BINDIR)/dog"
	cp libdogruntime.a "$(BINDIR)/libdogruntime.a"

uninstall:
	rm -f "$(BINDIR)/dog" "$(BINDIR)/libdogruntime.a"
