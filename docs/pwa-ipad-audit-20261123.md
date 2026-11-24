# iPad PWA 화면·설치 검사 — 2026-11-23

작업 브랜치는 `docs/mandatory-self-review`다. Apple·Android·공통 코어와 게임 에셋은 같은 Git 상태를 유지했다. 이 문서는 로컬 후보와 설치 시뮬레이터에서 확인한 범위를 기록한다.

## 재현과 수정

| 증상 | 재현과 원인 | 변경 |
| --- | --- | --- |
| 홈 화면에 다시 추가해도 전장 아래에 웹 배경 띠가 남음 | Safari에 v0.3 설치 문서를 보관한 워커가 남으면 v0.5 서버에서도 같은 HTML을 제공했다. iPadOS는 설치 당시 상태 막대 설정을 아이콘에 저장한다. | 게임 파일의 목록·크기·해시가 일치하는 버전에서 새 HTML을 검증한 뒤 워커를 활성화한다. Safari가 다시 해당 주소를 열면 새 HTML을 받는다. 새로 추가한 아이콘의 전장 아래에서 웹 배경 띠가 0행이었다. [이전 캐시를 둔 설치 시험](pwa-install-update-audit-20261123.md) |
| 가로·짧은 세로 창에서 상점 나가기 버튼을 누를 수 없음 | iPad WebKit의 1,194×834에서 버튼 중심의 아래 기준 Y는 −298.156이었다. 사용자 캡처와 종횡비가 같은 850×975 세로 창에서도 Y는 −21.6146이었다. 공통 HD 배치가 폭에 맞춰 위·아래를 잘랐다. | Web 호스트에서 HD 버튼의 원본 하단 여백까지 잘리는 창에 기존 HD 전체 맞춤 배치를 연결했다. 기지·상점·사령부의 그림과 터치는 같은 공통 변환을 쓴다. 상점·사령부의 나가기 버튼을 눌러 기지로 돌아왔다. [Web 호스트](../Sources/Platform/Web/Runtime.cpp) · [브라우저 검사](../Web/tests/browser-smoke.mjs) |

HD 상점·사령부의 나가기 버튼은 원본 좌표 Y=52에 높이 64다. 버튼의 아래쪽까지 잘리는 조건은 HD 768×1,024의 아래 여백 20을 사용해 계산한다. HD 1.0.3 IPA에서 확인한 버튼, 기지 그림과 기지 테이블은 준비된 파일과 바이트 단위로 일치했다. 정규 iPad 세로 상점 1,668×2,388의 변경 전후 PNG를 RGB로 비교한 결과가 일치했다. 좁은 창의 배경은 Web의 기존 배경과 패널 분리 그리기를 재사용한다. 넓은 창 시험에서도 월드·카메라·전장 HUD는 기존 iPad 표시 구성을 유지했다. 근거는 `original-hd-art-check.txt`, `pixel-audit.txt`와 `ipad-windowed-shop-before-after.png`다.

## 실행 범위와 근거

새 Web 빌드는 정식 `07a4388653167c5e765e56fa`, 치트 `62c43de0d5efdb6a5b97f12d`다. 설치 메타데이터만 바꾼 호환 후보는 `7dbde85598365346e5bda64c`다. 이 후보는 공개 v0.3·v0.5와 게임 파일이 바이트 단위로 일치한다. 메뉴 변경은 WASM을 바꾼다. 두 변경을 배포할 경우 호환 후보를 먼저 제공하고, 메뉴 변경이 있는 빌드는 기존 워커의 대기 절차로 갱신한다.

