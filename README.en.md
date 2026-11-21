# HeavyMach 2 Restoration

[한국어](README.md) | [English](README.en.md)

[Play](docs/public/quick-play.md) · [Installation](#play-and-install) · [Features](#restored-from-the-original) · [Developer's statement](docs/RESTORATION.md) · [Documentation](docs/README.md)

## Why I am restoring HeavyMach 2

HeavyMach influenced my decision to become a developer. I encountered it on an iPod touch in college and later created a US App Store account to buy it. Now that I am a developer, I wanted to make the game that once showed me a direction work again.

After restoring HeavyMach 1, I am continuing with HeavyMach 2, another work in the series by developer Byun Hae-jun and illustrator Park Jae-cheol. This project aims to preserve the original game and illustrations and make them playable on current devices.

I have no intention of making money from this restoration. I also want to record how the game was restored and what was fixed.

**[HeavyMach 2 developer's statement draft in Korean](docs/RESTORATION.md)** · [English excerpts on the earlier restorations](docs/en/RESTORATION.md)

## About the project

In HeavyMach 2, you assemble a mech, draw a route across the battlefield and fight along it. You can take missions at the command center and change equipment using loot and rewards. The restoration follows the original data and observed behavior. A shared C++ core runs on iPhone and iPad, Android, the installed PWA, and the Tauri Mac app. [How to play](docs/public/quick-play.md) · [Project documentation](docs/canonical.md)

## Play and install

| Platform | Installation and verification |
| --- | --- |
| iPhone and iPad | Landscape on iPhone and portrait on iPad. Both physical devices received the regular app and the save recording base visits. I entered White Mountain on the iPhone. All 14 portal directions were checked in simulators and host tests. [Build and install](docs/public/build-and-install.md) · [Device record](docs/white-mountain-dual-release-20261120.md) |
| Android | Both Galaxy editions used the same iPad save. The shop, command, battle and a short play session were checked. The Android port is in `develop`. [Build and install](Android/README.md) · [Device record](Android/검증-20261120.md) |
| PWA | Both Mac Chrome editions reopened offline and retained their saves. The iPhone simulator launched the installed editions from the Home Screen. A device-accessible install address and physical mobile checks are pending. [Build and install](Web/README.md) · [Device checks](docs/pwa-device-validation-20261120.md) |
| Tauri Mac | Separate regular and test bundles passed local signature verification. Earlier builds launched in an 820 × 900 pt portrait window, and the regular edition passed a save round trip. [Build the Mac app](Desktop/Tauri/README.md) · [Portrait window check](docs/pwa-device-validation-20261120.md#tauri-mac-세로-창) |

Building requires access to this private repository and local copies of the original game packages. The recovery code, PWA and Tauri implementations are on the working branch. The regular and test editions share the core and original title artwork and keep separate saves. Test controls differ by platform. [Verification status](docs/todo.md#현재-체크--2026-11-21) · [Test editions](docs/validation-build.md#플랫폼별-치트-실행) · [Reference app](Compatibility/README.md)

## Preparing from source

Use an Apple Silicon Mac, Xcode, Python 3.11 or later, CMake 3.20 or later, and XcodeGen. Repository access and local copies of HD 1.0.3, HD 1.0.0, and iPhone 2.0.1 are required.

```sh
git clone git@github.com:gelb2/HeavyMach2.git
cd HeavyMach2
git config core.hooksPath .githooks
```

Check the local path and SHA-256 in the [source manifest](docs/source-provenance.json), then prepare the resources, icons and OpenAL library for the selected SDK.

```sh
python3 tools/prepare_hd_assets.py
python3 tools/prepare_phone_menu_assets.py
python3 tools/prepare_campaign_assets.py
python3 tools/prepare_app_icons.py
python3 tools/build_openal.py --sdk iphonesimulator
xcodegen generate --spec Apple/iOS/project.yml
open Apple/iOS/HeavyMach2.xcodeproj
```

In Xcode, select `HeavyMach2Source` and an iPad simulator to run it. For a physical iPhone or iPad, prepare OpenAL for `iphoneos` and configure the development team, signing and device connection. Back up the current app's data container before updating. [Simulator commands, device installation and troubleshooting](docs/public/build-and-install.md)

## Restored from the original

Tap the ground to move, draw a path, or double-tap to dash using booster power. The original data supplies the attachment points for the body, lower body and weapons. Combat, BOTs, equipment, purchases, missions and rewards follow the HD rules. The standard iPhone edition is also a reference for phone menu coordinates and artwork. [Original features and evidence](docs/public/features.md#원작에서-복원한-것)

The game uses the original base, battlefield and equipment art and reuses prepared textures for the parts and their shadows. The original WAV samples play through OpenAL. [Graphics and audio restoration](docs/graphics-audio-fidelity-20261110.md)

## My contributions

**White Mountain.** I asked for a campaign that uses the six unused snow illustrations and the existing base artwork. I specified that bosses should be away from portals and asked for more varied enemy groups. I also directed the work to give 25 existing weapons an acquisition path and to make the laser air strike and collector BOTs usable in normal play. [White Mountain and unused content](docs/public/features.md#화이트마운틴에-실제로-들어갈-수-있게-만들었다)

**Shop and iPhone movement.** I asked to measure the original shop's controls, slots and item details again. I reported failed jumps after drawing a new route, Skyway layer jitter, apparent empty inventory slots, and a missing laser air strike in the iPad shop. [Fixes and tests](docs/public/features.md#iphone에서-본-화면과-입력-문제를-고쳤다)

**Android, PWA and Mac.** I asked the Android app to import the shared modules while preserving the files used by Apple apps. Each platform handles its system connections. The new recovery code transfers progress and equipment across all three save slots. I set the scope and reported defects, and worked with Codex on implementation and automated checks. [Platform development](docs/public/development-history.md#android웹mac과-저장-이동) · [Transfer saves](docs/save-transfer.md)

## Gameplay screenshots and clips

Changing a route in Ruin Field in an iPhone Air simulator recording. [MP4 · 6.1 seconds](docs/public/media/gameplay-20261121/iphone-route.mp4)

![Landscape gameplay on iPhone showing the route and HUD](docs/public/media/gameplay-20261121/iphone-route.gif)

Boss 05 fires in SnowBasin during a mission check in the iPhone simulator's test app.

![White Mountain Boss 05 in combat beside the Snow Pass portal](docs/public/media/gameplay-20261121/snow-boss.png)

[iPad portrait footage, iPhone shop, Galaxy play and installed PWA captures](docs/public/gameplay-gallery.md)

## Verified behavior and remaining work

Each iPhone and iPad simulator test run covered 126 scenarios over 2,100 frames. Both simulators also checked 14 White Mountain portal directions, four boss missions and 29 equipment scenarios. White Mountain routes and command on a physical iPad, installed PWA on mobile devices, direct interaction in the latest Tauri portrait builds, E-10 completion notifications, collection rewards and the final HD comparison remain open. [Evidence by feature](docs/feature-verification.md) · [Current TODO](docs/todo.md#현재-체크--2026-11-21)

## Implementation and restoration rules

The C++20 core shares the original-format readers, part assembly, game rules and UI. The Apple host uses Objective-C++; game graphics use OpenGL ES, effects use OpenAL Soft, and BGM uses AudioQueue. The original source code has not been obtained. New source is written from evidence in the executable, data and observed behavior.

Firing does not create a file/database write or a new audio context for each shot. Original and new parts follow the shared data, assembly and rendering paths. Ads are neither displayed nor requested.

[Technology and asset procedures](docs/technology-and-assets.md) · [Runtime, saves and audio](docs/runtime-contract.md) · [Reused code sources](docs/source-reuse.md)

## Development records

- [Development history](docs/public/development-history.md) · [Troubleshooting in Korean](docs/public/troubleshooting.md) · [Original and added features](docs/public/features.md) · [Writing source index](docs/developer-background.md)
- [How the work was directed and tested](docs/public/development-history.md#내가-일을-시키고-테스트한-방법) · [Code structure](docs/public/development-history.md#코드-구조)
- [Restoration plan](docs/restoration-plan.md) · [Standard and HD edition differences](docs/original-phone-hd-separation.md) · [Six-version audit](docs/ipa-version-audit.md)
- [Validation app and work order](docs/validation-build.md) · [Full-feature validation plan](docs/full-validation-development-plan-20261110.md) · [HD functions and restoration scope](docs/hd-original-function-audit-20261111.md)
- [Regular app installation and user play](docs/ipad-release-installation-20261111.md) · [Apple icons and concurrent run on November 12, 2026](docs/evidence/title-illustration-icons-20261112.json)
- [Report site and publication list](docs/report-site.md) — uses the existing private report site and requires the owner's account login.
- [HeavyMach 1 early-history review](docs/heavymach1-history-review.md) · [Reference-app validation plan](docs/reference-port-validation-plan.md) · [Model usage history](docs/model-usage-history.md)

These detailed records are currently in Korean.

## Repository and resources

This repository remains private. HeavyMach 1 and MachDefense are read-only references for procedures and evidence; this project does not write source, build output or saves into those repositories. Work branches merge through `develop` into `main`.

Original IPAs, executables, user saves and signing material are neither committed nor sent to external services. Prepared original resources and builds stay in ignored local storage. Code licensing does not grant rights to the original artwork, audio or game data. See the [reuse notices](docs/source-reuse.md) and the individual `ThirdParty` sources for external-code terms. Sound provenance and rights research remain in the [TODO](docs/todo.md).

[Working instructions](AGENTS.md)
