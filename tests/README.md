# Testing and benchmarks

Tests can be run from the project's root directory.

```bash
tests/run.sh
```

This will run all tests using the system's default compiler.

If [Clang](https://clang.llvm.org) is your compiler then you will also be 
provided with memory address sanitizing and code coverage.

If you need Valgrind you can provide `VALGRIND=1`.

### Examples

```bash
tests/run.sh                   # defaults
CC=clang-17 tests/run.sh       # use alternative compiler
CFLAGS="-O3" tests/run.sh      # use custom cflags
NOSANS=1 tests/run.sh          # do not use sanitizers
VALGRIND=1 tests/run.sh        # use valgrind on all tests
CC="emcc" tests/run.sh         # Test with Emscripten (webassembly)
CC="zig cc" tests/run.sh       # Test with the Zig C compiler
```

## Benchmarks

Measures the performance of

- Creating geometries
- Point-in-polygon
- Intersection
- Reading and writing GeoJSON, WKT, WKB, and Hex

For the latest results, see [docs/BENCHMARKS.md](../docs/BENCHMARKS.md).


### Examples

```bash
tests/run.sh bench              # run all benchmarks
tests/run.sh bench pip          # only point-in-polygon benchmarks
tests/run.sh bench pip_simple   # only the simple point-in-polygon benchmarks
tests/run.sh bench intersects   # only intersects benchmarks
tests/run.sh bench io           # only parsing and writing benchmarks
GEOS_BENCH=1 tests/run.sh bench # include the GEOS library in benchmarks 
```
