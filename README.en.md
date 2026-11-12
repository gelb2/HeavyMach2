# HeavyMach 2 Restoration

[한국어](README.md) | [English](README.en.md)

## Why I am restoring HeavyMach 2

HeavyMach influenced my decision to become a developer. I encountered it on an iPod touch in college and later created a US App Store account to buy it. Now that I am a developer, I wanted to make the game that once showed me a direction work again.

After restoring HeavyMach 1, I am continuing with HeavyMach 2, another work in the series by developer Byun Hae-jun and illustrator Park Jae-cheol. This project aims to preserve the original game and illustrations and make them playable on current devices.

I have no intention of making money from this restoration. I also want to record how the game was restored and what was fixed.

**[Developer's statement: translated excerpts](docs/en/RESTORATION.md)** · [Korean original](docs/RESTORATION.md)

## Project overview

This project restores HeavyMach 2 HD for current iPads. The original executable and game data are analyzed to implement movement, combat, equipment, missions, menus, audio and saves in a shared C++ core. Korean is the game's default language.

**The regular iPad build is currently playable.** The user confirmed movement, combat, drawing movement paths, accepting quests and completing quests near the first mission. They reported reaching Wasteland without encountering a crash up to that point. Full feature restoration and the final comparison with the original are still in progress. [Play records](docs/ipad-release-installation-20261111.md) · [Verified scope and remaining issues](docs/public/validation.md)

[How to play](docs/public/quick-play.md) · [Build and installation](docs/public/build-and-install.md) · [Using the validation app](docs/public/build-and-install.md#치트-앱-실행과-결과) · [TODO](docs/todo.md) — these detailed guides are currently in Korean.

## Playing and installing

Open **HM2 Source** on an iPad in portrait orientation. Tap the title screen and select a save slot. Enter a name for a new slot, or continue an existing one. Accept missions at a base and enter the field. Tap the ground to move or draw a route with your finger. Tap an enemy to select it; equipped weapons attack when their conditions are met.

There is currently no App Store or TestFlight release, public app download, or browser version. To install the game, obtain access to this private repository, prepare the original resources locally, and build and sign it in Xcode with your own development team. **Cloning the repository alone does not provide the game resources.** [Original resources and preparation](docs/public/build-and-install.md#원본-자원-준비)

| Target | Current status |
| --- | --- |
| Regular iPad app | A Release build was installed and launched on a physical device. User play has been reported through Wasteland. The current layout is portrait. |
| iPad validation app | A separate app runs 115 scenarios on the same frame clock. A 1,200-frame run and report output were verified in the simulator. |
| iPhone | A universal target and landscape settings exist. Current HD validation uses iPad; final iPhone presentation and gameplay validation remain. |
| Android, web/PWA, Mac and Tauri | Planned follow-up targets. This repository does not currently provide their game app build or installation paths. |

### Regular, validation and reference apps

| App | Purpose | Xcode scheme |
| --- | --- | --- |
| HM2 Source | Regular gameplay with normal progression and the user's save. Build the regular app in Release configuration. | `HeavyMach2Source` |
| HM2 Validation | A cheat validation app that prepares experience, level, equipment, item and progression conditions and runs multiple game states concurrently. | `HeavyMach2Validation` |
| HD reference app | Runs the original HD executable through a compatibility runtime. It has a separate app identity and saves. | [Compatibility guide](Compatibility/README.md) |

The regular and validation apps share the game core and use separate app IDs and save storage. The validation app runs automated checks and writes results; it currently has no freely controllable cheat menu. Both app icons use the original tap-to-start illustration, with `TEST` added only to the validation icon. [App icons](docs/app-icons.md)

## Preparing from source

Use an Apple Silicon Mac, Xcode, Python 3.11 or later, CMake 3.20 or later, and XcodeGen. Repository access and a local HD 1.0.3 original are required.

```sh
git clone git@github.com:gelb2/HeavyMach2.git
cd HeavyMach2
git config core.hooksPath .githooks
```

Check the local path and SHA-256 in the [source manifest](docs/source-provenance.json), then prepare the resources, icons and OpenAL library for the selected SDK.

```sh
python3 tools/prepare_hd_assets.py
python3 tools/prepare_app_icons.py
python3 tools/build_openal.py --sdk iphonesimulator
xcodegen generate --spec Apple/iOS/project.yml
open Apple/iOS/HeavyMach2.xcodeproj
```

In Xcode, select `HeavyMach2Source` and an iPad simulator to run it. For a physical iPad, prepare OpenAL for `iphoneos` and configure the development team, signing and device connection. Back up the current app's data container before updating. [Simulator commands, device installation and troubleshooting](docs/public/build-and-install.md)

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
- [Regular app installation and user play](docs/ipad-release-installation-20261111.md) · [Latest icon and concurrent-run records](docs/evidence/title-illustration-icons-20261112.json)
- [Report site and publication list](docs/report-site.md) — uses the existing private report site and requires the owner's account login.
- [HeavyMach 1 early-history review](docs/heavymach1-history-review.md) · [Reference-app validation plan](docs/reference-port-validation-plan.md) · [Model usage history](docs/model-usage-history.md)

These detailed records are currently in Korean.

## Repository and resources

This repository remains private. HeavyMach 1 and MachDefense are read-only references for procedures and evidence; this project does not write source, build output or saves into those repositories. Work branches merge through `develop` into `main`.

Original IPAs, executables, user saves and signing material are neither committed nor sent to external services. Prepared original resources and builds stay in ignored local storage. Code licensing does not grant rights to the original artwork, audio or game data. See the [reuse notices](docs/source-reuse.md) and the individual `ThirdParty` sources for external-code terms. Sound provenance and rights research remain in the [TODO](docs/todo.md).

[Working instructions](AGENTS.md)
