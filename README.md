# FlySight Viewer 2

A desktop application for viewing and analyzing FlySight GPS data with advanced trajectory smoothing, interactive plots, and video synchronization.

## Table of Contents

- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Build Instructions](#build-instructions)
  - [Full Build (Third-Party + Application)](#full-build-third-party--application)
  - [Third-Party Only Build](#third-party-only-build)
  - [Application Only Build](#application-only-build)
  - [Build Options](#build-options)
  - [Build Output Locations](#build-output-locations)
- [Clean Targets](#clean-targets)
  - [Individual Clean Targets](#individual-clean-targets)
  - [Manual Reset](#manual-reset)
  - [Clean Target Reference](#clean-target-reference)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Legacy Build Scripts](#legacy-build-scripts)

## Quick Start

```bash
# Configure the build
cmake -G "Visual Studio 17 2022" -A x64 -B build -S .

# Build everything (third-party dependencies + application)
cmake --build build --config Release
```

## Prerequisites

### Required Software

| Software | Version | Notes |
|----------|---------|-------|
| CMake | 3.18+ | Build system generator |
| Visual Studio | 2022 | With C++ desktop development workload |
| Qt | 6.x | See required components below |
| Boost | 1.65+ | See required components below |
| Python | 3.8+ | With development headers |

### Qt Components

The following Qt 6 components are required:

- Core
- Widgets
- PrintSupport
- QuickWidgets
- Quick
- Qml
- Location
- Positioning
- Multimedia
- MultimediaWidgets

Qt is typically installed via the Qt Online Installer. Ensure the Qt bin directory is in your PATH or that CMake can find it.

### Boost Components

The following Boost components are required:

- serialization
- timer
- chrono
- system

**Default Boost path:** `C:/Program Files/Boost/boost_1_87_0`

To use a different Boost installation, set the `BOOST_ROOT` CMake variable:

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DBOOST_ROOT="C:/path/to/boost"
```

### Python

Python 3.8 or later with development headers is required for pybind11 embedding.

- On Windows, install Python from [python.org](https://www.python.org/downloads/) and ensure "Download debug binaries" is checked during installation if you plan to build Debug configurations.
- The Python installation directory should be in your PATH.

## Build Instructions

### Full Build (Third-Party + Application)

This is the default build mode that builds all third-party dependencies (oneTBB, GTSAM, KDDockWidgets) and the main application:

```bash
# Configure
cmake -G "Visual Studio 17 2022" -A x64 -B build -S .

# Build Release
cmake --build build --config Release

# Build Debug (optional)
cmake --build build --config Debug
```

### Third-Party Only Build

There are two ways to build only the third-party dependencies:

#### Option 1: From the third-party directory

```bash
cd third-party
cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
cmake --build build --config Release
cmake --build build --config Debug
```

#### Option 2: Using FLYSIGHT_THIRD_PARTY_ONLY option

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DFLYSIGHT_THIRD_PARTY_ONLY=ON
cmake --build build --config Release
cmake --build build --config Debug
```

### Application Only Build

If third-party dependencies are already built and installed in the `third-party/*-install` directories, you can skip rebuilding them:

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DFLYSIGHT_BUILD_THIRD_PARTY=OFF
cmake --build build --config Release
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `FLYSIGHT_BUILD_THIRD_PARTY` | `ON` | Build third-party dependencies (oneTBB, GTSAM, KDDockWidgets) |
| `FLYSIGHT_BUILD_APP` | `ON` | Build the main FlySight Viewer application |
| `FLYSIGHT_THIRD_PARTY_ONLY` | `OFF` | Build only third-party dependencies (automatically disables app build) |

**Path Variables:**

| Variable | Default | Description |
|----------|---------|-------------|
| `THIRD_PARTY_DIR` | `<project>/third-party` | Root directory for third-party dependencies |
| `ONETBB_INSTALL_DIR` | `<third-party>/oneTBB-install` | oneTBB installation directory |
| `GTSAM_INSTALL_DIR` | `<third-party>/GTSAM-install` | GTSAM installation directory |
| `KDDW_INSTALL_DIR` | `<third-party>/KDDockWidgets-install` | KDDockWidgets installation directory |
| `BOOST_ROOT` | `C:/Program Files/Boost/boost_1_87_0` | Boost root directory |

### Build Output Locations

After a successful build:

| Component | Install Directory |
|-----------|-------------------|
| oneTBB | `third-party/oneTBB-install/` |
| GTSAM | `third-party/GTSAM-install/` |
| KDDockWidgets | `third-party/KDDockWidgets-install/` |
| Application | `build/` (executables in build root) |

## Clean Targets

### Individual Clean Targets

Clean individual third-party components:

```bash
# Clean oneTBB
cmake --build build --target clean-oneTBB

# Clean GTSAM
cmake --build build --target clean-GTSAM

# Clean KDDockWidgets
cmake --build build --target clean-KDDockWidgets

# Clean all third-party
cmake --build build --target clean-third-party
```

When building from the `third-party/` directory, an additional alias is available:

```bash
# Clean all (alias for clean-third-party)
cmake --build build --target clean-all
```

### Manual Reset

For a complete reset, manually delete the build and install directories:

**Windows (PowerShell):**

```powershell
# Remove build directory
Remove-Item -Recurse -Force build

# Remove third-party build artifacts
Remove-Item -Recurse -Force third-party\oneTBB-build
Remove-Item -Recurse -Force third-party\oneTBB-install
Remove-Item -Recurse -Force third-party\gtsam-build
Remove-Item -Recurse -Force third-party\GTSAM-install
Remove-Item -Recurse -Force third-party\KDDockWidgets-build
Remove-Item -Recurse -Force third-party\KDDockWidgets-install
```

**Windows (Command Prompt):**

```batch
rmdir /s /q build
rmdir /s /q third-party\oneTBB-build
rmdir /s /q third-party\oneTBB-install
rmdir /s /q third-party\gtsam-build
rmdir /s /q third-party\GTSAM-install
rmdir /s /q third-party\KDDockWidgets-build
rmdir /s /q third-party\KDDockWidgets-install
```

**Unix (Linux/macOS):**

```bash
rm -rf build
rm -rf third-party/oneTBB-build
rm -rf third-party/oneTBB-install
rm -rf third-party/gtsam-build
rm -rf third-party/GTSAM-install
rm -rf third-party/KDDockWidgets-build
rm -rf third-party/KDDockWidgets-install
```

### Clean Target Reference

| Target | Description |
|--------|-------------|
| `clean-oneTBB` | Removes `third-party/oneTBB-build` and `third-party/oneTBB-install` |
| `clean-GTSAM` | Removes `third-party/gtsam-build` and `third-party/GTSAM-install` |
| `clean-KDDockWidgets` | Removes `third-party/KDDockWidgets-build` and `third-party/KDDockWidgets-install` |
| `clean-third-party` | Cleans all third-party components |
| `clean-all` | Alias for `clean-third-party` (available when building from `third-party/`) |

## Project Structure

```
flysight-viewer-2/
├── CMakeLists.txt                    # Root superbuild configuration
├── README.md                         # This file
├── cmake/
│   ├── ThirdPartySuperbuild.cmake    # ExternalProject definitions for oneTBB, GTSAM, KDDockWidgets
│   └── PatchGTSAM.cmake              # Patches GeographicLib to fix Windows build issue
├── src/
│   └── CMakeLists.txt                # Main application build configuration
├── third-party/
│   ├── CMakeLists.txt                # Standalone third-party build
│   ├── BUILD.BAT                     # Legacy Windows build script
│   ├── CLEAN.BAT                     # Legacy Windows clean script
│   ├── oneTBB/                       # Intel TBB submodule
│   ├── gtsam/                        # GTSAM submodule
│   ├── KDDockWidgets/                # KDDockWidgets submodule
│   ├── Eigen/                        # Eigen headers
│   ├── QCustomPlot/                  # QCustomPlot library
│   └── pybind11/                     # pybind11 submodule
├── third-party/oneTBB-install/       # [Build artifact] oneTBB installation
├── third-party/GTSAM-install/        # [Build artifact] GTSAM installation
├── third-party/KDDockWidgets-install/# [Build artifact] KDDockWidgets installation
└── build/                            # [Build artifact] CMake build directory
```

## Troubleshooting

### 1. GTSAM GeographicLib/js error

**Symptom:** Build fails with errors related to `gtsam/3rdparty/GeographicLib/js`.

**Cause:** GTSAM includes GeographicLib which has a `js` subdirectory that fails to build on Windows.

**Solution:** The CMake build system automatically patches this file via `cmake/PatchGTSAM.cmake`. If you encounter this error, ensure:
- You are using the CMake build system (not the legacy BUILD.BAT)
- The `cmake/PatchGTSAM.cmake` file exists
- The patch script has write access to `third-party/gtsam/3rdparty/GeographicLib/CMakeLists.txt`

To manually apply the patch:
```bash
cmake -DGTSAM_SOURCE_DIR=third-party/gtsam -P cmake/PatchGTSAM.cmake
```

### 2. Qt not found

**Symptom:** CMake error: `Could not find a package configuration file provided by "Qt6"`.

**Solution:**
- Ensure Qt 6 is installed with all required components
- Add the Qt installation to your PATH or set `CMAKE_PREFIX_PATH`:
  ```bash
  cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"
  ```

### 3. Boost not found

**Symptom:** CMake error: `Could not find a configuration file for package "Boost"`.

**Solution:**
- Verify Boost is installed with the required components (serialization, timer, chrono, system)
- Set the correct Boost path:
  ```bash
  cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DBOOST_ROOT="C:/path/to/boost"
  ```
- Ensure `BOOST_LIBRARYDIR` points to the directory containing compiled `.lib` files (default: `${BOOST_ROOT}/stage/lib`)

### 4. Python development headers not found

**Symptom:** CMake error: `Could not find a package configuration file provided by "Python"` or missing `Python.h`.

**Solution:**
- Install Python 3.8+ with development headers
- On Windows, ensure "Download debug binaries" was checked during installation for Debug builds
- Verify Python is in your PATH:
  ```bash
  python --version
  ```

### 5. TBB configuration fails for GTSAM

**Symptom:** GTSAM build fails with TBB-related errors.

**Solution:**
- Ensure oneTBB was built successfully before GTSAM
- Verify `third-party/oneTBB-install/lib/cmake/TBB/` exists
- If rebuilding, clean GTSAM and rebuild:
  ```bash
  cmake --build build --target clean-GTSAM
  cmake --build build --config Release
  ```

### 6. KDDockWidgets Qt6 not found

**Symptom:** KDDockWidgets build fails with Qt6 not found errors.

**Solution:**
- Ensure Qt6 is properly installed and discoverable
- The KDDockWidgets build uses `-DKDDockWidgets_QT6=ON` to enable Qt6 support
- Set `CMAKE_PREFIX_PATH` if Qt is not in a standard location

### 7. Build takes too long

**Symptom:** Third-party builds (especially GTSAM) take a very long time.

**Solution:**
- GTSAM is a large library; initial builds can take 30+ minutes
- Use parallel builds: `cmake --build build --config Release -- /maxcpucount` (MSVC)
- After initial build, use `-DFLYSIGHT_BUILD_THIRD_PARTY=OFF` to skip rebuilding dependencies
- Consider building only Release OR Debug initially, not both

### 8. Visual Studio shows many projects

**Symptom:** The Visual Studio solution contains many projects (oneTBB, GTSAM, KDDockWidgets targets).

**Explanation:** This is expected behavior for a superbuild. The solution includes:
- ExternalProject targets for third-party dependencies
- The main FlySight Viewer application targets
- Clean targets for each component

**Tip:** In Visual Studio, set `FlySightViewer` as the startup project for development.

## Legacy Build Scripts

For backward compatibility, the original Windows batch files are still available in the `third-party/` directory.

### BUILD.BAT

**Location:** `third-party/BUILD.BAT`

**Usage:**
```batch
cd third-party
BUILD.BAT
```

**Important:** When using BUILD.BAT, you must manually patch GTSAM's GeographicLib before building:

1. Open `third-party/gtsam/3rdparty/GeographicLib/CMakeLists.txt`
2. Find the line: `add_subdirectory (js)`
3. Comment it out: `#add_subdirectory (js)`
4. Save the file
5. Run BUILD.BAT

### CLEAN.BAT

**Location:** `third-party/CLEAN.BAT`

**Usage:**
```batch
cd third-party
CLEAN.BAT
```

Note: CLEAN.BAT only removes oneTBB and GTSAM build artifacts, not KDDockWidgets.

### Migration Recommendation

We recommend migrating to the CMake-based build system for the following advantages:

1. **Automatic patching:** The GTSAM GeographicLib patch is applied automatically
2. **Cross-platform:** Works on Windows, Linux, and macOS (Windows is the primary platform)
3. **IDE integration:** Better integration with Visual Studio and other IDEs
4. **Flexible builds:** Easy control over what gets built via CMake options
5. **Clean targets:** Proper clean targets for individual or all components
6. **Parallel builds:** Better support for parallel compilation
