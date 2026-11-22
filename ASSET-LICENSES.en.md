# Artwork, audio, and game data

[한국어](ASSET-LICENSES.md)

Byun Hae-jun developed the original game, and Park Jae-cheol created its artwork. Rights in the original assets and material that incorporates them remain with the respective rights holders. The [MIT License](LICENSE) covers new restoration code. The repository contains no separate record of permission to publish or redistribute the original assets. [Credits](CREDITS.md) · [Code license scope](LICENSE-SCOPE.en.md)

| Material | Sources and terms |
| --- | --- |
| Original artwork, music, sound effects, maps, and game data | The project uses assets from the original HD 1.0.3 release and comparison records from the regular 2.0.1 release. The original IPA and executable are kept locally for comparison. [Versions and hashes](docs/source-provenance.json) · [Developer’s statement](docs/en/heavymach2-restoration.md) |
| Assets retrieved from the public build | The build preparation tool extracts campaign assets from `game.js` and `game.data` in the pinned `v0.2` PWA. Assets available from the public app remain subject to the respective rights holders’ terms. [Build resources](docs/en/public/build-and-install.md#clone-and-prepare-the-resources) · [Downloads and SHA-256](tools/prepare_build_resources.py) |
| Modified game assets | These include campaign additions and exit artwork in `Resources/Campaign` and shop graphics in `Resources/PhoneShopArt`. The original assets and artwork adapted from them have terms separate from the code license. [What the restoration adds](docs/en/public/features.md) · [Asset preparation and sources in Korean](docs/technology-and-assets.md) |
| App icons and social preview | The repository social preview uses the iPad title screen. The platform icons use the title artwork. Both include artwork from the original. [Social preview](Web/Shell/social-preview.jpg) · [Icon sources in Korean](docs/app-icons.md) |
| Screenshots, GIFs, and video | `docs/public/media` contains gameplay captured on physical devices and simulators. Rights in the game’s artwork, on-screen material, and audio still apply. [Devices, scenes, and capture details](docs/en/public/gameplay-gallery.md) |

See the [installation guide](docs/en/public/build-and-install.md) to build and play. For reuse in another product or redistribution of game assets or adaptations, check the required permission with the respective rights holders. [Third-party notices](THIRD_PARTY_NOTICES.en.md)
