# HeavyMach 2 정본과 작업 위치

최신 사용자 지시와 제공 원본이 우선한다. 이 문서는 각 요구를 소유하는 문서와 실제 생산자·소비자·검사로 연결한다. 날짜가 붙은 설치·계측·인계는 해당 실행의 기록으로 읽는다. 현재 구현과 최종 원작 동등성은 [기능별 근거](feature-verification.md)에서 구분한다.

| 업무 | 현행 기준 | 소비 코드와 검사 |
| --- | --- | --- |
| 화면 방향·표시·원본 그림·HUD | [디자인·에셋 정본](design-and-asset-rules.md) · [iPhone의 원작 참고 범위](iphone-development-contract.md) | [DisplayLayout](../Sources/Core/Presentation/DisplayLayout.cpp), [Screens](../Sources/Core/UI/Screens.cpp), [표시·입력 검사](../tests/DisplayLayoutTests.cpp) |
| 원본 자원·파츠·그래픽·음향 | [기술스택과 에셋](technology-and-assets.md) · [시간·저장·음향 계약](runtime-contract.md) | [캠페인 준비](../tools/prepare_campaign_assets.py), [텍스처 준비](../Sources/Core/Presentation/PreparedTiles.cpp), [음향 검사](../tests/AudioTests.cpp) |
| 플레이 진행·복구코드 | [플레이 안내](public/quick-play.md) · [저장 이동](save-transfer.md) | [GameSession](../Sources/Core/Session/GameSession.cpp), [복구코드](../Sources/Core/Data/RecoveryCode.cpp), [저장 검사](../tests/SaveTests.cpp) |
| 정식과 치트 검증 | [작업 순서](validation-build.md) · [기능별 근거](feature-verification.md) | [CMake 검사 등록](../CMakeLists.txt), [동시 실행](../Sources/Core/Validation/ConcurrentValidation.cpp), [브라우저 검증](../Web/tests/browser-smoke.mjs) |
| 구현 범위·이식 차이 | [복원 목표](restoration-goal.md) · [후속 TODO](todo.md) | [Android 코드](../Sources/Platform/Android), [웹 코드](../Sources/Platform/Web), [Tauri 호스트](../Desktop/Tauri/src-tauri/src/main.rs) |
| 원본 보존·비교판 | [원본 명세](source-provenance.json) · [호환 실행기](../Compatibility/README.md) · [검증 계획](reference-port-validation-plan.md) | [원본 보존 기준](runtime-preservation.md), [호환 패키지 검사](../tests/test_reference_package.py), [저장소 검사](../tools/check_no_ipa.py) |

## 플랫폼과 검증 현황

| 실행 환경 | 정식판과 치트판의 현재 근거 | 남은 범위 |
| --- | --- | --- |
| iPhone 가로·iPad 세로 | 두 실기기에 정식판과 기지 방문 저장을 설치했다. 사용자는 iPhone에서 화이트마운틴이 열린다고 확인했다. 포털 14방향은 시뮬레이터·호스트에서 검사했다. 치트 시뮬레이터는 기본 126조건·2,100프레임과 설원·공습 개별 검사를 실행했다. [정식](white-mountain-dual-release-20261120.md) · [치트](iphone-module-audit-20261118.md) · [설치](public/build-and-install.md) | 실제 iPad의 설원 출입과 사령부, 실기기 저장 이동 메뉴의 처음부터 끝까지 확인, 동일 시나리오의 전체 HD 일괄 대조 |
| Android | 공개 정식 APK는 v0.4, 치트 APK는 v0.1이다. 갤럭시에 두 앱을 따로 설치해 iPad 저장으로 상점과 전투를 확인했다. 같은 실기기에 설치한 정식 v0.4에서는 기존 슬롯과 코드 입력 화면을 확인했다. 정식 에뮬레이터에서는 복구코드 적용·백업·재실행을 확인했다. [빌드](../Android/README.md) · [기기 기록](../Android/검증-20261120.md) · [복구코드 검증](save-transfer.md#호스트별-확인-범위) | 갤럭시 시험 저장의 실제 교체, 장시간 플레이와 기종별 추가 대조 |
| 설치형 PWA | 공개 `v0.3`을 갤럭시의 설치 항목에 갱신했다. 홈 아이콘에서 시작해 전체 화면으로 열렸고 홈 복귀 뒤에도 전체 화면을 유지했다. 갤럭시의 배경음 연속 재생은 v0.2에서 확인했다. Mac Chrome의 정식·치트 독립 창에서 오프라인 재실행과 저장 유지를 확인했다. 휴대폰 가로·태블릿 세로·데스크톱이 현재 검증 기준이다. [설치](../Web/README.md) · [갤럭시 검사](pwa-lifecycle-audit-20261121.md#갤럭시-v03-설치-앱-확인) | 실물 iPhone·iPad, 갤럭시 반대 방향 손 회전, 접힘·분할 화면, 설치형 Safari의 오프라인 재시작 |
| Tauri Mac | 공개 ZIP을 받아 정식 앱의 서명을 검증했다. 실제 세로 창에서 새 슬롯과 저장, 기지에서 상점으로 갔다 돌아오는 동작을 확인했다. 정식·치트 앱은 별도 저장을 쓴다. [빌드](../Desktop/Tauri/README.md) · [창 검사](pwa-device-validation-20261120.md#tauri-mac-세로-창) | 정식판 사령부·음향과 치트판의 세로 창 직접 조작 |

공개 PWA와 앱 다운로드는 [버전과 배포](public/release-policy.md)에 둔다. 비공개 검사 보고는 [게시 기록](report-site.md)에 둔다. 원작 비교용 앱은 일반·치트 앱과 별도다.

2026-11-23의 iPad PWA 후보는 iPadOS 26.5 홈 화면 설치와 홈 복귀, 짧은 세로 창의 상점 나가기, 설치 앱의 오프라인 재실행을 별도로 검사했다. 공개판과 로컬 후보의 근거와 남은 화면 문제는 [iPad PWA 검사](pwa-ipad-audit-20261123.md)에 있다.

## 기록을 갱신하는 위치

- [GitHub 문서 목록](README.md)에서 게임 소개, 원작과 확장의 기능, 개발자의 변, 개발 과정으로 이동할 수 있다.
- [작업 자아비판문](development-self-review.md)과 [자아비판 사례](development-self-review-cases.md)에 착수 및 실패 재검사 절차를 둔다.
- [작업 정책](../.agents/policy.md)과 [사용자 결정 이력](../.agents/project.md)은 도구, 보존, 작업 범위를 설명한다.
- [복원 계획](restoration-plan.md), [초기 방향 계획](hd-landscape-plan.md), [이전 체크포인트](restoration-checkpoint-20261111.md)는 날짜별 기록이다. 현행 화면 요구는 위 표의 화면 정본을 따른다.
- [Android 초기 계획](android-port-plan-20261120.md)의 후속 구현은 Android 빌드·검증 문서에서 확인한다.
- [Apple 런타임 조사](iphone-apple-runtime-audit-20261114.md), [점프·경로 조사](iphone-path-jump-investigation-20261114.md), [원본 자원 로딩 조사](original-resource-loading-audit-20261114.md)는 구현 판단의 근거다.
- [호환 코드 고지](../Compatibility/THIRD-PARTY-NOTICES.md), [이전 코드 출처](source-reuse.md), [전체 참고 보고](report-site.md)를 보존한다.
- [31건 대조](code-document-audit-20261121.md)와 [수정·세 작품 에셋 소비 검사](code-document-fixes-20261121.md)에서 2026-11-21의 변경과 미수정 항목을 확인한다.
