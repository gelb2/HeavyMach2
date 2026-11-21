cmake_minimum_required(VERSION 3.20)
project(HeavyMach2Web LANGUAGES CXX)
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure this build with emcmake.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
option(HM2_WEB_VALIDATION "Build a separate test game" OFF)
get_filename_component(HM2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(HM2_WEB_ASSETS "${HM2_ROOT}/.local/prepared/Campaign/HD" CACHE PATH "Local prepared campaign inputs")
foreach(required IN ITEMS manifest.json MapInfo.mcb ItemInfo.tble)
    if(NOT EXISTS "${HM2_WEB_ASSETS}/${required}")
        message(FATAL_ERROR "Missing local prepared campaign member: ${required}")
    endif()
endforeach()
file(GLOB_RECURSE asset_inputs LIST_DIRECTORIES false CONFIGURE_DEPENDS "${HM2_WEB_ASSETS}/*")
foreach(asset IN LISTS asset_inputs)
    if(IS_SYMLINK "${asset}" OR "${asset}" MATCHES "\\.[iI][pP][aA]$")
        message(FATAL_ERROR "Unexpected file in campaign input: ${asset}")
    endif()
endforeach()

set(BUILD_TESTING OFF CACHE BOOL "Native core tests are built separately" FORCE)
add_library(HM2WebOpenAL INTERFACE)
target_link_libraries(HM2WebOpenAL INTERFACE -lopenal)
set(HM2_OPENAL_LIBRARY HM2WebOpenAL CACHE STRING "Emscripten OpenAL for shared sound effects" FORCE)
add_subdirectory("${HM2_ROOT}" core EXCLUDE_FROM_ALL)
target_compile_options(HeavyMach2Core PRIVATE -fexceptions)
add_executable(hm2web
    "${HM2_ROOT}/Sources/Platform/Web/Runtime.cpp"
    "${HM2_ROOT}/Sources/Platform/Web/Renderer.cpp")
target_link_libraries(hm2web PRIVATE HeavyMach2Core HeavyMach2Audio)
target_compile_definitions(hm2web PRIVATE HM2_WEB_VALIDATION=$<BOOL:${HM2_WEB_VALIDATION}>)
target_compile_options(hm2web PRIVATE -Wall -Wextra -fexceptions)
target_link_options(hm2web PRIVATE -fexceptions --no-entry
    "-sMODULARIZE=1" "-sEXPORT_ES6=1" "-sEXPORT_NAME=createGame"
    "-sENVIRONMENT=web" "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
    "-sASYNCIFY=1" "-sASYNCIFY_STACK_SIZE=262144" "-sSTACK_SIZE=2097152"
    "-sALLOW_MEMORY_GROWTH=1" "-sFORCE_FILESYSTEM=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','FS']"
    "SHELL:--preload-file \"${HM2_WEB_ASSETS}@/campaign\"")
set_target_properties(hm2web PROPERTIES
    OUTPUT_NAME game SUFFIX ".js" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    LINK_DEPENDS "${asset_inputs}")

file(GLOB shell_inputs LIST_DIRECTORIES false CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shell/*")
add_custom_target(package_web ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/Shell" "${CMAKE_BINARY_DIR}/dist"
            "$<IF:$<BOOL:${HM2_WEB_VALIDATION}>,validation,normal>"
    DEPENDS hm2web "${CMAKE_CURRENT_SOURCE_DIR}/package.py" ${shell_inputs}
    VERBATIM)
