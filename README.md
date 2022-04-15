# itiviti-cpp-analyzer

itiviti-cpp-analyzer (or ICA) is a Clang plugin, which brings several static analysis checks ([listed here](Checks.md)).

If you want to contribute to the project, see [CONTRIBUTING.md](./CONTRIBUTING.md).

- [itiviti-cpp-analyzer](#itiviti-cpp-analyzer)
  - [Building](#building)
    - [Prerequisites](#prerequisites)
    - [Build guide](#build-guide)
  - [Usage](#usage)
    - [Checks list](#checks-list)
    - [Bare compiler](#bare-compiler)
    - [CMake integration](#cmake-integration)
      - [Use as an external project](#use-as-an-external-project)
      - [Use as a subdirectory](#use-as-a-subdirectory)
    - [Supress a warning](#supress-a-warning)

## Building

### Prerequisites

You need Clang and CMake to build the plugin. Also you need `libclang-10-dev` and `libclang-cpp10-dev` packages. Optionally, you may need Boost 1.68+ headers (it can be downloaded during the build instead).

Currently, ICA only works with `clang-10`

### Build guide

```bash
[ ! -d build ] && mkdir build
cd build

cmake \
    -DGCC_TOOLCHAIN=<path/to/gcc/toolchain (probably, /usr/lib)> \
    -DBOOST_FROM_INTERNET=ON \
    -DTARGET_COMPILER=clang++-10 \
    ../

cmake --build . --parallel
```

* `BOOST_FROM_INTERNET` is only needed if you haven't Boost headers
* `BOOST_ROOT` can be specified instead
* `TARGET_COMPILER` is the compiler used for tests. Specify if you don't use `clang-10` for compilation

The built plugin will be in `./build/libica-plugin.so`

## Usage

You need `libica-plugin.so` and `clang-10`

### Checks list

A comma separated list of checks with optionally specified emit levels

Check names are listed [here](Checks.md). Alias to list all the checks - `all`

Emit levels:
* nothing - `none` - check is disabled
* warning - `warn` - default
* error - `err`

Example:

```
all=warn,-redundant-noexcept,erase-in-loop=err
```

will enable `all` checks with warning level, disable `redundant-noexcept` and set error level for `erase-in-loop` check

### Bare compiler

You need additional flags:
* `-load path/to/libica-plugin.so`
* `-add-plugin ica-plugin`
* `-plugin-arg-ica-plugin checks=$CHECKS`
* `-plugin-arg-ica-plugin no-url` - optionally disable integrating URL into check message

`CHECKS` is the [check list](README.md#checks-list)

Every argument for the compiler frontend is passed with `-Xclang`, so the final list looks like that:

```
-Xclang -load -Xclang ../build/libica-plugin.so \
    -Xclang -add-plugin -Xclang ica-plugin \
    -Xclang -plugin-arg-ica-plugin -Xclang checks=$CHECKS
```

### CMake integration

If you have a CMake project, there are options to use ICA easily, either as an external project or a subdirectory in your workspace. In any case, several CMake helpers should become available:

* `add_ica_checks(check1 check2 ...)` - load plugin and enable specified checks. You can use emit levels here as usual.
* `ica_no_url()` - disable integrating URL into check message.

Running `target_ica_checks(MyTarget VISIBILITY ...)` or `target_ica_no_url(MyTarget VISIBILITY)` will apply configuration to single target and/or its dependencies.

Here are some minimal integration examples:

#### Use as an external project

First you need to have your ICA built ([see](README.md#build-guide)) and installed:

```bash
cd build && cmake --install --install-prefix /path/to/ica/installation/
```

And add this to your _CMakeLists.txt_

```cmake
# If ICA is installed in unusual location
list(APPEND CMAKE_PREFIX_PATH "/path/to/ica/installation")

find_package(ICA CONFIG REQUIRED)
add_ica_checks(<your checks list>)
```

#### Use as a subdirectory

Add ICA sources as subdirectory to your project (probably through _git submodule_) and add this to your _CMakeLists.txt_

```cmake
# Set any other cache variables here: GCC_TOOLCHAIN, LLVM_ROOT, ...
set(BOOST_FROM_INTERNET ON)

add_subdirectory(itiviti-cpp-analyzer)
add_ica_checks(<your checks list>)
```

### Supress a warning

`// NOLINT` comment will suppress any warning on the line

```cpp
map.emplace(0, std::string{}); // NOLINT
```
[emplace-default-value](Checks.md#emplace-default-value) check will be suppressed here

