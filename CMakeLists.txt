cmake_minimum_required(VERSION 3.21)
project(ShovelScriptExtender)

# must be Win32
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Build Win32 (32-bit) only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# --- Define our library target with a UNIQUE name ---
add_library(d3d9_proxy SHARED
        d3d9_proxy.cpp
        d3d9.def
)

# --- Configure the d3d9_proxy target ---

# Set properties for the output file
set_target_properties(d3d9_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "d3d9"                    # Final DLL will be d3d9.dll
        ARCHIVE_OUTPUT_NAME "d3d9_proxy"      # Import library will be d3d9_proxy.lib
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Add preprocessor definitions
target_compile_definitions(d3d9_proxy PRIVATE _D3D9DLL_)

# Add include directories for DirectX and MinHook
target_include_directories(d3d9_proxy PRIVATE
        "${PROJECT_SOURCE_DIR}/vendor/Include"
        "${CMAKE_SOURCE_DIR}/MinHook/include"
)

# Add library directories for DirectX
target_link_directories(d3d9_proxy PRIVATE
        "${PROJECT_SOURCE_DIR}/vendor/Lib/x86"
)

# Link all necessary libraries (DirectX and MinHook)
target_link_libraries(d3d9_proxy PRIVATE
        d3d9  # This now correctly refers to the EXTERNAL d3d9.lib
        d3dx9
        "${CMAKE_SOURCE_DIR}/MinHook/lib/libMinHook.x86.lib"
)

# Add /LTCG flag to improve linker performance (addresses the warning)
target_link_options(d3d9_proxy PRIVATE "/LTCG")