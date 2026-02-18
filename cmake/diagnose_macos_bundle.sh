#!/bin/bash
# =============================================================================
# diagnose_macos_bundle.sh
# =============================================================================
#
# Diagnoses library loading issues in a macOS app bundle.
# Provides detailed analysis of rpaths, library IDs, and dependencies.
#
# Usage:
#   ./diagnose_macos_bundle.sh /path/to/MyApp.app
#
# This script is useful for:
# - Debugging CI failures related to library loading
# - Diagnosing user-reported issues
# - Verifying bundle correctness before distribution
#
# The script checks:
# - Bundle structure and contents
# - Executable rpaths and dependencies
# - Library IDs in Frameworks
# - Python extension modules
# - Qt plugins and QML modules
# - Absolute paths that may cause issues
#
# =============================================================================

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Counters
ISSUES=0
WARNINGS=0

log_section() {
    echo ""
    echo -e "${BLUE}=================================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=================================================================${NC}"
}

log_subsection() {
    echo ""
    echo -e "${CYAN}--- $1 ---${NC}"
}

log_ok() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    ((WARNINGS++)) || true
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    ((ISSUES++)) || true
}

log_info() {
    echo -e "      $1"
}

# Check if a path is a system library
is_system_library() {
    local path="$1"
    if [[ "$path" == /usr/lib/* ]] || \
       [[ "$path" == /System/Library/* ]] || \
       [[ "$path" == /Library/Frameworks/* ]] || \
       [[ "$path" == @* ]]; then
        return 0
    fi
    return 1
}

# Analyze a single binary's dependencies
analyze_binary() {
    local binary="$1"
    local binary_name="$2"
    local frameworks_dir="$3"
    local expected_rpath="$4"

    echo "Binary: $binary_name"

    # Check if file exists and is a Mach-O binary
    if [ ! -f "$binary" ]; then
        log_error "File not found: $binary"
        return
    fi

    if ! file "$binary" | grep -q "Mach-O"; then
        log_info "Not a Mach-O binary, skipping"
        return
    fi

    # Check rpaths
    local rpaths
    rpaths=$(otool -l "$binary" 2>/dev/null | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}')

    if [ -n "$rpaths" ]; then
        log_info "RPaths:"
        while IFS= read -r rpath; do
            if [ -n "$expected_rpath" ] && [[ "$rpath" == *"$expected_rpath"* ]]; then
                log_ok "  $rpath"
            else
                log_info "  $rpath"
            fi
        done <<< "$rpaths"
    else
        log_warn "No LC_RPATH entries found"
    fi

    # Check dependencies
    local deps
    deps=$(otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}')

    local has_issues=false
    while IFS= read -r dep; do
        [ -z "$dep" ] && continue

        # Check for absolute paths that aren't system libraries
        if [[ "$dep" == /* ]] && ! is_system_library "$dep"; then
            log_error "Absolute path: $dep"
            has_issues=true
        fi
    done <<< "$deps"

    if [ "$has_issues" = false ]; then
        log_ok "All dependencies use @rpath or system paths"
    fi
}

# Analyze library ID
analyze_library_id() {
    local dylib="$1"
    local expected_id="$2"

    local name
    name=$(basename "$dylib")

    local id
    id=$(otool -D "$dylib" 2>/dev/null | tail -1)

    if [ "$id" = "$expected_id" ]; then
        log_ok "$name -> $id"
    elif [[ "$id" == "@rpath/"* ]]; then
        log_warn "$name -> $id (expected $expected_id)"
    else
        log_error "$name -> $id (should be @rpath/$name)"
    fi
}

# Main diagnostic function
diagnose_bundle() {
    local bundle="$1"

    # =======================================================================
    # Bundle Information
    # =======================================================================
    log_section "Bundle Information"
    echo "Path: $bundle"
    echo "Size: $(du -sh "$bundle" 2>/dev/null | cut -f1)"

    if [ -f "$bundle/Contents/Info.plist" ]; then
        local version
        version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$bundle/Contents/Info.plist" 2>/dev/null || echo "unknown")
        local bundle_id
        bundle_id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || echo "unknown")
        echo "Version: $version"
        echo "Bundle ID: $bundle_id"
    fi

    # =======================================================================
    # Contents Structure
    # =======================================================================
    log_section "Contents Structure"
    find "$bundle/Contents" -maxdepth 2 -type d 2>/dev/null | sed "s|$bundle/||" | sort

    # =======================================================================
    # Executable Analysis
    # =======================================================================
    log_section "Executable Analysis"

    local macos_dir="$bundle/Contents/MacOS"
    local frameworks_dir="$bundle/Contents/Frameworks"

    if [ -d "$macos_dir" ]; then
        while IFS= read -r -d '' exe; do
            analyze_binary "$exe" "$(basename "$exe")" "$frameworks_dir" "@executable_path/../Frameworks"
        done < <(find "$macos_dir" -type f -print0 2>/dev/null)
    else
        log_error "MacOS directory not found"
    fi

    # =======================================================================
    # Frameworks Analysis
    # =======================================================================
    log_section "Frameworks Analysis"

    if [ -d "$frameworks_dir" ]; then
        log_subsection "Library IDs"

        local dylib_count=0
        local id_issues=0

        while IFS= read -r -d '' dylib; do
            # Skip symlinks
            [ -L "$dylib" ] && continue

            ((dylib_count++)) || true
            local name
            name=$(basename "$dylib")
            analyze_library_id "$dylib" "@rpath/$name"
        done < <(find "$frameworks_dir" -maxdepth 1 -name "*.dylib" -print0 2>/dev/null)

        echo ""
        echo "Total dylibs in Frameworks: $dylib_count"

        log_subsection "Critical Libraries Check"

        local critical_libs=("libgtsam" "libmetis-gtsam" "libcephes-gtsam" "libGeographic" "libtbb" "libpython")
        for lib in "${critical_libs[@]}"; do
            local found
            found=$(find "$frameworks_dir" -name "${lib}*.dylib" ! -type l 2>/dev/null | head -1)
            if [ -n "$found" ]; then
                log_ok "$lib: $(basename "$found")"
            else
                log_warn "$lib: Not found"
            fi
        done

        log_subsection "Framework Bundles"

        # Check for framework bundles (like KDDockWidgets)
        while IFS= read -r -d '' framework; do
            local framework_name
            framework_name=$(basename "$framework" .framework)
            echo "Framework: $framework_name"

            # Find binary inside framework
            local framework_binary
            framework_binary=$(find "$framework" -name "$framework_name" -type f 2>/dev/null | head -1)
            if [ -n "$framework_binary" ]; then
                local framework_id
                framework_id=$(otool -D "$framework_binary" 2>/dev/null | tail -1)
                log_info "ID: $framework_id"
            fi
        done < <(find "$frameworks_dir" -maxdepth 1 -name "*.framework" -print0 2>/dev/null)

    else
        log_error "Frameworks directory not found"
    fi

    # =======================================================================
    # Python Module Analysis
    # =======================================================================
    log_section "Python Module Analysis"

    local so_files
    so_files=$(find "$bundle" -name "*.so" -type f 2>/dev/null)

    if [ -n "$so_files" ]; then
        while IFS= read -r so_file; do
            local so_name
            so_name=$(basename "$so_file")
            local so_rel_path
            so_rel_path=$(echo "$so_file" | sed "s|$bundle/||")

            echo ""
            echo "Module: $so_name"
            log_info "Location: $so_rel_path"

            # Check rpaths
            local so_rpaths
            so_rpaths=$(otool -l "$so_file" 2>/dev/null | grep -A2 "LC_RPATH" | grep "path" | awk '{print $2}')
            if [ -n "$so_rpaths" ]; then
                log_info "RPaths:"
                while IFS= read -r rpath; do
                    log_info "  $rpath"
                done <<< "$so_rpaths"
            else
                log_warn "No LC_RPATH entries"
            fi

            # Check for absolute paths in dependencies
            local bad_deps
            bad_deps=$(otool -L "$so_file" 2>/dev/null | tail -n +2 | awk '{print $1}' | while read dep; do
                if [[ "$dep" == /* ]] && ! is_system_library "$dep"; then
                    echo "$dep"
                fi
            done)

            if [ -n "$bad_deps" ]; then
                log_error "Has absolute paths:"
                echo "$bad_deps" | sed 's/^/      /'
            else
                log_ok "No absolute paths in dependencies"
            fi
        done <<< "$so_files"
    else
        log_info "No Python extension modules found"
    fi

    # =======================================================================
    # Qt Plugin Analysis
    # =======================================================================
    log_section "Qt Plugin Analysis"

    local plugins_dir="$bundle/Contents/PlugIns"
    local qml_dir="$bundle/Contents/Resources/qml"

    log_subsection "PlugIns Directory"

    if [ -d "$plugins_dir" ]; then
        # List plugin categories
        local categories
        categories=$(find "$plugins_dir" -maxdepth 1 -type d | tail -n +2)

        while IFS= read -r category; do
            [ -z "$category" ] && continue
            local cat_name
            cat_name=$(basename "$category")
            local plugin_count
            plugin_count=$(find "$category" -name "*.dylib" -type f 2>/dev/null | wc -l | tr -d ' ')
            echo "$cat_name: $plugin_count plugins"

            # Check for issues in platform plugins (important for display)
            if [ "$cat_name" = "platforms" ]; then
                while IFS= read -r -d '' plugin; do
                    local plugin_name
                    plugin_name=$(basename "$plugin")
                    local bad_deps
                    bad_deps=$(otool -L "$plugin" 2>/dev/null | tail -n +2 | awk '{print $1}' | while read dep; do
                        if [[ "$dep" == /* ]] && ! is_system_library "$dep"; then
                            echo "$dep"
                        fi
                    done)
                    if [ -n "$bad_deps" ]; then
                        log_warn "$plugin_name has absolute paths"
                    else
                        log_ok "$plugin_name"
                    fi
                done < <(find "$category" -name "*.dylib" -type f -print0 2>/dev/null)
            fi
        done <<< "$categories"
    else
        log_warn "PlugIns directory not found"
    fi

    log_subsection "QML Modules"

    if [ -d "$qml_dir" ]; then
        local required_qml=("QtMultimedia")
        for mod in "${required_qml[@]}"; do
            if [ -d "$qml_dir/$mod" ]; then
                log_ok "$mod: Found"
            else
                log_warn "$mod: Not found"
            fi
        done
    else
        log_warn "QML directory not found"
    fi

    log_subsection "qt.conf"

    local qtconf="$bundle/Contents/Resources/qt.conf"
    if [ -f "$qtconf" ]; then
        log_ok "qt.conf found"
        log_info "Contents:"
        cat "$qtconf" | sed 's/^/      /'
    else
        log_warn "qt.conf not found"
    fi

    # =======================================================================
    # Summary
    # =======================================================================
    log_section "Diagnostic Summary"

    echo ""
    if [ $ISSUES -eq 0 ] && [ $WARNINGS -eq 0 ]; then
        log_ok "No issues found - bundle appears correctly configured"
    else
        echo "Results:"
        echo "  Errors:   $ISSUES"
        echo "  Warnings: $WARNINGS"
        echo ""

        if [ $ISSUES -gt 0 ]; then
            log_error "Bundle has critical issues that may prevent it from running"
        else
            log_warn "Bundle has warnings that should be reviewed"
        fi
    fi

    echo ""

    # =======================================================================
    # Optional: Try to run with library load tracing
    # =======================================================================
    log_section "Runtime Load Test (Optional)"

    echo "To test library loading at runtime, run:"
    echo "  DYLD_PRINT_LIBRARIES=1 \"$bundle/Contents/MacOS/\"* --help 2>&1 | head -50"
    echo ""
    echo "Or for more verbose output:"
    echo "  DYLD_PRINT_LIBRARIES=1 DYLD_PRINT_RPATHS=1 \"$bundle/Contents/MacOS/\"* --help 2>&1"

    return $ISSUES
}

# Entry point
if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/App.app"
    echo ""
    echo "Diagnoses library loading issues in a macOS app bundle."
    echo ""
    echo "Examples:"
    echo "  $0 build/install/FlySightViewer.app"
    echo "  $0 /Applications/FlySightViewer.app"
    exit 1
fi

bundle_path="$1"

if [ ! -d "$bundle_path" ]; then
    echo "Error: '$bundle_path' is not a directory"
    exit 1
fi

if [ ! -d "$bundle_path/Contents" ]; then
    echo "Error: '$bundle_path' is not a valid app bundle (no Contents directory)"
    exit 1
fi

diagnose_bundle "$bundle_path"
exit $?
