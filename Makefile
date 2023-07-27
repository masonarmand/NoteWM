WARNINGS = -Wpedantic -pedantic-errors
WARNINGS += -Werror
WARNINGS += -Wall
WARNINGS += -Wextra
WARNINGS += -Wold-style-definition
WARNINGS += -Wcast-align
WARNINGS += -Wformat=2
WARNINGS += -Wlogical-op
WARNINGS += -Wmissing-declarations
WARNINGS += -Wmissing-include-dirs
WARNINGS += -Wmissing-prototypes
WARNINGS += -Wnested-externs
WARNINGS += -Wpointer-arith
WARNINGS += -Wredundant-decls
WARNINGS += -Wsequence-point
WARNINGS += -Wshadow
WARNINGS += -Wstrict-prototypes
WARNINGS += -Wundef
WARNINGS += -Wunreachable-code
WARNINGS += -Wwrite-strings
WARNINGS += -Wdisabled-optimization
WARNINGS += -Wunsafe-loop-optimizations
WARNINGS += -Wfree-nonheap-object
WARNINGS += -Wswitch

CC = gcc
CFLAGS = -std=c89
LDFLAGS = -I./libs/inih -lX11
SOURCES = *.c ./libs/inih/ini.c
EXEC = notewm

.PHONY: all install clean

all: build

build:
	$(CC) -g $(CFLAGS) $(WARNINGS) $(SOURCES) $(LDFLAGS) -o $(EXEC)
	#$(CC) -g $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(EXEC)

run:
	./$(EXEC)

install: all
	install -m 755 $(EXEC) /usr/bin

clean:
	-rm $(EXEC)
