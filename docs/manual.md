| [Index](./index.md) | [Tutorial](./tutorial.md) | Manual |
| :-----------------: | :-----------------------: | :----: |

## Flags

**Mode flags**

| Flag | Mode | Description |
| :--: | :--: | ----------- |
| _(none)_ | assemble | Assemble `.evm` source to hex bytecode |
| `-d` | disassemble | Disassemble hex bytecode to assembly |
| `-x` | execute | Execute hex bytecode, print returndata |
| `-w file` | test | Load Dio config; run tests; exit. Combine with `-x` to also execute a bytecode input with that world state. |

**Mode modifiers**

| Flag | Requires | Description |
| :--: | :------: | ----------- |
| `-c` | assemble | Wrap output in a minimum viable constructor |
| `-C` | assemble | Wrap output in a universal constructor |
| `-j` | assemble | Output JUMPDEST labels instead of bytecode |
| `-g` | `-x` | Include `gasUsed` in JSON output |
| `-l` | `-x` | Include `logs` in JSON output |
| `-s` | `-x` | Include `status` in JSON output |
| `-n` | `-x` | Network mode: fetch account and storage state on demand over JSON-RPC |
| `-u` | `-w` | Update `gasUsed` fields in config file in-place |

**Shared**

| Flag | Description |
| :--: | ----------- |
| `-o input` | Pass input as a command-line string instead of file/stdin |

---

## Assembler

Reads `.evm` source, writes hex bytecode to stdout.

```sh
evm foo.evm
cat foo.evm | evm
evm -o 'RETURN(0, 32)'
```

### Opcodes and constants

Opcodes are uppercase.
Numeric constants (decimal or hex) are pushed with the minimum-width `PUSH`.
To force a wider push, use leading zeros in the hex literal:

```
0x20      # PUSH1 0x20
32        # same output
0x0020    # PUSH2 0x0020
0x000020  # PUSH3 0x000020
```

Precompile names (`ECRECOVER`, `IDENTITY`, etc.) are valid arguments and emit `PUSH0`/`PUSH1 <addr>`.

### Function syntax

Arguments in parentheses are evaluated left to right and pushed right to left, matching EVM stack order:

```
MSTORE(0, 42)
ADD(CALLDATALOAD(0), CALLDATALOAD(32))
RETURN(RETURNDATASIZE, CALLDATASIZE)
```

Nesting works arbitrarily deep.
Without parentheses, opcodes consume whatever is on the stack.

### Labels

Lowercase identifiers define jump destinations.
A bare label declares a `JUMPDEST`.
A label used as an argument pushes its bytecode offset:

```
start:
JUMP(start)
```

### Data sections

Raw data is appended after the instructions, delimited by `{}`.
Items are labeled and comma-separated.
A label used as an instruction argument pushes the **byte offset** of that item in the bytecode.
`#label` pushes the **byte length** of that item.

```
CODECOPY(0, payload, #payload)
RETURN(0, #payload)
{
    payload: 0xdeadbeef
}
```

| Item type | Syntax | Emitted bytes |
| :-------: | ------ | :-----------: |
| Hex | `name: 0xdeadbeef` | `deadbeef` |
| String | `name: "hello"` | UTF-8 bytes |
| Bytecode | `name: assemble file.evm` | assembled output |
| Deployed code | `name: construct file.evm` | constructor + runtime |

### Constructor wrapping

| Flag | Behavior |
| :--: | -------- |
| `-c` | Prepends a minimum viable constructor that deploys the assembled bytecode |
| `-C` | Prepends a universal constructor (`600b380380600b3d393df3…`) |

`evm -c foo.evm` produces initcode ready to send as a create transaction.

---

## Disassembler

`-d` reads hex bytecode and outputs valid assembly.

```sh
evm -d foo.out
cat foo.out | evm -d
evm foo.evm | evm -d    # round-trip
```

Output uses function syntax where possible and can be re-assembled with `evm`.

