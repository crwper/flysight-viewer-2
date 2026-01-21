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
#   5. Skips system libraries (/usr/lib, /System/Library)
#
# This script is idempotent - it can be run multiple times safely.
#
# =============================================================================

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

# Check if a path is a system library that should be skipped
is_system_library() {
    local path="$1"
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

# Fix a single dylib's ID and dependencies
fix_dylib() {
    local dylib="$1"
    local frameworks_dir="$2"
    local dylib_name
    dylib_name=$(get_lib_name "$dylib")

    log_info "Processing: $dylib_name"

    # Set the library's own ID to @rpath/libname.dylib
    install_name_tool -id "@rpath/$dylib_name" "$dylib" 2>/dev/null || true

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
            install_name_tool -change "$dep" "@rpath/$dep_name" "$dylib" 2>/dev/null || true
            log_info "  Fixed: $dep -> @rpath/$dep_name"
        fi
    done <<< "$deps"
}

# Fix the main executable
fix_executable() {
    local exe="$1"
    local frameworks_dir="$2"

    log_info "Processing executable: $(basename "$exe")"

    # Check if @executable_path/../Frameworks rpath already exists
    local rpaths
    rpaths=$(otool -l "$exe" | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)

    if ! echo "$rpaths" | grep -q "@executable_path/../Frameworks"; then
        install_name_tool -add_rpath "@executable_path/../Frameworks" "$exe" 2>/dev/null || true
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
            install_name_tool -change "$dep" "@rpath/$dep_name" "$exe" 2>/dev/null || true
            log_info "  Fixed: $dep -> @rpath/$dep_name"
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

        # Add rpath for finding libraries
        local rpaths
        rpaths=$(otool -l "$so_file" | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}' || true)

        if ! echo "$rpaths" | grep -q "@loader_path"; then
            # Calculate relative path from .so to Frameworks
            local so_dir
            so_dir=$(dirname "$so_file")
            local rel_path
            rel_path=$(python3 -c "import os.path; print(os.path.relpath('$frameworks_dir', '$so_dir'))" 2>/dev/null || echo "../Frameworks")
            install_name_tool -add_rpath "@loader_path/$rel_path" "$so_file" 2>/dev/null || true
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
                install_name_tool -change "$dep" "@rpath/$dep_name" "$so_file" 2>/dev/null || true
                log_info "  Fixed: $dep -> @rpath/$dep_name"
            fi
        done <<< "$deps"
    done < <(find "$bundle_dir" -name "*.so" -print0 2>/dev/null)
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

    log_info "Fixing rpaths in: $bundle_path"
    log_info "Frameworks directory: $frameworks_dir"

    # Create Frameworks directory if it doesn't exist
    mkdir -p "$frameworks_dir"

    # =======================================================================
    # Step 1: Fix all dylibs in Frameworks
    # =======================================================================

    if [[ -d "$frameworks_dir" ]]; then
        log_info "Processing dylibs in Frameworks..."

        # Find all dylibs (not symlinks)
        while IFS= read -r -d '' dylib; do
            # Skip symlinks
            [[ -L "$dylib" ]] && continue
            fix_dylib "$dylib" "$frameworks_dir"
        done < <(find "$frameworks_dir" -name "*.dylib" -print0 2>/dev/null)
    fi

    # =======================================================================
    # Step 2: Fix the main executable(s)
    # =======================================================================

    if [[ -d "$macos_dir" ]]; then
        log_info "Processing executables in MacOS..."

        while IFS= read -r -d '' exe; do
            # Skip if not a Mach-O executable
            if file "$exe" | grep -q "Mach-O"; then
                fix_executable "$exe" "$frameworks_dir"
            fi
        done < <(find "$macos_dir" -type f -print0 2>/dev/null)
    fi

    # =======================================================================
    # Step 3: Fix Python extension modules
    # =======================================================================

    log_info "Processing Python extension modules..."
    fix_python_modules "$bundle_path" "$frameworks_dir"

    # =======================================================================
    # Step 4: Verify the bundle
    # =======================================================================

    log_info "Verifying bundle..."

    # Check for any remaining absolute paths to third-party libraries
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

    if [[ $issues -eq 0 ]]; then
        log_info "Bundle verification passed - no issues found"
    else
        log_warn "Found $issues potential path issues (see warnings above)"
    fi

    log_info "Done!"
}

# Run main function
main "$@"
