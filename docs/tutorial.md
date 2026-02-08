| [Index](./index.md) | Tutorial | [Manual](./manual.md) |
| :-----------------: | :------: | :-------------------: |

## Hello, world!
Create a file called `hello.evm` with these contents (perhaps with `pbpaste > hello.evm`):
```
MSTORE(0, 0x48656c6c6f2c20576f726c64210a)
RETURN(18, 14)
```

If you do `evm hello.evm`, the output is `6d48656c6c6f2c20576f726c64210a5f52600e6012f3`.
These are [hexadecimal](https://en.wikipedia.org/wiki/Hexadecimal) characters representing the EVM bytecode assembled from `hello.evm`.
You can disassemble them back into the original program with `evm hello.evm | evm -d`.

You can run this bytecode with `evm hello.evm | evm -x`, and the output is `48656c6c6f2c20576f726c64210a`.
These are hexadecimal characters representing the binary output returned from the program.
In this case, the binary is a string so you can decode it like `evm hello.evm | evm -x | xxd -r -p` and see `Hello, world!\n`.

## Overview of the EVM

### Memory
Consider the program `hello.evm` above.
First it writes `0x48656c6c6f2c20576f726c64210a` into memory at position `0`.
Then, it returns `14` bytes of memory at position `18`.
The bytes are offset by `18` because, in the EVM, [stack](https://en.wikipedia.org/wiki/Stack_(abstract_data_type)) items are 32 bytes, so `MSTORE` always writes 32 bytes of memory.
Words are read and written from memory in [big-endian](https://en.wikipedia.org/wiki/Endianness), so the lower bytes are at higher offsets in the memory.

|  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
| -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: |
| 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 48 | 65 | 6c | 6c | 6f | 2c | 20 | 57 | 6f | 72 | 6c | 64 | 21 | 0a |

### Calldata
Calldata is how callers specify parameters.

Consider another program, `add.evm`:
```
MSTORE(0, ADD(CALLDATALOAD(4), CALLDATALOAD(36)))
RETURN(RETURNDATASIZE, MSIZE)
```
This program reads two `uint256` from the calldata, adds them (possibly [overflowing](https://en.wikipedia.org/wiki/Integer_overflow)), stores the sum in memory, and then returns it.
The parameters are loaded from 4 and 36 because, in the Solidity 4byte ABI, the first four bytes are reserved for the [function selector](https://docs.soliditylang.org/en/latest/abi-spec.html#function-selector).

Let's change the code to fail on overflow with `error Overflow()` (`0x35278d12`).
The next program, `overflow.evm`, checks for overflow by testing that the sum is greater than one of operands.
```
CALLDATALOAD(36)
ADD(CALLDATALOAD(4), DUP1)
JUMPI(overflow, LT(DUP2, DUP2))
0 MSTORE
RETURN(0, MSIZE)

overflow:
MSTORE(0, 0x35278d12)
REVERT(28, 4)
```
Reverting works like returning, except that the call status will be failure (`0`) instead of success (`1`).

Let's test `overflow.evm`.
Create `overflow.test.json`:
```json
[
    {
        "construct": "overflow.evm",
        "tests": [
            {
                "name": "0 + 0 = 0",
                "input": "0x771602f7",
                "output": "0x0000000000000000000000000000000000000000000000000000000000000000"
            },
            {
                "name": "1 + 2 = 3",
                "input": "0x771602f700000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000002",
                "output": "0x0000000000000000000000000000000000000000000000000000000000000003"
            },
            {
                "name": "1 + (-1) overflows",
                "input": "0x771602f70000000000000000000000000000000000000000000000000000000000000001ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                "status": "0x0",
                "output": "0x35278d12"
            }
        ]
    }
]
```
Run the tests with `evm -w overflow.test.json`.

Now let's examine these test cases.

In the first test case, the input is `0x771602f7`.
That is the selector for `add(uint256,uint256)`.
Our program does not check the selector however.
Additionally, both parameters are omitted.
They default to zero, so the sum is also zero.

In the second test case, both parameters are supplied, and they sum to 3.

In the third test case, we test the overflow case.
In 256-bit arithmetic, `2 ** 256 - 1` is equivalent to `-1`.
The program notices the overflow, because the sum is less than the operands, and reverts.
By default, test cases verify that the call does not revert, but this case instead verifies that it did revert as expected due to the specified `status` of `0`, which is failure.
