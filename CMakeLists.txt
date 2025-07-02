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
# Target 1: The Integrated D3D9 Proxy with Simple Overlay (d3d9_proxy.cpp)
#--------------------------------------------------------------------
add_library(d3d9_proxy SHARED
        d3d9_proxy.cpp
        d3d9.def
        compact_bitmap_font.hpp
        simple_yaml_parser.hpp
        game_text_hook.cpp
        font_texture_capture.h
)

# Set target properties for d3d9_proxy
set_target_properties(d3d9_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "d3d9_hook"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Include directories for d3d9_proxy
target_include_directories(d3d9_proxy PRIVATE
        "${PROJECT_SOURCE_DIR}/MinHook/include"
        "${PROJECT_SOURCE_DIR}/vendor"
)

# Link libraries for d3d9_proxy
target_link_libraries(d3d9_proxy PRIVATE
        minhook_objects
        d3d9
        user32
        gdi32
        shell32
        "${PROJECT_SOURCE_DIR}/vendor/Lib/x86/d3dx9.lib"
)

#--------------------------------------------------------------------
# Target 2: The OpenGL ES Hook Mod (gles_hook_mod.cpp) - OPTIONAL
# You can comment this out if you're not using it anymore
#--------------------------------------------------------------------
add_library(opengl_es_hook_mod SHARED
        gles_hook_mod.cpp
        glad/glad.c
        # ImGui sources
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_tables.cpp
        # ImGui backends
        imgui/backends/imgui_impl_opengl3.cpp
        imgui/backends/imgui_impl_dx9.cpp
        imgui/backends/imgui_impl_win32.cpp
)

set_target_properties(opengl_es_hook_mod PROPERTIES
        OUTPUT_NAME "opengl_es_hook_mod"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mods"
)

# Include directories for opengl_es_hook_mod
target_include_directories(opengl_es_hook_mod PRIVATE
        "${PROJECT_SOURCE_DIR}/glad"
        "${PROJECT_SOURCE_DIR}/imgui"
        "${PROJECT_SOURCE_DIR}/imgui/backends"
        "${PROJECT_SOURCE_DIR}/MinHook/include"
        "${PROJECT_SOURCE_DIR}/vendor"
        "${PROJECT_SOURCE_DIR}/EGL"
        "${PROJECT_SOURCE_DIR}/KHR"
)

# Link directories for opengl_es_hook_mod
target_link_directories(opengl_es_hook_mod PRIVATE
        "${PROJECT_SOURCE_DIR}/EGL"
)

# Link libraries for opengl_es_hook_mod
target_link_libraries(opengl_es_hook_mod PRIVATE
        opengl32
        d3d9
        libEGL
        shell32
        shlwapi
        minhook_objects
)

#--------------------------------------------------------------------
# Target 3: Other example mods (library.cpp - sound hook)
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


add_library(font_hook_finder_mod SHARED font_hook_finder.cpp)
target_link_libraries(font_hook_finder_mod PRIVATE minhook_objects)
set_target_properties(font_hook_finder_mod PROPERTIES
        OUTPUT_NAME "font_hook_finder"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mods"
)

# D3D9 Text Interceptor Module
add_library(d3d9_text_interceptor_mod SHARED d3d9_text_interceptor.cpp)
target_link_libraries(d3d9_text_interceptor_mod PRIVATE minhook_objects d3d9)
set_target_properties(d3d9_text_interceptor_mod PROPERTIES
        OUTPUT_NAME "d3d9_text_interceptor"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/mods"
)
#--------------------------------------------------------------------
# Create mods directory in build output
#--------------------------------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/mods)