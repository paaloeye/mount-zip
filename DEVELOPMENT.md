# Development Guide for mount-zip

This document provides project-specific context, conventions, and workflows for building and contributing to the `mount-zip` repository.

## Project Overview

`mount-zip` is a read-only FUSE filesystem that mounts ZIP archives using the `libzip` and `ICU` libraries. The primary implementation is contained in `mount-zip.cc` and the `lib/` directory.

## Build and Development

### Makefile Variables

| Variable               | Default | Effect                                                                        |
| ---------------------- | ------- | ----------------------------------------------------------------------------- |
| `DEBUG=1`              | off     | Enable debug symbols, disable optimisations, enable tree teardown on shutdown |
| `ASAN=1`               | off     | Enable AddressSanitizer                                                       |
| `UBSAN=1`              | off     | Enable UndefinedBehaviourSanitizer                                            |
| `FUSE_MAJOR_VERSION=2` | 3       | Build against FUSE 2 instead of FUSE 3 (Linux only)                           |

### Key Makefile Targets

| Target            | Description                                          |
| ----------------- | ---------------------------------------------------- |
| `make all`        | Build the `mount-zip` binary and the man page        |
| `make check-fast` | Run the fast subset of tests                         |
| `make check`      | Run the full test suite (includes slow tests)        |
| `make doc`        | Regenerate `mount-zip.1` from `README.md` via pandoc |
| `make clean`      | Remove all build artefacts                           |
| `make debug`      | Shorthand for `DEBUG=1 make all`                     |
| `make valgrind`   | Run tests under Valgrind (Linux only)                |

### macOS Environment

The `Makefile` detects macOS at parse time via `uname -s`, queries the
Homebrew prefix with `brew --prefix`, and overrides `PKG_CONFIG` to carry
pinned search paths for macFUSE (`/usr/local`), ICU, and libzip. `CPPFLAGS`
is extended with the Boost include directory. Both are exported so all
sub-makes inherit them.

```sh
make
make check-fast
```

## Documentation Pipeline

The `README.md` file serves as both the user guide and the source for the man page.

*   **Generation**: `pandoc` converts `README.md` to `roff` format.
*   **Formatting**: The `Makefile` uses `sed` post-processing on the `pandoc` output to ensure bulleted lists are rendered compactly (using `.PD 0`) in the man page.
*   **Markdown requirement**: Bulleted lists in `README.md` should be preceded by a blank line for correct `pandoc` parsing.

## Technical Standards

### Memory Safety

The project aims for full **ASAN compliance**. Always verify changes with `ASAN=1 make check-fast`.

### Resource Management (RAII)

*   Use RAII guards for resource cleanup (e.g., `ScopedFile`, `Cleanup`).
*   **Shutdown Performance**: Global teardown of the virtual tree is wrapped in `#ifndef NDEBUG`. It is only performed in debug builds to keep production shutdown nearly instant.

### Portability

*   The project is 32-bit compatible.
*   **Year 2038**: Always build with `-D_TIME_BITS=64` (handled in `Makefile`) to ensure correct timestamp handling on 32-bit systems.
*   **macOS**: The source guards Apple-specific differences with `#ifdef __APPLE__`. Notable divergences:
    *   No `memfd_create()` — `lib/reader.cc` uses a temp-file fallback.
    *   `typeof` must be undefined around the `fuse.h` include to satisfy `-pedantic`.
    *   `boost::hash_combine` requires an explicit `#include <boost/container_hash/hash.hpp>` because Apple Clang does not pull it in transitively.

## Testing

*   **Main Runner**: `tests/blackbox/test.py`
*   **Whitebox Tests**: C++ unit tests in `tests/whitebox/` using the [GoogleTest](https://github.com/google/googletest) framework.
*   **Unmounting**: The blackbox test runner detects the host OS and uses `umount -f` on macOS and `umount -l` on Linux.
