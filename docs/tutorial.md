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
Consider the program `hello.evm` above.
First it writes `0x48656c6c6f2c20576f726c64210a` into memory at position `0`.
Then, it returns `14` bytes of memory at position `18`.
The bytes are offset by `18` because, in the EVM, [stack](https://en.wikipedia.org/wiki/Stack_(abstract_data_type)) items are 32 bytes, so `MSTORE` always writes 32 bytes of memory.
Words are read and written from memory in [big-endian](https://en.wikipedia.org/wiki/Endianness), so the lower bytes are at higher offsets in the memory.

|  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
| -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: | -: |
| 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 00 | 48 | 65 | 6c | 6c | 6f | 2c | 20 | 57 | 6f | 72 | 6c | 64 | 21 | 0a |
