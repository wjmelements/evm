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
# Can read from stdin
$ cat add.evm | evm
6020353d35013d5260203df3
```

More examples can be found in the `tst/in` directory.

### Disassembler
```sh
$ cat selfdestruct.out
33ff
# Outputs valid assembly
$ evm -d selfdestruct.out
SELFDESTRUCT(CALLER)
# Can read from stdin
$ cat selfdestruct.out | evm -d
SELFDESTRUCT(CALLER)
```

### EVM Execution
```sh
$ cat quine.evm
CODECOPY(
    RETURNDATASIZE,
    RETURNDATASIZE,
    CODESIZE
)
RETURN(
    RETURNDATASIZE,
    CODESIZE
)
# Executes the code and outputs the returndata
$ evm -c quine.evm | evm -x
383d3d39383df3
$ evm -c quine.evm | evm -x | evm -d
CODECOPY(RETURNDATASIZE,RETURNDATASIZE,CODESIZE)
RETURN(RETURNDATASIZE,CODESIZE)
```
#### Configuration
By using `-w config.json`, you can define the precondition state before execution.
```json
[
    {
        "address": "0x80d9b122dc3a16fdc41f96cf010ffe7e38d227c3",
        "nonce": "0x",
        "code": "0x383d3d39383df3",
        "storage": {
            "0x00" : "0xf1ecf98489fa9ed60a664fc4998db699cfa39d40",
            "0x01" : "0x01"
        }
    }
]
```
Account configuration fields are optional and default to zero.
If you exclude address, one will be generated for you.
#### Warning
EVM execution should mostly work but may not implement every opcode and corner-case.
If you find a bug that disrupts you, please file an issue with its impact to you and code that reproduces it and I may find time to fix it, or alternatively you can submit a pull request.

## Roadmap
### Assembler
* Implicit stack use
* Stack underflow warnings
* Explicit Constructor
### Disassembler
* Label JUMPDEST in JUMPI and JUMP args
### Execution
* Flags to include state changes in JSON
* Mock calls via `-w` config

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
