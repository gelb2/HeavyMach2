# HeavyMach 1 WASM PR부터 HeavyMach 2 PWA까지

HeavyMach 1 PR #24는 게임 규칙과 좌표 계산 소스를 재사용하고 메뉴·HUD는 웹용 C++로 이식했다. HeavyMach 2는 메뉴 배치와 터치 판정도 iPhone·iPad·Android와 공유한다. 이번 PWA 검증 범위는 휴대폰 가로·태블릿 세로·데스크톱이다. Safari 탭에서 관측한 Shop과 Exit 겹침은 같은 크기의 공통 코드 검사에서도 재현했다. 이후 iPhone 17 Pro 시뮬레이터에 설치한 치트판에서는 홈 화면 아이콘으로 실행해 첫 기지의 Shop과 Exit가 각각 동작하는 것을 확인했다. [설치형 실행 기록](pwa-device-validation-20261120.md#iphone-홈-화면에서-실행한-치트판) 태블릿 가로의 관측 결과는 후속 작업 근거로 보존한다.

## 비교 기준

- **HeavyMach 1 WASM PR.** 2026-10-31 병합 `757523e1`. 첫 부모와 대조한 변경 파일은 47개다. PR 커밋 21개와 병합 소스를 대조했다. `Sources/Core`, Apple·Android 제품 소스와 루트 CMake 변경은 없었다. [PR #24](https://github.com/gelb2/HeavyMach/pull/24)
- **HeavyMach 1 설치형 PWA.** `433c9e44`에서 시작한 설치 런처, `8e87785d`의 설치 우선 흐름, `9aed4591`의 복구 모듈과 `fa3f3fbe`의 게임 버튼 수정을 `origin/main`의 `ca75b1b6`에서 추적했다. [설치 런처](https://github.com/gelb2/HeavyMach/blob/ca75b1b6/Distribution/Web/pwa/host.js) · [게임 버튼 수정](https://github.com/gelb2/HeavyMach/commit/fa3f3fbe)
- **HeavyMach 2.** `ab166bf`의 첫 웹 빌드에서 `39b952b`까지 비교했다. 정식판·치트판의 빌드, 레이아웃, 입력, 에셋, 저장, 설치와 Tauri의 웹 빌드 소비 경로를 읽었다. 현재 공통 라이브러리를 링크해 실패를 다시 재현했고, Chromium·Safari의 기존 화면과 로그를 대조했다.

## 모듈별 대조

| 영역 | HeavyMach 1의 소스 | HeavyMach 2의 소스와 확인 결과 |
| --- | --- | --- |
| 빌드와 게임 규칙 | PR `Web/CMakeLists.txt`가 게임·전투·저장의 `Sources/Core` 소스 목록을 웹 빌드에 명시한다. PR 당시 빌드는 정식판 `HM_VALIDATION=0`이다. | [`Web/CMakeLists.txt`](../Web/CMakeLists.txt)는 루트의 `HeavyMach2Core`를 링크하고 `Runtime.cpp`, `Renderer.cpp`를 추가한다. [Android CMake](../Android/cmake/CMakeLists.txt)도 같은 타깃을 링크한다. [Apple Xcode](../Apple/iOS/HeavyMach2.xcodeproj/project.pbxproj)는 해당 C++ 파일을 iPhone·iPad용 빌드의 Sources에 넣는다. 웹 치트판은 웹 타깃의 별도 옵션과 저장소를 쓴다. |
| 화면과 원본 메뉴 | PR의 `WebGameView`와 `HMGameView`가 `CanvasGeometry`와 `Viewport`를 재사용한다. `WebMenus`, `WebGameHUD`, `WebHUDEditor`는 별도 C++ 구현이다. PR에서는 HUD 편집에 HTML 조작부도 있었고, 후속 `fa3f3fbe`에서 이 버튼을 게임 렌더러로 옮겼다. | [`Screens`](../Sources/Core/UI/Screens.cpp)가 Phone 또는 HD 원본 좌표와 에셋으로 화면을 만들고 [`DisplayLayout`](../Sources/Core/Presentation/DisplayLayout.cpp)으로 표시와 입력을 계산한다. 웹에 별도 상점·사령부·HUD 구현은 없다. [`Renderer`](../Sources/Platform/Web/Renderer.cpp)는 공통 프레임을 그린다. 다만 데스크톱 정비소와 도움말 배경에 텍스처 이름으로 고르는 전용 합성이 있다. 해당 합성은 공통 버튼 좌표를 바꾸는 인터페이스가 아니다. |
| 화면 크기와 입력 | PR의 HTML은 실제 캔버스 크기·DPR·안전 영역과 포인터 좌표를 보낸다. `WebGameView`에서 뷰포트의 역변환을 쓰고, 논리 캔버스 계산에 넣는 높이는 최대 너비의 3/4로 제한한다. 세로에서 일시 정지하는 동작도 이 PR에 들어 있다. | [`display.js`](../Web/Shell/display.js)는 처음에 기기 유형을 결정한다. [`app.js`](../Web/Shell/app.js)는 실제 캔버스 크기와 안전 영역을 [`Runtime::resize`](../Sources/Platform/Web/Runtime.cpp)에 보내고, 실제 픽셀 수는 별도로 설정한다. 포인터는 공통 `Screens::input` 또는 `GameSession::inputWorld`로 전달한다. 웹의 계산값과 공통 코드의 기기 값이 일치한 상태에서도 아래 오류가 난다. 웹 매니페스트는 standalone을 지정하고 방향을 고정하지 않는다. 네이티브 iPad와 Android 태블릿은 세로로 실행한다. [Apple 방향 설정](../Apple/iOS/Info.plist) · [Android 방향 설정](../Android/app/src/main/java/com/gelb2/heavymach2/GameActivity.kt) |
| 에셋·폰트·오디오 | PR은 원본 이미지·음향·데이터와 사용 중인 추가 에셋의 경로를 유지한다. 패키지 하나가 20 MiB를 넘지 않게 나눈다. WebGL2와 OpenAL을 사용한다. | 준비된 `Campaign/HD`를 `/campaign`에 넣는다. 그 안의 `PhoneMenu`와 원본 HD 리소스를 공통 코드가 화면별로 요청한다. 브라우저는 공통 폰트 요청을 Canvas 2D에서 래스터화하며 로컬에 지정 폰트가 없으면 대체 폰트를 사용한다. 음향 ID와 시점은 공통 런타임에서 받고 재생은 [`audio.js`](../Web/Shell/audio.js)가 맡는다. 배치 재현은 원본 폰트 일치나 기기 스피커 출력의 검증을 포함하지 않는다. |
| 저장과 시스템 입력 | PR은 IDBFS를 쓰고 기존 `HvM.dat` 형식을 유지한다. 후속 배포의 `Modules/SaveTransfer`는 같은 C++ 코덱을 네이티브와 별도 `codec.wasm`에서 사용한다. | 공통 `GameSession`이 저장과 복구 검증을 맡고 브라우저는 [`storage.js`](../Web/Shell/storage.js)의 IndexedDB로 저장한다. Tauri는 같은 호출을 Mac 파일 저장에 연결한다. 게임 안의 복구코드 진입 버튼은 C++에서 그린다. 기존 HTML 입력창은 이름·복구코드 텍스트 입력과 클립보드를 맡는다. |
| 설치와 다운로드 | 후속 `Distribution/Web/pwa`는 설치 안내를 먼저 연다. 파일 수신량과 검증량을 표시하고 길이와 SHA-256을 확인한 뒤 오프라인 준비를 기록한다. 그 뒤 실행할 때 게임 프레임을 연다. | [`app.js`](../Web/Shell/app.js)가 게임 모듈을 초기화하고 [`sw.js`](../Web/Shell/sw.js)는 같은 배포 파일을 `cache.addAll`로 받는다. 패키지에 SHA-256 목록이 있지만 브라우저 서비스 워커는 이를 비교하지 않는다. 새 Chromium 프로필에서 91,435,806바이트 `game.data`에 페이지와 워커 양쪽이 요청했다. 진행 표시가 없다. 두 요청이 전체 파일을 두 번 내려받았는지는 확인하지 않았다. [HeavyMach 1 워커](https://github.com/gelb2/HeavyMach/blob/ca75b1b6/Distribution/Web/pwa/sw.js) |
| Tauri의 변경 범위 | 후속 설치형 PWA와 Tauri는 복구 코덱을 재사용한다. PR #24에는 Tauri 패키지가 없다. | [`prepare.py`](../Desktop/Tauri/scripts/prepare.py)는 정식 웹 빌드의 파일과 해시를 확인하고 그대로 묶는다. [`desktop-host.js`](../Desktop/Tauri/frontend/desktop-host.js)는 Mac 저장 연결만 추가하며 Mac 앱에서는 서비스 워커를 등록하지 않는다. 웹 C++와 공통 웹 화면을 변경하면 새 Mac 패키지에도 반영된다. |

HeavyMach 1 PR의 구현과 나중에 추가된 설치·캐시·복구 정책은 별개 커밋이다. MachDefense도 Wasm에서 기존 공통 소스를 빌드하고 런처에서 준비 단계와 받은 용량을 표시한다. 두 게임에서 재사용할 기준은 공통 게임 코드 소비, 운영체제 연결 분리, 검증이 끝난 뒤의 오프라인 준비 표시다. [HeavyMach 1 PR 빌드](https://github.com/gelb2/HeavyMach/blob/757523e1/Web/CMakeLists.txt) · [MachDefense WASM과 PWA](https://github.com/gelb2/MachDefense/commit/6a05e039e441eb9db3a51a199c6ae931ce0fb722) · [MachDefense 진행 표시](https://github.com/gelb2/MachDefense/blob/a9cc530/Distribution/Web/app.js)

## 현재 범위의 화면 검사

기존 `DisplayLayoutTests`의 리소스·화면 호스트를 가져와 별도 실행 파일을 만들고, 현재 `HeavyMach2Core`와 준비된 원본 리소스를 연결했다. 안전 영역을 0으로 두고 여섯 기지의 공통 `Screens::input`에 버튼 중앙의 누름·뗌을 보냈다. 현재 범위와 아래의 태블릿 가로 기록을 합쳐 60개 화면 조건을 검사했다. 수치는 캔버스 왼쪽 아래가 원점인 논리 좌표다.

| 크기와 화면 | 공통 C++에서 직접 재현한 결과 | 관련 코드 |
| --- | --- | --- |
| Safari 탭과 같은 874×338, 기지 | `Base001`의 Shop `2002` 중앙 `(764.75, 285.533)`이 Exit `2004`의 터치 영역 안에 들어간다. 누름과 뗌을 Exit가 받았다. 나머지 다섯 기지에서는 Shop이 받았다. | `Screens::base`는 Phone 원본 좌표를 `artScene`으로 확대한다. `keepControlVisible`은 Exit를 안전 영역 안으로 옮길 때 다른 버튼의 영역을 고려하지 않는다. [`ButtonDispatcher::input`](../Sources/Core/UI/Button.cpp)은 역순으로 검사하고 먼저 받은 버튼에서 끝낸다. |
| 휴대폰 전체 높이 모의 874×402, 기지 | `Base001`의 Shop 중앙은 `(764.75, 317.533)`이다. Exit 영역을 벗어났고 여섯 기지 모두 Shop이 입력을 받았다. | 같은 분류·폭에서 높이만 변경했다. 주소창 유무는 브라우저가 제공하는 높이에 영향을 준다. 이 결과는 실제 iPhone 설치판의 전체 통과를 증명하지 않는다. |
| 태블릿 834×1194 및 640×1024, 상점·사령부 | 두 세로 크기에서 두 화면 모두 여섯 기지의 복귀 버튼이 입력을 받았다. | 기존 iPad·Android 태블릿 정책과 같은 세로 조건이다. |

기존 `UniversalDisplayAndInput` 1개와 웹 분류 테스트 5개를 다시 실행해 통과했다. 휴대폰의 874×338 기지에서 Shop을 눌렀을 때 어느 버튼이 받는지는 기존 테스트에 없다. 작업 도중 출력된 기존 모바일 화면 검사 경고는 소스가 바뀐 이전 후보의 캡처를 가리켰다. 해당 캡처는 이번 브라우저 및 공통 코드 결과의 근거에서 제외했다.

## 수정 경계와 검증 순서

1. `DisplayConfiguration`의 현행 입력은 캔버스·안전 영역·기기 유형·compact·focusZoom이다. 실제 기기 유형과 화면 크기를 유지한 채 휴대폰 기지의 `artScene` 버튼 배치를 바꾸는 설정은 없다. `desktop`을 넘기면 데스크톱의 맞춤 배율뿐 아니라 화면·HUD 정책이 바뀐다. 웹에서 버튼 위치만 덮어쓰는 방식은 화면을 새로 만들 때마다 공통 배치를 재구현하게 된다.
2. 현재 작업 제약은 iPhone·iPad·Android가 소비하는 소스와 리소스를 읽기 전용으로 두는 것이다. 임의 JavaScript·DOM 게임 버튼, 웹의 별도 터치 판정, 가짜 화면 크기·배율, 가로·세로 스크롤을 통한 우회도 금지한다. 이 조건에서 설치 런처·다운로드 진행·검증된 캐시·실제 뷰포트 전달은 브라우저 호스트의 책임이다. Safari 탭의 배치 오류는 브라우저에서 플레이하는 경로의 항목으로 남긴다. 설치형 PWA는 홈 화면에서 실행해 실제 크기와 입력을 확인한다.
3. 공통 배치 변경을 별도로 요청할 경우 `Screens::base`에서 Phone 시설 위치를 공통으로 계산해 안전 영역과 서로 겹치지 않는 터치 영역을 확인한다. 같은 변환 결과를 그리기와 역입력에 사용한다. 기존 모바일의 적용 조건, 배경·버튼 에셋과 글꼴 정책을 먼저 검증한다.
4. 회귀 검사에는 휴대폰 가로 874×338·874×402와 기존 태블릿 세로 화면을 넣는다. 공통 검사에서 여섯 기지의 상점·사령부·포털과 복귀 버튼의 위치 및 실제 수신 버튼을 확인한다. 치트판에서 동일 경로·상점 거래·전장 경로 입력을 반복한다. 정식판과 치트판 설치 후 새 다운로드·중단 후 재시도·업데이트·오프라인 재실행·저장 유지까지 확인한다. 설치형 모바일 실기기와 최신 정식 산출물의 Tauri 재패키징은 후속 검사로 남는다.

웹 도입 커밋 `ab166bf`는 `Web`과 `Sources/Platform/Web`에만 런타임 소스를 추가했다. 같은 브랜치의 `5064c46`은 공통 `DisplayLayout`과 데스크톱 분류를 변경했다. `14b4b56`은 공통 `DisplayLayout`, `Screens`, `GameSession`, `Help`와 웹 렌더러를 함께 변경했다. 복구코드용 공통 변경은 `e0ba99b`, `d7284ed`, `b4aaef4`, `19a9eea`에 있다. 따라서 이 브랜치 전체를 기존 모바일 사용 파일과 분리해 개발했다고 보고할 수 없다. 이번 조사에서 추적 파일 변경은 이 문서와 투두 두 건이다.

## 후속 작업인 태블릿 가로

1194×834와 1024×640에서 상점·사령부의 복귀 버튼 `2100` 중심은 각각 `(995, -298.156)`, `(853.333, -293.333)`에 놓였다. 여섯 기지 모두 화면 아래였다. `DisplayLayout::ui(tabletMenu)`이 배경을 채우는 배율을 선택하고 `Screens::presentation`이 배경과 버튼에 함께 적용한다. 현재 PWA의 태블릿 기준은 세로다.

기존 화면과 실행 범위는 [PWA 기기별 검증](pwa-device-validation-20261120.md)에 있다. 공통 C++의 재현 소스와 로그는 로컬 `.local/evidence/pwa-architecture-audit-20261120`의 `core-layout-probe.cpp`와 `core-layout.log`에 있다. 브라우저 판본·좌표·화면 캡처는 `.local/evidence/pwa-gesture-20261120`의 `viewport-baseline`, `installed-phone`, `installed-tablets`에 남아 있다.