| 실행 | 확인 항목 | 근거 파일 |
| --- | --- | --- |
| iPadOS 26.5 Safari 홈 화면 설치, iPad Air 13 · Pro 11 시뮬레이터 | Air 13은 이전 캐시를 둔 재설치, Pro 11은 새 설치. 실제 실행에서 새 슬롯, 기지, Ruin Field, 이동, 인벤토리·임무·시스템 버튼을 확인했다. Pro 11 전장 하단 웹 배경 0행. | `ipad-v06-cache-install.log`, `ipad11-fresh.log`, `ipad11-fresh-frames/manifest.json` |
| 설치형 PWA의 홈 복귀 | Pro 11과 미니에서 전장 도중 홈으로 나간 뒤 아이콘으로 복귀했다. 복귀한 화면에서 HUD 버튼과 이동을 사용했다. Pro 11의 첫 실행은 빈 화면에서 실패했고 Web App 프로세스를 종료한 뒤 재시도해 통과했다. iPad 9의 이번 설치 자동화는 Safari 공유 버튼이 사라져 게임 실행에 도달하지 않았다. | `ipad11-reopen.log`, `mini-resume.log`, `ipad-resume-comparison.png`, `ipad11-resume.log`, `ipad9-retry.log` |
| Playwright WebKit 태블릿 기기 모의 | 정규 세로·짧은 세로·데스크톱 UA·가로 변경의 4/4에서 화면, 나가기 터치, 복구코드 UI와 이동 경로를 확인했다. 같은 폭에서 높이만 바꾸는 Playwright WebKit 시험에서는 실제 visual viewport 높이가 갱신되지 않았다. WebKit 모의는 폭도 1px 바꿔 시험했다. | `candidate/webkit-final-extra.log`, `candidate/webkit-final/webkit-devices.json`과 캡처 |
| iPad 크기 WebKit 치트판 | 캠페인 목록의 전장 44개 진입 44/44. 여기에는 화이트마운틴 설원 6개가 포함된다. 치트 경로에서 자원을 열고 프레임을 진행했다. | `candidate/ipad-campaign.log`, `candidate/campaign/webkit-campaign.json`과 설원 6장 |
| Mac의 오프라인 실행 | Chrome 설치 앱 정식·치트 2/2에서 오프라인 재실행과 저장을 확인했다. WebKit은 서버를 종료한 뒤 새 창에서 정식판을 시작했다. Playwright의 WebKit 오프라인 모의 옵션 자체에서도 별도의 실패를 재현했다. 캐시 호환 갱신과 첫 다운로드 파일 손상·재시도를 따로 검사했다. | `candidate/chromium-final.log`, `candidate/webkit-offline-server-stop.log`, `candidate/webkit-offline.log`, `upgrade-baseline.log`, `download.log` |
| Pro 11 설치형 오프라인 재실행 | 서버를 끈 첫 재실행에서 Safari 접속 오류가 나왔다. 게임 파일 없이 503만 응답하는 서버에서 재시작한 뒤 이전 슬롯과 전장을 열었다. 서버를 다시 완전히 끈 시험에서도 이전 슬롯을 열고 전장·HUD·이동을 재검사했다. 최초 실패 원인은 확인 범위를 넓혀야 한다. | `ipad11-offline.log`, `pro11-offline-now.png`, `ipad11-offline-503.log`, `ipad11-origin-503.log`, `ipad11-offline-repeat.log`, `ipad11-offline-repeat-origin.log` |
| 호스트 음향·휴대폰 회귀 | Node 단위 검사 7/7. Chromium의 OpenAL 메뉴 효과음에서 실제 재생 위치가 증가했고, 준비한 소스·버퍼와 텍스처를 재사용했다. 휴대폰 모의에서 90도·270도, 홈 복귀 두 번과 같은 배경음 재개를 확인했다. | `candidate/asset-use.log`, `candidate/phone-regression.log` |

