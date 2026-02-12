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
- [Deployment](#deployment)
  - [How It Works](#how-it-works)
  - [Windows Deployment](#windows-deployment)
  - [macOS Deployment](#macos-deployment)
  - [Linux Deployment](#linux-deployment)
  - [CI/CD](#cicd)
  - [Package Structures](#package-structures)
  - [Deployment Tips](#deployment-tips)
- [Troubleshooting](#troubleshooting)

## Quick Start

**Windows:**

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
cmake --build build --config Release
cmake --install build --config Release --prefix dist
```

**macOS / Linux:**

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix dist
```

The install step produces a self-contained deployment (ZIP-ready directory on Windows, `.app` bundle on macOS, AppDir on Linux) with Qt libraries, third-party dependencies, and a bundled Python runtime all included.

## Prerequisites

### Required Software

| Software | Version | Windows | macOS | Linux |
|----------|---------|---------|-------|-------|
| CMake | 3.18+ | [cmake.org](https://cmake.org/download/) | `brew install cmake` | `apt install cmake` |
| C++ Compiler | C++17 | Visual Studio 2022 | Xcode Command Line Tools | GCC 9+ or Clang 10+ |
| Ninja | (optional) | [ninja-build.org](https://ninja-build.org/) | `brew install ninja` | `apt install ninja-build` |
| Qt | 6.x | [Qt Online Installer](https://www.qt.io/download-qt-installer) | Qt Online Installer | Qt Online Installer |
| Boost | 1.65+ | [Prebuilt binaries](https://sourceforge.net/projects/boost/files/boost-binaries/) | `brew install boost` | `apt install libboost-all-dev` |
| Python | 3.8+ | [python.org](https://www.python.org/downloads/) | System or `brew install python` | `apt install python3-dev` |
| patchelf | (Linux only) | N/A | N/A | `apt install patchelf` |

On Windows, Visual Studio is required even when using Ninja as the generator, since MSVC provides the compiler toolchain. You can also use the Visual Studio generator directly (`-G "Visual Studio 17 2022" -A x64`) instead of Ninja.

### Qt Components

The following Qt 6 components are required:

- Core, Widgets, PrintSupport
- QuickWidgets, Quick, Qml, QuickControls2
- Location, Positioning
- Multimedia, MultimediaWidgets

Qt is typically installed via the Qt Online Installer. Ensure the Qt installation is discoverable by CMake (either in your PATH or via `CMAKE_PREFIX_PATH`).

### Boost Components

Required Boost components: serialization, timer, chrono, system.

If Boost is not found automatically, set `BOOST_ROOT`:

```bash
# Windows (prebuilt binaries)
cmake ... -DBOOST_ROOT="C:/Program Files/Boost/boost_1_87_0"

# macOS (Homebrew)
cmake ... -DBOOST_ROOT=/opt/homebrew   # Apple Silicon
cmake ... -DBOOST_ROOT=/usr/local      # Intel

# Linux (apt)
cmake ... -DBOOST_ROOT=/usr
```

### Python

Python 3.8+ with development headers is required for pybind11 embedding.

- **Windows:** Install from [python.org](https://www.python.org/downloads/). Check "Download debug binaries" during installation if you plan to build Debug configurations.
- **macOS / Linux:** System Python is usually sufficient. Ensure development headers are available (`Python.h`).

## Build Instructions

### Full Build (Third-Party + Application)

This is the default build mode that builds all third-party dependencies (oneTBB, GTSAM, KDDockWidgets) and the main application.

**Windows:**

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
cmake --build build --config Release
```

**macOS / Linux:**

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Third-Party Only Build

Build only the third-party dependencies without building the application:

**Windows:**

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DFLYSIGHT_THIRD_PARTY_ONLY=ON
cmake --build build --config Release
```

**macOS / Linux:**

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DFLYSIGHT_THIRD_PARTY_ONLY=ON
cmake --build build
```

Alternatively, build from the `third-party/` directory directly:

```bash
cd third-party
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Application Only Build

If third-party dependencies are already built and installed in the `third-party/*-install` directories:

**Windows:**

```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DFLYSIGHT_BUILD_THIRD_PARTY=OFF
cmake --build build --config Release
```

**macOS / Linux:**

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DFLYSIGHT_BUILD_THIRD_PARTY=OFF
cmake --build build
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
| `BOOST_ROOT` | Platform-dependent | Boost root directory |

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
cmake --build build --target clean-oneTBB
cmake --build build --target clean-GTSAM
cmake --build build --target clean-KDDockWidgets
cmake --build build --target clean-third-party   # all third-party
```

When building from the `third-party/` directory, `clean-all` is available as an alias for `clean-third-party`.

### Manual Reset

For a complete reset, delete the build and install directories:

**Windows (PowerShell):**

```powershell
Remove-Item -Recurse -Force build
Remove-Item -Recurse -Force third-party\oneTBB-build, third-party\oneTBB-install
Remove-Item -Recurse -Force third-party\gtsam-build, third-party\GTSAM-install
Remove-Item -Recurse -Force third-party\KDDockWidgets-build, third-party\KDDockWidgets-install
```

**macOS / Linux:**

```bash
rm -rf build
rm -rf third-party/{oneTBB,gtsam,GTSAM,KDDockWidgets}-{build,install}
```

### Clean Target Reference

| Target | Description |
|--------|-------------|
| `clean-oneTBB` | Removes `third-party/oneTBB-build` and `third-party/oneTBB-install` |
| `clean-GTSAM` | Removes `third-party/gtsam-build` and `third-party/GTSAM-install` |
| `clean-KDDockWidgets` | Removes `third-party/KDDockWidgets-build` and `third-party/KDDockWidgets-install` |
| `clean-third-party` | Cleans all third-party components |

## Project Structure

```
flysight-viewer-2/
├── CMakeLists.txt                         # Root superbuild configuration
├── README.md                              # This file
├── cmake/
│   ├── ThirdPartySuperbuild.cmake         # ExternalProject definitions for oneTBB, GTSAM, KDDockWidgets
│   ├── PatchGTSAM.cmake                   # Patches GeographicLib to fix Windows build issue
│   ├── BoostDiscovery.cmake               # Cross-platform Boost discovery
│   ├── QtPathDiscovery.cmake              # Finds Qt plugin/QML directories
│   ├── GenerateIcon.cmake                 # Application icon generation
│   ├── BundlePythonWindows.cmake          # Downloads/installs Python embeddable package
│   ├── BundlePythonMacOS.cmake            # Downloads/installs python-build-standalone
│   ├── BundlePythonLinux.cmake            # Downloads/installs python-build-standalone
│   ├── DeployThirdPartyWindows.cmake      # Copies TBB/GTSAM/KDDW/Boost DLLs
│   ├── DeployThirdPartyMacOS.cmake        # Copies dylibs to Frameworks, fixes paths
│   ├── DeployThirdPartyLinux.cmake        # Copies .so files to AppDir, sets RPATH
│   ├── CreateAppDir.cmake                 # Creates Linux AppDir structure with AppRun
│   ├── CreateAppImage.cmake               # Generates AppImage via appimagetool
│   ├── RunLinuxDeployQt.cmake             # Optional linuxdeployqt integration
│   ├── fix_macos_rpaths.sh                # Post-install rpath repair for macOS bundles
│   └── diagnose_macos_bundle.sh           # Diagnostic tool for macOS bundle issues
├── src/
│   └── CMakeLists.txt                     # Main application build configuration
├── third-party/
│   ├── CMakeLists.txt                     # Standalone third-party build
│   ├── oneTBB/                            # Intel TBB submodule
│   ├── gtsam/                             # GTSAM submodule
│   ├── KDDockWidgets/                     # KDDockWidgets submodule
│   ├── Eigen/                             # Eigen headers
│   ├── QCustomPlot/                       # QCustomPlot library
│   └── pybind11/                          # pybind11 submodule
├── third-party/oneTBB-install/            # [Build artifact] oneTBB installation
├── third-party/GTSAM-install/             # [Build artifact] GTSAM installation
├── third-party/KDDockWidgets-install/     # [Build artifact] KDDockWidgets installation
└── build/                                 # [Build artifact] CMake build directory
```

## Deployment

FlySight Viewer deploys as a self-contained application with a bundled Python interpreter, requiring no user-installed dependencies. **The `cmake --install` command handles all deployment steps automatically**, including Qt library bundling, third-party library copying, Python bundling, and platform-specific path fixups.

### How It Works

`cmake --install` triggers platform-specific install rules that handle:

| Concern | Windows | macOS | Linux |
|---------|---------|-------|-------|
| Qt libraries & plugins | `qt_generate_deploy_app_script` | `qt_generate_deploy_app_script` | `qt_generate_deploy_app_script` |
| Geoservices & QML modules | CMake install rules | CMake install rules | CMake install rules |
| Third-party libs (TBB, GTSAM, KDDW) | DLLs copied to app root | dylibs copied to `Frameworks/` | `.so` files copied to `usr/lib/` |
| Boost | DLLs copied (if dynamic) | N/A (static via Homebrew) | `.so` files copied |
| Python runtime | Embeddable package downloaded | python-build-standalone downloaded | python-build-standalone downloaded |
| Library path fixups | N/A (flat DLL layout) | `install_name_tool` (automatic) | `patchelf` (automatic) |
| `qt.conf` | Generated in app root | Generated in `Resources/` | Generated in `usr/bin/` |
| AppDir + AppRun | N/A | N/A | Created automatically |

There is no need to manually run `macdeployqt`, `windeployqt`, copy libraries, or fix rpaths. The CMake install rules handle all of this.

### Windows Deployment

```bash
# 1. Build
cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
cmake --build build --config Release

# 2. Install (creates self-contained deployment)
cmake --install build --config Release --prefix dist

# 3. Package (ZIP)
cd build && cpack -G ZIP -C Release
```

The install step produces a flat directory with the executable, all DLLs, Qt plugins, QML modules, and the bundled Python runtime.

### macOS Deployment

```bash
# 1. Build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Install (creates .app bundle with Frameworks, Python, etc.)
cmake --install build --prefix dist

# 3. Package (DMG)
cd build && cpack -G DragNDrop -C Release
```

After packaging, code sign and notarize for distribution:

```bash
# Code sign (must be the LAST step before DMG creation / notarization)
codesign --deep --force --verify --verbose \
  --sign "Developer ID Application: Your Name" \
  dist/FlySightViewer.app

# Create DMG from signed bundle
hdiutil create -volname "FlySightViewer" \
  -srcfolder dist/FlySightViewer.app \
  -ov -format UDZO FlySightViewer.dmg

# Notarize (required for Gatekeeper on other machines)
xcrun notarytool submit FlySightViewer.dmg \
  --apple-id your@email.com \
  --team-id XXXXXXXXXX \
  --password @keychain:notary-password \
  --wait
xcrun stapler staple FlySightViewer.dmg
```

**Important:** Do not modify the `.app` bundle after code signing. The install step fixes all rpaths and library paths before signing occurs.

### Linux Deployment

```bash
# 1. Build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Install (creates AppDir with AppRun, libraries, Python, etc.)
cmake --install build --prefix dist

# 3. Create AppImage
cmake --build build --target appimage
```

The `appimage` target runs pre-flight verification (checks for all critical libraries, Qt plugins, Python bundle) before invoking `appimagetool`.

### CI/CD

GitHub Actions (`.github/workflows/build.yml`) automates the full pipeline on all three platforms. The workflow:

1. Caches third-party dependencies between runs
2. Builds third-party deps (on cache miss) then the application
3. Runs `cmake --install` to trigger all deployment logic
4. Packages with CPack (ZIP on Windows, DMG on macOS, AppImage on Linux)
5. Uploads artifacts and creates GitHub Releases on version tags

### Package Structures

**Windows:**

```
FlySightViewer/
├── FlySightViewer.exe
├── flysight_cpp_bridge.pyd
├── Qt6Core.dll, Qt6Widgets.dll, ...
├── tbb12.dll, gtsam.dll, ...
├── qt.conf
├── plugins/
│   ├── platforms/
│   ├── geoservices/
│   └── ...
├── qml/
│   ├── QtLocation/
│   └── QtPositioning/
└── python/
    ├── python313.dll
    ├── python313.zip
    └── Lib/site-packages/
```

**macOS:**

```
FlySightViewer.app/
└── Contents/
    ├── MacOS/FlySightViewer
    ├── Frameworks/
    │   ├── QtCore.framework, ...
    │   ├── libtbb.12.dylib, libgtsam.dylib, ...
    │   └── libpython3.XX.dylib
    ├── PlugIns/
    │   ├── platforms/
    │   └── geoservices/
    └── Resources/
        ├── qt.conf
        ├── qml/
        │   ├── QtLocation/
        │   └── QtPositioning/
        └── python/
            └── lib/python3.XX/site-packages/
```

**Linux (AppImage contents):**

```
FlySightViewer.AppDir/
├── AppRun
├── FlySightViewer.desktop
├── FlySightViewer.png
└── usr/
    ├── bin/
    │   ├── FlySightViewer
    │   └── qt.conf
    ├── lib/
    │   ├── libQt6*.so.*, libtbb.so.*, libgtsam.so.*, ...
    │   └── libkddockwidgets-qt6.so.*
    ├── plugins/
    │   ├── platforms/
    │   └── geoservices/
    ├── qml/
    │   ├── QtLocation/
    │   └── QtPositioning/
    └── share/
        └── python/
            ├── bin/python3
            └── lib/python3.XX/site-packages/
```

### Deployment Tips

1. **Python Version Matching**: The bundled Python version must exactly match the version used to build the pybind11 bindings. A version mismatch will cause crashes at startup.

2. **Package Sizes**: Expect approximately 80-120 MB per platform (includes Qt, Python, and NumPy).

3. **Testing**: Always test the deployed package on a clean machine without development tools installed.

4. **macOS Signing Order**: Code signing must happen after `cmake --install` completes. The install step modifies binaries (rpath fixing), which invalidates any existing signatures.

## Troubleshooting

### 1. GTSAM GeographicLib/js error

**Symptom:** Build fails with errors related to `gtsam/3rdparty/GeographicLib/js`.

**Cause:** GTSAM includes GeographicLib which has a `js` subdirectory that fails to build on Windows.

**Solution:** The CMake build system automatically patches this file via `cmake/PatchGTSAM.cmake`. If you encounter this error, ensure:
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
- Set `CMAKE_PREFIX_PATH` to your Qt installation:
  ```bash
  # Windows
  cmake ... -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2019_64"

  # macOS (Homebrew Qt or Qt Online Installer)
  cmake ... -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.3/macos"

  # Linux
  cmake ... -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.3/gcc_64"
  ```

### 3. Boost not found

**Symptom:** CMake error: `Could not find a configuration file for package "Boost"`.

**Solution:**
- Verify Boost is installed with the required components (serialization, timer, chrono, system)
- Set `BOOST_ROOT` as shown in [Prerequisites > Boost Components](#boost-components)
- On Windows, also set `BOOST_LIBRARYDIR` if the compiled `.lib` files are not in the default location

### 4. Python development headers not found

**Symptom:** CMake error: `Could not find a package configuration file provided by "Python"` or missing `Python.h`.

**Solution:**
- Install Python 3.8+ with development headers
- On Windows, ensure "Download debug binaries" was checked during installation for Debug builds
- Verify Python is in your PATH: `python --version`
- If multiple Python versions are installed, set `Python_ROOT_DIR`:
  ```bash
  cmake ... -DPython_ROOT_DIR="/path/to/python"
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
- Use Ninja for faster builds: `-G Ninja`
- Use parallel builds: `cmake --build build --parallel`
- After initial build, use `-DFLYSIGHT_BUILD_THIRD_PARTY=OFF` to skip rebuilding dependencies

### 8. Visual Studio shows many projects

**Symptom:** The Visual Studio solution contains many projects (oneTBB, GTSAM, KDDockWidgets targets).

**Explanation:** This is expected behavior for a superbuild. The solution includes ExternalProject targets for third-party dependencies, the main application targets, and clean targets.

**Tip:** Set `FlySightViewer` as the startup project for development.
