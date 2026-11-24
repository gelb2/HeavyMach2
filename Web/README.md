# HeavyMach 2 웹 빌드

[공개 PWA](https://gelb2.github.io/HeavyMach2/) · [홈 화면 설치](../docs/public/build-and-install.md#pwa-설치) · [GitHub Pages 배포 기준](../docs/public/release-policy.md#pwa와-mac-산출물)

브라우저 호스트는 Emscripten으로 공통 C++ 세션, 화면, 복구코드 구현을 빌드한다. WebGL 2는 공통 프레임에 포함된 스프라이트, 아틀라스와 궤적을 그린다. DOM은 실행과 설치, 이름·복구코드 입력을 제공한다. 에셋은 로컬에서 준비한 캠페인 디렉터리만 읽는다.

## 빌드와 실행

macOS에서 Emscripten SDK를 활성화한 뒤 저장소 루트에서 실행한다. 검증에는 Emscripten 6.0.13을 사용했다. [캠페인과 아이콘 준비](../docs/public/build-and-install.md#공개-빌드에서-자원-준비)

```sh
python3 tools/prepare_build_resources.py
export EM_CACHE="$PWD/.local/emscripten-cache-hm2"
emcmake cmake -S Web -B build/web-normal -DCMAKE_BUILD_TYPE=Release
cmake --build build/web-normal --parallel 6
emcmake cmake -S Web -B build/web-validation -DCMAKE_BUILD_TYPE=Release -DHM2_WEB_VALIDATION=ON
cmake --build build/web-validation --parallel 6
python3 -m http.server 8773 --bind 127.0.0.1 --directory build
```

- 정식판은 [로컬 정식 주소](http://127.0.0.1:8773/web-normal/dist/)에서 연다.
- 검증판은 [로컬 검증 주소](http://127.0.0.1:8773/web-validation/dist/)에서 연다.

기본 에셋 입력은 `.local/prepared/Campaign/HD`다. 다른 준비본은 `-DHM2_WEB_ASSETS=경로`로 지정한다. 설치 아이콘은 [모바일판과 같은 탭투스타트 일러스트](../docs/app-icons.md)를 사용한다. 각 판의 1024px 준비본으로 192px·512px PNG를 만들고, 치트판에는 모바일판의 `TEST` 띠가 들어간다. `dist/asset-manifest.json`에 아이콘을 포함한 산출물의 크기와 SHA-256을 기록한다.

## 이 맥의 기존 웹 빌드 열기

Tauri 패키징 도구가 검증한 정식·치트 웹 파일이 각각 `Desktop/Tauri/build/frontend/`, `Desktop/Tauri/build/validation/frontend/`에 있으면 다시 컴파일하지 않고도 로컬 PWA를 열 수 있다. 아래 명령은 저장소 루트에서 실행한다. 앞의 새 빌드 서버가 8773번 포트를 쓰고 있다면 먼저 종료한다.

```sh
python3 -m http.server 8773 --bind 127.0.0.1 --directory Desktop/Tauri/build
```

이 명령의 주소는 [로컬 정식판](http://127.0.0.1:8773/frontend/)과 [로컬 치트판](http://127.0.0.1:8773/validation/frontend/)이다. 열린 페이지에서 [PWA 설치](../docs/public/build-and-install.md#pwa-설치)를 따른다.

## 설치하고 실행

브라우저에서 주소를 열면 설치가 첫 동작으로 표시된다. 설치 대화상자를 제공하는 브라우저에서 **게임 설치**를 누른다. iPhone·iPad는 Safari 공유 메뉴에서 **홈 화면에 추가**를 선택한 뒤 **웹 앱으로 열기**를 켠다. Mac Safari는 파일 메뉴에서 **Dock에 추가**를 선택한다. [iPhone Safari](https://support.apple.com/guide/iphone/open-as-web-app-iphea86e5236/26/ios/26) · [Mac Safari](https://support.apple.com/104996) · [Chrome](https://support.google.com/chrome/answer/9658361)

홈 화면이나 앱 목록의 아이콘으로 연 뒤 **게임 시작**을 누른다. PWA는 iPad와 데스크톱에서 세로, 휴대폰에서 가로로 연다. 첫 실행에서는 파일을 받아 검증할 때까지 연결을 유지한다. 진행 막대는 검증까지 끝난 용량을 표시한다. 검증에 실패하면 **다시 시도**로 이어서 받는다. **브라우저에서 플레이**도 선택할 수 있다. 설치 안내를 열어 두면 저장 잠금을 차지하지 않는다.

게임은 WebAssembly, WebGL 2와 IndexedDB를 사용한다. 오프라인 실행은 HTTPS 또는 로컬 테스트 주소의 서비스 워커를 사용한다. 설치 메뉴는 브라우저와 OS가 제공한다. 예를 들어 Firefox의 웹 앱 설치는 지원되는 Windows 판본에서 제공한다. [Firefox 설치 조건](https://support.mozilla.org/en-US/kb/web-apps-firefox-windows) · [플랫폼별 설치 방식](https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps/Guides/Making_PWAs_installable)

## 실기기 설치 주소

휴대폰·태블릿의 Safari 또는 Chrome에서 공개 HTTPS 주소를 열어 [설치 절차](../docs/public/build-and-install.md#pwa-설치)를 따른다. 위 `127.0.0.1` 주소는 빌드한 맥의 로컬 검사 주소다. [공개 HTTPS 설치 주소](https://gelb2.github.io/HeavyMach2/) · [플랫폼별 확인 범위](../docs/canonical.md#플랫폼과-검증-현황)

## 저장과 화면

브라우저는 같은 오리진 안에서 정식·치트판에 다른 IndexedDB 저장소를 사용한다. 호스트·포트·스킴을 바꾸려면 먼저 기존 설치에서 [복구코드로 슬롯 전체를 백업](../docs/save-transfer.md)한다. 서비스 워커 캐시는 판본·설치 경로·빌드 버전으로 구분한다. 파일 길이와 SHA-256을 검증한 뒤 캐시에서 Wasm·에셋을 읽는다. 검증이 끝난 파일은 재시도에 사용한다. 저장은 IndexedDB 트랜잭션을 기다린다. Tauri에서는 해당 판의 웹 빌드가 `window.HM2Desktop`을 통해 macOS에 저장한다. 키는 `save`, `backup`, `hud/compact`, `hud/regular`다.

앱을 열 때 [기기 분류](Shell/display.js)를 수행한다. iPhone·iPad는 기기 정보와 터치, Android는 화면의 짧은 변 600 CSS 픽셀을 기준으로 판단한다. Mac 앱은 데스크톱이다. 정보가 부족하면 데스크톱으로 시작하는 이유를 화면에 알린다. 휴대폰 설치형 PWA는 양쪽 가로를 요청한다. OS가 세로 화면을 반환하면 게임을 일시 정지하고 가로 안내를 표시한다. 휴대폰 설치 매니페스트는 전체 화면을 요청한다. 기존 Android 설치 앱 등에서 브라우저가 허용하면 **게임 시작**으로 전체 화면을 연다. Safari 홈 화면 앱은 기본 상태 막대를 사용하고 iPadOS가 제공하는 표시 영역에 캔버스를 맞춘다. 전체 화면과 방향이 바뀌면 같은 캔버스 크기·안전 영역·입력 변환을 다시 계산한다. 태블릿의 검사 기준은 세로다. 뷰포트·안전 영역은 크기가 바뀌면 계산하고 기기 분류는 유지한다. 접이식 기기의 접힘·펼침 후 재분류는 [후속 검증](../docs/todo.md)에 둔다. 데스크톱은 원본 HD 패널에 조작부를 맞춘다.

효과음은 같은 [C++ PCM과 OpenAL 구현](../Sources/Core/Audio)으로 재생한다. 웹은 Emscripten의 OpenAL 장치를 사용하며 사용자의 시작 입력과 앱 활성 상태를 연결한다. MP3 배경음은 브라우저 출력 어댑터가 담당한다. 홈 화면으로 나가면 배경음을 정지하고 돌아오면 같은 곡이 정지한 지점에서 이어진다. [원인과 검사 기록](../docs/pwa-lifecycle-audit-20261121.md)

## 설치형 PWA 확인

정식판과 치트판을 각각 설치하고 홈 화면, 앱 목록 또는 Dock의 아이콘으로 시작한다. iPad와 데스크톱 PWA는 세로, 휴대폰 PWA는 가로로 연다. 주소창과 탭이 없는 게임 화면에서 아래 항목을 확인한다.

1. 앱 아이콘으로 시작해 Android 상태바·탐색 버튼과 카메라 쪽 여백을 확인한다. Safari 홈 화면 앱에서는 상태 막대 아래부터 화면 하단까지 그림이 이어지는지 확인한다. 홈에 나갔다 돌아온 뒤 화면이 줄어들거나 터치 위치가 어긋나는지 확인한다.
2. 첫 기지에서 **Shop**을 눌러 상점으로 들어간다. 상점의 **Exit**로 기지에 돌아오고, 기지의 **Exit**를 눌러 슬롯 화면으로 돌아온다.
3. 사령부와 전장을 왕복한다. 전장에서 이동 경로를 그리고 효과음을 확인한다. 기지·상점·사령부 화면의 여백과 배경 비율을 캡처한다.
4. 앱을 닫고 네트워크 없이 아이콘으로 다시 연다. 같은 슬롯의 저장을 확인한다. 기기, OS, 빌드 번호와 홈 화면 실행 모습을 기록한다.

## 동일한 게임 파일을 사용하는 시작 페이지 갱신

이미 배포한 빌드와 게임 파일 및 런타임 파일이 같은 경우 `Web/package.py`에 이전 배포의 `asset-manifest.json`을 `--compatible-with`로 넘길 수 있다. 패키저가 이전 배포와 파일 목록·크기·SHA-256을 대조한다. 서비스 워커는 검증이 끝난 패키지를 활성화하고, 다음 탐색에서 새 시작 페이지를 제공한다. 기존 iPad 홈 화면 아이콘은 설치 당시 상태 막대 설정을 보관하므로, 상태 막대 변경은 Safari에서 새 문서를 연 뒤 만든 아이콘으로 확인한다. [코드 비교와 Safari 실측](../docs/pwa-install-update-audit-20261123.md)

iPad PWA의 설치, 짧은 창의 HD 메뉴와 전장 검증 범위를 기록했다. Web 호스트는 HD 하단 버튼이 잘리는 태블릿 창에 기존 전체 맞춤 메뉴 배치를 사용한다. [iPad PWA 검사와 재현 목록](../docs/pwa-ipad-audit-20261123.md)

## 개발 중 자동 검사

```sh
npm --prefix Web ci
npm --prefix Web test
npm --prefix Web run test:download
npm --prefix Web run smoke
npm --prefix Web run smoke -- --asset-use
npm --prefix Web run smoke -- --phone-lifecycle
npm --prefix Web run smoke -- --install-ui
npm --prefix Web run smoke -- --installed
HM2_WEB_TEST_DEVICES=iphone,android-phone,ipad-desktop-site,desktop-portrait npm --prefix Web run smoke -- --installed-device-matrix
HM2_WEB_ENGINE=webkit npm --prefix Web run smoke -- --device-matrix --check-tablet-field-bottom
```

자동 검사는 새 Chrome 프로필과 로컬 HTTP 서버에서 시작한다. 결과 이미지와 JSON은 `.local/evidence/web-smoke-20261120`에 기록한다. `HM2_WEB_TEST_RECOVERY_FIXTURE`에 테스트용 복구코드 파일을 지정하면 가져온 코드를 다시 생성해 파일과 비교한다. `--asset-use`는 기지 화면이 준비된 뒤 텍스처 재읽기·재업로드 여부와 반복 입력에서 OpenAL 소스·버퍼 재사용을 확인한다. 같은 검사를 WebKit과 Firefox에서 실행하려면 `HM2_WEB_ENGINE`에 `webkit`이나 `firefox`를 지정한다. `--installed`는 Mac에 설치한 Chrome 앱의 저장과 오프라인 재실행을 확인한다. `--installed-device-matrix`의 기기 크기·터치는 Mac에서 모의한 값이다. 브라우저 탭과 Mac의 기기 모의 결과는 휴대폰·태블릿의 홈 화면 설치 확인에 쓰지 않는다.

`--check-tablet-field-bottom`은 일반 iPad 세로, 짧은 iPad 세로, 데스크톱 세로에서 Ruin Field를 열고 HUD 양쪽 하단의 검은 픽셀을 센다. 캡처와 픽셀 수는 PNG와 JSON에 남는다. [iPad PWA 조사](../docs/pwa-ipad-audit-20261123.md#카메라-하한과-기지-원본-추가-대조)
