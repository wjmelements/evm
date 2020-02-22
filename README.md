EVM
========
An assembler for the Ethereum Virtual Machine

## Usage
```sh
$ cat echo.evm
CALLDATACOPY(RETURNDATASIZE, RETURNDATASIZE, CALLDATASIZE)
RETURN(RETURNDATASIZE, CALLDATASIZE)
$ evm echo.evm
363d3d37363df3
# Wrap a no-op constructor
$ evm -c echo.evm
60078060093d393df3363d3d37363df31
$ cat add.evm
MSTORE(RETURNDATASIZE,ADD(CALLDATALOAD(RETURNDATASIZE),CALLDATALOAD(32)))
RETURN(RETURNDATASIZE,32)
$ evm add.evm
6020353d35013d5260203df3
```

## Roadmap
* Constructor
* Implicit stack use
* Stack warnings

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
