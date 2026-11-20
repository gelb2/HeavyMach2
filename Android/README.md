# HeavyMach 2 Android

Android 앱은 저장소 루트의 `HeavyMach2Core`와 `HeavyMach2Audio`를 링크한다. 창·터치·키보드·GL·음향 장치·파일 입출력 연결 코드는 `Sources/Platform/Android/`에 있다. Gradle은 기존 `.local/prepared/Campaign/HD`를 읽는다. 게임 파일과 저장 포맷은 두 앱에서 같다.

| 앱 | Android 패키지 | 실행 |
| --- | --- | --- |
| HeavyMach 2 | `com.gelb2.heavymach2.restored` | 타이틀에서 슬롯을 고르고 이름을 정해 시작한다. |
| HeavyMach 2 Test | `com.gelb2.heavymach2.validation` | 독립 저장 슬롯으로 플레이하거나 ADB에서 설원·공습과 동시 전투 검사를 실행한다. |

## 빌드

JDK 17, Android SDK 36, NDK 28.2.13676358, CMake 3.22.1을 사용한다. 앱의 minSdk는 26이다. `ANDROID_HOME`은 설치한 SDK를 가리켜야 한다. [공통 캠페인 자원](../docs/public/build-and-install.md#원본-자원-준비)을 준비한 상태에서 이 디렉터리에서 실행한다.

```sh
./gradlew :app:assembleNormalRelease :app:assembleCheatDebug :app:testNormalDebugUnitTest
```

정식판 서명은 저장소 밖으로 공유하지 않는 로컬 설정 `.local/android-signing/release.properties`의 `keyAlias`·`storePassword`와 같은 디렉터리의 `release.p12`를 사용한다. 기존 설치를 업데이트할 때는 같은 키를 사용한다. 기본 APK에는 ARM64와 x86-64가 들어 있다.

## 단말 검증

아래 명령은 연결한 단말 하나를 지정한다. 정식판과 치트판을 나란히 설치하며 앱 데이터는 각각의 전용 디렉터리에 저장된다.

```sh
adb devices -l
adb -s "$SERIAL" install -r app/build/outputs/apk/normal/release/app-normal-release.apk
adb -s "$SERIAL" install -r app/build/outputs/apk/cheat/debug/app-cheat-debug.apk
python3 tools/validate_device.py "$SERIAL" campaign --output /tmp/hm2-campaign.json
python3 tools/validate_device.py "$SERIAL" concurrent --output /tmp/hm2-concurrent.json
```

`campaign`은 실제 포털·버튼·전투 호출을 사용해 설원 진출입, 보스, 상점의 레이저 공습 구매, 두 전장의 공습 피해를 검사한다. `concurrent`는 기존 C++ 동시 검사를 구동한다. 결과 JSON에서 `done`과 `pass`를 함께 확인한다. 정식판의 새 슬롯 생성·첫 전장 진입·재실행은 게임 화면에서 직접 검사한다.

현재 화면에 선택된 그림 파일은 치트판에서 `python3 tools/validate_device.py "$SERIAL" artwork`로 확인한다. [갤럭시와 iPhone 정식판의 화면·입력 검사](검증-20261120.md)에 같은 상품을 구매한 결과를 기록했다.

슬롯 화면의 **저장 이동**으로 iPhone·iPad·웹과 저장을 옮긴다. 슬롯 세 개 전체와 기존 앱 저장 보존 절차는 [복구코드 안내](../docs/save-transfer.md)를 따른다. 아이콘은 [Android 생성 절차](../docs/app-icons.md#android)에서 확인한다.
