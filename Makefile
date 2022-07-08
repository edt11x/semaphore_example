
all:
	gcc \
   -pedantic \
   -W \
   -Wall \
   -Wcast-qual \
   -Wconversion \
   -Wempty-body \
   -Wextra \
   -Wimplicit-fallthrough \
   -Wlogical-not-parentheses \
   -Wmissing-declarations \
   -Wmissing-prototypes \
   -Wpointer-arith \
   -Wshadow \
   -Wsizeof-array-argument \
   -Wstrict-prototypes \
   -Wswitch \
   -Wundef \
   -Wuninitialized \
   -Wunreachable-code \
   -Wunused-label \
   -Wwrite-strings \
	semexample.c -o semexample -lrt
