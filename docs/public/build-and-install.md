# HeavyMach 2 빌드와 설치

[README](../../README.md) · [플레이 방법](quick-play.md) · [확인한 범위](validation.md)

iPhone·iPad용 소스 복원판과 검증 앱을 빌드하고 설치하는 방법이다. [Android](../../Android/README.md) · [PWA](../../Web/README.md) · [Tauri Mac](../../Desktop/Tauri/README.md) 일반 앱은 `HeavyMach2Source`, 동시 치트 앱은 `HeavyMach2Validation`이다. 정식판은 일반 앱을 **Release** 구성으로 빌드한다. 저장소는 비공개이며 App Store·TestFlight·공개 앱 다운로드는 제공하지 않는다.

## 준비물

- 저장소를 읽을 수 있는 GitHub 계정과 Git 인증 설정.
- Apple Silicon Mac, iOS SDK와 iPad 시뮬레이터 런타임을 설치한 Xcode.
- Python 3.11 이상, CMake 3.20 이상, XcodeGen. Git 훅은 LFS를 연결한다. IPA와 Python 검사는 CI의 저장소 검사에서 실행한다.
- [원본 명세](../source-provenance.json)에 등록된 HD 1.0.3, HD 1.0.0, 일반판 2.0.1 원본을 로컬에 보유한 환경.
- 실기기 설치에는 본인의 Apple 개발 팀, 연결된 iPhone 또는 iPad와 개발자 모드.

2026년 11월 13일 문서 확인 환경은 Xcode 27.1, XcodeGen 2.45.4, CMake 4.4.4, Python 3.14.6이다. 앱 설정의 최소 OS는 iOS/iPadOS 15.0이며, Apple 자동검사 타깃은 17.0이다. 최소 OS별 실제 플레이 검증은 남아 있다. 현재 Apple 빌드는 arm64를 대상으로 한다.

## 소스 받기

```sh
git clone git@github.com:gelb2/HeavyMach2.git
cd HeavyMach2
git config core.hooksPath .githooks
```

위 SSH 주소는 계정의 저장소 접근 권한과 SSH 인증을 사용한다. HTTPS 인증을 사용하는 환경에서는 `https://github.com/gelb2/HeavyMach2.git`을 사용한다. 두 방식 모두 비공개 저장소 접근 권한이 필요하다.

## 원본 자원 준비

소스만 내려받은 상태에는 게임 그림·음원·맵이 없다. [source-provenance.json](../source-provenance.json)의 `source_root`와 `relative_path`로 원본 파일 위치를 확인한다. 다른 Mac에서는 보유한 원본 경로에 맞게 `source_root`를 로컬에서 설정한다. 원본 해시와 판본은 바꾸지 않는다.

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

두 디렉터리는 Git 추적에서 제외한다. 원본 IPA와 원작 포함 앱은 로컬에 보관한다. 준비 도구가 `Existing prepared resource differs`로 중단하면 기존 파일과 해시 차이의 원인을 확인하고, 보존한 자원을 임의로 덮어쓰지 않는다.

## iPhone·iPad 시뮬레이터 빌드

시뮬레이터용 OpenAL을 준비한 뒤 프로젝트를 생성한다. 원본 자원·아이콘 준비가 먼저다.

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

현재 HD 기준 검증에는 iPad를 사용한다. iPhone을 HD 비교 대상으로 대신 사용하지 않는다. iPhone은 가로, iPad는 세로로 타이틀·슬롯·기지·전장을 연다. [조작 안내](quick-play.md)

## 실제 iPhone·iPad에 설치

1. 기존 앱이 있으면 Xcode **Window → Devices and Simulators**에서 기기와 HM2 Source를 선택해 **Download Container**로 데이터를 백업한다. 사용자 저장은 `Library/Application Support/HeavyMach2/HeavyMach2.sav`다.
2. 저장소 루트에서 `python3 tools/build_openal.py --sdk iphoneos`를 실행한다. 시뮬레이터 라이브러리를 실기기 빌드에 사용하지 않는다.
3. `open Apple/iOS/HeavyMach2.xcodeproj`로 프로젝트를 연다.
4. 앱 타깃의 **Signing & Capabilities**에서 본인의 개발 팀을 지정한다. 앱 ID 등록이 충돌하면 본인 팀에서 쓸 고유 ID를 정한다. ID가 달라지면 별도 앱과 저장 공간이 된다.
5. `HeavyMach2Source` scheme의 **Edit Scheme → Run → Build Configuration**을 `Release`로 선택한다. 실행 대상을 연결한 기기로 설정한다.
6. 기기에서 Mac 신뢰·개발자 모드를 확인하고 잠금을 푼 뒤 Xcode에서 Run한다. 무선 실행은 최초 연결·페어링이 된 기기를 같은 네트워크에서 사용한다.
7. 설치 후 타이틀과 기존 슬롯을 확인하고 저장한 진행을 이어서 연다. 앱을 지워 재설치하는 방법은 저장을 보존하는 갱신 절차로 사용하지 않는다.

현재 정식 앱 ID는 `com.gelb2.heavymach2.restored`, 표시 이름은 **HM2 Source**다. 2026년 11월 12일 실제 iPad 갱신에서는 기존 28,204바이트 저장의 설치 전후 일치를 확인했다. 이는 당시 설치의 결과이며 앞으로의 모든 갱신을 보장하는 수치가 아니다. [설치 기록](../ipad-release-installation-20261111.md) · [2026-11-12 아이콘 확인](../evidence/title-illustration-icons-20261112.json) · [두 실기기 화이트마운틴 Release](../white-mountain-dual-release-20261120.md)