---

## Execution

`-x` reads hex bytecode, executes it, and prints returndata as hex to stdout.

```sh
evm -x foo.out
evm foo.evm | evm -x
evm -c foo.evm | evm -x    # deploy then run
```

Adding any of `-g`, `-l`, `-s` switches to JSON output.
`returnData` is always included.
Flags combine freely: `evm -xgls foo.out`.

### JSON call input

When the input to `-x` begins with `{`, it is parsed as a call object instead of raw bytecode:

| Key | Meaning |
| :-: | ------- |
| `to` | callee; when present, the input is a `CALL` instead of a `CREATE` |
| `from` | `msg.sender` |
| `data` / `input` | calldata, or initcode when `to` is absent |

With `-x` reading from stdin, each line is a separate call sharing the accumulated EVM state.

---

## Network mode (`-n`)

`evm -nx` executes against **live chain state**.
Instead of declaring every account and storage slot up front with `-w`, the interpreter fetches them lazily.
Each fetch is emitted as a [JSON-RPC](https://ethereum.org/en/developers/docs/apis/json-rpc/) request on **stdout**; the response is read from **stdin**.
`evm` opens no socket of its own — it must sit between the requests and a node.

```sh
# feed RPC replies on stdin, e.g. via a proxy script or bin/dio
echo '{"to":"0x6b175474e89094c44da98b954eedeac495271d0f","data":"0x18160ddd"}' \
    | evm -nx | your-rpc-proxy https://…
```

| Request | Emitted |
| ------- | ------- |
| `eth_blockNumber` | once, unless `blockNumber` is pinned in the call object |
| `eth_getCode` + `eth_getTransactionCount` + `eth_getBalance` | on first touch of an account, as one batch array |
| `eth_getStorageAt` | on first read of a storage slot |

Accounts created during execution are served locally and never fetched.
`bin/dio` implements this proxy against a real endpoint — see [dio](#dio).

---

## dio

`bin/dio` drives `evm -nx` against a node and writes a `-w` config JSON snapshotting every
account, balance, nonce, code, and storage slot the call touched, so the call replays
offline and deterministically with `evm -w`.
It links `libcurl`; build it with `make bin/dio`.

```sh
dio [provider-url] [outfile] [-o json] [file...]
```

- The provider URL is the first positional argument, or `$ETH_RPC_URL`. `http(s)://` and `ws(s)://` are supported.
- The second positional argument is the output file; output goes to stdout when omitted.
- The call JSON comes from `-o <json>`, from file arguments, or from stdin, and may be a single object or an array.

```sh
export ETH_RPC_URL=https://mainnet.infura.io/v3/KEY

# totalSupply() of DAI, config to stdout
echo '{"to":"0x6b175474e89094c44da98b954eedeac495271d0f","data":"0x18160ddd"}' | dio | jq

# write to a file
dio $ETH_RPC_URL dai.json < call.json

# CREATE entry: omit "to"; with "from", the deployed address is derived from from + nonce
echo '{"from":"0xd8da6bf26964af9d7eed9e03e53415d37aa96045","data":"0x<initcode>"}' | dio $ETH_RPC_URL
```

### Call JSON

| Key | Meaning | Default |
| :-: | ------- | :-----: |
| `to` | contract called; omit to generate a CREATE entry | — (CREATE) |
| `from` | `msg.sender` / deployer | `0x000…000` |
| `data` / `input` | calldata, or initcode when `to` is omitted | `0x` |
| `value` | wei sent with the call | `0x0` |
| `block` | block tag or number to pin state to | `latest` |

Each call object becomes a `tests` entry on the generated account, or a `constructTest` when it is a CREATE.

---

## Dio config (`-w`)

`-w config.json` loads a JSON array of account entries defining world state, runs any `tests` entries, then exits.
Add `-x` to also execute a bytecode input using that world state.
The recommended way to generate this config is `bin/dio`, which snapshots live chain state; see [dio](#dio).

```sh
evm -w tst/foo.json          # run tests
evm -xw tst/foo.json         # run tests, then execute stdin/file as bytecode
evm -uw tst/foo.json         # run tests and update gasUsed in-place
```

### Account fields

All fields are optional; unset fields default to zero.

| Key | Description | Default |
| :-: | ----------- | :-----: |
| `address` | Account address | auto-generated |
| `balance` | Account balance | `0x0` |
| `nonce` | Account nonce | `0x0` |
| `storage` | Storage map `{"slot": "value"}` | `{}` |
| `code` | Runtime bytecode (hex or path to `.evm`) | `0x` |
| `initcode` | Creation code (hex or path to `.evm`). If set, `code` is used as the expected deployed result. | — |
| `construct` | Like `initcode`, but wraps the file in a minimum constructor | — |
| `constructTest` | Test the constructor execution (see below) | — |
| `creator` | `msg.sender` for the constructor call | `0x000…000` |
| `import` | Path to another config file to merge | — |
| `tests` | Array of test transactions (see below) | `[]` |

### Test fields

| Key | EVM equivalent | Default | Notes |
| :-: | :------------: | :-----: | ----- |
| `name` | — | test index | Label shown in output |
| `input` | `msg.data` | `0x` | |
| `value` | `msg.value` | `0x0` | |
| `from` | `tx.origin` | `0x000…000` | |
| `gas` | gas limit | `0xffffffffffffffff` | |
| `op` | call type | `CALL` | `STATICCALL`, `DELEGATECALL`, etc. |
| `to` | callee address | account `address` | |
| `status` | expected status | `0x1` (success) | Set to `0x0` to assert revert |
| `output` | expected returndata | ignored | |
| `logs` | expected logs | ignored | Keyed by emitting address |
| `gasUsed` | expected gas | ignored (checked if set) | Hex |
| `accessList` | EIP-2929 warm slots | `{}` | `[{"addr": ["slot"]}]` |
| `blockNumber` | `block.number` | `0x13a2228` | |
| `timestamp` | `block.timestamp` | `0x65712600` | |
| `debug` | debug bitmask | `0x0` | See below |

### Constructor test

`constructTest` runs once after the constructor executes, before any `tests`, and reports the outcome to stderr.
It accepts a subset of the test fields:

| Key | Description | Default |
| :-: | ----------- | :-----: |
| `name` | Label shown in output | `"constructor"` |
| `from` | `msg.sender` for the constructor. Must equal `creator` if both are set. | `creator` (or `0x000…000`) |
| `gas` | Gas limit | `0xffffffffffffffff` |
| `output` | Expected deployed bytecode | ignored |
| `logs` | Expected emitted logs | ignored |
| `status` | Expected outcome | `0x1` (success) |
| `gasUsed` | Expected gas | ignored (checked/updated if set) |
| `debug` | Debug bitmask (see [Debug flags](#debug-flags)) | `0x0` |

Example — measure constructor gas with `-u`:
```json
[
    {
        "construct": "tst/in/quine.evm",
        "constructTest": {
            "name": "deploy quine",
            "gasUsed": "0xd583"
        }
    }
]
```
```sh
evm -uw tst/quine.json
```

### Debug flags

`debug` is a bitmask.
Combine values with `|` (e.g. `0x9` = Stack + Gas).

| Bit | Value | Output |
| :-: | :---: | ------ |
| 0 | `0x1` | Stack |
| 1 | `0x2` | Memory |
| 2 | `0x4` | Opcodes |
| 3 | `0x8` | Gas |
| 4 | `0x10` | Program counter |
| 5 | `0x20` | Calls |
| 6 | `0x40` | Logs |

### Updating gasUsed

`-u` runs tests and writes the measured `gasUsed` back into the config file in-place.

```sh
evm -uw tst/foo.json
git diff tst/foo.json
```
