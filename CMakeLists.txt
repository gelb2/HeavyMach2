cmake_minimum_required(VERSION 3.20)
project(HeavyMach2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(HeavyMach2Core STATIC
    Sources/Assets/Image.cpp
    Sources/Core/Data/Chunk.cpp
    Sources/Core/Data/DataTable.cpp
    Sources/Core/Data/Models.cpp
    Sources/Core/Presentation/AssemblyPreview.cpp)
target_include_directories(HeavyMach2Core PUBLIC Sources PRIVATE ThirdParty/stb)
target_compile_options(HeavyMach2Core PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math)

include(CTest)
if(BUILD_TESTING)
    add_executable(DataReaderTests tests/DataReaderTests.cpp)
    target_link_libraries(DataReaderTests PRIVATE HeavyMach2Core)
    target_compile_options(DataReaderTests PRIVATE -UNDEBUG)
    add_test(NAME ChunkRoundTripAndBoundaries COMMAND DataReaderTests chunk)
    add_test(NAME OriginalTableTokenRules COMMAND DataReaderTests table)
    add_test(NAME DarwinRandomCompatibility COMMAND DataReaderTests random)
    add_executable(OriginalAssetTests tests/OriginalAssetTests.cpp)
    target_link_libraries(OriginalAssetTests PRIVATE HeavyMach2Core)
    target_compile_options(OriginalAssetTests PRIVATE -UNDEBUG)
    set(HM2_HD_RESOURCES "" CACHE PATH "Local prepared original HD resources (not required by synthetic tests)")
    if(HM2_HD_RESOURCES)
        add_test(NAME OriginalMCBRoundTrip COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" roundtrip)
        add_test(NAME OriginalTables COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" tables)
        add_test(NAME OriginalAssemblyCombined COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" assembly)
        add_test(NAME OriginalImages COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" images)
    endif()
endif()
