# HeavyMach 2 코드·문서 대조 — 2026-11-21

작업 브랜치 `feat/recovery-pwa-tauri-20261120`의 `fca7406`에서 **31건**을 찾았다. 조사 기준의 `origin/develop`은 `e2e2c27`이며, 작업 브랜치가 36커밋 앞서 있었다. 설치·실행 근거는 해당 빌드와 검증 환경을 함께 적었다.

가장 큰 차이는 **웹 음향의 기술 계약**, **빌드 안내에 적힌 검사 자원 경로**, **복구코드와 세 플랫폼의 현재 상태**다. 파일·제목 위치로 가는 저장소 로컬 링크 737건은 모두 열 수 있다. 링크가 살아 있어도 그 문서가 현재 구현과 다른 경우가 많다.

## 정본과 문서 간 참조

| 번호 | 발견 | 확인한 근거와 영향 |
| --- | --- | --- |
| 01 | 현행 문서를 한곳에서 고르는 기준이 없다. | `AGENTS.md` 1–5행은 처음 읽을 파일을 나열한다. 화면·아트 요구에는 `design-and-asset-rules.md`, 실행에는 `runtime-contract.md`가 있다. 제품 전체에 대해 요구·설계·사용 안내·현재 상태·과거 증거를 구분하고 담당 문서와 소비 코드를 연결하는 색인은 없다. 파일 이름이 없어도 대체할 연결표가 있는지 확인했다. |
| 02 | 허브가 이미 있는 HeavyMach 2 저장소를 안내하지 않는다. | 허브의 `docs/project-workspace.md` 10행은 HeavyMach 2 저장소를 아직 만들지 않았다고 적는다. 허브 `.gitmodules`에는 HeavyMach 2 항목이 없고, 실제 작업 사본은 별도 Git 저장소다. 허브의 HeavyMach 연결을 따라가면 HeavyMach 1의 정본을 읽게 된다. 허브 등록 방식과 현재 로컬 위치를 문서에서 먼저 구분해야 한다. |
| 03 | 과거의 작업 보류가 현행 작업 범위로 남아 있다. | `restoration-plan.md` 11–25행, `restoration-goal.md` 3행, `validation-build.md` 첫 절, `.agents/project.md` 63–73행은 iOS 우선 단계의 플랫폼·설치·일부 병합 보류를 현재 시제로 남긴다. `Android/검증-20261120.md`, `pwa-device-validation-20261120.md`, `white-mountain-dual-release-20261120.md`에는 이후 사용자가 지시한 구현과 설치 기록이 있다. 새 작업을 시작할 때 날짜별 승인과 항구적인 제품 제약을 구분하기 어렵다. |
| 04 | 방향과 HUD 계획이 현행 정책에 연결되지 않는다. | `hd-landscape-plan.md` 22·27·44행의 좌우 여백 시험, iPhone 세로·iPad 가로 확장, 하단 고정 HUD는 초기 계획이다. `design-and-asset-rules.md`의 전체 화면·이동 HUD 규칙과 `pwa-wasm-audit-20261120.md`의 태블릿 가로 후속 범위로 넘어가는 표시가 없다. Apple 실제 방향은 `HM2Application.mm`의 iPhone 가로·iPad 세로에서 확인했다. |
| 05 | iPhone 상점의 승인 예외가 정본 내부 표와 다른 제품 문서에 반영되지 않았다. | `design-and-asset-rules.md` 19행은 11월 19일 상점·정비 격납고 편집을 허용한다. 같은 문서 52행과 `technology-and-assets.md` 3·67행은 그대로 Phone 원본 배경이라고 적는다. `Screens.cpp::phoneShop`은 `PhoneShopHangar.jpg`와 분리한 패널을 읽는다. 원본 Phone 버튼·HD 상품 그림과 편집한 배경의 출처를 별도로 표시해야 실제 배경을 잘못 교체하지 않는다. |
| 06 | TODO에 구현 여부와 검증 여부가 같은 체크박스로 묶여 있다. | `todo.md` 12·17–23·25–43행에서 정책, 계획, 구현, 남은 실기기 검사가 섞인다. 복구코드·분류·다운로드 진행 막대는 코드와 일부 실행 근거가 있다. 실기기 왕복, 접이식 기기, 오프라인 갱신 등은 남아 있다. 현재 개발자가 체크 표시만으로 어느 일을 다시 해야 할지 알 수 없다. |
| 07 | Android 운영 안내와 런타임 근거 일부가 진입 문서에서 연결되지 않는다. | README 두 판과 AGENTS에서 Markdown 링크만 따라가면 추적 중인 1차 문서 111개 중 11개에 닿지 않는다. Android README, 갤럭시 검증, Android 계획, 원본 자원 로딩 조사, iPhone 런타임·경로 조사, 런타임 보존 기준이 포함된다. `.agents` 두 파일은 AGENTS가 인라인 경로로 지시하므로 도달성 수치에만 포함했다. `Compatibility/THIRD-PARTY-NOTICES.md`와 과거 체크포인트는 보존 문서로 남길 수 있다. |
| 08 | 다른 프로젝트의 코드·문서 인용이 현재 저장소 경로처럼 보인다. | `technology-and-assets.md` 14행, `iphone-fixes-and-profiling-plan-20261114.md` 75–76행, `full-validation-development-plan-20261110.md` 101행에 다른 저장소의 파일이 인라인으로만 나온다. 경로 문자열이 현재 저장소에 없다는 이유로 끊긴 링크로 세지는 않았다. 출처 저장소와 고정 커밋을 같은 인용에 연결하면 복사 대상과 HM2 소비자를 구분할 수 있다. |
| 09 | 전체 기능의 완료 판정표가 없다. | `restoration-goal.md`와 `validation-build.md`는 원본 기능·버튼·데이터 ID부터 코드·치트 조건·정식 앱·원본 대조까지 연결하라고 한다. `full-validation-development-plan-20261110.md`의 목록과 이후 White Mountain·플랫폼별 보고에는 각각 근거가 있으나 현재 전체를 역추적할 수 있는 표가 없다. 126조건과 개별 캠페인·플랫폼 검사를 합산해 전체 완료를 판정할 수 없다. |
| 10 | 갤럭시 검증의 사이트 게시 이력이 제품 기록에서 누락됐다. | 기존 누적 사이트의 버전 322와 소스 `ae8082e`의 목록에는 갤럭시 검증 보고가 연결됐다. `Android/검증-20261120.md`에는 실제 보고 링크가 있다. 제품의 `docs/report-site.md`에는 이 게시 건·버전·근거로 가는 항목이 없었다. 원격 게시 상태 및 제품 브랜치 병합 상태를 각각 찾을 수 있어야 한다. |

