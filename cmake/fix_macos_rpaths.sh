#!/bin/bash
# =============================================================================
# fix_macos_rpaths.sh
# =============================================================================
#
# This script fixes all dylib paths in a macOS app bundle to use @rpath,
# ensuring the application is fully relocatable.
#
# Usage:
#   ./fix_macos_rpaths.sh /path/to/MyApp.app
#
# The script:
#   1. Sets library IDs to @rpath/libname.dylib for all dylibs in Frameworks
#   2. Updates all dependent library references to use @rpath
#   3. Fixes the main executable
#   4. Fixes Python extension modules (.so files)
#   5. Fixes Qt plugins and QML modules
#   6. Fixes bare install names (no path prefix) in all dylibs bundle-wide
#   7. Skips system libraries (/usr/lib, /System/Library)
#
# This script is idempotent - it can be run multiple times safely.
#
# =============================================================================

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters for summary
DYLIB_COUNT=0
EXE_COUNT=0
SO_COUNT=0
PLUGIN_COUNT=0
FIXED_COUNT=0

# =============================================================================
# Helper Functions
# =============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_section() {
    echo ""
    echo -e "${BLUE}=== $1 ===${NC}"
}

# Check if a path is a system library that should be skipped
is_system_library() {
    local path="$1"
    # Python.framework under /Library/Frameworks is a third-party install
    # (e.g., python.org installer) that needs @rpath rewriting to the
    # bundled libpython dylib — do not treat it as a system library.
    if [[ "$path" == */Python.framework/* ]]; then
        return 1  # false - needs rewriting
    fi
    if [[ "$path" == /usr/lib/* ]] || \
       [[ "$path" == /System/Library/* ]] || \
       [[ "$path" == /Library/Frameworks/* ]] || \
       [[ "$path" == @* ]]; then
        return 0  # true - is system library
    fi
    return 1  # false - not system library
}

# Get the library name from a path
get_lib_name() {
    basename "$1"
}

# Resolve a Python framework reference to the actual bundled dylib name.
# e.g. ".../Python.framework/Versions/3.13/Python" -> "libpython3.13.dylib"
# Returns the resolved name on stdout, or empty string if not a Python framework.
resolve_python_framework() {
    local dep="$1"
    local frameworks_dir="$2"

    # Match: .../Python.framework/Versions/<ver>/Python
    if [[ "$dep" =~ Python\.framework/Versions/([0-9]+\.[0-9]+)/Python$ ]]; then
        local version="${BASH_REMATCH[1]}"
        local candidate="libpython${version}.dylib"
        if [[ -f "$frameworks_dir/$candidate" ]]; then
            echo "$candidate"
            return
        fi
        # Fallback: search for any libpython dylib matching this version
        local found
        found=$(find "$frameworks_dir" -maxdepth 1 -name "libpython${version}*.dylib" ! -type l 2>/dev/null | head -1)
        if [[ -n "$found" ]]; then
            basename "$found"
            return
        fi
    fi
    echo ""
}

# No-op: Do NOT remove signatures before install_name_tool.
# On arm64, codesign --remove-signature can corrupt __LINKEDIT, causing
# install_name_tool to fail. Apple recommends running install_name_tool
# directly on signed binaries (it warns about invalidating the signature
# but succeeds), then re-signing afterward. The final signing pass in
# src/CMakeLists.txt handles re-signing all binaries.
strip_signature() {
    :
}

# Run install_name_tool, logging real failures while filtering the expected
# "will invalidate the code signature" warning that occurs when modifying
# a signed binary (this is harmless — the final signing pass re-signs).
run_install_name_tool() {
    local output
    if ! output=$(install_name_tool "$@" 2>&1); then
        if [[ "$output" != *"will invalidate the code signature"* ]]; then
            log_warn "install_name_tool $1 failed: $output"
        fi
    fi
}

# Handle versioned libraries and their symlinks
# Some libraries use versioned names like libfoo.1.2.3.dylib
fix_versioned_dylibs() {
    local frameworks_dir="$1"

    log_info "Processing versioned dylibs..."

    # Find real files (not symlinks) with version numbers
    while IFS= read -r -d '' dylib; do
        local dylib_name
        dylib_name=$(basename "$dylib")

        # Check if this looks like a versioned library (e.g., libfoo.1.dylib or libfoo.1.2.3.dylib)
        if [[ "$dylib_name" =~ ^lib.*\.[0-9]+\.dylib$ ]] || \
           [[ "$dylib_name" =~ ^lib.*\.[0-9]+\.[0-9]+\.dylib$ ]] || \
           [[ "$dylib_name" =~ ^lib.*\.[0-9]+\.[0-9]+\.[0-9]+\.dylib$ ]]; then
            log_info "  Found versioned: $dylib_name"

            # Extract base name (e.g., libfoo from libfoo.12.dylib)
            local base_name
            base_name=$(echo "$dylib_name" | sed -E 's/\.[0-9]+(\.[0-9]+)*\.dylib$/.dylib/')

            # Create symlink if it doesn't exist
            if [[ ! -e "$frameworks_dir/$base_name" ]]; then
                ln -sf "$dylib_name" "$frameworks_dir/$base_name"
                log_info "    Created symlink: $base_name -> $dylib_name"
            fi
        fi
    done < <(find "$frameworks_dir" -maxdepth 1 -name "*.dylib" ! -type l -print0 2>/dev/null)
}

# Fix a single dylib's ID and dependencies
fix_dylib() {
    local dylib="$1"
    local frameworks_dir="$2"
    local dylib_name
    dylib_name=$(get_lib_name "$dylib")

    log_info "Processing: $dylib_name"
    ((DYLIB_COUNT++)) || true

    # Strip code signature before modifying
    strip_signature "$dylib"

    # Set the library's own ID to @rpath/libname.dylib
    run_install_name_tool -id "@rpath/$dylib_name" "$dylib"

    # Get list of dependencies
    local deps
    deps=$(otool -L "$dylib" | tail -n +2 | awk '{print $1}')

    # Fix each dependency
    while IFS= read -r dep; do
        # Skip empty lines and system libraries
        [[ -z "$dep" ]] && continue
        is_system_library "$dep" && continue

        local dep_name
        dep_name=$(get_lib_name "$dep")

        # Check if this dependency exists in our Frameworks
        if [[ -f "$frameworks_dir/$dep_name" ]]; then
            run_install_name_tool -change "$dep" "@rpath/$dep_name" "$dylib"
            log_info "  Fixed: $dep -> @rpath/$dep_name"
            ((FIXED_COUNT++)) || true
        else
            # Handle Python framework -> bundled dylib mapping
            local resolved
            resolved=$(resolve_python_framework "$dep" "$frameworks_dir")
            if [[ -n "$resolved" ]]; then
                run_install_name_tool -change "$dep" "@rpath/$resolved" "$dylib"
                log_info "  Fixed (framework): $dep -> @rpath/$resolved"
                ((FIXED_COUNT++)) || true
            fi
        fi
    done <<< "$deps"
}

# Fix the main executable
fix_executable() {
    local exe="$1"
    local frameworks_dir="$2"

    log_info "Processing executable: $(basename "$exe")"
    ((EXE_COUNT++)) || true

    # Strip code signature before modifying
    strip_signature "$exe"

    # Check if @executable_path/../Frameworks rpath already exists
    local rpaths
    rpaths=$(otool -l "$exe" | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)

    if ! echo "$rpaths" | grep -q "@executable_path/../Frameworks"; then
        run_install_name_tool -add_rpath "@executable_path/../Frameworks" "$exe"
        log_info "  Added rpath: @executable_path/../Frameworks"
    fi

    # Get list of dependencies
    local deps
    deps=$(otool -L "$exe" | tail -n +2 | awk '{print $1}')

    # Fix each dependency
    while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue
        is_system_library "$dep" && continue

        local dep_name
        dep_name=$(get_lib_name "$dep")

        # Check if this dependency exists in our Frameworks
        if [[ -f "$frameworks_dir/$dep_name" ]]; then
            run_install_name_tool -change "$dep" "@rpath/$dep_name" "$exe"
            log_info "  Fixed: $dep -> @rpath/$dep_name"
            ((FIXED_COUNT++)) || true
        else
            # Handle Python framework -> bundled dylib mapping
            local resolved
            resolved=$(resolve_python_framework "$dep" "$frameworks_dir")
            if [[ -n "$resolved" ]]; then
                run_install_name_tool -change "$dep" "@rpath/$resolved" "$exe"
                log_info "  Fixed (framework): $dep -> @rpath/$resolved"
                ((FIXED_COUNT++)) || true
            fi
        fi
    done <<< "$deps"
}

# Fix Python extension modules (.so files)
fix_python_modules() {
    local bundle_dir="$1"
    local frameworks_dir="$2"

    # Find all .so files in the bundle (pybind11 modules)
    while IFS= read -r -d '' so_file; do
        log_info "Processing Python module: $(basename "$so_file")"
        ((SO_COUNT++)) || true

        # Strip code signature before modifying
        strip_signature "$so_file"

        # Add rpath for finding libraries
        local rpaths
        rpaths=$(otool -l "$so_file" | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)

        if ! echo "$rpaths" | grep -q "@loader_path"; then
            # Calculate relative path from .so to Frameworks
            local so_dir
            so_dir=$(dirname "$so_file")
            local rel_path
            rel_path=$(python3 -c "import os.path; print(os.path.relpath('$frameworks_dir', '$so_dir'))" 2>/dev/null || echo "../Frameworks")
            run_install_name_tool -add_rpath "@loader_path/$rel_path" "$so_file"
            log_info "  Added rpath: @loader_path/$rel_path"
        fi

        # Fix dependencies
        local deps
        deps=$(otool -L "$so_file" | tail -n +2 | awk '{print $1}')

        while IFS= read -r dep; do
            [[ -z "$dep" ]] && continue
            is_system_library "$dep" && continue

            local dep_name
            dep_name=$(get_lib_name "$dep")

            if [[ -f "$frameworks_dir/$dep_name" ]]; then
                run_install_name_tool -change "$dep" "@rpath/$dep_name" "$so_file"
                log_info "  Fixed: $dep -> @rpath/$dep_name"
                ((FIXED_COUNT++)) || true
            else
                # Handle Python framework -> bundled dylib mapping
                local resolved
                resolved=$(resolve_python_framework "$dep" "$frameworks_dir")
                if [[ -n "$resolved" ]]; then
                    run_install_name_tool -change "$dep" "@rpath/$resolved" "$so_file"
                    log_info "  Fixed (framework): $dep -> @rpath/$resolved"
                    ((FIXED_COUNT++)) || true
                fi
            fi
        done <<< "$deps"
    done < <(find "$bundle_dir" -name "*.so" -print0 2>/dev/null)
}

# Fix Qt plugins
fix_qt_plugins() {
    local bundle_dir="$1"
    local frameworks_dir="$2"
    local plugins_dir="$bundle_dir/Contents/PlugIns"
    local qml_dir="$bundle_dir/Contents/Resources/qml"

    # Process PlugIns directory
    if [[ -d "$plugins_dir" ]]; then
        log_info "Processing Qt plugins..."

        while IFS= read -r -d '' plugin; do
            # Skip symlinks
            [[ -L "$plugin" ]] && continue

            local plugin_name
            plugin_name=$(basename "$plugin")
            log_info "  Plugin: $plugin_name"
            ((PLUGIN_COUNT++)) || true

            # Strip code signature
            strip_signature "$plugin"

            # Calculate relative path to Frameworks
            local plugin_dir
            plugin_dir=$(dirname "$plugin")
            local rel_path
            rel_path=$(python3 -c "import os.path; print(os.path.relpath('$frameworks_dir', '$plugin_dir'))" 2>/dev/null || echo "../../Frameworks")

            # Add rpath if not present
            local rpaths
            rpaths=$(otool -l "$plugin" | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)

            if ! echo "$rpaths" | grep -q "@loader_path/$rel_path"; then
                run_install_name_tool -add_rpath "@loader_path/$rel_path" "$plugin"
            fi

            # Fix dependencies
            local deps
            deps=$(otool -L "$plugin" | tail -n +2 | awk '{print $1}')

            while IFS= read -r dep; do
                [[ -z "$dep" ]] && continue
                is_system_library "$dep" && continue

                local dep_name
                dep_name=$(get_lib_name "$dep")

                if [[ -f "$frameworks_dir/$dep_name" ]]; then
                    run_install_name_tool -change "$dep" "@rpath/$dep_name" "$plugin"
                    ((FIXED_COUNT++)) || true
                else
                    # Handle Python framework -> bundled dylib mapping
                    local resolved
                    resolved=$(resolve_python_framework "$dep" "$frameworks_dir")
                    if [[ -n "$resolved" ]]; then
                        run_install_name_tool -change "$dep" "@rpath/$resolved" "$plugin"
                        log_info "  Fixed (framework): $dep -> @rpath/$resolved"
                        ((FIXED_COUNT++)) || true
                    fi
                fi
            done <<< "$deps"
        done < <(find "$plugins_dir" -name "*.dylib" -print0 2>/dev/null)
    fi

    # Process QML directory
    if [[ -d "$qml_dir" ]]; then
        log_info "Processing QML modules..."

        while IFS= read -r -d '' qml_lib; do
            # Skip symlinks
            [[ -L "$qml_lib" ]] && continue

            local lib_name
            lib_name=$(basename "$qml_lib")
            log_info "  QML lib: $lib_name"
            ((PLUGIN_COUNT++)) || true

            # Strip code signature
            strip_signature "$qml_lib"

            # Calculate relative path to Frameworks
            local lib_dir
            lib_dir=$(dirname "$qml_lib")
            local rel_path
            rel_path=$(python3 -c "import os.path; print(os.path.relpath('$frameworks_dir', '$lib_dir'))" 2>/dev/null || echo "../../../Frameworks")

            # Add rpath if not present
            local rpaths
            rpaths=$(otool -l "$qml_lib" | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)

            if ! echo "$rpaths" | grep -q "@loader_path"; then
                run_install_name_tool -add_rpath "@loader_path/$rel_path" "$qml_lib"
            fi

            # Fix dependencies
            local deps
            deps=$(otool -L "$qml_lib" | tail -n +2 | awk '{print $1}')

            while IFS= read -r dep; do
                [[ -z "$dep" ]] && continue
                is_system_library "$dep" && continue

                local dep_name
                dep_name=$(get_lib_name "$dep")

                if [[ -f "$frameworks_dir/$dep_name" ]]; then
                    run_install_name_tool -change "$dep" "@rpath/$dep_name" "$qml_lib"
                    ((FIXED_COUNT++)) || true
                else
                    # Handle Python framework -> bundled dylib mapping
                    local resolved
                    resolved=$(resolve_python_framework "$dep" "$frameworks_dir")
                    if [[ -n "$resolved" ]]; then
                        run_install_name_tool -change "$dep" "@rpath/$resolved" "$qml_lib"
                        log_info "  Fixed (framework): $dep -> @rpath/$resolved"
                        ((FIXED_COUNT++)) || true
                    fi
                fi
            done <<< "$deps"
        done < <(find "$qml_dir" -name "*.dylib" -print0 2>/dev/null)
    fi
}

# Print summary
print_summary() {
    local bundle_path="$1"
    local issues="$2"

    log_section "RPATH Fix Summary"
    echo "Bundle: $bundle_path"
    echo ""
    echo "Items processed:"
    echo "  Dylibs in Frameworks: $DYLIB_COUNT"
    echo "  Executables:          $EXE_COUNT"
    echo "  Python modules (.so): $SO_COUNT"
    echo "  Qt plugins/QML:       $PLUGIN_COUNT"
    echo ""
    echo "Dependencies fixed:     $FIXED_COUNT"
    echo "Issues found:           $issues"
    echo ""

    if [[ $issues -eq 0 ]]; then
        log_info "Bundle verification passed - no issues found"
    else
        log_warn "Found $issues potential path issues (see warnings above)"
    fi
}

# =============================================================================
# Main Script
# =============================================================================

main() {
    # Check arguments
    if [[ $# -ne 1 ]]; then
        echo "Usage: $0 /path/to/MyApp.app"
        exit 1
    fi

    local bundle_path="$1"

    # Validate bundle path
    if [[ ! -d "$bundle_path" ]]; then
        log_error "Bundle not found: $bundle_path"
        exit 1
    fi

    if [[ ! -d "$bundle_path/Contents" ]]; then
        log_error "Invalid app bundle (no Contents directory): $bundle_path"
        exit 1
    fi

    local frameworks_dir="$bundle_path/Contents/Frameworks"
    local macos_dir="$bundle_path/Contents/MacOS"

    log_section "Starting RPATH Fixes"
    log_info "Bundle: $bundle_path"
    log_info "Frameworks: $frameworks_dir"

    # Create Frameworks directory if it doesn't exist
    mkdir -p "$frameworks_dir"

    # =======================================================================
    # Step 1: Handle versioned dylibs and create symlinks
    # =======================================================================

    if [[ -d "$frameworks_dir" ]]; then
        fix_versioned_dylibs "$frameworks_dir"
    fi

    # =======================================================================
    # Step 2: Fix all dylibs in Frameworks
    # =======================================================================

    log_section "Processing Frameworks"

    if [[ -d "$frameworks_dir" ]]; then
        # Find all dylibs (not symlinks)
        while IFS= read -r -d '' dylib; do
            # Skip symlinks
            [[ -L "$dylib" ]] && continue
            fix_dylib "$dylib" "$frameworks_dir"
        done < <(find "$frameworks_dir" -maxdepth 1 -name "*.dylib" -print0 2>/dev/null)
    fi

    # =======================================================================
    # Step 3: Fix the main executable(s)
    # =======================================================================

    log_section "Processing Executables"

    if [[ -d "$macos_dir" ]]; then
        while IFS= read -r -d '' exe; do
            # Skip if not a Mach-O executable
            if file "$exe" | grep -q "Mach-O"; then
                fix_executable "$exe" "$frameworks_dir"
            fi
        done < <(find "$macos_dir" -type f -print0 2>/dev/null)
    fi

    # =======================================================================
    # Step 4: Fix Python extension modules
    # =======================================================================

    log_section "Processing Python Modules"
    fix_python_modules "$bundle_path" "$frameworks_dir"

    # =======================================================================
    # Step 5: Fix Qt plugins and QML modules
    # =======================================================================

    log_section "Processing Qt Plugins and QML"
    fix_qt_plugins "$bundle_path" "$frameworks_dir"

    # =======================================================================
    # Step 6: Fix bare install names in all dylibs
    # =======================================================================
    # Some bundled libraries (e.g., Tcl extensions in Python) have bare
    # install names (just the filename, no @rpath/@loader_path prefix).
    # When the app uses the disable-library-validation entitlement,
    # Gatekeeper performs enhanced Mach-O scanning and rejects apps
    # containing dylibs with bare install names.

    log_section "Fixing Bare Install Names"

    while IFS= read -r -d '' dylib; do
        # Skip symlinks
        [[ -L "$dylib" ]] && continue

        # Get the library's install name (LC_ID_DYLIB)
        local install_name
        install_name=$(otool -D "$dylib" 2>/dev/null | tail -1)

        # Check if the install name is bare (no path separator)
        if [[ -n "$install_name" ]] && [[ "$install_name" != */* ]]; then
            log_info "  Fixing bare ID: $install_name in $(basename "$dylib")"
            run_install_name_tool -id "@loader_path/$install_name" "$dylib"
            ((FIXED_COUNT++)) || true
        fi
    done < <(find "$bundle_path" -name "*.dylib" -print0 2>/dev/null)

    # =======================================================================
    # Step 7: Verify the bundle
    # =======================================================================
    # Note: Ad-hoc signing is handled by the CMake install pipeline
    # (final signing pass in src/CMakeLists.txt) — not by this script.

    log_section "Verifying Bundle"

    # Check for any remaining absolute paths to third-party libraries
    # and bare install names (no path prefix)
    local issues=0

    while IFS= read -r -d '' file; do
        local deps
        deps=$(otool -L "$file" 2>/dev/null | tail -n +2 | awk '{print $1}' || true)

        while IFS= read -r dep; do
            [[ -z "$dep" ]] && continue

            # Check for non-system absolute paths
            if [[ "$dep" == /* ]] && ! is_system_library "$dep"; then
                log_warn "Potential issue in $(basename "$file"): $dep"
                ((issues++)) || true
            fi

            # Check for bare install names (no path separator)
            if [[ "$dep" != */* ]] && [[ "$dep" == *.dylib ]]; then
                log_warn "Bare install name in $(basename "$file"): $dep"
                ((issues++)) || true
            fi
        done <<< "$deps"
    done < <(find "$bundle_path" \( -name "*.dylib" -o -name "*.so" \) -print0 2>/dev/null)

    if [[ -d "$macos_dir" ]]; then
        while IFS= read -r -d '' exe; do
            if file "$exe" | grep -q "Mach-O"; then
                local deps
                deps=$(otool -L "$exe" 2>/dev/null | tail -n +2 | awk '{print $1}' || true)

                while IFS= read -r dep; do
                    [[ -z "$dep" ]] && continue
                    if [[ "$dep" == /* ]] && ! is_system_library "$dep"; then
                        log_warn "Potential issue in $(basename "$exe"): $dep"
                        ((issues++)) || true
                    fi
                done <<< "$deps"
            fi
        done < <(find "$macos_dir" -type f -print0 2>/dev/null)
    fi

    # =======================================================================
    # Print summary
    # =======================================================================

    print_summary "$bundle_path" "$issues"

    log_info "Done!"
}

# Run main function
main "$@"
