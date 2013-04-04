
# This is how the git Makefile does it:
# http://git.kernel.org/cgit/git/git.git/tree/Makefile?id=c965c029330b1f81cc107c5d829e7fd79c61d8ea#n175
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Defaults
CC = gcc
MARCH_SWITCH = 

# Defaults (i.e. Linux)
ifeq ($(uname_S),Linux)
	MARCH_SWITCH = -march=native
endif

ifeq ($(uname_S),Darwin)
	CC = clang
endif

CFLAGS = -O3 -Wall -ICD/Optimized_64bit --std=c99 -pthread -fno-strict-aliasing -flto $(MARCH_SWITCH)
DEFINES = -DSKEIN_LOOP=0
SRCS = CD/Optimized_64bit/*.c skeincrack.c

TARGET = skeincrack

all: $(TARGET)

run: all
	./$(TARGET)

skeincrack: Makefile skeincrack.c
	$(CC) $(CFLAGS) $(DEFINES) $(SRCS) -o $@

pgo: Makefile skeincrack.c
	$(CC) $(CFLAGS) $(DEFINES) -fprofile-generate $(SRCS) -o $(TARGET)
	./$(TARGET) --pgo
	$(CC) $(CFLAGS) $(DEFINES) -fprofile-use $(SRCS) -o $(TARGET)

.PHONY: clean
clean:
	rm -f *.o *.gcda $(TARGET)