## 빌드·설치·검증 안내

| 번호 | 발견 | 확인한 근거와 영향 |
| --- | --- | --- |
| 11 | 한·영 README와 설치 안내가 Android·PWA·타우리 빌드가 없다고 적는다. | `README.md`·`README.en.md` 27–34행 및 `docs/public/build-and-install.md` 158–162행과 `Android/app/build.gradle.kts`, `Web/CMakeLists.txt`, `Desktop/Tauri/src-tauri/tauri.conf.json`을 대조했다. 갤럭시 정식·치트 설치, iPhone 설치형 PWA 시뮬레이터, PWA와 타우리 로컬 빌드 근거가 있다. 공개 배포 여부와 저장소에서 빌드 가능한지 여부를 따로 보여 줘야 한다. |
| 12 | Apple 안내의 첫 화면이 현재 iPhone 정식 설치·설원 진행을 찾게 해 주지 않는다. | README와 `docs/public/{build-and-install,quick-play,validation}.md`의 일반 플레이·정식 설치·검증 안내는 11월 12–13일 iPad 결과를 중심으로 한다. `white-mountain-dual-release-20261120.md`에는 iPhone과 iPad 실기기 Release, 기지 방문 저장과 포털 확인이 있다. 11월 12일 수치는 당시 결과로 유효하며, 현재 설치본으로 안내할 별도 입구가 빠져 있다. |
| 13 | README의 원본 준비물 목록이 빌드 명령과 맞지 않는다. | 한·영 README 48행은 HD 1.0.3만 원본 준비물로 열거한다. 실제 순서의 `prepare_phone_menu_assets.py`는 일반판 2.0.1을, `prepare_campaign_assets.py`는 HD 1.0.0 설원 그림도 사용한다. Apple 설치 안내의 상세 목록에는 세 판본이 있다. Web·Android 빌드 안내도 준비된 캠페인 폴더를 입력으로 사용한다. |
| 14 | 존재하지 않는 로컬 커밋 의존성을 안내한다. | `docs/public/build-and-install.md` 12행은 Git 훅이 gitleaks를 쓴다고 한다. 추적 중인 `.githooks` 네 개는 LFS 호출만 하고 pre-commit은 없다. `AGENTS.md` 6항과 일치하는 쪽은 현재 훅이다. CI의 IPA 검사와 Python 검사는 `.github/workflows/repository.yml`에서 별도로 실행한다. |
| 15 | 공통 검사 명령이 현재 캠페인에 맞지 않는 자원 경로를 지정한다. | `docs/public/build-and-install.md` 135–139행은 `.local/prepared/HD`를 지정한다. 해당 입력으로 `ScreensTests`는 기지 버튼 수 단언에서, `SessionTests`는 복구코드의 맵 검증에서 실패했다. 같은 실행 파일에 `.local/prepared/Campaign/HD`를 지정하자 두 검사 모두 통과했다. `DisplayLayoutTests`와 `HelpSessionTests`는 양쪽에서 통과했다. 근거는 아래 재현 절차에 있다. |
| 16 | 현재 Apple 치트 실행 시간·조건 수가 오래된 수치로 안내된다. | `docs/public/build-and-install.md` 115·154행과 `validation-build.md`의 현행 절은 115조건·1,200프레임을 안내한다. `HM2Application.mm` 84–97행은 표시 구성이 켜진 앱에서 `displayValidationFrames`를 사용하고, `ConcurrentValidation.hpp` 19행은 이 경우 2,100프레임을 반환한다. 126조건·2,100프레임의 양 기기 결과는 `iphone-module-audit-20261118.md`에 있다. 11월 12일의 115조건 결과는 그대로 보존할 수 있다. |
| 17 | 일반 안내에 Apple 공습 검사 모드가 없다. | 설치 문서의 `HM2_WHITE_MOUNTAIN_VALIDATION` 표는 1–3만 설명한다. `HM2Application.mm` 53–58행은 4를 `AirStrikeBalanceValidation`으로 실행한다. `campaign-and-ipad-followup-20261120.md`에는 시나리오 수치가 있으나 명령 안내에서 그 검사에 도달할 수 없다. |
| 18 | 정식·치트의 공통점과 실행 방식이 플랫폼별로 구분되지 않는다. | README는 Apple 동시 자동검사 방식을 두 빌드의 일반 설명으로 둔다. Android 치트는 독립 저장에서 직접 플레이하고 ADB로 자동 검사도 실행한다. PWA·타우리 치트는 검증 전용 웹 빌드이며 `app.js`의 `HM2Validation` 경로가 있다. Apple·Android·웹·Mac별 실행 방법, 저장 분리, 자동·직접 검증을 한 표에서 찾기 어렵다. |
| 19 | Android 초기 계획에서 구현·설치 결과로 넘어가는 참조가 없다. | `android-port-plan-20261120.md` 5·60행은 NDK 26.3과 API 24에서 정적 링크한 당시 상태다. 실제 설치 경로는 `Android/README.md` 및 `검증-20261120.md`에 있다. Gradle은 NDK 28.2.13676358·minSdk 26을 지정한다. 계획의 링크 검사와 설치 가능 최소 버전을 혼동할 수 있다. |
| 20 | 휴대폰·태블릿이 열 실제 PWA 주소를 안내하지 않는다. | `Web/README.md` 16–30행은 맥의 `127.0.0.1` 주소와 홈 화면 설치 방법을 나란히 적는다. 해당 주소는 다른 실제 기기가 맥에 접속할 때 쓸 수 없다. 현재 테스트 주소와 실기기용 HTTPS 설치 주소의 상태 및 접근 범위를 분리해 안내할 자리가 없다. |

