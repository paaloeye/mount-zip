# How to Build

## Prerequisites

To build **mount-zip**, you need the following libraries:

*   [Boost Intrusive](https://www.boost.org)
*   [ICU](https://icu.unicode.org)
*   [libfuse >= 3.1](https://github.com/libfuse/libfuse)
*   [libzip >= 1.9.1](https://libzip.org)

### Debian / Ubuntu

```sh
$ sudo apt install libboost-container-dev libicu-dev libfuse3-dev libzip-dev
```

For compatibility reasons, **mount-zip** can optionally use the old FUSE 2
library [libfuse >= 2.9](https://github.com/libfuse/libfuse). On Debian systems,
you can install FUSE 2 by installing the following package:

```sh
$ sudo apt install libfuse-dev
```

To build **mount-zip**, you also need the following tools:

*   C++20 compiler (g++ or clang++)
*   [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
*   [GNU make](https://www.gnu.org/software/make/)
*   [GoogleTest](https://github.com/google/googletest) (for unit tests)
*   [Pandoc](https://pandoc.org) to generate the man page

```sh
$ sudo apt install g++ pkg-config make libgtest-dev pandoc
```

To test **mount-zip**, you also need the following tools:

*   `umount`
*   [Python >= 3.8](https://www.python.org)

```sh
$ sudo apt install mount python3
```

### macOS

macOS requires [macFUSE](https://osxfuse.github.io) instead of libfuse. Install
it from the official disk image (it installs a kernel extension and requires
approval in **System Settings → Privacy & Security**):

```sh
$ brew install --cask macfuse
```

Then install the remaining dependencies via [Homebrew](https://brew.sh):

```sh
$ brew install boost icu4c libzip googletest pandoc
```

The `Makefile` detects macOS automatically and locates all Homebrew-installed
libraries without any extra environment variables.

## Build **mount-zip**

```sh
$ make
```

### With debugging assertions

```sh
$ DEBUG=1 make
```

### With FUSE 2 (Linux only)

```sh
$ FUSE_MAJOR_VERSION=2 make
```

## Test **mount-zip**

### All tests (including slow tests)

```sh
$ make check
```

### Only fast tests

```sh
$ make check-fast
```

> [!NOTE]
> On macOS the test runner requires `python3` (from Xcode CLT or Homebrew) and
> uses `umount -f` automatically instead of the Linux-only `umount -l`.

## Install **mount-zip**

```sh
$ sudo make install
```

## Uninstall **mount-zip**

```sh
$ sudo make uninstall
```
