# CMake Build Guide

This document explains the CMake commands used in this project and how the
configure / build / configuration-type system works.

---

## Two-Phase Build System

CMake separates work into two distinct phases:

```
Source files  ──►  [Configure]  ──►  Build system files  ──►  [Build]  ──►  Binary
(CMakeLists.txt)                     (.sln, Makefiles, …)
```

**Configure phase** reads `CMakeLists.txt` and writes a native build system
into the build directory. No compilation happens here.

**Build phase** hands off to the native build system (MSBuild, Make, Ninja,
…) which actually compiles and links the code.

---

## Command Reference

### Configure

```bash
cmake -S . -B build
```

| Flag | Meaning |
|---|---|
| `-S .` | Source directory — where `CMakeLists.txt` lives |
| `-B build` | Build directory — where generated files are written |

On first run this also downloads **yaml-cpp** via `FetchContent` (requires
internet). Subsequent runs are fast cache hits unless `CMakeLists.txt` changes.

### Build

```bash
cmake --build build
```

Compiles only what has changed since the last build (incremental). On a clean
build or after source changes this compiles everything; if nothing changed it
does nothing at all.

#### Useful `--build` flags

```bash
# Build a specific configuration (MSVC / multi-config generators only)
cmake --build build --config Release

# Clean compiled artefacts, then do a full rebuild
cmake --build build --clean-first

# Delete compiled artefacts without rebuilding
cmake --build build --target clean

# Use N parallel jobs (speeds up the build)
cmake --build build --parallel 8
```

---

## Generators and Compiler Selection

CMake does not compile code itself — it generates input files for a **native
build system**. The generator is chosen at configure time.

On Windows, CMake auto-detects the highest installed Visual Studio version and
uses that as the default generator. It does **not** fall back to g++ unless
Visual Studio is absent or you override manually:

```bash
# Force a specific Visual Studio version
cmake -S . -B build -G "Visual Studio 17 2022"

# Use MinGW Makefiles (g++)
cmake -S . -B build -G "MinGW Makefiles"

# Use Ninja with g++ (fast, cross-platform)
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
```

Run `cmake --help` to list all generators available on your machine.

---

## Build Configurations

A **build configuration** controls optimization level and debug-symbol
generation. Visual Studio and other multi-config generators select it at
**build time** (`--config`); single-config generators (Ninja, Makefiles) set
it at **configure time** (`-DCMAKE_BUILD_TYPE`).

| Configuration | Optimization | Debug symbols | Typical use |
|---|---|---|---|
| `Debug` | Off (`/Od`) | Yes (`/Zi`) | Day-to-day development, debugger |
| `Release` | Full (`/O2`) | No | Production / benchmarking |
| `RelWithDebInfo` | On (`/O2`) | Yes (`/Zi`) | Profiling, crash analysis in prod |

### Multi-config generator (MSVC / Visual Studio) — configure once

```bash
cmake -S . -B build

cmake --build build --config Debug          # → bin/Debug/netto-brutto-rechner.exe
cmake --build build --config Release        # → bin/Release/netto-brutto-rechner.exe
cmake --build build --config RelWithDebInfo # → bin/RelWithDebInfo/netto-brutto-rechner.exe
```

### Single-config generator (Ninja, Makefiles) — configure per type

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release   # → bin/Release/netto-brutto-rechner

cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug     # → bin/Debug/netto-brutto-rechner
```

All binaries land under `bin/` (gitignored) as configured in `CMakeLists.txt`.

---

## Quick-Reference Cheat Sheet

```bash
# First time setup
cmake -S . -B build

# Incremental build (default Debug on MSVC)
cmake --build build

# Build a specific configuration
cmake --build build --config Release

# Wipe compiled artefacts and rebuild
cmake --build build --clean-first

# Clean only (no rebuild)
cmake --build build --target clean

# Full reset (re-runs configure + downloads)
rm -rf build && cmake -S . -B build
```
