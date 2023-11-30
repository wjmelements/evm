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


## Updating the README
The `README.md` is assembled by concatentation when `make`.
See the `Makefile`.
If you want to update the opcodes supported table, it should be updated automatically.
Otherwise, the files you want to edit are:
* `make/.rme.md`
* `make/ops.sh`
* `CONTRIBUTING.md`
