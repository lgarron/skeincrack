
# This is how the git Makefile does it:
# http://git.kernel.org/cgit/git/git.git/tree/Makefile?id=c965c029330b1f81cc107c5d829e7fd79c61d8ea#n175
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Defaults
CC = gcc
MARCH_SWITCH = 
DEFINES = -DSKEIN_LOOP=0
MARCH_SWITCH = -march=native

# Defaults (i.e. Linux)
ifeq ($(uname_S),Linux)
	# Currently all Linux optimizations are defaults.
endif

ifeq ($(uname_S),Darwin)
	# clang is faster than gcc on OSX for me (lgarron).
	CC = clang
	# "-march=native" is slower for clang on OSX for me (lgarron).
	MARCH_SWITCH = 
	# gcc is slow and has bad results for me (lgarron) if SKEIN_LOOP is defined. clang is fine.
	ifeq ($(CC),gcc)
		DEFINES = 
	endif
endif

CFLAGS = -O3 -Wall -ICD/Optimized_64bit --std=c99 -pthread -fno-strict-aliasing -flto $(MARCH_SWITCH)
SRCS = CD/Optimized_64bit/*.c skeincrack.c
TARGET = skeincrack

all: $(TARGET)

run: all
	./$(TARGET)

test: all
	./$(TARGET) --benchmark

skeincrack: Makefile skeincrack.c
	$(CC) $(CFLAGS) $(DEFINES) $(SRCS) -o $@

pgo: Makefile skeincrack.c
	$(CC) $(CFLAGS) $(DEFINES) -fprofile-generate $(SRCS) -o $(TARGET)
	./$(TARGET) --pgo
	$(CC) $(CFLAGS) $(DEFINES) -fprofile-use $(SRCS) -o $(TARGET)

.PHONY: clean
clean:
	rm -f *.o *.gcda $(TARGET)
