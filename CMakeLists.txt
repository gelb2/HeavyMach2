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
    Sources/Core/Data/Maps.cpp
    Sources/Core/Data/PlayerEquipment.cpp
    Sources/Core/Data/Weapons.cpp
    Sources/Core/Simulation/TerrainCollision.cpp
    Sources/Core/Simulation/BipodMotion.cpp
    Sources/Core/Simulation/WorldNavigation.cpp
    Sources/Core/Simulation/LocomotionScene.cpp
    Sources/Core/Presentation/PartSprites.cpp
    Sources/Core/Presentation/PlayerMapPreview.cpp
    Sources/Core/Presentation/AssemblyPreview.cpp
    Sources/Core/Presentation/MapPreview.cpp)
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
    add_executable(MapTests tests/MapTests.cpp)
    target_link_libraries(MapTests PRIVATE HeavyMach2Core)
    target_compile_options(MapTests PRIVATE -UNDEBUG)
    add_test(NAME MapRecordsAndCamera COMMAND MapTests)
    add_executable(TerrainTests tests/TerrainTests.cpp)
    target_link_libraries(TerrainTests PRIVATE HeavyMach2Core)
    target_compile_options(TerrainTests PRIVATE -UNDEBUG)
    add_test(NAME TerrainCollisionRules COMMAND TerrainTests)
    add_executable(LocomotionTests tests/LocomotionTests.cpp)
    target_link_libraries(LocomotionTests PRIVATE HeavyMach2Core)
    target_compile_options(LocomotionTests PRIVATE -UNDEBUG)
    add_test(NAME BipodGaitAimAndNavigation COMMAND LocomotionTests)
    add_executable(WeaponsTests tests/WeaponsTests.cpp)
    target_link_libraries(WeaponsTests PRIVATE HeavyMach2Core)
    target_compile_options(WeaponsTests PRIVATE -UNDEBUG)
    add_test(NAME WeaponDataAndMuzzle COMMAND WeaponsTests)
    add_executable(OriginalAssetTests tests/OriginalAssetTests.cpp)
    target_link_libraries(OriginalAssetTests PRIVATE HeavyMach2Core)
    target_compile_options(OriginalAssetTests PRIVATE -UNDEBUG)
    set(HM2_HD_RESOURCES "" CACHE PATH "Local prepared original HD resources (not required by synthetic tests)")
    if(HM2_HD_RESOURCES)
        add_test(NAME OriginalMCBRoundTrip COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" roundtrip)
        add_test(NAME OriginalTables COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" tables)
        add_test(NAME OriginalAssemblyCombined COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" assembly)
        add_test(NAME OriginalImages COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" images)
        add_test(NAME OriginalMaps COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" maps)
        add_test(NAME OriginalPlayerEquipment COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" player)
        add_test(NAME OriginalTerrainLocomotion COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" locomotion)
        add_test(NAME OriginalWeapons COMMAND OriginalAssetTests "${HM2_HD_RESOURCES}" weapons)
    endif()
endif()
