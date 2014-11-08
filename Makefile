SHELL=/bin/bash
CC=gcc
CPP=g++
CFLAGS=-O3 -fdiagnostics-color=auto -pthread -std=gnu11
CXXFLAGS=$(filter-out -std=gnu11, $(CFLAGS)) -std=gnu++11 -fno-exceptions -Wno-write-strings
MKDIRS=lib bin tst/bin .pass .pass/tst/bin .make .make/bin .make/tst/bin
INCLUDE=$(addprefix -I,include)
EXECS=$(addprefix bin/,)
TESTS=$(addprefix tst/bin/,)

.PHONY: default all clean again check distcheck dist-check
.SECONDARY:
default: all
all: $(EXECS) $(TESTS)
clean:
	rm -rf $(MKDIRS)
again: clean all
check: $(addprefix .pass/,$(TESTS))

FNM=\([a-z_A-Z]*\)
.make/%.d: %.c
	@mkdir -p $(@D)
	@$(CC) -MM $(INCLUDE) $< -o $@
.make/%.d: %.cpp
	@mkdir -p $(@D)
	$(CPP) -MM $(INCLUDE) $< -o $@
.make/bin/%.d: .make/%.d | .make/bin
	@sed 's/include\/$(FNM).h/lib\/\1.o/g' $< > $@
	@sed -i 's/$(FNM).o:/bin\/\1:/g' $@
.make/tst/bin/%.d: .make/tst/%.d | .make/tst/bin
	@sed 's/include\/$(FNM).h/lib\/\1.o/g' $< > $@
	@sed -i 's/$(FNM).o:/tst\/bin\/\1:/g' $@
MAKES=$(addsuffix .d,$(addprefix .make/, $(EXECS) $(TESTS)))
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
lib/%.o: src/%.cpp include/%.h | lib
	$(CPP) -c $(CXXFLAGS) $(INCLUDE) $< -o $@
lib/%.o: src/%.c include/%.h | lib
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
tst/bin/%: tst/%.cpp lib/%.o | tst/bin
	$(CPP) $(CXXFLAGS) $(INCLUDE) $^ -o $@
tst/bin/%: tst/%.c lib/%.o | tst/bin
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@
