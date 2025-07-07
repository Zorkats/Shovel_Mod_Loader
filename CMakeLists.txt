cmake_minimum_required(VERSION 3.21)
project(ShovelKnightMod)

# Enforce 32-bit builds for game compatibility
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Build Win32 (32-bit) only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#--------------------------------------------------------------------
# MinHook Object Library (shared between targets)
#--------------------------------------------------------------------
add_library(minhook_objects OBJECT
        MinHook/src/buffer.c
        MinHook/src/hde/hde32.c
        MinHook/src/hook.c
        MinHook/src/trampoline.c
)

target_include_directories(minhook_objects PRIVATE
        "${PROJECT_SOURCE_DIR}/MinHook/include"
)

# Set position independent code for the object library
set_target_properties(minhook_objects PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

#--------------------------------------------------------------------
# Target 1: The Enhanced D3D9 Proxy with Mod Loader
#--------------------------------------------------------------------
add_library(d3d9_proxy SHARED
        d3d9_proxy.cpp
        d3d9.def
        compact_bitmap_font.hpp
        simple_yaml_parser.hpp
        enhanced_bgm_system.hpp
        mod_manager.hpp
        debug_console.hpp
        font_texture_capture.h
)

# Set target properties for d3d9_proxy
set_target_properties(d3d9_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "d3d9_hook"  # Changed to d3d9 to replace system DLL
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Include directories for d3d9_proxy
target_include_directories(d3d9_proxy PRIVATE
        "${PROJECT_SOURCE_DIR}/MinHook/include"
        "${PROJECT_SOURCE_DIR}/vendor"
        "${PROJECT_SOURCE_DIR}"
)

# Link libraries for d3d9_proxy
target_link_libraries(d3d9_proxy PRIVATE
        minhook_objects
        d3d9
        user32
        gdi32
        shell32
        psapi
        "${PROJECT_SOURCE_DIR}/vendor/Lib/x86/d3dx9.lib"
)

#--------------------------------------------------------------------
# Target 2: Enhanced Sound Hook Mod
#--------------------------------------------------------------------
add_library(sound_hook_mod SHARED
        sound_hook.cpp
)

set_target_properties(sound_hook_mod PROPERTIES
        OUTPUT_NAME "sound_hook"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mods"
)

target_include_directories(sound_hook_mod PRIVATE
        "${PROJECT_SOURCE_DIR}/MinHook/include"
)

target_link_libraries(sound_hook_mod PRIVATE
        minhook_objects
)

#--------------------------------------------------------------------
# Target 3: Font Hook Finder (Development tool)
#--------------------------------------------------------------------
add_library(font_hook_finder_mod SHARED
        font_hook_finder.cpp
)

target_link_libraries(font_hook_finder_mod PRIVATE
        minhook_objects
        psapi
)

set_target_properties(font_hook_finder_mod PROPERTIES
        OUTPUT_NAME "font_hook_finder"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mods"
)

#--------------------------------------------------------------------
# Target 4: D3D9 Text Interceptor (Development tool)
#--------------------------------------------------------------------
add_library(d3d9_text_interceptor_mod SHARED
        d3d9_text_interceptor.cpp
)

target_link_libraries(d3d9_text_interceptor_mod PRIVATE
        minhook_objects
        d3d9
)

set_target_properties(d3d9_text_interceptor_mod PROPERTIES
        OUTPUT_NAME "d3d9_text_interceptor"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mods"
)

#--------------------------------------------------------------------
# Create required directories and copy files
#--------------------------------------------------------------------
# Create mods directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/mods)

# Copy configuration files
configure_file(${CMAKE_SOURCE_DIR}/bgm_database.yaml
        ${CMAKE_BINARY_DIR}/bin/bgm_database.yaml COPYONLY)

configure_file(${CMAKE_SOURCE_DIR}/config/mod_config.ini
        ${CMAKE_BINARY_DIR}/bin/mods/config.ini COPYONLY)

# Create installation script
file(WRITE ${CMAKE_BINARY_DIR}/bin/install.bat
        "@echo off
echo Shovel Knight Mod Loader Installation
echo =====================================
echo.
echo This will install the mod loader to your Shovel Knight directory.
echo Make sure to backup your original d3d9.dll if it exists!
echo.
set /p SK_PATH=Enter your Shovel Knight directory path:
echo.
echo Copying files...
copy /Y d3d9.dll \"%SK_PATH%\\d3d9.dll\"
xcopy /Y /E mods \"%SK_PATH%\\mods\\\"
copy /Y bgm_database.yaml \"%SK_PATH%\\bgm_database.yaml\"
echo.
echo Installation complete!
echo.
echo To uninstall, delete d3d9.dll from your Shovel Knight directory.
echo.
pause"
)