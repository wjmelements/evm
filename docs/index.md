| Index | [Tutorial](./tutorial.md) | [Manual](./manual.md) |
| :---: | :-----------------------: | :-------------------: |

`evm` is a low-level Ethereum Virtual Machine toolkit.

* Assembler
* Disassembler
* Runtime
* Test Environment
* Fork and Snapshot
* Gas Profiler


## Installation
Building from source requires `gcc` (Linux) or `clang` (MacOS).
```sh
git clone https://github.com/wjmelements/evm.git
make -C evm bin/evm
sudo install evm/bin/evm /usr/local/bin/
```

To also build `bin/dio` (on-chain state snapshotter), `libcurl` development headers are required:
```sh
make -C evm bin/evm bin/dio
sudo install evm/bin/evm evm/bin/dio /usr/local/bin/
```

### Uninstallation
```sh
sudo rm -f /usr/local/bin/evm /usr/local/bin/dio
```

### Update
```sh
git -C evm pull origin master
make -C evm bin/evm
sudo install evm/bin/evm /usr/local/bin/
```
