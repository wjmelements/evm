CC=gcc
CPP=g++
CFLAGS=-O3 -fdiagnostics-color=auto -pthread -std=gnu11
CPPFLAGS=$(filter-out -std=gnu11, $(CFLAGS)) -stdu=gnu++11 -fno-exceptions -Wno-write-strings
MKDIRS=lib bin tst/bin
INCLUDE=$(addprefix -I,include)
EXECS=$(addprefix bin/,)
TESTS=$(addprefix tst/bin,)

.PHONY: default all clean again
default: all
all: $(EXECS) $(TESTS)
clean:
	rm -rf $(MKDIRS)
again: clean all
$(MKDIRS):
	mkdir -p $@
bin/%: %.cpp | bin
	$(CPP) $(CPPFLAGS) $(INCLUDE) $< -o $@
bin/%: %.c | bin
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@
lib/%.o: src/%.cpp include/%.h | lib
	$(CPP) -c $(CPPFLAGS) $(INCLUDE) $< -o $@
lib/%.o: src/%.c include/%.h | lib
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
tst/bin/%: tst/%.cpp src/%.cpp include/%.h | tst/bin
	$(CPP) $(CPPFLAGS) $(INCLUDE) $< -o $@
tst/bin/%: tst/%.c src/%.c include/%.h | tst/bin
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@
