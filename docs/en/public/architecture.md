# HeavyMach 2 code structure

[한국어](../../public/architecture.md) · [Documentation](index.md) · [Development history](development-history.md) · [Build and install](build-and-install.md)

Game rules and screen layout live in the shared C++20 core. iPhone and iPad build it with Xcode. Android builds the same core with Gradle and the NDK. The web build compiles the core to Wasm. The Tauri Mac app bundles the web files with a Rust host.

## Code and data

| Location | Role |
| --- | --- |
| [Shared core](../../../Sources/Core) | Handles movement, projectiles, enemies, missions, progression, and saves. It also assembles mech parts and lays out the camera, HUD, and menus. |
| [Apple host](../../../Sources/Platform/Apple) · [Xcode project](../../../Apple/iOS/HeavyMach2.xcodeproj) | Connects touch input and display on iPhone and iPad, OpenGL ES rendering, OpenAL sound effects, and AudioQueue background music. |
| [Android host](../../../Sources/Platform/Android) · [Gradle project](../../../Android) | Kotlin and JNI pass input to the C++ host. The host connects EGL and OpenGL ES, audio devices, and app-private files. |
| [Web host](../../../Sources/Platform/Web) · [Web shell](../../../Web/Shell) | Connects Wasm and WebGL 2. It handles installation, offline resources, browser input, and IndexedDB. |
| [Tauri Mac](../../../Desktop/Tauri) | Runs the web interface in an app window. The Rust host manages user files and save backups. |
| [Campaign expansion data](../../../Resources/Campaign) · [Public build resource tool](../../../tools/prepare_build_resources.py) | Holds snowfield and equipment data. A public clone prepares the campaign resources by verifying hashes from a pinned PWA build. |

## How rendering and input meet

1. The host passes the surface size, safe area, device class, and input to the game.
2. `DisplayLayout` calculates the visible area. `Screens` determines the artwork and positions of buttons and the layout used to handle touches.
3. `GameSession` advances movement, combat, items, and missions. The renderer draws the battlefield and HUD using that state and the prepared textures.
4. Sound effects use prepared OpenAL buffers and sources. The host writes progress when the game requests a save.

[Display layout code](../../../Sources/Core/Presentation/DisplayLayout.cpp) · [Screen and button code](../../../Sources/Core/UI/Screens.cpp) · [Session code](../../../Sources/Core/Session/GameSession.cpp) · [Timing, saves, and audio](../../runtime-contract.md)

## Regular and test editions and saves

The regular and test editions share game logic and use separate saves. The test edition can set up equipment, missions, starting locations, and other test conditions. A shared codec creates and validates recovery codes. Apple and Android use app-private files. The PWA uses IndexedDB per origin, and Tauri uses the app data folder. The compatibility runner documents how to run the original for comparison and regenerate resources. [Compatibility runner](../../../Compatibility/README.md)

[Recovery codes and checks by platform](../../save-transfer.md) · [Test app and test coverage](../../validation-build.md) · [Display and asset rules](../../design-and-asset-rules.md)
