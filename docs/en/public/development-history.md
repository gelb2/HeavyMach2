# How HeavyMach 2 was restored

[한국어](../../public/development-history.md) · [Restored and added features](features.md) · [Developer’s statement in Korean](../../RESTORATION.md) · [Troubleshooting](troubleshooting.md) · [Testing method](testing-method.md) · [Documentation](index.md)

## Comparing the regular and HD editions

We compared package hashes, game data, weapons, parts, missions, layouts, and artwork across four regular and two HD releases. We used both a compatibility port that runs the original app and the restoration built from new C++ source. The restoration core uses HD 1.0.3 for its game data, while the iPhone menus also reference regular edition 2.0.1. Behavior observed in those releases provided references for each device. [Original version audit](../../ipa-version-audit.md) · [Compatibility port](../../../Compatibility/README.md) · [Original HD functions](../../hd-original-function-audit-20261111.md)

## Restoring continuous play on iPad

The first work focused on connecting the base, battlefield, Command Center, and saves on iPad. The Command Center completion screen, for example, switches among completed missions and saves rewards and item quantities. With the same save and inputs, the original HD and the restoration awarded 80 Cr and then 100 Cr, with matching final credits, EXP, and reward quantities. I accepted and completed missions and checked combat and collection in the regular iPad app. [Command Center comparison](../../command-detail-restoration-20261111.md) · [Player checks on iPad](../../ipad-release-installation-20261111.md)

## iPhone layout and shared features

We adapted the base, shop, and battlefield to the iPhone landscape display. We measured the original phone shop and equipment screens again and used the shared renderer to lay out the artwork, trade buttons, and slots together. [Shop comparison](../../iphone-shop-reference-coordinates-20261119.md) · [iPhone troubleshooting](troubleshooting.md#1-double-tap-dash-fails-after-changing-the-route-on-iphone)

## White Mountain and unused content

We investigated the remaining White Mountain base artwork and six snowfield images from HD 1.0.0. New portals, boss missions, and enemy placements connect Desert 01 with the snowfields. Using the original weapon and sound data, we added ways to get or use 25 weapons, the laser air strike, and Item Picker BOTs in play. The test app on each of the two devices' simulators ran 14 portal directions, four boss missions, and 29 equipment checks. [Original artwork and data](../../original-unused-content-20261119.md) · [Map, boss, and weapon results](../../white-mountain-implementation-20261119.md) · [Boss distance, fonts, and regular app installation](../../white-mountain-dual-release-20261120.md)

I asked whether players could actually reach White Mountain in the original. I had the remaining artwork used to connect the base and battlefields, and asked for bosses to appear farther from the entry portal. [Work I directed](features.md#my-contributions) · [Boss placement fix](troubleshooting.md#5-snowfield-bosses-appear-at-the-portal)

This was my note on the unfinished features.

> (HeavyMach 2 had a few features for which the original developers had already made effects or set values such as attack power. There were also some with only pseudocode and comments, or with just the effects missing. Melee attacks and flamethrowers were examples. I suspect the original developers took some inspiration from Armored Core.)

The investigation confirmed icons and commented-out item rows for both features. It found no linked firing definitions. [Unused weapons and special attacks](../../original-unused-content-20261119.md#무기와-입수-경로)

## Android, web, Mac, and save transfer

Before starting Android, I gave Codex this instruction.

> If the modules are structured properly, building the Android app gives you no reason to touch the files iOS uses. You must build Android without changing any file used by iOS or iPadOS. If a problem comes up, examine the architecture and dependencies between the modules again.

The platforms share C++ modules for game rules, screens, and assets. OS integrations such as touch and audio devices live in the platform hosts. We installed the regular and test APKs on a Galaxy and checked the same iPad save in both apps. The port was merged into `develop` in PR #23. Later work used the same core for regular and test PWAs, a Tauri Mac app, and a recovery code that transfers all slots. Both PWAs installed from Mac Chrome reopened offline with their saves intact, and the iPhone simulator launched the app from the Home Screen. The device records describe the Tauri portrait window and save checks for each host. [Galaxy play](../../../Android/검증-20261120.md) · [PWA and Mac](../../pwa-device-validation-20261120.md) · [Recovery codes](../../save-transfer.md)

## Assets and current checks

We compared how HeavyMach 1, MachDefense, and HeavyMach 2 use artwork and sound effects. HeavyMach 2 on the web and Tauri also uses the shared OpenAL effects path. We extracted 1,359 images and 58 sound files from both web editions and compared them with the Apple and Android packages. Output buffers from combat in the installed Chrome app confirmed the original machine-gun, missile, impact, and explosion sounds. The TODO tracks clipped base titles in portrait on tablets and an unused branch in the shared mobile screens. [Asset comparison](../../code-document-fixes-20261121.md) · [Current checklist](../../todo.md#현재-체크--2026-11-21)

## How I directed the work and tested it

I began restoring HeavyMach 2 without a separate harness to see how the 6.1 Sol High model performed on its own. The model struggled to distinguish the implementation and policy differences between the regular and HD editions and often refused instructions as a result. On November 15, I decided to bring back a small part of the harness first. [Notes from the time](../../harness-adoption-note-20261115.md)

I played the game and reported issues with jumping, the shop, and the snowfield bosses. I asked for the PWA to be checked after launching it from the installed icon. Codex reproduced the inputs and positions I reported and checked the fixes in the test build. I kept separate records of my own play in the regular edition. [Symptoms, causes, fixes, and checks](troubleshooting.md) · [Player captures](gameplay-gallery.md)

## Code structure

Resources read from the HD and regular editions go through the preparation tool into the same campaign directory. The C++ core reads the original tables and maps, assembles the mech, and runs combat, missions, buttons, and saves. Each host connects display, input, audio, and storage to its platform. [Local resource preparation in Korean](../../public/build-and-install.md#원본-자원-준비) · [Technology and original assets](../../technology-and-assets.md) · [Code and data locations](architecture.md)

| Host | Display, input, and storage |
| --- | --- |
| [Apple](../../../Sources/Platform/Apple) | The Objective-C++ host connects touch and OpenGL ES. Original WAV effects use OpenAL and background music uses AudioQueue. |
| [Android](../../../Sources/Platform/Android) | The Kotlin Activity and JNI pass input to the shared NDK core. The app uses EGL and GLES, OpenAL, and app-private storage. |
| [Web/PWA](../../../Sources/Platform/Web) | Emscripten builds the core to Wasm. Rendering and effects use WebGL 2 and OpenAL. The web host handles installation caching and IndexedDB. |
| [Tauri Mac](../../../Desktop/Tauri/README.md) | The app bundles the same verified web files. The Rust host writes progress, pre-restore backups, and HUD layouts to the Mac app data folder. |