Playwright WebKit의 오프라인 모의 옵션에는 워커에 네트워크가 필요 없는 응답도 막는 문제가 보고돼 있다. 서버만 종료하는 비교는 이 옵션을 쓰지 않았다. [Playwright 재현과 검토](https://github.com/microsoft/playwright/issues/42775)

## 재현이 남은 항목

- **정규 iPad 세로 기지 제목.** 834×1,194에서 기지 제목의 왼쪽이 잘린다. 제목은 기지 원본 그림에 들어 있고 공통 HD 배치가 폭을 잘라낸다. HD 원본 포팅 실행의 보관 캡처는 제목 전체를 보여 준다. 사용자 지정 보호 범위 안에서 원본 그림·제목·시설 버튼을 함께 유지할 배치가 필요하다. 증거는 `candidate/webkit-final/webkit-ipad-base.png`와 기존 `reference-port/hd-on-iphone-base.png`다. [이전 조사](code-document-fixes-20261121.md) · [원본 HD 포팅판 조사](hd-original-function-audit-20261111.md)
- **iPad 창 전장 하단의 검은 지도 영역.** 수정 후 1,194×834에서 양쪽 하단 76 CSS 픽셀, 850×975에서 89.5 CSS 픽셀이 검정이다. 정상 세로 설치에서 사라진 웹 배경 띠와 별개다. 캔버스가 전체 창을 채운 상태에서 공통 카메라가 지도 아래까지 이동을 허용한다. 지면 스프라이트가 없는 영역에 WebGL 배경이 드러난다. 공통 카메라·HUD·입력의 위치를 함께 검증해야 한다. 증거는 `candidate/webkit-final/webkit-ipad-resized-field.png`와 `pixel-audit.txt`다. [카메라 계산](../Sources/Core/Simulation/CombatScene.cpp) · [카메라 하한](../Sources/Core/Simulation/WorldNavigation.cpp)

실물 iPad의 공개 HTTPS 설치·오프라인 재실행과 실제 스피커 음향은 별도다. 공개 주소는 v0.5다.

## 카메라 하한과 기지 원본 추가 대조

Ruin Field의 너비 5%와 95%에서 맨 아래부터 이어지는 검은 행을 센다. WebKit 치트판에서 짧은 창으로 바꾸고, 상점·사령부에서 돌아온 뒤 실제 버튼으로 전장에 진입했다. `--check-tablet-field-bottom`을 붙이면 일반 조작 검사와 별도로 양쪽에 띠가 남은 전장을 실패로 기록한다. 가로 창에서 76 CSS 픽셀, 짧은 세로 창에서 89.5 CSS 픽셀을 검출해 두 조건 모두 실패했다. 캡처와 물리·CSS 픽셀 수는 `boundary-followup/webkit-field/`에 있다. [실행 명령](../Web/README.md#개발-중-자동-검사) · [실제 캡처 검사](../Web/tests/browser-smoke.mjs)

| 전장 창 | 기본 태블릿 정책의 월드 배율 | 기존 화면 채움 정책의 고정 배율 | 기본 정책의 입력 하한 | 화면 채움 정책의 입력 하한 |
| --- | --- | --- | --- | --- |
| 850×975 | 1.10677 | 1.00000 | 86.03 월드 단위 | 0 |
| 1,194×834 | 1.55469 | 1.16830 | 52.39 월드 단위 | 0 |

같은 공통 코어에 연결한 카메라 진단에서, 배율과 입력 하한을 유지하고 카메라의 아래쪽 여유만 0으로 놓은 경우 두 창 모두 지형 아래쪽이 화면 하단에 닿았다. 공통 코드가 제공하는 기존 화면 채움 정책은 이 여유와 함께 배율·터치 하한도 바꾼다. 변경에 필요한 입력은 **태블릿 HD 배율과 터치 변환을 유지하면서 지도 아래쪽 여유만 끄는 옵션**이다. 현재 Web 호스트가 전달하는 공개 표시 구성에는 이 옵션이 없다. 진단은 `boundary-followup/camera-policy-probe.jsonl`에 보관했다. [월드 카메라 구성](../Sources/Core/Simulation/CombatScene.cpp) · [뷰포트에 맞는 카메라 경계](../Sources/Core/Simulation/WorldNavigation.cpp)

기지 원본은 768×1,024다. WebKit의 834×1,194 창에서 그림을 같은 비율로 화면 전체에 맞추면 가로 양쪽에서 각각 30.75 CSS 픽셀을 자른다. 그림 전체를 보이면 상하 각각 41 CSS 픽셀이 비게 된다. 보관된 HD 원본 포팅 캡처는 제목 전체를 보여 준다. 원본 제목과 PWA 캡처를 `boundary-followup/base-title-original-and-pwa.png`에 붙였다. 제목과 시설 버튼을 같은 원본 좌표에 유지하면서 빈 공간도 없애려면, 원본의 시각 요소를 가리지 않는 배경 구성에 대한 근거가 필요하다. 한 장짜리 원본을 그대로 그리는 두 크기 계산으로는 이 조건을 동시에 충족할 수 없다. [공통 그림·입력 변환](../Sources/Core/Presentation/DisplayLayout.cpp) · [W3C 크기 계산](https://www.w3.org/TR/css-images-3/#cover-contain)

## 네이티브와 Web의 동일 조건 비교

iPad Pro 11 시뮬레이터에서 복원판 네이티브 렌더러의 프레임버퍼를 캡처했다. WebKit에서는 별도 치트 빌드로 같은 공통 코어와 Web 렌더러를 실행했다. 양쪽 모두 Ruin Field의 0번 게이트, 같은 초기 장비와 위치, 배율 2의 출력, 여백 0을 사용했다. 외부 진단에서 논리 화면 크기를 주입했으며 네이티브 앱의 지원 방향이나 OS 창 정책은 바꾸지 않았다. 플레이어가 멈춘 상태에서 진입 직후, 60틱과 180틱을 비교했다.

| 화면 | 네이티브 검은 행 | WebKit 검은 행 |
| --- | --- | --- |
| 834×1,194 | 0 → 0 → 0 | 0 → 0 → 0 |
| 850×975 | 48.5 → 89.5 → 89.5 | 48.5 → 89.5 → 89.5 |
| 1,194×834 | 0 → 76 → 76 | 0 → 76 → 76 |

단위는 양쪽 측정 열에서 읽은 논리 픽셀이다. 9/9 프레임에서 검은 행 수가 일치했고 플레이어 위치·카메라 중심·배율·지형 하단도 소수점 기록 정밀도 안에서 일치했다. 일반 세로에서는 지도 아래 구간을 HD HUD가 덮는다. 짧은 창에서는 HUD 양옆으로 지도 아래가 드러난다. 차이는 렌더러 진입 전에 정해지는 공통 HD 표시 조건에서 생긴다. 이 결과는 Web 전용 오류라는 앞선 설명의 범위를 바로잡는다. 증거는 `matched-renderers/comparison.json`, `matched-renderers/matched-850x975.png`, `matched-renderers/matched-1194x834.png`다. [원래 카메라 구성](../Sources/Core/Simulation/CombatScene.cpp) · [Web 렌더러](../Sources/Platform/Web/Renderer.cpp) · [네이티브 렌더러](../Sources/Platform/Apple/HM2Renderer.mm)

## 2026-11-24 세로 PWA 전장 수정

iPad와 데스크톱 PWA를 세로, 휴대폰 PWA를 가로로 검사한다. 설치 매니페스트도 각 방향을 지정한다. 창 방향을 적용하는 동작은 OS와 브라우저가 제공한다. 위 가로 iPad 캡처는 원인 조사에 사용한 기록이다.

Web 호스트는 원본 HD 3:4보다 짧은 세로 PWA 전장에서 공통 코드의 지도 경계 제한과 작은 화면용 전장 HUD를 함께 사용한다. HUD도 같은 창에 맞춰야 시작 위치의 메크가 가리지 않는다. 고정 카메라가 창별 배율을 정하고 화면 좌표와 터치 역변환을 함께 계산한다. 표적·이동에 따른 줌은 이 분기에서 꺼져 있다. 일반 세로 화면, 기지·상점·사령부, 네이티브와 Tauri는 기존 표시 설정을 사용한다. [Web 호스트](../Sources/Platform/Web/Runtime.cpp) · [전장 출력과 입력](../Sources/Core/Session/GameSession.cpp) · [공통 카메라](../Sources/Core/Presentation/CameraPresentation.cpp)

Ruin Field의 같은 초기 상태와 180틱에서 850×975 하단 검은 띠는 89.5 CSS 픽셀에서 0으로 줄었다. 834×1,194 전장의 전체 RGB 픽셀은 수정 전후 같았다. 850×975의 기본 월드 배율은 1.10677에서 1.0으로 바뀌었고, 카메라의 지도 아래 여유는 0이 됐다. `field-fix/ipad-portrait-field-before-after.png`, `field-fix/regular-portrait-pixel-diff.txt`, `field-fix/matched-test.log`에 결과를 보관했다.

세로 WebKit 3/3과 Mac Chrome의 설치형 데스크톱 PWA 1/1에서 하단 양쪽을 0행으로 확인했다. 각 조건에서 상점과 사령부를 나왔고, 전장에서 경로를 그려 이동한 다음 복귀 포털을 눌러 기지로 돌아갔다. 다시 전장에 들어가 원래 세로 크기로 돌렸다. 휴대폰 Chromium 검사에서 양쪽 가로 방향과 복귀를 확인했다. iPad Pro 11 시뮬레이터에서는 같은 설치 매니페스트의 첫 카메라 후보 정식판을 홈 화면에 추가해 일반 세로 전장·HUD·홈 복귀를 확인했다. 짧은 세로 창의 최종 HUD는 WebKit과 Mac Chrome 설치 앱에서 확인했다. `field-fix/portrait-final.log`, `field-fix/desktop-final.log`, `field-fix/phone-lifecycle.log`, `field-fix/ipad11-installed.log` 참조.

최종 로컬 빌드는 정식 `da4c665d94d2b47c40a11545`와 치트 `c4fb89ea0e66fbffa458e9a5`다. 실물 iPad 검사와 기지 제목 잘림은 열린 항목이다. 공개판은 이 작업에서 갱신하지 않았다.
