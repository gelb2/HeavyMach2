# HeavyMach 2 빌드와 설치

[English](../en/public/build-and-install.md) · [README](../../README.md) · [플레이 방법](quick-play.md) · [확인한 범위](validation.md)

| 플랫폼 | 설치·실행 | 소스에서 빌드 |
| --- | --- | --- |
| PWA | [홈 화면·앱 목록에 설치](#pwa-설치) | [Wasm 빌드와 로컬 실행](#pwa-빌드와-로컬-실행) |
| iPhone·iPad | [Xcode에서 실제 기기에 설치](#실제-iphoneipad에-설치) | [정식판·치트판과 시뮬레이터](#iphoneipad-시뮬레이터-빌드) |
| Android | [갤럭시에 APK 설치](#android-빌드와-설치) | [SDK·Gradle·APK 경로](#android-빌드와-설치) |
| Tauri Mac | [기존 맥 앱 가져와 실행](#tauri-mac-앱-받기) | [웹 파일로 Mac 앱 빌드](#tauri-mac-빌드) |

## PWA 설치

iPhone·iPad·갤럭시·Mac에서 공개 HTTPS 설치 주소를 연다. 빌드한 맥은 로컬 `127.0.0.1` 주소도 쓸 수 있다. [HeavyMach 2 PWA](https://gelb2.github.io/HeavyMach2/) · [이 맥의 기존 웹 빌드 열기](../../Web/README.md#이-맥의-기존-웹-빌드-열기)

1. 설치 주소를 연다. 게임 화면에서 **게임 설치**가 보이면 눌러 브라우저 안내를 따른다. **설치 방법**이 보이면 다음 메뉴를 사용한다.

   | 기기·브라우저 | 설치 메뉴 |
   | --- | --- |
   | iPhone·iPad Safari | **공유 → 홈 화면에 추가**. **웹 앱으로 열기**가 보이면 켜고 **추가**를 누른다. |
   | 갤럭시 Chrome | **⋮ → 홈 화면에 추가 또는 앱 설치 → 설치**. 설치 확인이 나오면 승인한다. |
   | Mac Chrome·Edge | 주소창의 설치 아이콘이나 브라우저 메뉴의 웹 앱 설치를 선택한다. |
   | Mac Safari | **파일 → Dock에 추가**를 선택한다. |

2. 홈 화면, 앱 목록이나 Dock에 생긴 **HeavyMach 2** 아이콘을 열고 **게임 시작**을 누른다. 휴대폰은 가로, 태블릿은 세로로 연다.
3. 첫 실행에는 네트워크를 유지하고 에셋 다운로드와 검증이 끝날 때까지 기다린다. 진행 막대는 검증을 마친 용량을 표시한다. 실패하면 **다시 시도**를 누른다. 오프라인 실행과 저장 유지는 Mac Chrome 설치판에서 확인했다. [iPhone Safari](https://support.apple.com/guide/iphone/open-as-web-app-iphea86e5236/ios) · [Android Chrome](https://support.google.com/chrome/answer/9658361?co=GENIE.Platform%3DAndroid) · [Mac Safari](https://support.apple.com/104996) · [설치 확인 기록](../pwa-device-validation-20261120.md)

진행을 옮기려면 슬롯 화면의 **저장 이동**에서 원래 앱의 복구코드를 만들고 설치한 앱에 붙여넣는다. 복원하면 받을 앱의 슬롯 세 개가 교체된다. [복구코드와 백업 방법](../save-transfer.md)

## 준비물

- Git과 Python 3.11 이상. 처음 빌드 자원을 준비할 때는 약 88 MiB를 내려받는다.
- 공통 자원 준비에는 Python 3.11 이상과 C++ 컴파일러가 필요하다. 공통 코어와 웹을 빌드할 때는 CMake 3.20 이상을 사용한다.
- iPhone·iPad는 Apple Silicon Mac, iOS SDK와 시뮬레이터 런타임을 설치한 Xcode, XcodeGen이 필요하다. Android·PWA·Tauri의 도구는 아래 플랫폼별 빌드 절에 적었다.
- 실기기 설치에는 본인의 Apple 개발 팀, 연결된 iPhone 또는 iPad와 개발자 모드.

2026년 11월 13일 문서 확인 환경은 Xcode 27.1, XcodeGen 2.45.4, CMake 4.4.4, Python 3.14.6이다. 앱 설정의 최소 OS는 iOS/iPadOS 15.0이며, Apple 자동검사 타깃은 17.0이다. 최소 OS별 실제 플레이 검증은 남아 있다. 현재 Apple 빌드는 arm64를 대상으로 한다.

## 소스 받기

```sh
git clone https://github.com/gelb2/HeavyMach2.git
cd HeavyMach2
python3 tools/prepare_build_resources.py
```

공개 클론에서 바로 빌드하려면 기본 브랜치 `main`을 사용한다. Android 정식 APK는 `v0.4`, Mac ZIP과 Android 치트 APK는 `v0.1`, 설치형 PWA는 `v0.3`에 있다. [버전과 배포 기준](release-policy.md)

## 공개 빌드에서 자원 준비

`prepare_build_resources.py`는 이미 공개한 `v0.2` PWA의 고정 버전에서 `game.js`와 `game.data`를 내려받는다. 두 파일의 크기와 SHA-256을 검사해 캠페인 파일 1,576개를 `.local/prepared/Campaign/HD/`에 준비한다. iOS와 웹에 필요한 정식·치트 아이콘은 캠페인의 타이틀 그림으로 만든다. Android만 빌드할 때는 `python3 tools/prepare_build_resources.py --campaign-only`를 사용하면 Apple 아이콘 도구가 필요 없다.

소스에서 실제 플레이 빌드를 만드는 데 필요한 원작 자원은 이 경로로 준비할 수 있다. 같은 명령을 다시 실행하면 받은 파일을 다시 검사해 재사용한다. 다른 준비 자원이 이미 들어 있는 클론이라면 기존 파일을 확인하고, 공개 빌드와 대조할 때는 새 클론을 사용한다. [코드와 원작 자원의 라이선스 범위](../../LICENSE-SCOPE.md)

## 원본 자원 준비

다음 절차는 원작과 대조하거나 콘텐츠를 다시 준비할 때 사용한다. 일반 빌드는 위의 공개 자원 준비 절차를 따른다. [source-provenance.json](../source-provenance.json)의 `source_root`와 `relative_path`로 원본 파일 위치를 확인한다. 다른 Mac에서는 보유한 원본 경로에 맞게 `source_root`를 로컬에서 설정한다. 원본 해시와 판본은 바꾸지 않는다.

도구가 고정한 HD 1.0.3 IPA의 SHA-256은 다음과 같다.

```text
7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2
```

원본은 저장소 밖에 두고 다음 명령을 저장소 루트에서 실행한다.

```sh
python3 tools/prepare_hd_assets.py
python3 tools/prepare_phone_menu_assets.py
python3 tools/prepare_campaign_assets.py
python3 tools/prepare_app_icons.py
```

첫 명령은 원본 해시를 검사하고 허용된 자원을 `.local/prepared/HD/`에 바이트 그대로 준비한다. 파일별 크기·SHA-256은 그 안의 `manifest.json`에 기록한다. 현재 준비 명세는 자원 1,479개다. 실행파일은 포함하지 않는다.

확장 자원 준비는 원본을 수정하지 않고 HD·Phone 자원을 `.local/prepared/Campaign/HD`에 따로 만들어 Apple 앱에서 사용한다. 원본 명세의 HD 1.0.0도 필요하다.

마지막 명령은 탭투스타트의 `Main_BG.png`로 `.local/prepared/AppIcons.xcassets/`를 만든다. 일반 아이콘은 원본 일러스트의 중앙 정사각형 영역을 사용하고 치트 아이콘에는 `TEST`를 표시한다. [아이콘 생성 방식](../app-icons.md)

준비 자원과 아이콘은 Git 추적에서 제외한 `.local/` 아래에 둔다. 원본 IPA와 원작 포함 앱은 로컬에 보관한다. 준비 도구가 `Existing prepared resource differs`로 중단하면 기존 파일과 해시 차이의 원인을 확인하고, 보존한 자원을 임의로 덮어쓰지 않는다.

## PWA 빌드와 로컬 실행

Emscripten SDK를 활성화하고 [공통 자원과 아이콘](#공개-빌드에서-자원-준비)을 준비한다. 기존 검증에는 Emscripten 6.0.13을 사용했다. 저장소 루트에서 빌드한다. [Emscripten 설치 안내](https://emscripten.org/docs/getting_started/downloads.html) · [웹 빌드 세부 사항](../../Web/README.md)

```sh
export EM_CACHE="$PWD/.local/emscripten-cache-hm2"
emcmake cmake -S Web -B build/web-normal -DCMAKE_BUILD_TYPE=Release
cmake --build build/web-normal --parallel 6
emcmake cmake -S Web -B build/web-validation -DCMAKE_BUILD_TYPE=Release -DHM2_WEB_VALIDATION=ON
cmake --build build/web-validation --parallel 6
```

PWA를 이 맥에서 설치해 실행할 때는 서버를 시작한다.

```sh
python3 -m http.server 8773 --bind 127.0.0.1 --directory build
```

브라우저에서 [정식판](http://127.0.0.1:8773/web-normal/dist/) 또는 [치트판](http://127.0.0.1:8773/web-validation/dist/)을 연다. 산출물은 각 빌드 폴더의 `dist/` 전체다. [이 맥의 기존 웹 빌드 열기](../../Web/README.md#이-맥의-기존-웹-빌드-열기) · [실기기용 HTTPS 주소 준비](../../Web/README.md#실기기-설치-주소)

## iPhone·iPad 시뮬레이터 빌드

캠페인 자원과 아이콘을 준비한 뒤 시뮬레이터용 OpenAL을 빌드한다. Xcode 프로젝트는 저장소에 포함돼 있다. XcodeGen 명령은 `project.yml`을 바꿨거나 Xcode 프로젝트를 다시 생성할 때 실행한다.

```sh
python3 tools/build_openal.py --sdk iphonesimulator
xcodegen generate --spec Apple/iOS/project.yml
xcodebuild -project Apple/iOS/HeavyMach2.xcodeproj \
  -scheme HeavyMach2Source -configuration Release \
  -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath build/ios-simulator \
  ARCHS=arm64 ONLY_ACTIVE_ARCH=YES CODE_SIGNING_ALLOWED=NO build
```

일반 앱의 산출물은 `build/ios-simulator/Build/Products/Release-iphonesimulator/HeavyMach2Source.app`이다. 치트 앱은 같은 명령에서 scheme을 `HeavyMach2Validation`으로 바꾸며 산출물도 `HeavyMach2Validation.app`이다. 같은 DerivedData 디렉터리를 재사용한다.

Xcode에서 검증할 시뮬레이터 한 대를 부팅하고 ID를 확인한다.

```sh
xcrun simctl list devices booted
```

아래 `SIMULATOR_UUID`를 해당 기기의 ID로 바꾼다. 여러 기기가 부팅돼 있어도 지정한 기기만 설치·실행한다.

```sh
HM2_SIM_ID='SIMULATOR_UUID'
xcrun simctl install "$HM2_SIM_ID" build/ios-simulator/Build/Products/Release-iphonesimulator/HeavyMach2Source.app
xcrun simctl launch "$HM2_SIM_ID" com.gelb2.heavymach2.restored
```

iPhone은 가로, iPad는 세로로 타이틀·슬롯·기지·전장을 연다. 원작 HD와 비교할 때는 iPad를 쓴다. [조작 안내](quick-play.md)

## 실제 iPhone·iPad에 설치

1. 기존 앱이 있으면 Xcode **Window → Devices and Simulators**에서 기기와 HM2 Source를 선택해 **Download Container**로 데이터를 백업한다. 사용자 저장은 `Library/Application Support/HeavyMach2/HeavyMach2.sav`다.
2. [자원과 아이콘](#공개-빌드에서-자원-준비)을 준비한 뒤 저장소 루트에서 `python3 tools/build_openal.py --sdk iphoneos`를 실행한다. Xcode 프로젝트를 다시 생성할 때는 `xcodegen generate --spec Apple/iOS/project.yml`을 실행한다.
3. `open Apple/iOS/HeavyMach2.xcodeproj`로 프로젝트를 연다.
4. 앱 타깃의 **Signing & Capabilities**에서 본인의 개발 팀을 지정한다. 앱 ID 등록이 충돌하면 본인 팀에서 쓸 고유 ID를 정한다. ID가 달라지면 별도 앱과 저장 공간이 된다.
5. `HeavyMach2Source` scheme의 **Edit Scheme → Run → Build Configuration**을 `Release`로 선택한다. 실행 대상을 연결한 기기로 설정한다.
6. 기기에서 Mac 신뢰·개발자 모드를 확인하고 잠금을 푼 뒤 Xcode에서 Run한다. 무선 실행은 최초 연결·페어링이 된 기기를 같은 네트워크에서 사용한다.
7. 기존 앱 위에 설치한다. 새 빌드에서 타이틀과 저장 슬롯을 확인하고, 저장한 진행을 이어서 연다.

현재 정식 앱 ID는 `com.gelb2.heavymach2.restored`, 표시 이름은 **HM2 Source**다. 2026년 11월 12일 실제 iPad 갱신에서는 기존 28,204바이트 저장의 설치 전후 일치를 확인했다. [설치 기록](../ipad-release-installation-20261111.md) · [2026-11-12 아이콘 확인](../evidence/title-illustration-icons-20261112.json) · [두 실기기 화이트마운틴 Release](../white-mountain-dual-release-20261120.md)

## 치트 앱 실행과 결과

`HeavyMach2Validation`을 빌드해 일반 앱과 별도로 설치한다. 앱 ID는 `com.gelb2.heavymach2.validation`, 이름은 **HM2 Validation**, 아이콘에는 `TEST`가 있다. 아래 자동 실행 절차는 iPad 시뮬레이터에서 확인했다.

화이트마운틴 전용 검사는 같은 치트 앱의 실행 모드를 고른다. 시뮬레이터의 기존 검증 앱을 종료한 뒤 다음 명령에서 모드 값을 바꾼다. 값 1은 전장·포털, 2는 보스·의뢰, 3은 무기·공습·BOT, 4는 공습 피해 보정을 실행한다. [공습 검사 기록](../campaign-and-ipad-followup-20261120.md#공습-레벨-보정)

```sh
SIMCTL_CHILD_HM2_WHITE_MOUNTAIN_VALIDATION=1 xcrun simctl launch "$HM2_SIM_ID" com.gelb2.heavymach2.validation
```

결과와 GPU 캡처는 검증 앱 컨테이너의 `Documents/ConcurrentValidation/<실행 UUID>/`에 저장한다.

```sh
xcrun simctl install "$HM2_SIM_ID" build/ios-simulator/Build/Products/Release-iphonesimulator/HeavyMach2Validation.app
xcrun simctl launch "$HM2_SIM_ID" com.gelb2.heavymach2.validation
```

기본 치트 검사는 126조건을 2,100프레임 동안 실행한다. 마지막 화면에서 멈추면 치트 앱 컨테이너의 `Documents/ConcurrentValidation/<실행 UUID>/report.json`을 확인한다. 아래 명령으로 컨테이너를 찾는다. [iPhone·iPad 실행 기록](../iphone-module-audit-20261118.md)

```sh
xcrun simctl get_app_container "$HM2_SIM_ID" com.gelb2.heavymach2.validation data
```

치트 앱을 다시 실행하면 별도 앱 컨테이너에 새 UUID로 결과를 저장한다. [실행한 조건과 남은 검사](validation.md) · [검사 구조](../validation-build.md)

## Android 빌드와 설치

갤럭시에서는 [v0.4 정식 APK](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4)를 기존 정식 앱 위에 설치한다. 설치 후 슬롯 화면에서 **저장 이동 → 코드 입력**을 열 수 있다. [v0.1 치트 APK](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1)는 별도 앱이다. 각 릴리스의 `SHA256SUMS.txt`에서 파일 해시를 확인할 수 있다.

JDK 17, Android SDK 36, NDK 28.2.13676358, CMake 3.22.1과 호스트의 C++ 컴파일러가 필요하다. Android Studio의 SDK Manager나 아래 명령으로 SDK를 준비한 뒤 `ANDROID_HOME`을 SDK 디렉터리로 설정한다. 앱은 Android 8.0(API 26) 이상과 ARM64·x86-64를 대상으로 한다. [Gradle 구성과 검증](../../Android/README.md) · [AGP 8.13 도구 버전](https://developer.android.com/build/releases/agp-8-13-0-release-notes)

```sh
sdkmanager "platform-tools" "platforms;android-36" "build-tools;35.0.0" \
  "ndk;28.2.13676358" "cmake;3.22.1"
sdkmanager --licenses
```

[공통 자원](#공개-빌드에서-자원-준비)을 준비하고 저장소 루트에서 다음 명령을 실행한다. macOS에서는 JDK 17이 설치된 상태에서 `export JAVA_HOME="$(/usr/libexec/java_home -v 17)"`로 활성화할 수 있다. SDK 도구의 `platform-tools`와 `cmdline-tools/latest/bin`을 `PATH`에 넣는다.

```sh
cd Android
./gradlew :app:assembleNormalDebug :app:assembleCheatDebug
adb devices -l
```

배포용 정식판은 [로컬 서명 키를 설정](../../Android/README.md#정식판-서명)한 뒤 `:app:assembleNormalRelease`로 빌드한다.

| 앱 | APK 경로 — `Android/` 기준 |
| --- | --- |
| 정식판 Release | `app/build/outputs/apk/normal/release/app-normal-release.apk` |
| 일반 개발판 Debug | `app/build/outputs/apk/normal/debug/app-normal-debug.apk` |
| 치트판 Debug | `app/build/outputs/apk/cheat/debug/app-cheat-debug.apk` |

갤럭시의 USB 디버깅을 켜고 컴퓨터의 연결 요청을 허용한다. `ANDROID_SERIAL`을 `adb devices -l`에 표시된 기기 ID로 바꿔 설치한다. 기존 정식 앱을 갱신하려면 같은 서명이 필요하다. 적용 전 기존 진행은 복구코드로 백업할 수 있다.

```sh
HM2_ANDROID_SERIAL='ANDROID_SERIAL'
adb -s "$HM2_ANDROID_SERIAL" install -r app/build/outputs/apk/normal/debug/app-normal-debug.apk
adb -s "$HM2_ANDROID_SERIAL" install -r app/build/outputs/apk/cheat/debug/app-cheat-debug.apk
```

앱 목록에서 **HeavyMach 2** 또는 **HeavyMach 2 Test**를 연다. 두 앱의 저장 공간은 분리돼 있다. [갤럭시 실행과 검증](../../Android/검증-20261120.md)

## Tauri Mac 앱 받기

Apple Silicon Mac과 macOS 14 이상용이다. GitHub Release에서 `HeavyMach2-Tauri-Mac-v0.1-arm64.zip`을 받는다. `SHA256SUMS.txt`로 파일을 확인하고 ZIP을 푼 뒤 **HeavyMach 2 Desktop.app**을 응용 프로그램 폴더로 옮긴다. [Mac 앱 ZIP 다운로드](https://github.com/gelb2/HeavyMach2/releases/download/v0.1/HeavyMach2-Tauri-Mac-v0.1-arm64.zip) · [릴리스와 SHA-256 목록](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1)

[소스에서 Tauri 앱을 빌드](#tauri-mac-빌드)한 경우 아래 경로에서 앱을 찾는다.

1. 빌드한 맥에서 아래 명령으로 Finder의 앱 폴더를 연다. 저장소 루트에서 실행한다.

   ```sh
   open Desktop/Tauri/src-tauri/target/release/bundle/macos
   ```

2. **HeavyMach 2 Desktop.app**을 응용 프로그램 폴더에 복사하고 연다. 치트판용 **HeavyMach 2 Desktop QA.app**은 별도 앱과 저장 공간을 쓴다.
3. 다른 맥으로 옮길 때는 Finder에서 해당 `.app`을 압축해 ZIP을 전달한다. 받는 맥에서 ZIP을 풀어 앱을 응용 프로그램 폴더로 옮긴다. 현재 번들은 임시 서명본이다. 자신이 빌드했거나 출처를 확인한 앱의 첫 실행이 차단되면 **시스템 설정 → 개인정보 보호 및 보안 → 확인 없이 열기**를 사용한다. [Apple 첫 실행 안내](https://support.apple.com/102445)

## Tauri Mac 빌드

Apple Silicon Mac에 Node.js, Rust의 Cargo, Xcode 명령줄 도구를 준비한다. 먼저 [Wasm 빌드](#pwa-빌드와-로컬-실행)에서 정식판을 `build/web-normal/dist/`에 만든다. 치트판도 만들려면 `build/web-validation/dist/`를 준비한다. Mac 번들 생성에는 로컬 HTTP 서버를 띄울 필요가 없다.

```sh
cd Desktop/Tauri
npm ci
npm run build
npm run build:validation
open src-tauri/target/release/bundle/macos
```

결과는 `Desktop/Tauri/src-tauri/target/release/bundle/macos/`의 **HeavyMach 2 Desktop.app**과 **HeavyMach 2 Desktop QA.app**이다. 정식판만 필요하면 `npm run build`까지만 실행한다. 패키징 도구는 각 웹 빌드 파일의 길이와 SHA-256을 확인한 뒤 앱에 묶는다. [Tauri 빌드·저장 문서](../../Desktop/Tauri/README.md)

## 코드·저장소 검사

```sh
python3 -m unittest discover -s tests -p 'test_*.py'
python3 tools/check_no_ipa.py --all
```

공통 코어를 검사할 때는 준비 자원 경로를 명시한다. 현재 화면과 저장 복원에 필요한 캠페인 자원은 `.local/prepared/Campaign/HD`다. 아래 명령은 화면·HUD, 복구·세션 전환, 표시·입력, 도움말, HD 저장 형식을 검사한다.

```sh
cmake -S . -B build/core-campaign -DCMAKE_BUILD_TYPE=Debug \
  -DHM2_HD_RESOURCES="$PWD/.local/prepared/Campaign/HD"
cmake --build build/core-campaign --parallel 4 \
  --target ScreensTests SessionTests DisplayLayoutTests HelpSessionTests SaveTests
ctest --test-dir build/core-campaign --output-on-failure \
  -R '^(OriginalScreensAndHUD|OriginalSessionTransitions|UniversalDisplayAndInput|OriginalHelpSession|OriginalHDSaveEnvelope)$'
```

원작의 적 배치와 구입 목록처럼 원본 레코드의 수·순서를 대조하는 검사에는 `.local/prepared/HD`를 사용한다. 빌드 디렉터리를 나눠 검사 입력이 섞이지 않도록 한다.

```sh
cmake -S . -B build/core-original -DCMAKE_BUILD_TYPE=Debug \
  -DHM2_HD_RESOURCES="$PWD/.local/prepared/HD"
cmake --build build/core-original --parallel 4
ctest --test-dir build/core-original --output-on-failure \
  -E '^(OriginalScreensAndHUD|OriginalSessionTransitions)$'
```

원본과 캠페인 입력을 한 번에 검사하려면 두 실행 결과를 함께 본다. 자원 경로를 생략하면 자원이 필요한 검사가 등록되지 않는다. Apple GPU·음향 출력은 Xcode의 `HeavyMach2Source` scheme에 연결된 테스트와 치트 앱으로 확인하고 iPhone이나 iPad 중 실행 대상을 기록한다.

## 오류 해결

| 증상 | 확인할 항목 |
| --- | --- |
| clone에서 `Repository not found` | 저장소 주소가 `https://github.com/gelb2/HeavyMach2.git`인지 확인한다. |
| 공개 자원의 다운로드·해시 검사 오류 | 연결을 확인하고 `prepare_build_resources.py`를 다시 실행한다. 이미 검증한 다운로드는 재사용한다. |
| 원본 파일을 찾을 수 없음 | `source-provenance.json`의 로컬 기준 경로와 상대 경로를 확인한다. |
| `HD source IPA SHA-256 mismatch` | 입력 파일을 [원본 해시](#원본-자원-준비)와 대조하고 일치하는 HD 1.0.3 원본을 선택한다. |
| XcodeGen이 `.local/prepared` 경로를 찾지 못함 | 자원과 아이콘 준비를 먼저 실행한 뒤 프로젝트를 생성한다. |
| `-lopenal` 링크 실패·다른 플랫폼용 라이브러리 오류 | 선택한 SDK의 `build_openal.py`를 실행했는지 확인한다. |
| 서명 오류·`errSecInternalComponent` | Xcode의 로그인 계정·팀·인증서·키체인 접근과 서명 로그를 확인한다. |
| 무료 개발 앱 설치 한도 | 설치된 개발 앱을 확인한다. 다른 앱을 지우기 전 해당 저장을 백업하고 제거 대상을 정한다. |
| 치트 앱 화면이 멈춤 | 기본 2,100프레임 또는 선택한 시나리오의 완료 여부와 `report.json`을 확인한다. 완료 뒤 display 갱신을 멈추는 현재 동작과 크래시를 구분한다. |

빌드 출력은 `build/`를 재사용하고 현재 실행 후보·비교/복구에 필요한 앱과 로그를 보존한다. 새 설치·실행을 확인한 뒤 더 쓰지 않을 이전 산출물만 정리한다. 원본·소스·사용자 저장·검증 근거는 유지한다.

## 다른 플랫폼과 포팅판

현재 정식판의 기준 방향은 iPhone·갤럭시 가로, iPad 세로다. 실기기 설치와 로컬 실행의 확인 범위는 [검증 현황](validation.md)에 있다. [플랫폼별 증상과 해결 기록](troubleshooting.md)

원본을 실행하는 비교 앱은 [Compatibility 안내](../../Compatibility/README.md)를 따른다. 위 일반/치트 앱 빌드에는 원본 ARM 실행파일을 넣지 않는다. 원본 IPA·원작 포함 비교 앱·서명 비밀·사용자 저장은 GitHub·보고 사이트·클라우드로 보내지 않는다.
