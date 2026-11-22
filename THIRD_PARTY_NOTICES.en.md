# Third-party code and notices

[한국어](THIRD_PARTY_NOTICES.md)

## Game and build dependencies

| Component | Use and version | Terms and source notices |
| --- | --- | --- |
| OpenAL Soft | 1.22.2. Statically linked for sound effects on Apple and Android. | Most source files use LGPL-2.0-or-later. Some use BSD-3-Clause. See the notices in each file. [Bundled license](ThirdParty/OpenALSoft/COPYING) · [LGPL](LICENSES/OpenAL-Soft-LGPL-2.0.txt) · [BSD](LICENSES/OpenAL-Soft-BSD-3-Clause.txt) · [Pinned source](LICENSES/OpenAL-Soft-source.json) |
| stb_image | 2.28. Reads PNG and JPEG in the shared C++ code. | This project uses the MIT option. Copyright © 2017 Sean Barrett. [License in the source](ThirdParty/stb/stb_image.h) · [Upstream license](https://github.com/nothings/stb/blob/master/LICENSE) |
| Code adapted from MachDefense | Data chunks, random number handling, PCM, sound effects, Apple music playback, and parts of the OpenAL build script. | MIT. [Preserved license](ThirdParty/MachDefense/LICENSE) · [Adapted code and source commit in Korean](docs/source-reuse.md) · [Audio source list](LICENSES/OpenAL-Soft-source.json) |
| Gradle and Wrapper | Android build tooling. Gradle distribution 8.13. The Wrapper JAR includes `META-INF/LICENSE`. | Apache-2.0. [Pinned version and SHA-256](Android/gradle/wrapper/gradle-wrapper.properties) · [Gradle license](https://github.com/gradle/gradle/blob/v8.13.0/LICENSE) · [Distribution NOTICE](https://github.com/gradle/gradle/blob/v8.13.0/subprojects/distributions-full/src/toplevel/NOTICE) |
| Emscripten and musl | Build Wasm and JavaScript for the web and the Tauri web view. The recorded build check used SDK 6.0.13. Web sound effects use the SDK’s OpenAL implementation. | Emscripten uses MIT or University of Illinois/NCSA. See musl’s MIT and per-file notices. [Emscripten license](https://github.com/emscripten-core/emscripten/blob/6.0.13/LICENSE) · [musl COPYRIGHT](https://github.com/emscripten-core/emscripten/blob/6.0.13/system/lib/libc/musl/COPYRIGHT) · [Web link configuration](Web/CMakeLists.txt) |
| Tauri | 2.12.5. Hosts the Mac desktop app. | MIT or Apache-2.0. See each Rust and npm package’s own notices. [MIT](https://github.com/tauri-apps/tauri/blob/tauri-v2.12.5/LICENSE-MIT) · [Apache-2.0](https://github.com/tauri-apps/tauri/blob/tauri-v2.12.5/LICENSE-APACHE-2.0) · [Cargo.lock](Desktop/Tauri/src-tauri/Cargo.lock) · [package-lock.json](Desktop/Tauri/package-lock.json) |

The repository contains the statically linked OpenAL Soft source and the app source. Apple builds use the [OpenAL build script](tools/build_openal.py). Android builds use [CMake](Android/cmake/CMakeLists.txt). For Android relinking, include the [OpenSL resume fix](Android/cmake/opensl-resume.cmake) applied by that build. See [LGPL section 6](LICENSES/OpenAL-Soft-LGPL-2.0.txt) for relinking terms for distributed builds. [Platform build instructions](docs/en/public/build-and-install.md)

## Compatibility runner for original-game comparison

The touchHLE-based runtime and its patches retain their MPL-2.0 notices. SDL2 and `rust-sdl2` retain their respective licenses and copyright notices. The project pins the runtime, SDL bindings, and baseline patch by commit. [Compatibility notices in Korean](Compatibility/THIRD-PARTY-NOTICES.md) · [Pinned commits](Compatibility/runtime.json) · [SDL2 license](https://github.com/libsdl-org/SDL/blob/SDL2/LICENSE.txt) · [rust-sdl2 source and notice](https://github.com/touchHLE/rust-sdl2/tree/b67f98fe6a147773b6f05d50ae775d9fe16561e7)

[Code license scope](LICENSE-SCOPE.en.md) · [Original asset terms](ASSET-LICENSES.en.md) · [Credits](CREDITS.md)
