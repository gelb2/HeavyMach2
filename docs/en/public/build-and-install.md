# Build and install HeavyMach 2

[한국어](../../public/build-and-install.md) · [Game overview](../../../README.en.md)

## Play the published builds

- **PWA:** Open the public game. In Safari on iPhone or iPad, choose **Share → Add to Home Screen** and turn on **Open as Web App**. In Android Chrome, choose **Install game** or the browser’s app installation command. In Mac Safari, choose **File → Add to Dock**. Open the app icon and wait for the first download to finish before playing offline. Play in landscape on a phone and in portrait on iPad. [Open the PWA](https://gelb2.github.io/HeavyMach2/)
- **Android:** Download the regular or test APK from `v0.1`. You can also install the package using `adb install`. Existing installations can be updated with a matching signature. [Android downloads](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1)
- **Mac:** Download the `v0.1` ZIP for Apple Silicon, check `SHA256SUMS.txt`, and move the extracted app to Applications. The app requires macOS 14 or later. [Mac ZIP](https://github.com/gelb2/HeavyMach2/releases/download/v0.1/HeavyMach2-Tauri-Mac-v0.1-arm64.zip) · [First launch on macOS](https://support.apple.com/102445)

To move progress between platforms, use the **Transfer Save** button on the slot screen. Importing a code replaces the receiving app’s three slots after confirmation. [Save transfer instructions](../../save-transfer.md)

## Clone and prepare the resources

Use Git and Python 3.11 or later. An Apple Silicon Mac with Xcode can build all four targets. Run these commands from a new directory:

```sh
git clone https://github.com/gelb2/HeavyMach2.git
cd HeavyMach2
python3 tools/prepare_build_resources.py
```

The script downloads about 88 MiB from the pinned public PWA build, verifies the two files by SHA-256, and prepares the campaign, maps, images, sound, and app icons. The playable build does not require an original IPA, a private path, or a Git LFS download. For Android-only builds use `python3 tools/prepare_build_resources.py --campaign-only` to skip Apple icon tooling. Original-package comparison and resource regeneration use a separate workflow. [Build inputs](../../public/build-and-install.md#공개-빌드에서-자원-준비) · [Code and asset license scope](../../../LICENSE-SCOPE.en.md)

## iPhone and iPad

Install Xcode, an iOS Simulator runtime, and CMake 3.20 or later. Build OpenAL for the simulator and open the included Xcode project.

```sh
python3 tools/build_openal.py --sdk iphonesimulator
open Apple/iOS/HeavyMach2.xcodeproj
```

Select `HeavyMach2Source` or `HeavyMach2Validation`, choose an iPhone or iPad simulator, and press Run. For a physical device build OpenAL with `--sdk iphoneos`, select your own development team, and enable Developer Mode on that device. The project’s deployment target is iOS/iPadOS 15.0. [Command-line builds, device signing, and test output](../../public/build-and-install.md#iphoneipad-시뮬레이터-빌드)

## Android

Install JDK 17, SDK 36, NDK `28.2.13676358`, CMake `3.22.1`, and SDK Platform-Tools. Set `ANDROID_HOME` to the SDK directory and add its `platform-tools` directory to `PATH`. On macOS, `export JAVA_HOME="$(/usr/libexec/java_home -v 17)"` selects the installed JDK 17.

```sh
cd Android
./gradlew :app:assembleNormalDebug :app:assembleCheatDebug
adb devices -l
adb -s DEVICE_SERIAL install -r app/build/outputs/apk/normal/debug/app-normal-debug.apk
adb -s DEVICE_SERIAL install -r app/build/outputs/apk/cheat/debug/app-cheat-debug.apk
```

Replace `DEVICE_SERIAL` with the emulator or device ID. Open **HeavyMach 2** or **HeavyMach 2 Test**. The apps use separate saves. For a signed distribution build use `:app:assembleNormalRelease` with your own signing key. [Android build and signing details](../../../Android/README.md)

## Build and run the PWA

Activate the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) on macOS. Use CMake 3.20 or later and run these commands from the repository root:

```sh
emcmake cmake -S Web -B build/web-normal -DCMAKE_BUILD_TYPE=Release
cmake --build build/web-normal --parallel 6
emcmake cmake -S Web -B build/web-validation -DCMAKE_BUILD_TYPE=Release -DHM2_WEB_VALIDATION=ON
cmake --build build/web-validation --parallel 6
python3 -m http.server 8773 --bind 127.0.0.1 --directory build
```

Open the [local regular build](http://127.0.0.1:8773/web-normal/dist/) or the [local test build](http://127.0.0.1:8773/web-validation/dist/). Install from Chrome or Safari, then open the app icon. The loopback address works on the build Mac. Use HTTPS for installation on another device. [Web build and verification](../../../Web/README.md)

## Build the Mac app

Build the two web editions above. Stop the HTTP server with Control-C. With Node.js, Rust and the Xcode command-line tools installed, run:

```sh
cd Desktop/Tauri
npm ci
npm run build
npm run build:validation
open src-tauri/target/release/bundle/macos
```

Open **HeavyMach 2 Desktop.app** or **HeavyMach 2 Desktop QA.app**. The bundles contain the web files and run without a local server. The regular and test apps keep separate saves. [Mac build and save locations](../../../Desktop/Tauri/README.md)
