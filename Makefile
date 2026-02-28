SHELL=/bin/bash
CC=gcc
CPP=g++
CCSTD=-std=gnu11
CXXSTD=-std=gnu++11
CFLAGS=-O3 -fdiagnostics-color=auto -Wno-multichar -pthread -g $(CCSTD)
CXXFLAGS=$(filter-out $(CCSTD), $(CFLAGS)) $(CXXSTD) -fno-exceptions -Wno-write-strings -Wno-pointer-arith
OCFLAGS=$(filter-out $(CCSTD), $(CFLAGS)) -fmodules
MKDIRS=lib bin tst/bin .pass .pass/tst/bin .make .make/bin .make/tst/bin .make/lib .pass/tst/in .pass/tst/diotst
SECP256K1=secp256k1/.libs/libsecp256k1.a
INCLUDE=$(addprefix -I,include) -Isecp256k1/include
EXECS=$(patsubst %.c, bin/%, $(wildcard *.c))
TESTS=$(patsubst tst/%.c, tst/bin/%, $(wildcard tst/*.c))
SRC=$(wildcard src/*.cpp) $(wildcard src/*.m) $(wildcard src/%.c)
LIBS=$(patsubst src/%.cpp, lib/%.o, $(wildcard src/*.cpp)) $(patsubst src/%.m, lib/%.o, $(wildcard src/*.m)) $(patsubst src/%.c, lib/%.o, $(wildcard src/*.c))
INTEGRATIONS=$(addprefix tst/in/,$(shell ls tst/in)) $(addprefix tst/dio,$(shell ls tst/*.json))


.PHONY: default all clean again check distcheck dist-check force-version
.SECONDARY:
default: all
all: $(EXECS) $(TESTS) README.md
clean:
	rm -rf $(MKDIRS)
	make -C secp256k1 clean
again: clean all
check: $(addprefix .pass/,$(TESTS) $(INTEGRATIONS))

FNM=\([-+a-z_A-Z0-9/]*\)
.make/%.d: %.m
	@mkdir -p $(@D)
	@$(CC) -MM $(CCSTD) $(INCLUDE) $< -o $@
.make/%.d: %.c
	@mkdir -p $(@D)
	@$(CC) -MM $(CCSTD) $(INCLUDE) $< -o $@
.make/%.d: %.cpp
	@mkdir -p $(@D)
	$(CPP) -MM $(CXXSTD) $(INCLUDE) $< -o $@
.make/lib/%.o.d: .make/src/%.d | .make/lib
	@sed 's/$(FNM)\.o/lib\/\1.o/g' $< > $@
.make/bin/%.d: .make/%.d | .make/bin
	@sed 's|secp256k1/include/secp256k1_recovery.h|$(SECP256K1)|g; s/include\/$(FNM).h/lib\/\1.o/g' $< > $@
	@sed -i.bak 's/$(FNM).o:/bin\/\1:/g' $@
	@perl make/depend.pl $@ > $@.bak
	@mv $@.bak $@
.make/tst/bin/%.d: .make/tst/%.d | .make/tst/bin
	@sed 's|secp256k1/include/secp256k1_recovery.h|$(SECP256K1)|g; s/include\/$(FNM).h/lib\/\1.o/g' $< > $@
	@sed -i.bak 's/$(FNM).o:/tst\/bin\/\1:/g' $@
	@perl make/depend.pl $@ > $@.bak
	@mv $@.bak $@
MAKES=$(addsuffix .d,$(addprefix .make/, $(EXECS) $(TESTS) $(LIBS)))
-include $(MAKES)
distcheck dist-check:
	@rm -rf .pass
	@make --no-print-directory check
.pass/tst/bin/%: tst/bin/% | .pass/tst/bin
	@printf "$<: "
	@$<\
		&& echo -e "\033[0;32mpass\033[0m" && touch $@\
		|| echo -e "\033[0;31mfail\033[0m"
.pass/tst/in/%: bin/evm tst/in/% | .pass/tst/in
	@printf "$(patsubst .pass/tst/in/%,tst/in/%,$@): "
	@bin/evm $(patsubst .pass/tst/in/%,tst/in/%,$@) | diff $(patsubst .pass/tst/in/%.evm,tst/out/%.out, $@) - \
		&& echo -e "\033[0;32mpass\033[0m" && touch $@\
		|| echo -e "\033[0;31mfail\033[0m"
.pass/tst/diotst/%.json: bin/evm tst/%.json | .pass/tst/diotst
	@echo [$(patsubst .pass/tst/diotst/%,tst/%,$@)]
	@$(subst $(eval ) , -w ,$^) && touch $@
$(MKDIRS):
	@mkdir -p $@
$(EXECS): | bin
bin/%: %.cpp
	$(CPP) $(CXXFLAGS) $(INCLUDE) $^ -o $@
bin/%: %.c
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@
lib/%.o: src/%.m include/%.h | lib
	$(CC) -c $(OCFLAGS) $(INCLUDE) $< -o $@
lib/%.o: src/%.cpp include/%.h | lib
	$(CPP) -c $(CXXFLAGS) $(INCLUDE) $< -o $@
lib/%.o: src/%.c include/%.h | lib
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
tst/bin/%: tst/%.m | tst/bin
	$(CC) $(OCFLAGS) $(INCLUDE) $^ -o $@
tst/bin/dio: | bin/evm
tst/bin/%: tst/%.cpp | tst/bin
	$(CPP) $(CXXFLAGS) $(INCLUDE) $^ -o $@
tst/bin/%: tst/%.c | tst/bin
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@


make/.ops.out: make/ops.sh bin/ops src/ops.c tst/evm.c src/evm.c
	make/ops.sh > $@
make/.precompiles.out: make/precompiles.sh bin/precompiles
	make/precompiles.sh > $@
README.md: make/.ops.out make/.precompiles.out make/.rme.md CONTRIBUTING.md
	cat make/.rme.md make/.ops.out make/.precompiles.out CONTRIBUTING.md > $@

bin/evm: lib/version.o
src/version.c: force-version
	@printf 'const char *evm_build_version = "%s";\n' \
		"$$(git describe --tags --match 'v[0-9]*.[0-9]*.[0-9]*' --dirty 2>/dev/null || echo unknown)" \
		> $@.tmp
	@cmp -s $@.tmp $@ 2>/dev/null || mv $@.tmp $@
	@rm -f $@.tmp

secp256k1/autogen.sh: .gitmodules
	git submodule update --init --recursive secp256k1

secp256k1/configure: secp256k1/autogen.sh
	cd $(dir $@); ./autogen.sh

secp256k1/Makefile: secp256k1/configure
	cd $(dir $@); ./configure --enable-module-recovery

secp256k1/.libs/libsecp256k1.a: secp256k1/Makefile
	$(MAKE) -C secp256k1
