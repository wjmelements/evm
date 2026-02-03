| [Index](./index.md) | [Tutorial](./tutorial.md) | Manual |
| :-----------------: | :-----------------------: | :----: |

## Basic Usage

```
# Assemble
evm tst/in/hello.evm
cat tst/in/hello.evm | evm
# Minimum constructor (-c)
evm -c tst/in/hello.evm
cat tst/in/hello.evm | evm -c
# Disassemble (-d)
evm -d tst/out/quine.out
cat tst/out/quine.out | evm -d
# Execution (-x)
evm -x tst/out/quine.out
evm tst/in/quine.evm | evm -x
# Test (-w)
evm -w tst/string.json
```
