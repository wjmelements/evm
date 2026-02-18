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
```
JSON fields: `construct` (path to .evm), `tests` (array of `from`, `value`, `gasUsed`, `output`, `debug`).
`gasUsed` in hex. Get it from: `bin/evm -w tst/foo.json` output or by running with `-gs`.

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
