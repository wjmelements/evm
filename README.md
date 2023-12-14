EVM
========
Fast assembler and disassembler for the Ethereum Virtual Machine (EVM) supporting expressive syntax.

## Installation
```sh
# Build from source
git clone https://github.com/wjmelements/evm.git
cd evm
make bin/evm
# Append bin/ to your $PATH
export PATH=$PATH:$pwd/bin
# Install
echo -n PATH='$PATH:' >> ~/.bashrc
echo $pwd/bin' >> ~/.bashrc
source ~/.bashrc
```

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

Besides declaring code, contracts can be constructed from assembly source.
If code is also supplied for the entry, the code will be used to verify the result of the constructor.
```json
[
    {
        "construct": "tst/in/quine.evm",
        "code": "0x383d3d39383df3",
        "tests": [
            {
                "name": "ignores calldata",
                "input": "0xdeadbeef",
                "output": "0x383d3d39383df3"
            }
        ]
    }
]
```
Run unit tests with `tests` entries.
All `-w` output goes to `stderr`, while `-x` output goes to `stdout`.
```sh
evm -w tst/quine.json
```
```
# tst/in/quine.evm
ignores calldata: pass
```
##### Update Config
A `gasUsed` test field can be supplied (or updated) in-place with `-u`
```sh
evm -uw tst/quine.json
git diff tst/quine.json
```
```sh
diff --git a/tst/quine.json b/tst/quine.json
index 361c65f..1e8a9f4 100644
--- a/tst/quine.json
+++ b/tst/quine.json
@@ -5,6 +5,7 @@
         "tests": [
             {
                 "name": "ignores calldata",
+                "gasUsed": "0x525b",
                 "input": "0xdeadbeef",
                 "output": "0x383d3d39383df3"
             }
```
#### JSON Output
Using any of the following `-x` options will output JSON instead of the returndata.
The JSON will always contain the returndata but other outputs can be specified.
* `-g`: gasUsed
* `-l`: logs
* `-s`: status
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
## Opcodes supported
| Opname | Assembly and Disassembly | Execution |
| :---: | :---:| :---: |
| STOP | ✅ |✅ |
| ADD | ✅ |✅ |
| MUL | ✅ |✅ |
| SUB | ✅ |✅ |
| DIV | ✅ |✅ |
| SDIV | ✅ |✅ |
| MOD | ✅ |✅ |
| SMOD | ✅ |✅ |
| ADDMOD | ✅ |❓ |
| MULMOD | ✅ |✅ |
| EXP | ✅ |✅ |
| SIGNEXTEND | ✅ |✅ |
| LT | ✅ |✅ |
| GT | ✅ |✅ |
| SLT | ✅ |✅ |
| SGT | ✅ |✅ |
| EQ | ✅ |❓ |
| ISZERO | ✅ |✅ |
| AND | ✅ |❓ |
| OR | ✅ |✅ |
| XOR | ✅ |✅ |
| NOT | ✅ |❓ |
| BYTE | ✅ |✅ |
| SHL | ✅ |✅ |
| SHR | ✅ |✅ |
| SAR | ✅ |❓ |
| SHA3 | ✅ |✅ |
| ADDRESS | ✅ |✅ |
| BALANCE | ✅ |✅ |
| ORIGIN | ✅ |❓ |
| CALLER | ✅ |✅ |
| CALLVALUE | ✅ |✅ |
| CALLDATALOAD | ✅ |✅ |
| CALLDATASIZE | ✅ |✅ |
| CALLDATACOPY | ✅ |✅ |
| CODESIZE | ✅ |✅ |
| CODECOPY | ✅ |✅ |
| GASPRICE | ✅ |❓ |
| EXTCODESIZE | ✅ |✅ |
| EXTCODECOPY | ✅ |✅ |
| RETURNDATASIZE | ✅ |✅ |
| RETURNDATACOPY | ✅ |✅ |
| EXTCODEHASH | ✅ |❓ |
| BLOCKHASH | ✅ |❓ |
| COINBASE | ✅ |❓ |
| TIMESTAMP | ✅ |❓ |
| NUMBER | ✅ |❓ |
| DIFFICULTY | ✅ |❓ |
| GASLIMIT | ✅ |❓ |
| CHAINID | ✅ |❓ |
| SELFBALANCE | ✅ |✅ |
| BASEFEE | ✅ |❓ |
| POP | ✅ |❓ |
| MLOAD | ✅ |❓ |
| MSTORE | ✅ |✅ |
| MSTORE8 | ✅ |✅ |
| SLOAD | ✅ |✅ |
| SSTORE | ✅ |✅ |
| JUMP | ✅ |✅ |
| JUMPI | ✅ |✅ |
| PC | ✅ |✅ |
| MSIZE | ✅ |✅ |
| GAS | ✅ |✅ |
| JUMPDEST | ✅ |✅ |
| TLOAD | ✅ |❓ |
| TSTORE | ✅ |❓ |
| MCOPY | ✅ |❓ |
| PUSH0 | ✅ |✅ |
| PUSH1 | ✅ |✅ |
| PUSH2 | ✅ |✅ |
| PUSH3 | ✅ |✅ |
| PUSH4 | ✅ |❓ |
| PUSH5 | ✅ |❓ |
| PUSH6 | ✅ |❓ |
| PUSH7 | ✅ |✅ |
| PUSH8 | ✅ |❓ |
| PUSH9 | ✅ |❓ |
| PUSH10 | ✅ |❓ |
| PUSH11 | ✅ |❓ |
| PUSH12 | ✅ |❓ |
| PUSH13 | ✅ |❓ |
| PUSH14 | ✅ |❓ |
| PUSH15 | ✅ |✅ |
| PUSH16 | ✅ |❓ |
| PUSH17 | ✅ |❓ |
| PUSH18 | ✅ |✅ |
| PUSH19 | ✅ |❓ |
| PUSH20 | ✅ |✅ |
| PUSH21 | ✅ |❓ |
| PUSH22 | ✅ |❓ |
| PUSH23 | ✅ |✅ |
| PUSH24 | ✅ |❓ |
| PUSH25 | ✅ |❓ |
| PUSH26 | ✅ |❓ |
| PUSH27 | ✅ |❓ |
| PUSH28 | ✅ |❓ |
| PUSH29 | ✅ |❓ |
| PUSH30 | ✅ |❓ |
| PUSH31 | ✅ |❓ |
| PUSH32 | ✅ |✅ |
| DUP1 | ✅ |✅ |
| DUP2 | ✅ |✅ |
| DUP3 | ✅ |✅ |
| DUP4 | ✅ |✅ |
| DUP5 | ✅ |✅ |
| DUP6 | ✅ |✅ |
| DUP7 | ✅ |❓ |
| DUP8 | ✅ |❓ |
| DUP9 | ✅ |❓ |
| DUP10 | ✅ |❓ |
| DUP11 | ✅ |❓ |
| DUP12 | ✅ |❓ |
| DUP13 | ✅ |❓ |
| DUP14 | ✅ |❓ |
| DUP15 | ✅ |❓ |
| DUP16 | ✅ |❓ |
| SWAP1 | ✅ |✅ |
| SWAP2 | ✅ |✅ |
| SWAP3 | ✅ |❓ |
| SWAP4 | ✅ |❓ |
| SWAP5 | ✅ |❓ |
| SWAP6 | ✅ |❓ |
| SWAP7 | ✅ |❓ |
| SWAP8 | ✅ |❓ |
| SWAP9 | ✅ |❓ |
| SWAP10 | ✅ |❓ |
| SWAP11 | ✅ |❓ |
| SWAP12 | ✅ |❓ |
| SWAP13 | ✅ |❓ |
| SWAP14 | ✅ |❓ |
| SWAP15 | ✅ |❓ |
| SWAP16 | ✅ |❓ |
| LOG0 | ✅ |✅ |
| LOG1 | ✅ |✅ |
| LOG2 | ✅ |✅ |
| LOG3 | ✅ |✅ |
| LOG4 | ✅ |✅ |
| CREATE | ✅ |❓ |
| CALL | ✅ |✅ |
| CALLCODE | ✅ |❓ |
| RETURN | ✅ |✅ |
| DELEGATECALL | ✅ |✅ |
| CREATE2 | ✅ |❓ |
| AUTH | ✅ |❓ |
| AUTHCALL | ✅ |❓ |
| STATICCALL | ✅ |✅ |
| REVERT | ✅ |✅ |
| INVALID | ✅ |❓ |
| SELFDESTRUCT | ✅ |❓ |
# Contributing
Please use camelCase for methods and variables but snake\_case for types.
Write errors to stderr.
Use the C preprocessor.

## Makefile

- `make`: build the things

- `make again`: rebuild the things

- `make clean`: remove the things

- `make check`: run the tests

- `make distcheck`: run all the tests


## Test-driven development

If you are fixing a bug or adding a feature, first write a test in the `tst` directory that fails now but would pass after your change.
If you create a new test file, you may need to update the `TESTS` field in the `Makefile`.
Then make your change.
Lastly, verify your test now passes.

### Unit tests
Some unit tests written in C are found in `tst/*.c`.
If they pass, they should have no output and exit with a status of 0.

### Assembler tests
Assembler tests live in `tst/in/*.evm`.
The assembler assembles those files and compares the output to the corresponding file in `tst/out/`.

### Execution tests
EVM execution tests use the `dio` testing system.
They can be found at `tst/*.json`.
They can be run individually with `evm -w`.

## Updating the README
The `README.md` is assembled by concatentation when `make`.
See the `Makefile`.
If you want to update the opcodes supported table, it should be updated automatically.
Otherwise, the files you want to edit are:
* `make/.rme.md`
* `make/ops.sh`
* `CONTRIBUTING.md`
