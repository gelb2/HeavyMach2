cmake_minimum_required(VERSION 3.22)
project(HeavyMach2Android LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(HM2_CHEAT "Include on-device validation entry points" OFF)
get_filename_component(HM2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(LIBTYPE STATIC CACHE STRING "" FORCE)
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "" FORCE)
set(ALSOFT_BACKEND_OBOE OFF CACHE BOOL "" FORCE)
set(ALSOFT_REQUIRE_OPENSL ON CACHE BOOL "" FORCE)
add_subdirectory("${HM2_ROOT}/ThirdParty/OpenALSoft" openal EXCLUDE_FROM_ALL)
include(opensl-resume.cmake)
set(HM2_OPENAL_LIBRARY OpenAL CACHE STRING "Vendored target for shared audio" FORCE)
add_subdirectory("${HM2_ROOT}" core EXCLUDE_FROM_ALL)

set(platform "${HM2_ROOT}/Sources/Platform/Android")
add_library(heavymach2 SHARED
    "${platform}/Bridge.cpp"
    "${platform}/Media.cpp"
    "${platform}/Renderer.cpp"
    "${platform}/Runtime.cpp")
if(HM2_CHEAT)
    target_sources(heavymach2 PRIVATE "${platform}/Validation.cpp")
endif()
target_compile_definitions(heavymach2 PRIVATE HM2_CHEAT=$<BOOL:${HM2_CHEAT}>)
target_compile_options(heavymach2 PRIVATE -Wall -Wextra -ffp-contract=off -fno-fast-math)
target_link_libraries(heavymach2 PRIVATE HeavyMach2Core HeavyMach2Audio OpenAL GLESv1_CM android log)
target_link_options(heavymach2 PRIVATE -Wl,--no-undefined "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/heavymach2.map")


# Keep the on-device validation runner debuggable at the Java layer while
# running the same optimized C++ simulation as the normal mobile build.
if(HM2_CHEAT)
    target_compile_options(HeavyMach2Core PRIVATE -O2)
    target_compile_options(heavymach2 PRIVATE -O2)
endif()
