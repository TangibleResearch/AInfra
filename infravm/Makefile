CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDLIBS ?= -lcurl

.PHONY: all clean

all: infravm

infravm: infravm.c infravm.h vm_openai.h vm_ollama.h ../optimizer/c/optimizer.c ../optimizer/c/optimizer.h
	$(CC) $(CFLAGS) -I../optimizer/c -o $@ infravm.c ../optimizer/c/optimizer.c $(LDLIBS)

clean:
	rm -f infravm