## 제품 정책과 코드·검사의 차이

| 번호 | 발견 | 확인한 근거와 영향 |
| --- | --- | --- |
| 21 | PWA·타우리의 음향 구현이 OpenAL 계약과 다르다. | `runtime-contract.md` 27–33행과 `technology-and-assets.md` 15–26행은 웹이 Emscripten OpenAL API를 쓴다고 적는다. 실제 `Web/CMakeLists.txt` 25–40행은 `HeavyMach2Core`만 연결한다. `Sources/Platform/Web/Runtime.cpp` 170–175행에서 넘긴 이벤트를 `Web/Shell/audio.js` 38–68행이 Web Audio로 직접 실행한다. 효과음마다 새 BufferSource·StereoPanner를 만들고, 재생 시점에 캐시에 없으면 파일을 읽고 디코딩한다. 타우리는 같은 웹 빌드를 사용한다. 실제 출력 일치나 지연·중단 동등성을 이번 정적 대조에서 판정하지 않았다. |
| 22 | 복구코드의 공통 사용·호환 계약이 없다. | `RecoveryCode.cpp`와 `GameSession.cpp` 757–797행에 `HM2-1-` 형식, 슬롯 셋 전체, 복원 전 백업, 지원하지 않는 맵 거부가 있다. Apple·Android·Web의 입력 어댑터와 Mac 저장 경로가 연결돼 있다. 플레이 안내와 각 빌드 안내를 함께 읽어도 슬롯 전체 교체, 판본별 저장 영역, 코드 호환, 원래 진행을 백업하고 옮기는 순서, 실기기 미검증 범위를 한곳에서 찾을 수 없다. |
| 23 | 설치 주소를 바꾸면 기존 PWA 저장에 접근할 수 없는 조건을 안내하지 않는다. | `Web/Shell/storage.js` 13–25행 및 44–50행은 실행한 오리진의 IndexedDB를 사용하고, 그 안에서 정식·치트 이름으로 나눈다. `Web/README.md` 34행의 설치 경로·빌드 버전 분리는 서비스 워커 캐시의 설명이다. 설치 주소에서 호스트나 포트를 바꾸기 전 기존 오리진에서 복구코드를 보관해야 한다는 사용 안내가 빠져 있다. |
| 24 | Tauri 저장 교체 후 오류의 처리 계약과 검사가 없다. | `Desktop/Tauri/src-tauri/src/storage.rs`의 `write`는 `persist`로 파일을 교체한 뒤 디렉터리 동기화와 다시 읽기를 수행한다. 그 뒤 오류를 반환하면 `GameSession::restoreRecoveryCode`는 메모리의 이전 저장을 유지한다. 이 경로에서는 디스크가 이미 교체됐을 수 있다. 기존 Rust 검사는 잘못된 크기 등 교체 전 실패를 다룬다. 교체 뒤 오류가 난 경우의 재시작·복구 규칙은 README와 실행 계약에 없다. 이는 코드 경로 확인이며 디스크 오류를 강제로 재현한 결과는 아니다. |
| 25 | 웹에서 미확인 기기와 실제 지원 방향의 기준이 빠져 있다. | 정본 A04는 기기 유형과 창 크기를 분리하고 미확인 기기를 조용히 대체하지 말라고 한다. `Web/Shell/display.js`는 시작할 때 UA·터치·화면의 짧은 변 600 CSS px로 추정하고, 남은 경우 desktop을 반환한다. `Web/README.md`는 시작 후 고정한다고 설명한다. 접이식에서 분류를 유지/다시 판단할 시점, 미확인 기기 판정, 휴대폰 가로·태블릿 세로를 강제하는지 검증 기준으로만 두는지에 대한 정본이 없다. 적용 범위를 결정할 항목이다. |
| 26 | 태블릿 세로 허브 제목이 화면 요구를 충족하지 못한다. | 설치형 Chrome의 iPad 모의 834×1194에서 `Ruin Field Base` 제목 왼쪽을 잘라 표시했다. 공통 `DisplayLayout.cpp::ui(tabletMenu)`의 비례 확대에서 생긴다. 정본 D03은 배경과 버튼의 표시를 요구한다. `UniversalDisplayAndInput`은 텍스처 채움·비례·역입력을 검사하고 이 글자 잘림을 잡지 못했다. 이미 `todo.md`에 기록했다. 동일한 실제 iPad 네이티브와 설치형 PWA의 대조는 남아 있다. |
| 27 | 소비되지 않는 Phone 배경 분기가 소스에 남아 있다. | `Screens.cpp` 989–994행에서 실제 Phone 상점은 `phoneShop(false)`로 반환한다. 뒤의 Phone `ShopBG.jpg` 분기는 같은 조건에서 도달하지 않는다. 정비와 사령부에도 같은 형태가 남아 있다. 소스 검색만으로 화면별 사용 자원을 조사하면 이 이름을 실제 소비 경로로 오인할 수 있다. 앞서 정식 PWA·iOS 번들·갤럭시 APK의 Phone 상점 자원 일치를 확인한 범위는 유지된다. |
| 28 | 현재 시각 검사와 오래된 세션 경고를 구분하는 안내가 없다. | Git 훅은 LFS 전용이고 작업 사본의 `.codex`에는 추적된 프로젝트 자동 검사 등록이 없다. 그런데 이번 읽기 호출에서도 배경·화면 ERROR가 표시됐다. 로컬 증거의 이유는 `Captured source candidate is stale`이며 새 빌드의 아트 실패를 관측한 결과가 아니다. `design-and-asset-rules.md`의 수동 증거 검사, 예전 하네스 문서, 세션 알림 각각의 적용 범위와 근거 위치를 연결해야 한다. 경고를 발행한 세션 설정의 소유 위치는 이번 조사에서 확정하지 않았다. |
| 29 | 공습 문서에서 실제 피해 산식에 넣는 기준 HP를 모두 재현할 수 없다. | `campaign-and-ipad-followup-20261120.md` 35–49행은 시험 전장의 주요 전차 HP와 보간식을 적는다. `QuickItems.cpp` 22–35행에 있는 별도 보정값 14개와 보간점 선택은 표에 없다. 예를 들어 SnowRidge 시험 전차 HP는 16,000이고 Lv42의 실제 산식 기준은 18,000이다. 실제 적 HP와 설계 기준값의 차이를 표시해야 같은 피해를 계산할 수 있다. |
| 30 | 앱 아이콘 문서에서 Android 소비 경로가 빠져 있다. | `docs/app-icons.md`는 Xcode 에셋 카탈로그와 웹·타우리 파생 규칙을 설명한다. Android는 `Android/app/build.gradle.kts` 80–120행에서 같은 원본 `Main_BG.png`의 중앙 768×768 영역을 읽어 192px PNG와 치트판 `TEST` 띠를 별도로 만든다. 그림 출처가 같고 플랫폼별 생성 도구와 해시는 다르다. 해당 경로로 연결하는 참조가 없다. |
| 31 | 코드 출처 문서가 초기 렌더 단계를 현재 단계로 적는다. | `source-reuse.md` 15행은 개별 이미지로 출력하는 첫 조립을 현재 상태로 둔다. 정적 `AssemblyPreview`에는 이 설명이 적용된다. 현재 전장 프레임에는 `VisualLayer.hpp`의 atlas 배치와 `PreparedTiles.cpp::prepareShadow`·`CombatScene.cpp`의 그림자 준비·소비가 있다. `graphics-audio-fidelity-20261110.md`와 `tests/PartShadowTests.cpp`가 이 후속 범위를 설명한다. 정적 조립과 현재 전장 렌더링의 연결이 빠져 있다. |

