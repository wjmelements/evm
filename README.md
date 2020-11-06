EVM
========
Fast assembler and disassembler for the Ethereum Virtual Machine (EVM) supporting expressive syntax.

## Usage
### Assembler
```sh
# Optional function-syntax
$ cat echo.evm
CALLDATACOPY(RETURNDATASIZE, RETURNDATASIZE, CALLDATASIZE)
RETURN(RETURNDATASIZE, CALLDATASIZE)
# Outputs evm bytecode
$ evm echo.evm
363d3d37363df3
# Wrap the minimum viable constructor with -c
$ evm -c echo.evm
66363d3d37363df33d5260076019f3
# Labels are lowercase, opcodes are uppercase
$ cat infinite.evm
start:
JUMP(start)
$ evm infinite.evm
5b600056
# Use decimal and hexadecimal constants directly without explicitly specifying PUSHx width
$ cat add.evm
MSTORE(RETURNDATASIZE,ADD(CALLDATALOAD(RETURNDATASIZE),CALLDATALOAD(32)))
RETURN(RETURNDATASIZE,0x20)
$ evm add.evm
6020353d35013d5260203df3
```

More examples can be found in the `tst/in` directory.

### Disassembler
```sh
# Outputs valid assembly
$ bin/evm -d tst/out/selfdestruct.out
SELFDESTRUCT(CALLER)
```

## Roadmap
* Implicit stack use
* Stack underflow warnings
* Explicit Constructor

# Contributing
Please use camelCase for methods and variables but snake\_case for types.
Write errors to stderr.
Use the C preprocessor.

## Makefile

- make: build the things

- make again: rebuild the things

- make clean: remove the things

- make check: run the tests

- make distcheck: run all the tests
