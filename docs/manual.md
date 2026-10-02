| [Index](./index.md) | [Tutorial](./tutorial.md) | Manual |
| :-----------------: | :-----------------------: | :----: |

## Flags

**Mode flags**

| Flag | Mode | Description |
| :--: | :--: | ----------- |
| _(none)_ | assemble | Assemble `.evm` source to hex bytecode |
| `-d` | disassemble | Disassemble hex bytecode to assembly |
| `-x` | execute | Execute hex bytecode, print returndata |
| `-w file` | test | Load Dio config (repeatable); run tests; exit. With `-x`, also execute the input in that state. |

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
| `-D flags` | `-x` or `-w` | Human-readable debug output; overrides `debug` in `-w` tests (see [Debug flags](#debug-flags)) |
| `-t` | `-x` or `-w` | Emit an [EIP-3155](https://eips.ethereum.org/EIPS/eip-3155) JSON trace (see [Tracing](#tracing)) |
| `-m` | `-t` | Include `memory` in each trace step |
| `-T file` | — | Append debug and trace output to `file` instead of stderr |

**Shared**

| Flag | Description |
| :--: | ----------- |
| `-o input` | Pass input as a command-line string instead of file/stdin |
| `-v`, `--version` | Print the version and exit |

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
| `value` | `msg.value` |
| `nonce` | nonce of `from` before this call; it persists |
| `chainId` | `block.chainid` for this call only |
| `blockOverrides` | block values for this call only, keyed like geth's `eth_call` (see below) |
| `stateOverrides` | account state set before this call, keyed like geth's `eth_call` (see below) |

With `-x` reading from stdin, each line is a separate call sharing the accumulated EVM state.

#### Block overrides

`blockOverrides` sets the values the block opcodes read:
```json
{"to":"0x…","data":"0x…","blockOverrides":{"number":"0x1312d00","time":"0x68255820"}}
```

| Key | Opcode | Default |
| :-: | :----: | :-----: |
| `number` | `NUMBER` | `0x13a2228` |
| `time` | `TIMESTAMP` | `0x65712600` |
| `gasLimit` | `GASLIMIT` | `0x1c9c380` |
| `baseFeePerGas` | `BASEFEE` | `0x7` |
| `blobBaseFee` | `BLOBBASEFEE` | `0x1` |
| `prevRandao` | `PREVRANDAO` | `0x0` |
| `feeRecipient` | `COINBASE` | `0x4838B106FCe9647Bdf1E7877BF73cE8B0BAD5f97` |

`CHAINID` is set by the top-level `chainId` key instead.
Block overrides apply to that call only; the next call sees the previous values again.
Any other key is an error.
Without `-n`, unset values take the defaults above; with `-n`, they are fetched as described in [network mode](#network-mode--n).
The coinbase is warm ([EIP-3651](https://eips.ethereum.org/EIPS/eip-3651)) once it is known.

#### State overrides

`stateOverrides` maps each address to the fields to set before the call:
```json
{"to":"0x…","data":"0x…","stateOverrides":{"0x…":{"code":"0x…","balance":"0x…","nonce":"0x…","stateDiff":{"0x0":"0x1"}}}}
```

| Key | Effect |
| :-: | ------ |
| `balance` | replaces the balance |
| `nonce` | replaces the nonce |
| `code` | replaces the code; `"0x"` clears it |
| `stateDiff` | sets the listed storage slots, leaving the rest unchanged |
| `state` | replaces the entire storage; unlisted slots read as zero |

An account takes either `state` or `stateDiff`, not both.
Unlike `blockOverrides`, state overrides persist: later calls see the overridden state and any changes made on top of it.
A `nonce` override for `from` must match the call's `nonce`, when both are given.

### Tracing

`-t` emits an [EIP-3155](https://eips.ethereum.org/EIPS/eip-3155) JSON trace for `-x` and `-w`: one line per step, then a summary line per transaction.

```sh
evm -txo 385952593df3 2>&1 >/dev/null
```
```jsonl
{"pc":0,"op":56,"gas":"0xffffffffffff3095","stack":[],"depth":1,"returnData":"0x","refund":0,"memSize":0,"opName":"CODESIZE","gasCost":"0x2"}
…
{"output":"0x0000000000000000000000000000000000000000000000000000000000000006","gasUsed":"0xe878","pass":true}
```

`-m` adds the optional `memory` field to each step, as hex.
Like `memSize`, it covers memory in whole 32-byte words, as `MSIZE` reports it.

The `gasCost` of a `CALL` or `CREATE` step includes the gas it forwards.
The summary line omits `stateRoot`.
`-t` overrides any `debug` flags from `-w` tests, and cannot be combined with `-D`.
Trace and debug output both go to stderr, or are appended to the file given by `-T`.

---

## Network mode (`-n`)

`evm -nx` executes against **live chain state**.
Instead of declaring every account and storage slot up front with `-w`, the interpreter fetches them lazily.
Each fetch is emitted as a [JSON-RPC](https://ethereum.org/en/developers/docs/apis/json-rpc/) request on **stdout**; the response is read from **stdin**.
`evm` opens no socket of its own — it must sit between the requests and a node.
Wiring stdout back to stdin needs a bidirectional pipe, not a plain shell `|`.

| Request | Emitted |
| ------- | ------- |
| `eth_blockNumber` | once, on the first fetch or `NUMBER` |
| `eth_getCode` + `eth_getTransactionCount` + `eth_getBalance` | on first touch of an account, as one batch array, without overridden fields |
| `eth_getStorageAt` | on first read of a storage slot |
| `eth_chainId` | once, on the first `CHAINID` |
| `eth_getBlockByNumber` | once per block, on the first `TIMESTAMP`, `GASLIMIT`, `BASEFEE`, `PREVRANDAO`, or `COINBASE` |

Accounts created during execution are served locally and never fetched.
Overridden account fields and storage slots are never fetched; an account with `balance`, `nonce`, and `code` all overridden is not fetched at all, and an account with a `state` override never fetches storage.
Overriding `number` to N fetches the header of block N and state at block N - 1.
If block N does not exist yet, its header fields fall back to their defaults with a warning.
Accounts and storage are fetched once per process, so later calls reuse them regardless of `number`.
`blobBaseFee` is not fetched; override it.
Until the coinbase is known, accessing it costs the cold surcharge.

With `-n`, JSON output reports the block values each call read, in the same `chainId` and `blockOverrides` keys, so they can be replayed.
`bin/dio` implements this proxy against a real endpoint — see [dio](#dio).

---

## dio

`bin/dio` drives `evm -nx` against a node and writes a `-w` config JSON snapshotting every
account, balance, nonce, code, and storage slot the call touched, so the call replays
offline and deterministically with `evm -w`.
It links `libcurl` and execs the `evm` binary at runtime, so build both:

```sh
make bin/evm bin/dio
```

    dio [provider-url] [outfile] [-o json] [file...]

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
| `block` | `latest`, or a `0x`-prefixed hex block number, to pin state to | `latest` |
| `nonce`, `chainId`, `blockOverrides`, `stateOverrides` | as in [JSON call input](#json-call-input) | — |

Each call object becomes a `tests` entry on the generated account, or a `constructTest` when it is a CREATE.
Replay with `-w` runs the calls in their original order, after every account they fetched: each call is recorded on the account it calls, placed last, unless that would reorder calls, in which case it goes on the last entry with an explicit `to`.
Each entry records only the block values its call read, such as `timestamp` or `chainId`.
An entry's `stateOverrides` are recorded on its test, while each account records its chain state.

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
| `nonce` | nonce of `from` | unchanged | Set before the call |
| `stateOverrides` | account state | `{}` | As in [state overrides](#state-overrides) |
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
| `gasLimit` | `block.gaslimit` | `0x1c9c380` | |
| `chainId` | `block.chainid` | `0x1` | |
| `baseFee` | `block.basefee` | `0x7` | |
| `blobBaseFee` | `block.blobbasefee` | `0x1` | |
| `prevRandao` | `block.prevrandao` | `0x0` | |
| `coinbase` | `block.coinbase` | `0x4838B106FCe9647Bdf1E7877BF73cE8B0BAD5f97` | |
| `debug` | debug bitmask | `0x0` | See below |

Block values and `stateOverrides` set by a test persist to later tests.

### Constructor test

`constructTest` runs once after the constructor executes, before any `tests`, and reports the outcome to stderr.
It accepts a subset of the test fields:

| Key | Description | Default |
| :-: | ----------- | :-----: |
| `name` | Label shown in output | `"constructor"` |
| `from` | `msg.sender` for the constructor. Must equal `creator` if both are set. | `creator` (or `0x000…000`) |
| `gas` | Gas limit | `0xffffffffffffffff` |
| `nonce` | Nonce of `from`, which determines the deployed address | unchanged |
| `stateOverrides` | Account state set before the constructor | `{}` |
| block fields | As in [test fields](#test-fields) | defaults |
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
