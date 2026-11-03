cmake_minimum_required(VERSION 3.21)

project(OmniOffice
    VERSION 1.0.0
    DESCRIPTION "OmniOffice - All-in-One office suite built with Qt"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Concurrent
    Core
    Gui
    Network
    Pdf
    PrintSupport
    Svg
    Widgets
)

qt_standard_project_setup(REQUIRES 6.8)

option(OMNI_BUILD_TESTS "Build the unit tests" OFF)
option(OMNI_DEPLOY_ON_BUILD "Copy the Qt runtime next to the executable after each build (Windows)" ON)

# Compiler settings shared by every target of the project.
add_library(omni_build_options INTERFACE)
target_compile_options(omni_build_options INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /permissive- /W3>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)
target_compile_definitions(omni_build_options INTERFACE
    OMNI_VERSION_STRING="${PROJECT_VERSION}"
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

add_subdirectory(src/core)
add_subdirectory(src/formats)
add_subdirectory(src/modules/writer)
#add_subdirectory(src/modules/sheet)
#add_subdirectory(src/modules/slides)
#add_subdirectory(src/modules/pdf)
#add_subdirectory(src/modules/notes)
#add_subdirectory(src/modules/files)
#add_subdirectory(src/modules/mail)
add_subdirectory(src/app)

if(OMNI_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()
