# HeavyMach 2 Restoration

[한국어](README.md) | [English](README.en.md)

## Why I am restoring HeavyMach 2

HeavyMach influenced my decision to become a developer. I encountered it on an iPod touch in college and later created a US App Store account to buy it. Now that I am a developer, I wanted to make the game that once showed me a direction work again.

After restoring HeavyMach 1, I am continuing with HeavyMach 2, another work in the series by developer Byun Hae-jun and illustrator Park Jae-cheol. This project aims to preserve the original game and illustrations and make them playable on current devices.

I have no intention of making money from this restoration. I also want to record how the game was restored and what was fixed.

**[Developer's statement: translated excerpts](docs/en/RESTORATION.md)** · [Korean original](docs/RESTORATION.md)

## About the project

HeavyMach 2 restores the original game's movement, combat, equipment, missions, menus, audio and saves from its executable, data and observed behavior. The shared C++ core runs on iPhone and iPad, Android, the installed PWA, and the Tauri Mac app. [Current project documentation](docs/canonical.md) · [Play and transfer saves](docs/public/quick-play.md)

## Play and install

| Platform | Installation and verification |
| --- | --- |
| iPhone and iPad | Landscape on iPhone and portrait on iPad. White Mountain entry and exit were checked in the regular builds on both physical devices. [Build and install](docs/public/build-and-install.md) · [Device record](docs/white-mountain-dual-release-20261120.md) |
| Android | The regular and test APKs ran on a Galaxy, with shop, gameplay and the iPad save checked. [Build and install](Android/README.md) · [Device record](Android/검증-20261120.md) |
| PWA | Installed launch was checked on Mac Chrome and in the iPhone simulator. [Build and install](Web/README.md) · [Verification by device](docs/pwa-device-validation-20261120.md) |
| Tauri Mac | Separate regular and test app bundles. [Build the Mac app](Desktop/Tauri/README.md) |

Building requires access to this private repository and the local original game packages. See [verification status](docs/public/validation.md) for distribution and device coverage. The regular and test editions use the same core and original title illustration with separate saves. Test controls differ by platform. The [compatibility app](Compatibility/README.md) runs separately for comparison. [Test editions](docs/validation-build.md#플랫폼별-치트-실행) · [App icons](docs/app-icons.md)

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

## Implemented paths and remaining work

The shared code connects the title screen, save slots, bases, shops, maintenance, inventory, missions, help, field movement, attacks, items, BOTs, game over and saves. Individual checks and full-game validation have different scopes.

After the icon update on November 12, 2026, the iPad simulator validation app completed 115 scenarios over 1,200 frames, emitted shots from 110 weapon definitions, and observed up to 26 simultaneously progressing effects with one effect context. There were no resource rereads or image reuploads after warm-up. These are measurements from that run, not a full-game pass rate. [Run manifest](docs/evidence/title-illustration-icons-20261112.json)

Missing completion sounds and toasts for an E-10 destruction mission, and suspected item grants missing from collection quests, need follow-up. All missions and the full campaign, movement for 23 lower-body equipment types, all BOT outcomes, OS audio interruptions and route changes, and the final comparison with the HD reference app also remain. [Validation scope](docs/public/validation.md) · [Detailed TODO](docs/todo.md)

## Implementation and restoration rules

The C++20 core shares the original-format readers, part assembly, game rules and UI. The Apple host uses Objective-C++; game graphics use OpenGL ES, effects use OpenAL Soft, and BGM uses AudioQueue. The original source code has not been obtained. New source is written from evidence in the executable, data and observed behavior.

Firing does not create a file/database write or a new audio context for each shot. Original and new parts follow the shared data, assembly and rendering paths. Ads are neither displayed nor requested.

[Technology and asset procedures](docs/technology-and-assets.md) · [Runtime, saves and audio](docs/runtime-contract.md) · [Reused code sources](docs/source-reuse.md)

## Development records

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