## 검사 범위와 재현

- Git 추적 파일 875개에서 Markdown 113개를 찾았다. 자체 문서 111개·9,199행의 Markdown/HTML 링크를 파싱했다. 로컬 링크 737회는 파일과 Markdown 제목 위치가 일치했다. 외부 링크는 149회이며, 이 중 기존 비공개 사이트의 고유 주소 50개의 파일·제목 위치를 보유한 사이트 소스와 대조했다. 압축된 본문은 풀어서 확인했다. 저장소 밖의 일반 외부 웹사이트에 대한 접속 가능성은 별도의 검사다.
- 아래 경로의 원고 분류 결과, 인라인 참조 후보 118개, 문서 그래프, 확인한 링크, 사이트 소스 링크 검사와 재현 로그를 보존했다. 인라인 후보의 타 프로젝트 경로와 과거 출력물을 현재 로컬 링크 오류 수에 합치지 않았다. 이 수치에는 조사 후 추가한 이 보고서가 포함되지 않는다.
- 공통 코어, 에셋 준비와 캠페인, Apple, Android, 웹/PWA, 타우리, 호환 포팅판, 저장소·게시의 진입 문서와 소비 코드를 대조했다. 서브시스템의 모든 함수와 모든 실제 기기·기종·원작 장면에 대해 새 실행 검사를 한 것은 아니다. 버전이 명시된 보고서의 당시 결과는 별도로 보존한다.
- Python 저장소 검사 36개, IPA 검사 2,139개 파일 버전, `UniversalDisplayAndInput`·`OriginalScreensAndHUD`·`OriginalHelpSession`·`OriginalSessionTransitions`·`OriginalHDSaveEnvelope`의 선택 재검사를 통과했다. HUD 잘림처럼 선택 검사에서 다루지 않는 관측은 표에 별도로 적었다.

