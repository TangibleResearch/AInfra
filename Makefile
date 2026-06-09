CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2
LDLIBS ?= -lcurl

.PHONY: all compiler vm clean example optimizer ainfra-compiler infravm ainfra-example

all: compiler vm optimizer

compiler:
	cargo build --manifest-path compiler/Cargo.toml

vm: vm/a2vm

vm/a2vm: vm/a2vm.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

example: all
	./compiler/target/debug/compiler examples/basic.a2 -o examples/basic.a2bc
	./vm/a2vm examples/basic.a2bc

clean:
	cargo clean --manifest-path compiler/Cargo.toml
	rm -f vm/a2vm examples/*.a2bc

ainfra-compiler:
	cargo build --manifest-path ainfra-compiler/Cargo.toml

optimizer:
	cargo build --manifest-path optimizer/rust/Cargo.toml

infravm:
	$(MAKE) -C infravm

ainfra-example: optimizer ainfra-compiler infravm
	./ainfra-compiler/target/debug/ainfra-compiler examples/local-stub.ainfra -o data/objects/local-stub.aif
	./infravm/infravm data/objects/local-stub.aif
