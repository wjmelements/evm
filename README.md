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
#### Configuration (Dio)
<img alt="The World Tarot Card" src="https://upload.wikimedia.org/wikipedia/commons/f/ff/RWS_Tarot_21_World.jpg" height=120>

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

Besides declaring code, contracts can be `construct`ed from assembly source.
If `code` is also supplied for the entry, the code will be used to verify the result of the constructor.

| Dio Key | Description | Example Value | Default Value or Behavior |
| :-----: | ----------- | ------------- | :-----------------------: |
| `address` | address for the account | `0x83F20F44975D03b1b09e64809B757c47f942BEeA` | |
| `balance` | value in the account | `0xde0b6b3a7640000` | `0x0` |
| `nonce` | nonce of the account | `0x1` | `0x0` |
| `storage` | account storage | `{"0x1":"0x115eec47f6cf7e35000000"}` | `{}` |
| `creator` | address of the account creating this contract | `0x3249936bDdF8bF739d3f06d26C40EEfC81029BD1` | `0x0000000000000000000000000000000000000000` |
| `initcode` | account creation code | `0x66383d3d39383df33d5260076019f3` | `code` mocked without constructor |
| `code` | account code; validated if `initcode` or `construct` specified | `0x383d3d39383df3` | `0x` |
| `construct` | specify `code` by file | `tst/in/quine.evm` | `code` |
| `import` | load another configuration | `tst/quine.json` | |
| `tests` | transactions executed sequentially, after account configuration | <pre>[<br>    {<br>    "input": "0x18160ddd",<br>        "output":"0x115eec47f6cf7e35000000"<br>    }<br>]</pre> | `[]` |

See the next section for test configuration.

##### Testing
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

| Test Key | Description | Example Value | Default Value or Behavior | 
| :------: | ----------- | :-----------: | :-----------------------: |
| `name` | label for the test case | `"decimals() = 18"` | index of the testcase |
| `input` | `msg.data` | `0x313ce567` | `0x` |
| `value` | `msg.value` | `0x38d7ea4c68000` | `0x0` |
| `from` | `tx.origin` | `0xd1236a6A111879d9862f8374BA15344b6B233Fbd` | `0x0000000000000000000000000000000000000000` |
| `gas` | `tx.gasLimit` | `0x5208` | `0xffffffffffffffff` |
| `op` | type of call | `STATICCALL` | `CALL` |
| `to` | account called | `0x83F20F44975D03b1b09e64809B757c47f942BEeA` | account `address` |
| `status` | expected return status | `0x0` (revert) | `0x1` (success) |
| `output` | expected return or revert data | `0x0000000000000000000000000000000000000000000000000000000000000012` | ignored |
| `logs` | expected logs by account | <pre>{<br>    "0x6b175474e89094c44da98b954eedeac495271d0f": [<br>        {<br>            "topics": [<br>                "0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef",<br>                "0xae461ca67b15dc8dc81ce7615e0320da1a9ab8d5",<br>                "0x650c1c32c383290a300ff952d2a1d238ee08e62f"<br>            ],<br>        "data": "0x000000000000000000000000000000000000000000000000000717500b588103"<br>    }<br>    ]}</pre> | ignored |
| `gasUsed` | expected gas used | `0x5208` | ignored |
| `accessList` | [EIP-2929](https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2929.md) | `[{"0x22d8432cc7aa4f8712a655fc4cdfb1baec29fca9":["0x6"]}] | `{}` |
| `blockNumber` | `block.number` | `0x1312d00` | `0x13a2228` |
| `debug` | debug flags | `0x20` | `0x0` |

The current `debug` flags:

| Debug Flag | Description |
| :--------: | ----------- |
| 0x1 | Stack |
| 0x2 | Memory |
| 0x4 | Opcodes |
| 0x8 | Gas |
| 0x10 | PC |
| 0x20 | Calls |
| 0x40 | Logs |

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
| ADDMOD | ✅ | ❌ |
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
| GASPRICE | ✅ | ❌ |
| EXTCODESIZE | ✅ |✅ |
| EXTCODECOPY | ✅ |✅ |
| RETURNDATASIZE | ✅ |✅ |
| RETURNDATACOPY | ✅ |✅ |
| EXTCODEHASH | ✅ | ❌ |
| BLOCKHASH | ✅ | ❌ |
| COINBASE | ✅ |✅ |
| TIMESTAMP | ✅ |❓ |
| NUMBER | ✅ |❓ |
| DIFFICULTY | ✅ | ❌ |
| GASLIMIT | ✅ | ❌ |
| CHAINID | ✅ |❓ |
| SELFBALANCE | ✅ |✅ |
| BASEFEE | ✅ | ❌ |
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
| CALLCODE | ✅ | ❌ |
| RETURN | ✅ |✅ |
| DELEGATECALL | ✅ |✅ |
| CREATE2 | ✅ |❓ |
| AUTH | ✅ | ❌ |
| AUTHCALL | ✅ | ❌ |
| STATICCALL | ✅ |✅ |
| REVERT | ✅ |✅ |
| INVALID | ✅ | ❌ |
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
