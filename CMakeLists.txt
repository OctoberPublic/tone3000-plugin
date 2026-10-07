cmake_minimum_required(VERSION 3.22.1)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" T3K_VERSION)
string(STRIP "${T3K_VERSION}" T3K_VERSION)
project(LocalRig VERSION ${T3K_VERSION} LANGUAGES C CXX)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT MSVC)
    add_compile_options(-fsigned-char)
endif()
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(CMAKE_TLS_VERIFY ON)
include(cmake/cpm.cmake)
if (UNIX AND NOT APPLE)
    CPMAddPackage(
        NAME freetype
        GITHUB_REPOSITORY freetype/freetype
        GIT_TAG VER-2-13-3
        VERSION 2.13.3
        SOURCE_DIR ${LIB_DIR}/freetype
        EXCLUDE_FROM_ALL YES
        OPTIONS
            "FT_DISABLE_ZLIB ON"
            "FT_DISABLE_BZIP2 ON"
            "FT_DISABLE_PNG ON"
            "FT_DISABLE_HARFBUZZ ON"
            "FT_DISABLE_BROTLI ON"
            "SKIP_INSTALL_ALL ON"
    )
    if (NOT TARGET freetype)
        message(FATAL_ERROR "T3K_STATIC_FREETYPE: the freetype target was not created by "
            "${LIB_DIR}/freetype; its CMake layout likely changed. Update the "
            "package block in CMakeLists.txt.")
    endif()
    set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(T3K_FONTCONFIG REQUIRED fontconfig)
    list(FILTER T3K_FONTCONFIG_LINK_LIBRARIES EXCLUDE REGEX "freetype")
    list(FILTER T3K_FONTCONFIG_INCLUDE_DIRS EXCLUDE REGEX "freetype")

    add_library(pkgconfig_juce_graphics_LINUX_DEPS INTERFACE)
    add_library(juce::pkgconfig_juce_graphics_LINUX_DEPS ALIAS pkgconfig_juce_graphics_LINUX_DEPS)
    target_link_libraries(pkgconfig_juce_graphics_LINUX_DEPS INTERFACE
        freetype
        ${T3K_FONTCONFIG_LINK_LIBRARIES})
    target_include_directories(pkgconfig_juce_graphics_LINUX_DEPS INTERFACE
        ${T3K_FONTCONFIG_INCLUDE_DIRS})
    target_compile_options(pkgconfig_juce_graphics_LINUX_DEPS INTERFACE
        ${T3K_FONTCONFIG_CFLAGS_OTHER})
    # FreeType's own FT_EXPORT macro stamps every API function with
    # visibility("default") (public-macros.h), overriding the hidden preset,
    # so left alone the plugin .so would re-export all ~150 FT_* symbols as
    # preemptible. In a host that already carries a FreeType, the dynamic
    # linker resolves a dlopen'd library's preemptible references against the
    # global scope first: our calls would bind to the host's copy for every
    # symbol it has and to our own for the rest, mixing two implementations
    # on one FT_Library. --exclude-libs makes the archive's symbols local in
    # each binary that links it, so they are neither exported nor preempted.
    target_link_options(pkgconfig_juce_graphics_LINUX_DEPS INTERFACE
        ${T3K_FONTCONFIG_LDFLAGS_OTHER}
        "LINKER:--exclude-libs,libfreetype.a")
    message(STATUS "FreeType 2.13.3 linked statically into the Linux GUI targets (fontconfig stays dynamic)")
endif()

# Download JUCE using CPM
CPMAddPackage(
    NAME JUCE
    GIT_TAG 9.0.3
    VERSION 9.0.3
    GITHUB_REPOSITORY juce-framework/JUCE
    SOURCE_DIR ${LIB_DIR}/juce
)


include(CTest)
if(BUILD_TESTING)
    CPMAddPackage(NAME GOOGLETEST GITHUB_REPOSITORY google/googletest
        GIT_TAG v1.15.2 VERSION 1.15.2 SOURCE_DIR ${LIB_DIR}/googletest
        OPTIONS "INSTALL_GTEST OFF")
endif()
add_subdirectory(plugin)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
