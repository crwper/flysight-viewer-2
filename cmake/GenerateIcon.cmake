# =============================================================================
# GenerateIcon.cmake
# =============================================================================
#
# Generates a placeholder PNG icon for the application if one doesn't exist.
# This ensures the AppImage always has an icon for desktop integration.
#
# The generated icon is a simple colored square with "FS" text to represent
# FlySight. In production, this should be replaced with a proper designed icon.
#
# =============================================================================

# =============================================================================
# Configuration
# =============================================================================

set(ICON_SIZE 256 CACHE STRING "Icon size in pixels")
set(ICON_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources" CACHE PATH "Icon output directory")
set(ICON_FILENAME "FlySightViewer.png" CACHE STRING "Icon filename")
set(ICON_PATH "${ICON_OUTPUT_DIR}/${ICON_FILENAME}")

# =============================================================================
# Check if icon already exists
# =============================================================================

if(EXISTS "${ICON_PATH}")
    message(STATUS "GenerateIcon: Icon already exists at ${ICON_PATH}")
    return()
endif()

message(STATUS "GenerateIcon: Creating placeholder icon at ${ICON_PATH}")

# Create output directory if it doesn't exist
file(MAKE_DIRECTORY "${ICON_OUTPUT_DIR}")

# =============================================================================
# Generate icon using Python (if available)
# =============================================================================

find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_FOUND)
    # Python script to generate a simple PNG icon
    set(ICON_GENERATOR_SCRIPT "${CMAKE_BINARY_DIR}/generate_icon.py")

    file(WRITE "${ICON_GENERATOR_SCRIPT}" [=[
#!/usr/bin/env python3
"""
Generate a placeholder icon for FlySight Viewer.
Creates a simple PNG with the application initials.
"""
import sys
import struct
import zlib

def create_png(width, height, filename):
    """Create a simple PNG file with a colored background and text."""

    # Generate pixel data: blue gradient background
    raw_data = []
    for y in range(height):
        row = []
        for x in range(width):
            # Create a gradient blue background
            r = 30
            g = 60 + int(40 * y / height)
            b = 120 + int(80 * (1 - y / height))

            # Add "FS" letters pattern (simplified)
            cx = x - width // 2
            cy = y - height // 2

            # Simple F shape
            in_f = False
            if -60 < cx < -20:
                if -60 < cy < -40:  # Top bar
                    in_f = True
                elif -40 < cy < -20:  # Middle bar
                    if cx < -40:
                        in_f = True
                elif -60 < cx < -40:  # Vertical
                    if -60 < cy < 60:
                        in_f = True

            # Simple S shape (approximation)
            in_s = False
            if 20 < cx < 60:
                if -60 < cy < -40:  # Top
                    in_s = True
                elif -20 < cy < 0:  # Middle
                    in_s = True
                elif 40 < cy < 60:  # Bottom
                    in_s = True
                elif 20 < cx < 40:  # Left parts
                    if -40 < cy < -20:
                        in_s = True
                elif 40 < cx < 60:  # Right parts
                    if 0 < cy < 40:
                        in_s = True

            if in_f or in_s:
                r, g, b = 255, 255, 255  # White text

            row.extend([r, g, b])
        raw_data.append(bytes(row))

    # Create PNG file
    def png_chunk(chunk_type, data):
        """Create a PNG chunk."""
        chunk = chunk_type + data
        crc = zlib.crc32(chunk) & 0xffffffff
        return struct.pack('>I', len(data)) + chunk + struct.pack('>I', crc)

    # PNG signature
    signature = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    ihdr = png_chunk(b'IHDR', ihdr_data)

    # IDAT chunk (compressed image data)
    raw_with_filter = b''.join(b'\x00' + row for row in raw_data)
    compressed = zlib.compress(raw_with_filter, 9)
    idat = png_chunk(b'IDAT', compressed)

    # IEND chunk
    iend = png_chunk(b'IEND', b'')

    # Write PNG file
    with open(filename, 'wb') as f:
        f.write(signature + ihdr + idat + iend)

    print(f"Created icon: {filename}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: generate_icon.py <output_path> [size]")
        sys.exit(1)

    output_path = sys.argv[1]
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 256

    create_png(size, size, output_path)
]=])

    # Run the Python script to generate the icon
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${ICON_GENERATOR_SCRIPT}" "${ICON_PATH}" "${ICON_SIZE}"
        RESULT_VARIABLE gen_result
        OUTPUT_VARIABLE gen_output
        ERROR_VARIABLE gen_error
    )

    if(gen_result EQUAL 0)
        message(STATUS "GenerateIcon: Created placeholder icon using Python")
    else()
        message(WARNING "GenerateIcon: Python icon generation failed: ${gen_error}")
    endif()
else()
    message(STATUS "GenerateIcon: Python not found, creating minimal placeholder")

    # Create a minimal valid PNG (1x1 pixel, blue)
    # This is a base64-encoded 1x1 blue PNG
    # In a real build, you'd want a proper icon
    file(WRITE "${ICON_PATH}.b64" "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==")

    # Unfortunately CMake can't decode base64 natively, so we'll create a note
    message(WARNING "GenerateIcon: Created minimal placeholder. Please provide a proper 256x256 PNG icon.")
endif()

# =============================================================================
# Verify icon was created
# =============================================================================

if(EXISTS "${ICON_PATH}")
    message(STATUS "GenerateIcon: Icon ready at ${ICON_PATH}")
else()
    message(WARNING "GenerateIcon: Icon was not created. AppImage may lack proper icon.")
endif()
