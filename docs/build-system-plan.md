# Build System Patterns and Guidelines

This document summarizes patterns established during the GitHub Workflow CI/CD fixes project (documented in `docs/implementation-plan/`). These patterns should be followed when making future modifications to the build system, CMake configuration, or GitHub Actions workflows.

## Table of Contents

1. [CMake Variable Management](#cmake-variable-management)
2. [Cross-Platform Library Discovery](#cross-platform-library-discovery)
3. [Third-Party Library Deployment](#third-party-library-deployment)
4. [RPATH and Library Path Configuration](#rpath-and-library-path-configuration)
5. [Qt Plugin and QML Deployment](#qt-plugin-and-qml-deployment)
6. [Python Bundling](#python-bundling)
7. [GitHub Actions Workflow Patterns](#github-actions-workflow-patterns)
8. [Verification and Diagnostics](#verification-and-diagnostics)
9. [Platform-Specific Considerations](#platform-specific-considerations)

---

## CMake Variable Management

### Single Source of Truth

Define cache variables exactly once to avoid conflicts and ensure consistent behavior between clean and incremental builds.

**Pattern:**
```cmake
# Define once at the top of the file or in a shared module
set(MY_VAR "default_value" CACHE PATH "Description")
```

**Anti-pattern:**
```cmake
# DON'T define the same cache variable multiple times
set(MY_VAR "value1" CACHE PATH "...")  # Line 35
# ... later in the file ...
set(MY_VAR "value2" CACHE PATH "...")  # Line 200 - BAD
```

### Variable Definition Conventions

Group related variables together at the top of `src/CMakeLists.txt`:

```cmake
# Third-party path variables (defined once, cached, can be overridden)
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/../third-party" CACHE PATH "...")
set(GTSAM_ROOT "${THIRD_PARTY_DIR}/GTSAM-install" CACHE PATH "...")
set(TBB_ROOT "${THIRD_PARTY_DIR}/oneTBB-install" CACHE PATH "...")
```

### Include Guards for CMake Modules

Prevent issues from multiple inclusion:

```cmake
# At the top of cmake/MyModule.cmake
if(_MY_MODULE_INCLUDED)
    return()
endif()
set(_MY_MODULE_INCLUDED TRUE)
```

---

## Cross-Platform Library Discovery

### Shared Discovery Modules

Create reusable CMake modules for cross-platform library detection (e.g., `cmake/BoostDiscovery.cmake`, `cmake/QtPathDiscovery.cmake`).

**Structure:**
```cmake
# cmake/LibraryDiscovery.cmake

# Include guard
if(_LIBRARY_DISCOVERY_INCLUDED)
    return()
endif()
set(_LIBRARY_DISCOVERY_INCLUDED TRUE)

# Platform-specific defaults
if(WIN32)
    set(_DEFAULT_PATH "C:/path/to/library")
elseif(APPLE)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE _ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_ARCH STREQUAL "arm64")
        set(_DEFAULT_PATH "/opt/homebrew")
    else()
        set(_DEFAULT_PATH "/usr/local")
    endif()
else()
    set(_DEFAULT_PATH "/usr")
endif()

# Set cache variable with default
if(NOT DEFINED CACHE{LIBRARY_ROOT})
    set(LIBRARY_ROOT "${_DEFAULT_PATH}" CACHE PATH "Library root directory")
    message(STATUS "Library: Using default LIBRARY_ROOT=${LIBRARY_ROOT}")
else()
    message(STATUS "Library: Using cached LIBRARY_ROOT=${LIBRARY_ROOT}")
endif()
```

### Multiple Search Path Strategy

When locating resources, check multiple platform-specific paths:

```cmake
set(_SEARCH_PATHS
    "${PREFIX}/lib/cmake/Package"      # Standard
    "${PREFIX}/lib64/cmake/Package"    # 64-bit Linux
    "${PREFIX}/share/cmake/Package"    # Alternate
)

foreach(_path ${_SEARCH_PATHS})
    if(EXISTS "${_path}/PackageConfig.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_path}/..")
        message(STATUS "Found Package config at: ${_path}")
        break()
    endif()
endforeach()
```

### Environment Variable Support

Allow CI/CD to override paths via environment variables:

```cmake
if(DEFINED ENV{LIBRARY_ROOT} AND NOT DEFINED LIBRARY_ROOT)
    set(LIBRARY_ROOT "$ENV{LIBRARY_ROOT}")
endif()
```

---

## Third-Party Library Deployment

### Platform-Specific Deployment Scripts

Use separate CMake scripts for each platform's deployment needs:

- `cmake/DeployThirdPartyWindows.cmake` - DLL deployment to flat structure
- `cmake/DeployThirdPartyMacOS.cmake` - Framework bundling with install_name_tool
- `cmake/DeployThirdPartyLinux.cmake` - Shared library deployment to AppDir

### Symlink Preservation (Unix)

Always use `cp -P` to preserve symlinks when copying libraries:

```cmake
execute_process(
    COMMAND cp -P "${lib}" "${DEST_DIR}/"
    RESULT_VARIABLE cp_result
)
```

### Separate Real Files from Symlinks

Process real files first, then recreate symlinks:

```cmake
# First pass: copy real files
foreach(lib ${LIBS})
    if(NOT IS_SYMLINK "${lib}")
        # Copy and process
    endif()
endforeach()

# Second pass: copy symlinks
foreach(lib ${LIBS})
    if(IS_SYMLINK "${lib}")
        execute_process(COMMAND cp -P "${lib}" "${DEST_DIR}/")
    endif()
endforeach()
```

---

## RPATH and Library Path Configuration

### macOS RPATH Strategy

Use `@executable_path` and `@loader_path` references:

```cmake
if(APPLE)
    set_target_properties(MyApp PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
        MACOSX_RPATH TRUE
    )
endif()
```

For pybind11 modules (installed deeper in the directory structure):

```cmake
set_target_properties(my_bridge PROPERTIES
    INSTALL_RPATH "@loader_path/../../../../../Frameworks;@executable_path/../Frameworks"
    INSTALL_NAME_DIR "@rpath"
)
```

### macOS Library ID Fixing

Fix library IDs at install time:

```cmake
install(CODE "
    execute_process(
        COMMAND install_name_tool -id \"@rpath/\${DYLIB_NAME}\" \"\${DYLIB}\"
    )
")
```

**Important:** Remove code signatures before modifying on macOS 12+:
```bash
codesign --remove-signature "$dylib"
install_name_tool -id "@rpath/libname.dylib" "$dylib"
```

### Linux RPATH Strategy

Use `$ORIGIN` relative paths for relocatable binaries:

```cmake
if(UNIX AND NOT APPLE)
    set_target_properties(MyApp PROPERTIES
        BUILD_RPATH "$ORIGIN/../lib"
        INSTALL_RPATH "$ORIGIN/../lib"
    )
endif()
```

Use `patchelf` at install time to ensure correct RPATH:

```cmake
install(CODE "
    find_program(PATCHELF_EXE patchelf REQUIRED)
    execute_process(
        COMMAND \"\${PATCHELF_EXE}\" --set-rpath \"\\\$ORIGIN/../lib\" \"\${EXECUTABLE}\"
    )
")
```

### Build vs Install RPATH

Support both development and distribution scenarios:

```cmake
set_target_properties(MyApp PROPERTIES
    BUILD_RPATH "${TBB_ROOT}/lib;${GTSAM_ROOT}/lib"
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH FALSE  # Use BUILD_RPATH during development
)
```

---

## Qt Plugin and QML Deployment

### qt.conf for Runtime Discovery

Create a `qt.conf` file to help Qt find plugins:

**Windows:**
```ini
[Paths]
Plugins = plugins
Qml2Imports = qml
```

**macOS (in Contents/Resources/):**
```ini
[Paths]
Plugins = PlugIns
Qml2Imports = Resources/qml
```

**Linux (in usr/bin/):**
```ini
[Paths]
Prefix = ..
Plugins = plugins
Qml2Imports = qml
```

### Qt Path Discovery Function

Create a function to find Qt resource directories reliably:

```cmake
function(find_qt6_resource_dirs)
    # Strategy 1: Use QT6_INSTALL_PREFIX if available
    if(DEFINED QT6_INSTALL_PREFIX)
        set(_qt6_prefix "${QT6_INSTALL_PREFIX}")
    # Strategy 2: Derive from Qt6_DIR
    elseif(DEFINED Qt6_DIR)
        get_filename_component(_qt6_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    # Strategy 3: Derive from Qt6::Core location
    else()
        get_target_property(_loc Qt6::Core LOCATION)
        get_filename_component(_qt6_prefix "${_loc}/../.." ABSOLUTE)
    endif()

    # Platform-specific plugin paths
    if(WIN32)
        set(_plugins "${_qt6_prefix}/plugins")
    elseif(APPLE)
        # Try multiple locations
        foreach(_path "${_qt6_prefix}/share/qt6/plugins" "${_qt6_prefix}/plugins")
            if(EXISTS "${_path}")
                set(_plugins "${_path}")
                break()
            endif()
        endforeach()
    else()
        set(_plugins "${_qt6_prefix}/plugins")
    endif()

    set(QT6_PLUGINS_DIR "${_plugins}" PARENT_SCOPE)
endfunction()
```

### Deploy Plugins Not Handled by Qt

`qt_generate_deploy_app_script` may miss some plugins. Deploy manually:

```cmake
# Geoservices plugins for map functionality
if(EXISTS "${QT6_PLUGINS_DIR}/geoservices")
    install(DIRECTORY "${QT6_PLUGINS_DIR}/geoservices"
        DESTINATION "${PLUGIN_DEST}"
        FILES_MATCHING PATTERN "*.so" PATTERN "*.dylib" PATTERN "*.dll"
    )
endif()
```

---

## Python Bundling

### Version Consistency

Derive bundle version from build-time Python for pybind11 compatibility:

```cmake
# After find_package(Python)
set(FLYSIGHT_PYTHON_VERSION "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}"
    CACHE INTERNAL "Python major.minor version")

# In bundling modules, use the detected version
if(DEFINED FLYSIGHT_PYTHON_VERSION)
    set(BUNDLE_PYTHON_VERSION "${FLYSIGHT_PYTHON_VERSION}" CACHE STRING "...")
else()
    set(BUNDLE_PYTHON_VERSION "3.13" CACHE STRING "...")  # Fallback
endif()
```

### Download Retry Logic

Implement robust downloads for CI environments:

```cmake
function(_download_with_retry URL OUTPUT_PATH MAX_RETRIES)
    set(_retry_count 0)
    while(_retry_count LESS ${MAX_RETRIES})
        math(EXPR _retry_count "${_retry_count} + 1")

        file(DOWNLOAD "${URL}" "${OUTPUT_PATH}"
            STATUS _status
            TIMEOUT 300
        )

        list(GET _status 0 _error)
        if(_error EQUAL 0)
            return()
        endif()

        file(REMOVE "${OUTPUT_PATH}")  # Clean up partial download
    endwhile()

    message(FATAL_ERROR "Download failed after ${MAX_RETRIES} attempts")
endfunction()
```

### pybind11 Module Installation

Install to Python site-packages, not next to the executable:

```cmake
if(APPLE)
    install(TARGETS my_bridge
        LIBRARY DESTINATION "MyApp.app/Contents/Resources/python/lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages"
    )
elseif(UNIX)
    install(TARGETS my_bridge
        LIBRARY DESTINATION "${APPDIR}/usr/share/python/lib/python${BUNDLE_PYTHON_VERSION}/site-packages"
    )
elseif(WIN32)
    install(TARGETS my_bridge
        LIBRARY DESTINATION "python/Lib/site-packages"
        RUNTIME DESTINATION "python/Lib/site-packages"
    )
endif()
```

---

## GitHub Actions Workflow Patterns

### Verification Steps After Each Phase

Add explicit verification after dependency installation, configuration, and installation:

```yaml
- name: Verify build tools (Linux)
  if: runner.os == 'Linux'
  run: |
    echo "=== Verifying required build tools ==="
    command -v patchelf && patchelf --version || { echo "patchelf not found"; exit 1; }
    echo "=== All required tools available ==="
```

### Platform-Specific Dependency Installation

Use conditional steps with clear organization:

```yaml
- name: Install dependencies (Linux)
  if: runner.os == 'Linux'
  run: |
    sudo apt-get update
    sudo apt-get install -y \
      ninja-build \
      patchelf \
      libboost-all-dev

- name: Install dependencies (macOS)
  if: runner.os == 'macOS'
  run: |
    brew install ninja boost
```

### Export Environment Variables for CMake

Set paths in `$GITHUB_ENV` for consistent passing to CMake:

```yaml
- name: Install Boost (Windows)
  if: runner.os == 'Windows'
  run: |
    echo "BOOST_ROOT=C:\path\to\boost" >> $GITHUB_ENV
    echo "BOOST_LIBRARYDIR=C:\path\to\boost\lib" >> $GITHUB_ENV
```

### CMake Configuration Pattern

Pass all necessary variables explicitly:

```yaml
- name: Configure
  run: |
    cmake -G Ninja -B build -S src \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=build/install \
      -DTHIRD_PARTY_DIR="${GITHUB_WORKSPACE}/third-party" \
      -DGTSAM_ROOT="${GITHUB_WORKSPACE}/third-party/GTSAM-install" \
      -DTBB_ROOT="${GITHUB_WORKSPACE}/third-party/oneTBB-install" \
      -DTBB_DIR="${GITHUB_WORKSPACE}/third-party/oneTBB-install/lib/cmake/TBB" \
      ${BOOST_ROOT:+-DBOOST_ROOT="$BOOST_ROOT"}
```

---

## Verification and Diagnostics

### Install-Time Verification

Add verification code that runs during `cmake --install`:

```cmake
install(CODE "
    message(STATUS \"Verifying installation...\")

    set(EXPECTED_FILES
        \"\${CMAKE_INSTALL_PREFIX}/lib/libfoo.so\"
        \"\${CMAKE_INSTALL_PREFIX}/bin/myapp\"
    )

    foreach(f \${EXPECTED_FILES})
        if(EXISTS \"\${f}\")
            message(STATUS \"  Found: \${f}\")
        else()
            message(WARNING \"  MISSING: \${f}\")
        endif()
    endforeach()
")
```

### Standalone Diagnostic Scripts

Create diagnostic tools for debugging (e.g., `cmake/diagnose_macos_bundle.sh`):

```bash
#!/bin/bash
# Check library IDs
for dylib in "$BUNDLE/Contents/Frameworks"/*.dylib; do
    id=$(otool -D "$dylib" | tail -1)
    if [[ "$id" == "@rpath/"* ]]; then
        echo "OK: $(basename $dylib)"
    else
        echo "WARN: $(basename $dylib) has ID: $id"
    fi
done
```

### Debug Mode in Launchers

Add optional debug output to launcher scripts:

```bash
# In AppRun or launcher script
if [ -n "${MY_APP_DEBUG}" ]; then
    echo "=== Debug Mode ==="
    echo "LD_LIBRARY_PATH: ${LD_LIBRARY_PATH}"
    echo "QT_PLUGIN_PATH: ${QT_PLUGIN_PATH}"
    ls -la "${HERE}/usr/lib/" | head -20
    echo "================="
fi
```

---

## Platform-Specific Considerations

### Windows

- DLLs go in flat directory structure (same directory as executable or in PATH)
- No RPATH concept; library discovery uses PATH and same-directory
- Use `RUNTIME DESTINATION` for DLLs in install commands
- Boost library directory naming: `lib64-msvc-14.3`

### macOS

- Use app bundle structure: `MyApp.app/Contents/{MacOS,Frameworks,Resources,PlugIns}/`
- Library ID must be set with `install_name_tool -id`
- RPATH uses `@executable_path`, `@loader_path`, `@rpath`
- Remove code signatures before modifying binaries
- Homebrew paths differ by architecture: `/opt/homebrew` (ARM64) vs `/usr/local` (Intel)

### Linux

- AppDir structure: `MyApp.AppDir/usr/{bin,lib,plugins,share}/`
- RPATH uses `$ORIGIN` for relative paths
- Use `patchelf` to modify RPATH post-build
- Symlinks must be preserved for versioned libraries
- AppRun script sets environment variables
- libfuse2 required to run AppImages (or use `--appimage-extract-and-run`)

---

## Quick Reference: File Locations

| Purpose | Files |
|---------|-------|
| Library discovery | `cmake/BoostDiscovery.cmake`, `cmake/QtPathDiscovery.cmake` |
| Third-party deployment | `cmake/DeployThirdParty{Windows,MacOS,Linux}.cmake` |
| Python bundling | `cmake/BundlePython{Windows,MacOS,Linux}.cmake` |
| AppDir/AppImage | `cmake/CreateAppDir.cmake`, `cmake/CreateAppImage.cmake` |
| RPATH fixing | `cmake/fix_macos_rpaths.sh`, inline in DeployThirdParty*.cmake |
| Superbuild | `cmake/ThirdPartySuperbuild.cmake` |
| CI/CD | `.github/workflows/build.yml` |

---

## See Also

- [Implementation Plan Overview](implementation-plan/00-overview.md) - Full project documentation
- Individual phase documents in `docs/implementation-plan/` for detailed implementation notes
