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
    Sources/Core/Data/Machines.cpp
    Sources/Core/Data/SaveData.cpp
    Sources/Core/Progression/Progression.cpp
    Sources/Core/Progression/Equipment.cpp
    Sources/Core/Session/GameSession.cpp
    Sources/Core/Validation/ConcurrentValidation.cpp
    Sources/Core/Simulation/TerrainCollision.cpp
    Sources/Core/Simulation/BipodMotion.cpp
    Sources/Core/Simulation/WorldNavigation.cpp
    Sources/Core/Simulation/LocomotionScene.cpp
    Sources/Core/Simulation/WeaponController.cpp
    Sources/Core/Simulation/ProjectileMotion.cpp
    Sources/Core/Simulation/Particles.cpp
    Sources/Core/UI/Button.cpp
    Sources/Core/Simulation/EnemySpawner.cpp
    Sources/Core/Simulation/Trail.cpp
    Sources/Core/Simulation/VehicleMotion.cpp
    Sources/Core/Simulation/ProjectileImpact.cpp
    Sources/Core/Simulation/WeaponEffects.cpp
    Sources/Core/Simulation/ProjectileVisual.cpp
    Sources/Core/Simulation/EnemyActors.cpp
    Sources/Core/Simulation/EnemyVehicle.cpp
    Sources/Core/Simulation/CombatScene.cpp
    Sources/Core/Simulation/ObjectCollision.cpp
    Sources/Core/UI/Text.cpp
    Sources/Core/UI/Screens.cpp
    Sources/Core/Presentation/PartSprites.cpp
    Sources/Core/Presentation/VisualLayer.cpp
    Sources/Core/Presentation/PreparedTiles.cpp
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
    add_executable(WeaponControllerTests tests/WeaponControllerTests.cpp)
    target_link_libraries(WeaponControllerTests PRIVATE HeavyMach2Core)
    target_compile_options(WeaponControllerTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalWeaponControllerRules COMMAND WeaponControllerTests)
    add_executable(ProjectileTests tests/ProjectileTests.cpp)
    target_link_libraries(ProjectileTests PRIVATE HeavyMach2Core)
    target_compile_options(ProjectileTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalProjectileMotion COMMAND ProjectileTests)
    add_executable(ParticleTests tests/ParticleTests.cpp)
    target_link_libraries(ParticleTests PRIVATE HeavyMach2Core)
    target_compile_options(ParticleTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalParticlePoolAndMotion COMMAND ParticleTests)
    add_executable(ButtonTests tests/ButtonTests.cpp)
    target_link_libraries(ButtonTests PRIVATE HeavyMach2Core)
    target_compile_options(ButtonTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalGPUButtons COMMAND ButtonTests)
    add_executable(SpawnerTests tests/SpawnerTests.cpp)
    target_link_libraries(SpawnerTests PRIVATE HeavyMach2Core)
    target_compile_options(SpawnerTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalSpawnerAndClocks COMMAND SpawnerTests)
    add_executable(TrailTests tests/TrailTests.cpp)
    target_link_libraries(TrailTests PRIVATE HeavyMach2Core)
    target_compile_options(TrailTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalTrailGeometryAndFade COMMAND TrailTests)
    add_executable(MachineTests tests/MachineTests.cpp)
    target_link_libraries(MachineTests PRIVATE HeavyMach2Core)
    target_compile_options(MachineTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalMachineTableRules COMMAND MachineTests)
    add_executable(VehicleTests tests/VehicleTests.cpp)
    target_link_libraries(VehicleTests PRIVATE HeavyMach2Core)
    target_compile_options(VehicleTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalVehicleAndWeaponAim COMMAND VehicleTests)
    add_executable(TextTests tests/TextTests.cpp)
    target_link_libraries(TextTests PRIVATE HeavyMach2Core)
    target_compile_options(TextTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalGPUText COMMAND TextTests)
    add_executable(ImpactTests tests/ImpactTests.cpp)
    target_link_libraries(ImpactTests PRIVATE HeavyMach2Core)
    target_compile_options(ImpactTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalProjectileImpact COMMAND ImpactTests)
    add_executable(WeaponEffectsTests tests/WeaponEffectsTests.cpp)
    target_link_libraries(WeaponEffectsTests PRIVATE HeavyMach2Core)
    target_compile_options(WeaponEffectsTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalWeaponEmissionConsumers COMMAND WeaponEffectsTests)
    add_executable(VisualLayerTests tests/VisualLayerTests.cpp)
    target_link_libraries(VisualLayerTests PRIVATE HeavyMach2Core)
    target_compile_options(VisualLayerTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalVisualLayersAndAtlases COMMAND VisualLayerTests)
    add_executable(ObjectCollisionTests tests/ObjectCollisionTests.cpp)
    target_link_libraries(ObjectCollisionTests PRIVATE HeavyMach2Core)
    target_compile_options(ObjectCollisionTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalObjectCollision COMMAND ObjectCollisionTests)
    add_executable(VisibilityTests tests/VisibilityTests.cpp)
    target_link_libraries(VisibilityTests PRIVATE HeavyMach2Core)
    target_compile_options(VisibilityTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalCameraVisibility COMMAND VisibilityTests)
    add_executable(PreparedTileTests tests/PreparedTileTests.cpp)
    target_link_libraries(PreparedTileTests PRIVATE HeavyMach2Core)
    target_compile_options(PreparedTileTests PRIVATE -UNDEBUG)
    add_test(NAME PreparedOriginalTileCache COMMAND PreparedTileTests)
    add_executable(ProjectileVisualTests tests/ProjectileVisualTests.cpp)
    target_link_libraries(ProjectileVisualTests PRIVATE HeavyMach2Core)
    target_compile_options(ProjectileVisualTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalProjectileVisuals COMMAND ProjectileVisualTests)
    add_executable(EnemyActorsTests tests/EnemyActorsTests.cpp)
    target_link_libraries(EnemyActorsTests PRIVATE HeavyMach2Core)
    target_compile_options(EnemyActorsTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalTurretAircraftActors COMMAND EnemyActorsTests)
    if(HM2_HD_RESOURCES)
        add_test(NAME Original695SpawnerActors COMMAND EnemyActorsTests "${HM2_HD_RESOURCES}")
    endif()
    add_executable(EnemyVehicleTests tests/EnemyVehicleTests.cpp)
    target_link_libraries(EnemyVehicleTests PRIVATE HeavyMach2Core)
    target_compile_options(EnemyVehicleTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalEnemyVehicleAI COMMAND EnemyVehicleTests)
    add_executable(SaveTests tests/SaveTests.cpp)
    target_link_libraries(SaveTests PRIVATE HeavyMach2Core)
    target_compile_options(SaveTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalHDSaveEnvelope COMMAND SaveTests)
    add_executable(CombatSceneTests tests/CombatSceneTests.cpp)
    target_link_libraries(CombatSceneTests PRIVATE HeavyMach2Core)
    target_compile_options(CombatSceneTests PRIVATE -UNDEBUG)
    if(HM2_HD_RESOURCES)
        add_test(NAME OriginalBattleRuntime COMMAND CombatSceneTests "${HM2_HD_RESOURCES}")
        add_executable(SessionTests tests/SessionTests.cpp)
        target_link_libraries(SessionTests PRIVATE HeavyMach2Core)
        target_compile_options(SessionTests PRIVATE -UNDEBUG)
        add_test(NAME OriginalSessionTransitions COMMAND SessionTests "${HM2_HD_RESOURCES}")
    endif()
    add_executable(ProgressionTests tests/ProgressionTests.cpp)
    target_link_libraries(ProgressionTests PRIVATE HeavyMach2Core)
    target_compile_options(ProgressionTests PRIVATE -UNDEBUG)
    add_test(NAME OriginalExperienceRules COMMAND ProgressionTests)
    if(HM2_HD_RESOURCES)
        add_test(NAME OriginalInventoryMissionsAndDrops COMMAND ProgressionTests "${HM2_HD_RESOURCES}")
        add_executable(EquipmentTests tests/EquipmentTests.cpp)
        target_link_libraries(EquipmentTests PRIVATE HeavyMach2Core)
        target_compile_options(EquipmentTests PRIVATE -UNDEBUG)
        add_test(NAME OriginalEquipmentTradeAndBots COMMAND EquipmentTests "${HM2_HD_RESOURCES}")
        add_executable(ScreensTests tests/ScreensTests.cpp)
        target_link_libraries(ScreensTests PRIVATE HeavyMach2Core)
        target_compile_options(ScreensTests PRIVATE -UNDEBUG)
        add_test(NAME OriginalScreensAndHUD COMMAND ScreensTests "${HM2_HD_RESOURCES}")
        add_executable(ConcurrentValidationTests tests/ConcurrentValidationTests.cpp)
        target_link_libraries(ConcurrentValidationTests PRIVATE HeavyMach2Core)
        target_compile_options(ConcurrentValidationTests PRIVATE -UNDEBUG)
        add_test(NAME ConcurrentOriginalGameScenarios COMMAND ConcurrentValidationTests "${HM2_HD_RESOURCES}")
    endif()
    if(HM2_HD_RESOURCES)
        add_test(NAME OriginalStageVehicleEncounters COMMAND EnemyVehicleTests "${HM2_HD_RESOURCES}")
    endif()
    add_executable(OriginalAssetTests tests/OriginalAssetTests.cpp)
    target_link_libraries(OriginalAssetTests PRIVATE HeavyMach2Core)
    target_compile_options(OriginalAssetTests PRIVATE -UNDEBUG)
    set(HM2_HD_RESOURCES "" CACHE PATH "Local prepared original HD resources (not required by synthetic tests)")
    if(HM2_HD_RESOURCES)
        add_test(NAME OriginalMachines COMMAND MachineTests "${HM2_HD_RESOURCES}/MachList.tble")
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

