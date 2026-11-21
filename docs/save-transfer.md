# 복구코드로 저장 옮기기

[플레이 안내](public/quick-play.md) · [플랫폼과 설치](canonical.md#플랫폼과-검증-현황)

복구코드는 **슬롯 세 개 전체**를 옮긴다. 임무·기지·지도·레벨·경험치·크레딧·보유 아이템과 장비가 들어간다. HUD 배치 설정은 각 앱의 별도 기록이다. 한 기기에서 이어서 플레이한 진행을 다른 기기에 옮기려면 새 코드를 만든다.

1. 원본 앱의 슬롯 화면에서 **저장 이동**을 연다. iPhone·iPad·Android에서는 **코드 복사**, PWA·Tauri에서는 **코드 만들기 → 복사**를 누른다. 코드 전체를 다른 기기에서도 볼 수 있는 개인 메모에 보관한다.
2. 받을 앱에 보존할 진행이 있으면 같은 방법으로 먼저 그 앱의 코드를 만든다.
3. 받을 앱에서 **저장 이동**을 열어 코드를 붙여넣는다. PWA·Tauri는 **코드 확인 → 슬롯 교체**를 누른다. Apple·Android는 **코드 입력** 뒤 화면의 확인 순서를 따른다.
4. 교체한 슬롯을 골라 **OK**로 시작한다.

PWA 저장은 브라우저 오리진의 IndexedDB에 있다. 스킴·호스트·포트를 바꾸려면 **기존 설치 주소의 앱**에서 코드를 먼저 복사한다. 같은 오리진에서는 정식판과 치트판의 저장 이름이 다르다. 복구코드는 운영체제 사이에서 옮길 수 있으며 저장을 실시간으로 동기화하지 않는다.

공통 형식은 `HM2-1-`이다. 다른 형식, 잘린 코드와 현재 앱에 없는 맵은 교체 전에 거부한다. 현재 내장 캠페인과 호환되는 빌드를 사용한다. 슬롯 메뉴가 없다면 기존 앱의 저장을 유지하며 기능이 들어간 빌드로 갱신한다.

## 개발과 확인

[RecoveryCode.cpp](../Sources/Core/Data/RecoveryCode.cpp)는 저장 전체를 인코딩하고, [GameSession.cpp](../Sources/Core/Session/GameSession.cpp)는 코드 검사 → 기존 기록 백업 → 저장 교체 → 세션 갱신을 담당한다. Apple·Android는 OS 입력과 앱 전용 파일을 연결한다. 웹은 [store](../Web/Shell/storage.js)의 트랜잭션을 기다리고, Mac은 [Tauri 저장 어댑터](../Desktop/Tauri/src-tauri/src/storage.rs)의 교체 결과를 기다린다.

[코덱 검사](../tests/SaveTests.cpp)와 [세션 검사](../tests/SessionTests.cpp)를 사용한다.

## 호스트별 확인 범위

| 호스트 | 확인한 동작 | 남은 확인 |
| --- | --- | --- |
| iPhone·iPad | 두 시뮬레이터의 Apple 런타임에서 저장 이동 버튼, 검증·교체·복원 전 백업·새 런타임의 슬롯을 확인했다. | 시스템 대화상자를 통한 복사·붙여넣기·교체 전체 검사. XCTest의 기기 지정 단계에 실패한 기록이 있다. |
| Android | 정식·치트 에뮬레이터 앱에서 잘못된 코드 거부, 유효 코드 취소 시 저장 보존, 두 시험 슬롯 복원과 재실행을 확인했다. | 갤럭시에서 최신 복구 UI로 코드 이동. 기존 갤럭시 설치 검사는 [iPad 파일 저장 전체 적용](../Android/검증-20261120.md#ipad-사용자-저장과-갤럭시-플레이) 범위다. |
| PWA·Tauri | 웹과 Mac에서 같은 검사용 코드를 가져온 뒤 다시 생성했다. 브라우저에서는 정식·치트 저장 분리도 검사했다. [웹 왕복 기록](pwa-device-validation-20261120.md) · [최신 웹·Mac 검사](code-document-fixes-20261121.md#실행-근거) | 실기기 PWA 설치 주소를 사용한 복구와 플레이. 최신 Tauri 정식·치트 세로 창에서의 직접 조작. |

네이티브 성공 로그는 `.local/evidence/native-recovery-20261120/`의 `apple-style-iphone-result.txt`·`apple-style-ipad-result.txt`와 `android-release-style.log`·`android-validation-style.log`에 있다. 실제 기기와 PWA 오리진 이동은 [TODO](todo.md#현재-체크--2026-11-21)에 남아 있다.
