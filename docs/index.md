| Index | [Tutorial](./tutorial.md) | [Manual](./manual.md) |
| :---: | :-----------------------: | :-------------------: |

`evm` is a low-level Ethereum Virtual Machine toolkit.

* Assembler
* Disassembler
* Runtime
* Test Environment
* Gas Profiler


## Installation
Building from source requires `gcc` (Linux) or `clang` (MacOS).
```sh
git clone https://github.com/wjmelements/evm.git
make -C evm bin/evm
sudo install evm/bin/evm /usr/local/bin/
```

### Uninstallation
```sh
sudo rm -f /usr/local/bin/evm
```

### Update
```sh
git -C evm pull origin master
make -C evm bin/evm
sudo install evm/bin/evm /usr/local/bin/
```
