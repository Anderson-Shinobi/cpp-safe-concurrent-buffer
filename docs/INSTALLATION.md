# Installation

## Requirements

- CMake 3.20 or newer
- A C++20 compiler
- A threading implementation supported by CMake

## Configure a Local Installation

Use a user-owned prefix to avoid changing system directories:

```bash
cmake -S . -B build-install \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DCMAKE_INSTALL_PREFIX="$PWD/local-install"
```

## Build

```bash
cmake --build build-install --parallel
```

## Install

```bash
cmake --install build-install
```

## Installed Layout

The installation contains the public header, the compiled library, and the
CMake package files. A typical Unix-like prefix uses:

```text
local-install/
├── include/safe_concurrent_buffer.hpp
└── lib/
    ├── libconcurrent_buffer.a
    └── cmake/cpp_safe_concurrent_buffer/
        ├── cpp_safe_concurrent_bufferConfig.cmake
        ├── cpp_safe_concurrent_bufferConfigVersion.cmake
        └── cpp_safe_concurrent_bufferTargets*.cmake
```

Exact library directory names and configuration-specific target files can vary
by platform, generator, and installation conventions.

## Consuming with find_package

An external project can consume the installed package without source-tree
paths:

```cmake
find_package(cpp_safe_concurrent_buffer 0.3 REQUIRED)

target_link_libraries(
    my_target
    PRIVATE
        cpp_safe_concurrent_buffer::concurrent_buffer
)
```

The imported target supplies the public include directory, C++20 requirement,
and threading dependency.

## Configuring the Consumer

```bash
cmake \
    -S examples/installed_consumer \
    -B build-consumer \
    -DCMAKE_PREFIX_PATH="$PWD/local-install"
```

## Build and Run the Consumer

```bash
cmake --build build-consumer --parallel
./build-consumer/installed_consumer
```

## Custom Prefixes

`CMAKE_INSTALL_PREFIX` selects where the package is installed.
`CMAKE_PREFIX_PATH` tells a consuming CMake project which prefixes to search.
They may be different absolute paths, and an installed tree can be relocated as
a unit before configuring the consumer.

Installing into a protected system directory can require elevated permissions
and can conflict with system package management. Prefer a user-owned prefix
unless a system administrator has selected and reviewed the destination.

## Uninstallation

CMake does not provide a universal generated `uninstall` target. The configure
and install process records installed files in:

```text
build-install/install_manifest.txt
```

Review that manifest before removing anything:

```bash
cmake -E cat build-install/install_manifest.txt
```

Remove only the exact files listed in the reviewed manifest, then remove empty
package directories if appropriate. Do not broadly delete an installation
prefix, especially a shared system prefix.
