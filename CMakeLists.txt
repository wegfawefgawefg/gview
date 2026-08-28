cmake_minimum_required(VERSION 3.20)

project(gview VERSION 0.1.0 LANGUAGES CXX)

option(GVIEW_STRICT "Enable strict compiler warnings" ON)
option(GVIEW_WARN_AS_ERROR "Treat warnings as errors" OFF)
option(GVIEW_BUILD_TESTS "Build tests" ON)
option(GVIEW_WITH_IMGUI "Build optional ImGui authoring tools" OFF)
option(GVIEW_WITH_SDL3_RENDERER "Build the SDL3 renderer and FreeType/HarfBuzz text backend" OFF)
set(GVIEW_IMGUI_TARGET "" CACHE STRING "Existing Dear ImGui target for authoring tools")
set(GVIEW_GLAYOUT_SOURCE_DIR "" CACHE PATH "Optional glayout source tree")

if(NOT TARGET glayout::graph)
    set(GLAYOUT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLAYOUT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLAYOUT_BUILD_SDL_DEMO OFF CACHE BOOL "" FORCE)
    if(GVIEW_GLAYOUT_SOURCE_DIR)
        add_subdirectory("${GVIEW_GLAYOUT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glayout")
    else()
        include(FetchContent)
        FetchContent_Declare(glayout
            GIT_REPOSITORY https://github.com/wegfawefgawefg/glayout.git
            GIT_TAG 947f5c6)
        FetchContent_MakeAvailable(glayout)
    endif()
endif()

function(gview_warnings target)
    if(NOT GVIEW_STRICT)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(GVIEW_WARN_AS_ERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(GVIEW_WARN_AS_ERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(gview_core
    src/collection.cpp
    src/compile.cpp
    src/focus.cpp
    src/io.cpp
    src/paint.cpp
    src/runtime.cpp
    src/runtime_metrics.cpp
    src/types.cpp
)
add_library(gview::core ALIAS gview_core)
target_link_libraries(gview_core PUBLIC glayout::graph)
target_include_directories(gview_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(gview_core PUBLIC cxx_std_20)
gview_warnings(gview_core)

add_library(gview_authoring src/authoring.cpp)
add_library(gview::authoring ALIAS gview_authoring)
target_link_libraries(gview_authoring PUBLIC gview::core)
target_include_directories(gview_authoring PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(gview_authoring PUBLIC cxx_std_20)
gview_warnings(gview_authoring)

if(GVIEW_WITH_SDL3_RENDERER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GVIEW_SDL3 REQUIRED IMPORTED_TARGET sdl3>=3.2.0)
    pkg_check_modules(GVIEW_FREETYPE REQUIRED IMPORTED_TARGET freetype2)
    pkg_check_modules(GVIEW_HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
    add_library(gview_sdl3
        src/sdl3_font.cpp
        src/sdl3_renderer.cpp
    )
    add_library(gview::sdl3 ALIAS gview_sdl3)
    target_link_libraries(gview_sdl3 PUBLIC gview::core PkgConfig::GVIEW_SDL3
        PkgConfig::GVIEW_FREETYPE PkgConfig::GVIEW_HARFBUZZ)
    target_include_directories(gview_sdl3 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_compile_features(gview_sdl3 PUBLIC cxx_std_20)
    gview_warnings(gview_sdl3)
endif()

if(GVIEW_WITH_IMGUI)
    if(GVIEW_IMGUI_TARGET AND TARGET ${GVIEW_IMGUI_TARGET})
        set(GVIEW_RESOLVED_IMGUI_TARGET ${GVIEW_IMGUI_TARGET})
    elseif(TARGET imgui)
        set(GVIEW_RESOLVED_IMGUI_TARGET imgui)
    elseif(TARGET ImGui::ImGui)
        set(GVIEW_RESOLVED_IMGUI_TARGET ImGui::ImGui)
    else()
        message(FATAL_ERROR "GVIEW_WITH_IMGUI requires an existing Dear ImGui target")
    endif()
    add_library(gview_imgui
        src/imgui_editor.cpp
        src/imgui_focus.cpp
        src/imgui_overlay.cpp)
    add_library(gview::imgui ALIAS gview_imgui)
    target_link_libraries(gview_imgui PUBLIC gview::authoring ${GVIEW_RESOLVED_IMGUI_TARGET})
    target_include_directories(gview_imgui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_compile_features(gview_imgui PUBLIC cxx_std_20)
    gview_warnings(gview_imgui)
endif()

if(GVIEW_BUILD_TESTS)
    enable_testing()
    add_executable(gview_tests tests/runtime_tests.cpp)
    target_link_libraries(gview_tests PRIVATE gview::core gview::authoring)
    target_compile_features(gview_tests PRIVATE cxx_std_20)
    gview_warnings(gview_tests)
    add_test(NAME gview_tests COMMAND gview_tests)
endif()
