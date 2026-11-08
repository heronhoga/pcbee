cmake_minimum_required(VERSION 3.24)

# raylib 5.5 still declares compatibility with CMake < 3.5.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# Preserve static-CRT selection in dependencies with older policy versions.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
project(PCBEE VERSION 0.2.0 LANGUAGES C)

# Let language servers use the same include paths and defines as the compiler.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(MSVC)
    # MSVC provides C99 features through its C11 mode. Bundle the CRT.
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PCBEE_BUILD_APP "Build the desktop editor (downloads raylib)" ON)
option(PCBEE_SMOKE_TEST "Register a desktop test; requires a graphical session" OFF)
option(PCBEE_SANITIZE "Enable AddressSanitizer and UBSan" OFF)
include(CTest)

add_library(pcbee_options INTERFACE)
if(MSVC)
    target_compile_options(pcbee_options INTERFACE /W4 /utf-8)
else()
    target_compile_options(pcbee_options INTERFACE -Wall -Wextra -Wpedantic)
endif()
if(PCBEE_SANITIZE)
    if(MSVC OR NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "PCBEE_SANITIZE requires GCC or Clang with ASan/UBSan")
    endif()
    target_compile_options(pcbee_options INTERFACE -g -fno-omit-frame-pointer -fsanitize=address,undefined)
    target_link_options(pcbee_options INTERFACE -fsanitize=address,undefined)
endif()

add_library(pcb_core STATIC src/pcb.c src/learn.c src/i18n.c src/export.c)
target_include_directories(pcb_core PUBLIC src)
target_link_libraries(pcb_core PUBLIC pcbee_options)
if(NOT MSVC)
    target_link_libraries(pcb_core PUBLIC m)
endif()

if(BUILD_TESTING)
    add_executable(test_pcb tests/test_pcb.c)
    target_link_libraries(test_pcb PRIVATE pcb_core)
    # Assertions contain the test operations and must run in Release too.
    if(MSVC)
        target_compile_options(test_pcb PRIVATE /UNDEBUG)
    else()
        target_compile_options(test_pcb PRIVATE -UNDEBUG)
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/build")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work/examples")
    configure_file(examples/demo.pcb test-work/examples/demo.pcb COPYONLY)
    add_test(NAME pcb_core COMMAND test_pcb)
    set_tests_properties(pcb_core PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work" TIMEOUT 30)
    add_executable(test_learn tests/test_learn.c)
    target_link_libraries(test_learn PRIVATE pcb_core)
    if(MSVC)
        target_compile_options(test_learn PRIVATE /UNDEBUG)
    else()
        target_compile_options(test_learn PRIVATE -UNDEBUG)
    endif()
    add_test(NAME pcb_learning COMMAND test_learn)
    add_executable(test_export tests/test_export.c)
    target_link_libraries(test_export PRIVATE pcb_core)
    if(MSVC)
        target_compile_options(test_export PRIVATE /UNDEBUG)
    else()
        target_compile_options(test_export PRIVATE -UNDEBUG)
    endif()
    add_test(NAME pcb_export COMMAND test_export)
    set_tests_properties(pcb_export PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-work" TIMEOUT 30)
endif()

if(PCBEE_BUILD_APP)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        URL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/5.5
        URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(raylib)
    # Keep raylib's development headers/libraries out of the app package.
    set_property(DIRECTORY "${raylib_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    if(MSVC)
        set_property(TARGET raylib PROPERTY C_STANDARD 11)
    endif()

    add_executable(pcbee src/main.c src/ui.c src/font_data.c)
    target_link_libraries(pcbee PRIVATE pcb_core raylib)
    target_compile_definitions(pcbee PRIVATE PCBEE_VERSION="${PROJECT_VERSION}")
    if(APPLE)
        set_target_properties(pcbee PROPERTIES
            MACOSX_BUNDLE TRUE OUTPUT_NAME PCBEE
            MACOSX_BUNDLE_BUNDLE_NAME PCBEE
            MACOSX_BUNDLE_GUI_IDENTIFIER org.pcbee.editor
             MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
             MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
        add_custom_command(TARGET pcbee POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:pcbee>/Contents/Resources/fonts"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/IBMPlexMono-Regular.ttf"
                "$<TARGET_BUNDLE_DIR:pcbee>/Contents/Resources/fonts/IBMPlexMono-Regular.ttf"
            VERBATIM)
    elseif(WIN32)
        target_sources(pcbee PRIVATE src/windows_entry.c)
        target_link_libraries(pcbee PRIVATE shell32)
        set_target_properties(pcbee PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MINGW)
            target_link_options(pcbee PRIVATE -static -static-libgcc)
        endif()
    endif()
    if(NOT APPLE)
        add_custom_command(TARGET pcbee POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:pcbee>/assets/fonts"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/IBMPlexMono-Regular.ttf"
                "$<TARGET_FILE_DIR:pcbee>/assets/fonts/IBMPlexMono-Regular.ttf"
            VERBATIM)
        install(FILES assets/fonts/IBMPlexMono-Regular.ttf DESTINATION assets/fonts)
    endif()
    if(BUILD_TESTING AND PCBEE_SMOKE_TEST)
        add_test(NAME desktop_smoke COMMAND pcbee --smoke-test)
        set_tests_properties(desktop_smoke PROPERTIES TIMEOUT 30)
    endif()

    install(TARGETS pcbee RUNTIME DESTINATION . BUNDLE DESTINATION .)
    install(FILES README.md PCB_DESIGN_GUIDE.md DESTINATION .)
    install(FILES examples/demo.pcb DESTINATION examples)
    install(FILES assets/fonts/OFL.txt DESTINATION licenses RENAME ibm-plex-mono.txt)
    install(FILES "${raylib_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME raylib.txt)
    install(FILES "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md"
        DESTINATION licenses RENAME glfw.txt)

    # Name packages using the target architecture, not the host CPU.
    set(package_arch "${CMAKE_SYSTEM_PROCESSOR}")
    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        set(package_arch "${CMAKE_OSX_ARCHITECTURES}")
    elseif(WIN32 AND CMAKE_GENERATOR_PLATFORM)
        set(package_arch "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    string(TOLOWER "${package_arch}" package_arch)
    if(package_arch MATCHES "^(amd64|x86_64|x64)$")
        set(package_arch x64)
    elseif(package_arch MATCHES "^(aarch64|arm64)$")
        set(package_arch arm64)
    else()
        string(REPLACE ";" "-" package_arch "${package_arch}")
    endif()
    if(APPLE)
        set(package_os macos)
        set(CPACK_GENERATOR ZIP)
    elseif(WIN32)
        set(package_os windows)
        set(CPACK_GENERATOR ZIP)
    else()
        set(package_os linux)
        set(CPACK_GENERATOR TGZ)
    endif()
    set(CPACK_PACKAGE_NAME pcbee)
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "pcbee-${PROJECT_VERSION}-${package_os}-${package_arch}")
    set(CPACK_PACKAGE_CHECKSUM SHA256)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    include(CPack)
endif()
