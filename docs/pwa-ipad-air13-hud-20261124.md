# iPad Air 13 PWA 전장 HUD 크기

공개 v0.7에서 iPad Pro 11의 HUD는 네이티브와 같은 크기였다. iPad Air 13 시뮬레이터에서는 같은 Ruin Field의 HUD 배율이 약 2.3% 작았다. 세 버튼은 네이티브보다 왼쪽으로 15~22픽셀, 아래로 6픽셀 이동했다. HUD 양옆에는 전장 지형이 보였다. [v0.7 검사와 에셋 대조](pwa-ipad-hud-update-20261124.md)

iPadOS 26.5는 Air 13 홈 화면 앱의 창 크기를 1,024×1,366으로 보고했다. 기본 상태 막대 아래 웹 표시 영역은 1,024×1,334였다. v0.7은 이 짧은 높이를 공통 HD 배율 계산에 전달했다. 같은 파일을 그려도 높이 제한을 먼저 받는 Air 13에서 차이가 생겼다. [공통 HD 배율](../Sources/Core/Presentation/DisplayLayout.cpp) · [Apple의 상태 막대와 홈 화면 앱 설명](https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html)

웹 런타임은 전장 진입·이탈을 웹 호스트에 전달한다. iPad 홈 화면 앱이 화면 전체 폭을 쓰고, 세로 표시 높이에서 확인한 상태 막대 높이 32만큼이 빠진 전장에만 창 전체 높이를 좌표계로 적용한다. 캔버스 하단을 고정해 같은 좌표에서 화면과 터치를 처리한다. 슬롯·기지·상점·사령부는 기존 웹 표시 영역을 사용한다. [전장 상태](../Sources/Platform/Web/Runtime.cpp) · [창 크기 판정](../Web/Shell/display.js) · [캔버스와 입력](../Web/Shell/app.js)

갱신 검사에서는 이전 판의 시작 화면이 새 서비스 워커의 활성화 중에 다시 탐색했고 WebKit의 활성화가 끝나지 않았다. 새 워커는 활성화 전에 기존 창의 파일 판본을 기록한다. 기존 플레이 창은 이미 시작한 판으로 계속 실행하고, 다시 연 창은 새 판으로 시작한다. [서비스 워커 전환](../Web/Shell/sw.js) · [서비스 워커 활성화 규격](https://www.w3.org/TR/service-workers/#activation-algorithm)

## 실행 기록

| 환경 | 측정 결과 |
| --- | --- |
| iPad Pro 11 정식 네이티브와 설치형 v0.7 | 1,668×2,420 픽셀 Ruin Field에서 `MENU`, `MISSION`, `ITEM` 글자 정렬 위치 차이 0픽셀. |
| iPad Air 13 정식 네이티브와 공개 v0.7 | 2,048×2,732 픽셀 Ruin Field에서 세 버튼의 가로 차이 −15, −18, −22픽셀, 세로 차이 +6픽셀. |
| iPad Air 13 홈 화면 정식 후보 | 세 버튼의 가로·세로 차이 모두 0픽셀. 인벤토리·임무·시스템 메뉴, 전장 이동, 가로 안내와 세로 복귀를 확인했다. |
| iPad Air 13 홈 화면 치트 후보 | 세 버튼의 가로·세로 차이 모두 0픽셀. 가로 안내, 세로 복귀, 인벤토리·임무·시스템 메뉴와 경로 이동을 확인했다. |
| WebKit 크기 모의 | iPad 전체·작은 창과 데스크톱에서 기지·상점·사령부 왕복, 전장 포털과 세로 복귀를 확인했다. |
| WebKit과 Chromium 갱신 | v0.7 플레이 창을 둔 채 기존 시작 화면과 새 탐색에서 새 판을 확인했다. 서버를 끈 뒤 새 판으로 실행했다. Chromium에서는 v0.7에서 만든 28,204바이트 저장의 SHA-256도 같았다. |

실물 iPad Air 13은 페어링됐다. iPadOS가 잠금 상태에서 원격 앱 실행을 거부해 실물 HUD 비교는 시뮬레이터 결과에 포함하지 않았다.

로컬 증거는 `.local/evidence/ipad-pwa-hotfix.BdRMxq/v07-verification/`에 있다. `ipad-air13-hud-before-after.png`는 원본 캡처의 하단을 같은 배율로 잘라 비교한다. `ipad-air13-hud-candidate-comparison.json`은 픽셀 측정이다. `ipad-air13-pwa-native-compare.log`, `ipad-air13-field-final-normal.log`, `ipad-air13-field-candidate-cheat.log`는 화면 실행 기록이다. 갱신 과정은 `ipad-air13-hud-final-update-webkit.log`와 `ipad-air13-hud-final-update-save.log`에 남겼다. 보호된 Apple·Android·공통 소스와 에셋에 Git 변경은 없다.
