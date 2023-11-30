SHELL=/bin/bash
CC=gcc
CPP=g++
CCSTD=-std=gnu11
CXXSTD=-std=gnu++11
CFLAGS=-O3 -fdiagnostics-color=auto -Wno-multichar -pthread -g $(CCSTD)
CXXFLAGS=$(filter-out $(CCSTD), $(CFLAGS)) $(CXXSTD) -fno-exceptions -Wno-write-strings -Wno-pointer-arith
OCFLAGS=$(filter-out $(CCSTD), $(CFLAGS)) -fmodules
MKDIRS=lib bin tst/bin .pass .pass/tst/bin .make .make/bin .make/tst/bin .make/lib .pass/tst/in .pass/tst/diotst
INCLUDE=$(addprefix -I,include)
EXECS=$(addprefix bin/,evm ops)
TESTS=$(addprefix tst/bin/,address dio evm hex keccak label ops scanstack scan uint256 vector)
SRC=$(wildcard src/*.cpp) $(wildcard src/*.m)
LIBS=$(patsubst src/%.cpp, lib/%.o, $(wildcard src/*.cpp)) $(patsubst src/%.m, lib/%.o, $(wildcard src/*.m))
INTEGRATIONS=$(addprefix tst/in/,$(shell ls tst/in)) $(addprefix tst/dio,$(shell ls tst/*.json))


.PHONY: default all clean again check distcheck dist-check
.SECONDARY:
default: all
all: $(EXECS) $(TESTS) README.md
clean:
	rm -rf $(MKDIRS)
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
	@sed 's/include\/$(FNM).h/lib\/\1.o/g' $< > $@
	@sed -i.bak 's/$(FNM).o:/bin\/\1:/g' $@
	@perl make/depend.pl $@ > $@.bak
	@mv $@.bak $@
.make/tst/bin/%.d: .make/tst/%.d | .make/tst/bin
	@sed 's/include\/$(FNM).h/lib\/\1.o/g' $< > $@
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
tst/bin/%: tst/%.cpp | tst/bin
	$(CPP) $(CXXFLAGS) $(INCLUDE) $^ -o $@
tst/bin/%: tst/%.c | tst/bin
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@


make/.ops.out: make/ops.sh bin/ops src/ops.c tst/evm.c 
	make/ops.sh > $@
README.md: make/.ops.out make/.rme.md CONTRIBUTING.md
	cat make/.rme.md make/.ops.out CONTRIBUTING.md > $@