# Keep the platform audio backend separate from the portable simulation core.
# Build this repository's library with tools/build_openal.py before enabling.
set(HM2_OPENAL_LIBRARY "" CACHE FILEPATH "HM2-owned OpenAL Soft static library for this host")
if(HM2_OPENAL_LIBRARY)
    add_library(HeavyMach2Audio STATIC Sources/Core/Audio/PCM.cpp Sources/Core/Audio/OpenALEffects.cpp)
    target_include_directories(HeavyMach2Audio PUBLIC Sources PRIVATE ThirdParty/OpenALSoft/include)
    target_compile_options(HeavyMach2Audio PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math)
    target_link_libraries(HeavyMach2Audio PUBLIC "${HM2_OPENAL_LIBRARY}")
    if(APPLE)
        target_link_libraries(HeavyMach2Audio PUBLIC "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation")
    endif()
    if(BUILD_TESTING)
        add_executable(AudioTests tests/AudioTests.cpp)
        target_link_libraries(AudioTests PRIVATE HeavyMach2Audio)
        target_compile_options(AudioTests PRIVATE -UNDEBUG)
        add_test(NAME OriginalAudioPCMAndMusicBuffers COMMAND AudioTests)
        if(HM2_HD_RESOURCES)
            add_test(NAME OriginalAudioLoopback COMMAND AudioTests "${HM2_HD_RESOURCES}")
        endif()
    endif()
endif()