## 치트 앱 실행과 결과

`HeavyMach2Validation`을 빌드해 일반 앱과 별도로 설치한다. 앱 ID는 `com.gelb2.heavymach2.validation`, 이름은 **HM2 Validation**, 아이콘에는 `TEST`가 있다. 실제 iPad의 치트 앱 설치·검증은 최신 기록에서 완료하지 않았으며, 현재 확인된 자동 실행 대상은 iPad 시뮬레이터다.

화이트마운틴 전용 검사는 같은 치트 앱의 실행 모드를 고른다. 시뮬레이터의 기존 검증 앱을 종료한 뒤 다음 명령에서 모드 값을 바꾼다. 값 1은 전장·포털, 2는 보스·의뢰, 3은 무기·공습·BOT, 4는 공습 피해 보정을 실행한다. [공습 검사 기록](../campaign-and-ipad-followup-20261120.md#공습-레벨-보정)

```sh
SIMCTL_CHILD_HM2_WHITE_MOUNTAIN_VALIDATION=1 xcrun simctl launch "$HM2_SIM_ID" com.gelb2.heavymach2.validation
```

결과와 GPU 캡처는 검증 앱 컨테이너의 `Documents/ConcurrentValidation/<실행 UUID>/`에 저장한다.

```sh
xcrun simctl install "$HM2_SIM_ID" build/ios-simulator/Build/Products/Release-iphonesimulator/HeavyMach2Validation.app
xcrun simctl launch "$HM2_SIM_ID" com.gelb2.heavymach2.validation
```

기기 표시에 연결된 기본 치트 실행은 126조건과 2,100프레임을 사용한다. 마지막 화면을 그린 뒤 검사를 멈춘다. [양 기기 실행 기록](../iphone-module-audit-20261118.md) 앱이 자동 종료되는 것으로 기다리지 않는다. 결과는 치트 앱 컨테이너의 `Documents/ConcurrentValidation/<실행 UUID>/report.json`에 저장한다. 다음 명령으로 컨테이너 위치를 확인한다.

```sh
xcrun simctl get_app_container "$HM2_SIM_ID" com.gelb2.heavymach2.validation data
```

현재 치트 앱에는 자유 조작용 조건 선택 화면이 없다. 재실행은 새 UUID 아래 새 검사 결과를 만든다. 일반 앱의 사용자 저장은 사용하지 않는다. 전체 게임 검사 완료 여부와 남은 조건은 [검증 범위](validation.md), 검사 구조는 [치트 앱 문서](../validation-build.md)를 따른다.

## 코드·저장소 검사

문서·패키징 도구 검사는 원본 앱을 실행하지 않는다.

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
| clone에서 `Repository not found` 또는 인증 오류 | 비공개 저장소 접근 권한과 선택한 SSH/HTTPS 인증을 확인한다. |
| 원본 파일을 찾을 수 없음 | `source-provenance.json`의 로컬 기준 경로와 상대 경로를 확인한다. |
| `HD source IPA SHA-256 mismatch` | 명세에 등록된 HD 1.0.3 원본인지 확인한다. 검사 해시를 바꿔 통과시키지 않는다. |
| XcodeGen이 `.local/prepared` 경로를 찾지 못함 | 자원과 아이콘 준비를 먼저 실행한 뒤 프로젝트를 생성한다. |
| `-lopenal` 링크 실패·다른 플랫폼용 라이브러리 오류 | 선택한 SDK의 `build_openal.py`를 실행했는지 확인한다. |
| 서명 오류·`errSecInternalComponent` | Xcode의 로그인 계정·팀·인증서·키체인 접근과 실제 오류를 확인한다. 같은 오류를 소스 빌드 실패로 취급하지 않는다. |
| 무료 개발 앱 설치 한도 | 설치된 개발 앱을 확인한다. 다른 앱을 지우기 전 해당 저장을 백업하고 제거 대상을 정한다. |
| 치트 앱 화면이 멈춤 | 기본 2,100프레임 또는 선택한 시나리오의 완료 여부와 `report.json`을 확인한다. 완료 뒤 display 갱신을 멈추는 현재 동작과 크래시를 구분한다. |

빌드 출력은 `build/`를 재사용하고 현재 실행 후보·비교/복구에 필요한 앱과 로그를 보존한다. 새 설치·실행을 확인한 뒤 더 쓰지 않을 이전 산출물만 정리한다. 원본·소스·사용자 저장·검증 근거는 유지한다.

## 다른 플랫폼과 포팅판

iPhone·iPad 유니버설 앱이 목표이며 현재 설정은 iPhone 가로·iPad 세로다. 현재 HD의 정상동작 판정은 iPad에서 진행한다. [Android APK](../../Android/README.md), [설치형 PWA](../../Web/README.md), [Tauri Mac 앱](../../Desktop/Tauri/README.md)은 각각 빌드 경로가 있다. 실기기 설치와 로컬 실행의 확인 범위는 [검증 현황](validation.md)에 있다.

원본을 실행하는 비교 앱은 [Compatibility 안내](../../Compatibility/README.md)를 따른다. 위 일반/치트 앱 빌드에는 원본 ARM 실행파일을 넣지 않는다. 원본 IPA·원작 포함 비교 앱·서명 비밀·사용자 저장은 GitHub·보고 사이트·클라우드로 보내지 않는다.