```sh
cmake --build build/recovery-native --target DisplayLayoutTests ScreensTests HelpSessionTests SessionTests SaveTests
build/recovery-native/ScreensTests .local/prepared/HD
build/recovery-native/ScreensTests .local/prepared/Campaign/HD
build/recovery-native/SessionTests .local/prepared/HD
build/recovery-native/SessionTests .local/prepared/Campaign/HD
```

이 실행에서는 `ScreensTests`와 `SessionTests`가 첫 입력에서 실패하고 둘째 입력에서 성공했다. `build/recovery-native`는 `HM2_HD_RESOURCES`에 캠페인 준비 경로를 넣어 구성한 빌드 디렉터리다. 로컬 원본 결과와 스캔은 `.local/evidence/code-document-audit-20261121/`에 있다. 설치형 PWA의 제목 잘림 캡처는 `.local/evidence/app-icons-web-mac-20261121/device-layouts/chromium-ipad-desktop-site-base.png`에 있다.

## 루트 진입 문서에서 링크로 닿지 않는 파일

- Android: `Android/README.md`, `Android/검증-20261120.md`, `docs/android-port-plan-20261120.md`.
- 구현·검증 근거: `docs/iphone-apple-runtime-audit-20261114.md`, `docs/iphone-path-jump-investigation-20261114.md`, `docs/original-resource-loading-audit-20261114.md`, `docs/runtime-preservation.md`.
- 작업 정책·기록·고지: `.agents/policy.md`, `.agents/project.md`, `docs/restoration-checkpoint-20261111.md`, `Compatibility/THIRD-PARTY-NOTICES.md`.
