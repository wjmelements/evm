SHELL=/bin/bash
CC=gcc
CPP=g++
CCSTD=-std=gnu11
CXXSTD=-std=gnu++11
CFLAGS=-O3 -fdiagnostics-color=auto -Wno-multichar -pthread -g $(CCSTD)
CXXFLAGS=$(filter-out $(CCSTD), $(CFLAGS)) $(CXXSTD) -fno-exceptions -Wno-write-strings -Wno-pointer-arith
OCFLAGS=$(filter-out $(CCSTD), $(CFLAGS)) -fmodules
MKDIRS=lib bin tst/bin .pass .pass/tst/bin .make .make/bin .make/tst/bin .make/lib
INCLUDE=$(addprefix -I,include)
EXECS=$(addprefix bin/,)
TESTS=$(addprefix tst/bin/,ops scanstack)
SRC=$(wildcard src/*.cpp) $(wildcard src/*.m)
LIBS=$(patsubst src/%.cpp, lib/%.o, $(wildcard src/*.cpp)) $(patsubst src/%.m, lib/%.o, $(wildcard src/*.m))


.PHONY: default all clean again check distcheck dist-check
.SECONDARY:
default: all
all: $(EXECS) $(TESTS)
clean:
	rm -rf $(MKDIRS)
again: clean all
check: $(addprefix .pass/,$(TESTS))

FNM=\([-+a-z_A-Z/]*\)
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
	@sed -i '' 's/$(FNM).o:/bin\/\1:/g' $@
	@perl make/depend.pl $@ > $@.bak
	@mv $@.bak $@
.make/tst/bin/%.d: .make/tst/%.d | .make/tst/bin
	@sed 's/include\/$(FNM).h/lib\/\1.o/g' $< > $@
	@sed -i '' 's/$(FNM).o:/tst\/bin\/\1:/g' $@
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
