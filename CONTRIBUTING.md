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
