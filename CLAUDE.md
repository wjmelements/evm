# EVM — Claude Notes

Fast assembler, disassembler, and executor for the Ethereum Virtual Machine.

## Build

```sh
make          # build all
make check    # run all tests
make again    # clean rebuild
make distcheck # clean test run
```

`bin/evm` is the main binary. Libraries are in `lib/`, built from `src/`.

## Key Files

| File | Role |
|------|------|
| `src/evm.c` | EVM interpreter — `doCall`, `doSupportedPrecompile` |
| `src/scan.c` | Assembler scanner/parser |
| `src/ops.c` | Opcode table and `parseOp` (hand-rolled trie) |
| `src/precompiles.c` | `PrecompileIsSupported`, `precompileName[]` |
| `include/precompiles.h` | `PRECOMPILES` X-macro — source of truth for precompile list |
| `include/ops.h` | `OPS` X-macro — source of truth for opcode table |
| `evm.c` | `main()` — CLI entry point |
| `dio.c` | `bin/dio` — fetches on-chain state via JSON-RPC, generates evm test config |
| `src/dio.c` | Config JSON parser and test runner (`applyConfig`, `loadConfig`) |
| `src/config.c` | `writeConfig` — serializes state to config JSON |

## bin/dio — On-chain state fetcher

`bin/dio` fetches live contract state from an Ethereum node and emits a config JSON suitable for `bin/evm -w`.

```sh
# From stdin / -o flag:
echo '{"to":"0x...","from":"0x...","data":"0x..."}' | dio https://mainnet.infura.io/v3/KEY
# Or pipe through jq:
echo '{"to":"0x...","data":"0x..."}' | dio $ETH_RPC_URL | jq

# With a create (no "to"):
echo '{"data":"0x<initcode>","from":"0x<deployer>"}' | dio $ETH_RPC_URL
```

- `to` is required for calls; omit `to` to generate a CREATE entry
- `from` is optional; when present for creates, the deployed address is computed from `from`+nonce
- `block` defaults to `"latest"`
- Provider URL can also be set via `ETH_RPC_URL` env var
- Fetches code, storage, balance, and nonce for all touched accounts
- Output is written to stdout (or `outfile` positional arg)

## Adding a Precompile

1. Mark it supported in `include/precompiles.h`: `PRECOMPILE(NAME,0xN,1)`
2. Add a `case NAME:` in `doSupportedPrecompile` in `src/evm.c`
3. Precompile names are automatically usable as assembler arguments (via `tryParsePrecompile` in `src/scan.c`)

### ECRECOVER example (address 0x1)
Uses `secp256k1/.libs/libsecp256k1.a` (built with `--enable-module-recovery`).
The library is linked automatically via the sentinel header mechanism (see Build System below).

## Assembler Syntax

- Function syntax: `MSTORE(MSIZE, 42)` — args evaluated left-to-right, pushed right-to-left
- Nested calls: `RETURN(0, ADD(#str, #str))`
- Data sections: `{ str: 0xdeadbeef }` — defines labeled data inline
- `#label` — push byte length of labeled data
- Lowercase identifiers are jump labels
- Precompile names (e.g. `ECRECOVER`, `IDENTITY`) are valid arguments, emit `PUSH0`/`PUSH1 <addr>`
- `MSIZE` as an MSTORE offset is idiomatic for sequential writes

## Testing

### Assembler tests (`tst/in/*.evm` → `tst/out/*.out`)
```sh
make .pass/tst/in/foo.evm
```
The assembler output (hex bytecode) must match `tst/out/foo.out`.

### Execution tests (`tst/*.json`)
```sh
make .pass/tst/diotst/foo.json
# or run directly:
bin/evm -w tst/foo.json
# update gasUsed in-place:
bin/evm -u -w tst/foo.json
```

Config JSON is an array of entry objects. Key entry fields:

| Field | Description |
|-------|-------------|
| `address` | Contract address (hex). Omit for CREATE entries with known `from` — address is computed. |
| `balance` | Account balance |
| `nonce` | Account nonce |
| `code` | Deployed bytecode (hex) |
| `initcode` | Constructor bytecode — triggers deployment via `txCreate`/`evmConstruct` |
| `creator` | Deployer address; used to compute CREATE address when `address` is omitted |
| `storage` | Object mapping slot keys to values |
| `tests` | Array of call test objects (see below) |
| `constructTest` | Object asserting constructor execution results |
| `import` | Path to another config JSON to load first |

Call test fields: `name`, `from`, `to`, `value`, `input`, `output`, `gasUsed`, `gas`, `status`, `logs`, `debug`, `blockNumber`, `timestamp`.

`constructTest` fields: `name`, `from`, `gasUsed`, `gas`, `output`, `status`, `logs`, `debug`, `blockNumber`, `timestamp`.

`gasUsed` is in hex. Get it from `bin/evm -w tst/foo.json` output or update in-place with `bin/evm -u -w tst/foo.json`.

### Unit tests (`tst/*.c`)
Exit 0 with no output = pass.

## Build System (Makefile)

- Header → object dep tracking via `CC -MM` + sed + `make/depend.pl`
- `include/FOO.h` → `lib/FOO.o` automatically
- External libraries: add a sentinel header in `include/` included from a public header (e.g. `include/secp256k1_libs.h` included from `evm.h`), add a sed rule in `.make/bin/%.d` and `.make/tst/bin/%.d` generation to convert it to the `.a` path
- `make/depend.pl` passes `.a` files through (they survive the filter)
- Clear dep cache with `rm -rf .make` when dep rules change

## Conventions

- camelCase for functions/variables, snake_case for types
- Errors to stderr
- Use the C preprocessor (X-macros) for tables
- `PRECOMPILES` and `OPS` macros are the single source of truth — adding to them propagates everywhere
