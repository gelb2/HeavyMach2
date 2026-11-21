# iPhone 수정·동시 검사·Instruments 측정 계획

2026-11-14 사용자 지시를 정리했다. 최근 iPhone TODO를 조사하고 공통 코어에서 수정한 뒤 치트빌드·시뮬레이터·Instruments로 확인한다. 이 문서는 계획이며 검사 통과나 발열 해결 보고가 아니다. 기존 임무·전 캠페인 등 다른 TODO는 닫지 않는다.

## 현재 기준과 확인한 코드

- 작업 브랜치는 `feat/iphone-universal-20261113`이다. 계획 전 HEAD는 `d346503`이며 설치한 정식 Release의 게임 소스는 `64ebe70`과 같다. 이전 검사·실패·사용자 진행 저장을 보존한다.
- `CombatScene::updateDisplay`의 표적 목표 줌은1.0..1.6이다. 작은 화면 최소 줌은 `max(width,height)/(1022×0.5)`다. 실제 iPhone912×420에서는 약1.785로 목표 범위보다 크다. 최소값이 가변 줌을 막는 코드 경로가 확인됐으며 실제 사용자 증상과 연결하는 회귀 검사가 필요하다.
- 경로 입력은 `CombatScene::input`→`WorldNavigation::inputWorld`이며 `DashGesture::pathDrag`가 경로 드래그 중 탭 시간을 지운다. 사용자가 보고한 여러 경로→새 단일/여러 경로→더블탭 순서는 기존 단순 두 탭 검사와 다르다. 아직 원인을 확정하지 않았다.
- Apple `HM2SourceView`는 표시 콜백에서 공통 `GameRuntime::tick/draw`를 호출한다. `HM2Renderer`는 GLES1을 사용한다. `HM2TextRasterizer`에는 UIKit/CoreGraphics로 A8 글자를 만드는 코드가 있으며 `TextPreparationCache`와 GPU 텍스처 준비에 연결된다. 프레임별 호출 여부·원작 `Texture2D(Text)` 근거·캐시 수명은 추가 감사한다. 프레임워크 이름만으로 준수/위반이나 발열 원인을 판정하지 않는다.
- Xcode27.1의 설치된 xctrace에서 Allocations, Leaks, Time Profiler, Game Memory, Game Performance, Animation Hitches, Metal System Trace, Power Profiler 템플릿을 확인했다. 실기기별 사용 가능 트랙과 실제 데이터 수집 성공은 아직 확인하지 않았다.

## 1. 에셋·스택 감사와 기능별 수정

| 과제 | 조사·구현 위치와 방법 | 완료를 판단할 근거 |
|---|---|---|
| 발열·절차 위반 의심 | Apple 호스트/글자 준비/renderer와 공통 UI·VisualLayer·reader·파츠 조립의 실제 호출 관계를 대조한다. 정식판에 Validation 실행이 섞였는지도 빌드 설정/바이너리에서 확인한다. | 파일·함수·원본 근거·호출 주기·메모리/CPU/GPU 비용을 연결한 표. 최종 기기 trace와 함께 판정. |
| 생명주기·중단/복귀 | iPhone/iPad의 비활성화·백그라운드·전화/음향 중단을 공통 pause/menu에 연결하고 복귀만으로 자동 재개하지 않는다. 컨텍스트 ID/생성·해제 수와 장치 suspend/resume을 추적한다. | 중단 중 게임 tick/입력/음향 정지, 복귀 메뉴 유지·사용자 재개, 반복 후 컨텍스트 중복/재생성 루프 없음. 실제 중단과 테스트 주입을 구분. |
| 경로 재지정 후 점프 | 사용자 두 순서를 현재 코어에서 실패로 재현한다. 경로/포인터/이전 탭 시간/점프 판정의 이벤트 순서를 원본과 대조해 원인을 고친다. | 실제 입력 경로로 점프 시작·SP 소비·이동·효과음·기존 경로 결과 확인. 단순 더블탭, 적 선택, UI 경계, 취소도 회귀 검사. |
| 락온 후 가변 줌 | 전체 화면을 덮는 출력 영역과 카메라 줌을 분리한다. 최소 줌/유한 맵 경계와 원본 배경 소비를 조사해 줌 범위와 양립하는 표시를 구현한다. 단순히 줌 제한을 없애 검은 바깥을 노출하지 않는다. | 가까운/먼 적 선택·해제·경로에서 현재/목표 줌과 보이는 월드 범위가 변함. HUD·전체 출력 영역 고정, 동일 역입력·판정·맵 경계 확인. |
| 원본 배경과 버튼 배치 | 일반판/HD 허브·기지·타이틀·로딩 자원 및 원본 버튼 좌표/콜백을 대응한다. 별도 iPhone 원본 그림을 선택할 필요가 입증되면 공통 자원 선택/화면 구성에 한정해 반영한다. | 원본 자원·표시 변환·건물/길 위 버튼의 실제 hit 영역을 연결한 표. 인게임/아이템/무기 버튼은 HD 공통 경로 유지. |
| 기기/화면 정책 | 기존 `DisplayConfiguration`과 플랫폼 경계에서 해결할 수 있는지 먼저 검토한다. 기기 종류와 실제 창/방향/안전 영역을 구분한다. | 좁은 데스크톱 창·태블릿 분할 화면을 잘못 분류하지 않는 선택 기준. 새 모듈 필요 여부와 근거. |
| HUD 편집/저장 | HM1 현행 작업 사본의 `HUDLayout.hpp`, [HUDConfiguration.hpp](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/Sources/Core/HUDConfiguration.hpp), `HMHUDEditor.inc`를 참고한다. HM2 조립과 버튼 ID를 유지하며 공통 그룹별 그림/입력 변환과 편집 초안을 구현한다. | 하단 자유 배치, 적용/취소/기본값, 재실행 복원, 입력 경계 동기화. 드래그·사격 때 저장0회, 적용 성공 시에만 설정 저장. 원래 진행 저장 포맷은 유지. |

에셋 로드·재사용·렌더러 전달은 원작 절차를 따른다. 파일/ID 참조→공통 reader→파츠/atlas 조립→원작 캐시와 수명→GPU 제출·해제 순서를 근거와 연결한다. 성능을 이유로 별도 iPhone 로더·UI 렌더러·프레임별 이미지 합성/디코딩/업로드·임의 무제한 캐시를 추가하지 않는다. 원작 글자 생성·동적 자원 갱신은 실제 호출 사건과 구분한다. 현재 코드가 원본과 다르면 차이를 기록하고 근거에 맞춰 수정한다. 원작 절차 미확정을 추측으로 채우지 않는다.

## 2. 구현 중 검사와 마지막 동시 치트·시뮬레이터

구현 중에는 관련 C++ 회귀 검사와 컴파일을 사용한다. 기능 하나마다 포팅판·앱을 번갈아 실행하지 않는다. 정식판과 치트판은 같은 게임/표시/자원 코어를 사용한다. 테스트마다 게임 수치나 성공 결과를 강제로 넣지 않는다.

현재115조건 동시 실행에 경로 재지정2종·가변 줌/경계·HUD 편집/저장/입력·배경/버튼 대응 조건을 연결한다. 모든 독립 상태를 같은 프레임 구간에 갱신한다. 시작/완료, 실제 점프·발사·피해·자원 소비·저장·CPU/GPU 준비 비용을 상태별로 기록한다. 무기110종/5계열과 기존 음향·BOT·임무·저장 회귀를 유지한다.

공통 구현을 끝낸 뒤 iPhone 가로·iPad 세로의 마지막 시뮬레이터 검사 묶음을 실행한다. 실제 Source OS 입력과 치트 결과를 구분한다. HD 원작 동작 기준은 iPad에서 확인하며 일반판 원본은 iPhone 표시/입력 기준으로 사용한다. 원작 최종 일괄 대조를 구현 중 기능별 왕복 검사로 바꾸지 않는다. 실패하면 원인과 관련 변경을 보존하고 해당 경로를 수정한 뒤 다시 검사한다.

## 3. 마지막 Instruments 측정

사용자 최종 지시대로 구현과 치트 연결을 마친 뒤 실행 검증·성능 측정을 진행한다. 현재 서명 Release와 dSYM/UUID·자원 명세를 비교 기준으로 보존하고, 수정 전/후 측정은 마지막 단계에서 수행한다. 기능적으로 의도한 차이는 기록하며 같은 상태가 아닌 수치를 직접 우열 비교하지 않는다.

### 실행 조건과 데이터 보존

- 실제 iPhone의 정식 Release를 기본 성능 대상으로 한다. 동시 치트는 별도 Release/검사 컨테이너의 스트레스 검사이며 일반 플레이 성능으로 보고하지 않는다. iPad 회귀와 Simulator 수치도 별도 표로 남긴다.
- 사용자 앱 데이터는 측정 전후 백업하고 체크섬을 확인한다. 반복 검사용 저장은 분리된 컨테이너에 둔다. 사용자 진행을 초기화/되감거나 검사 저장으로 덮어쓰지 않는다.
- 소스/바이너리/자원 SHA, dSYM UUID, 기기/OS, 실제 drawable·안전 영역, 프레임 목표, 밝기·충전 여부·저전력 상태·초기 thermalState, 사용 저장과 입력 순서, 시작/종료 시간을 기록한다. 같은 초기 열 상태로 비교하고 식지 않은 기기의 뒤 실행을 동등한 기준으로 쓰지 않는다.
- 기존60Hz 목표를 유지한다. 계측을 위해 게임 tick/RNG·사거리·음향·저장 타이밍을 바꾸지 않는다. 필요한 프레임/이벤트 표식은 메모리 집계나 signpost로 남기며 프레임별 파일 쓰기·콘솔 폭주를 만들지 않는다.

### 수집 항목

| 기록 | 실제로 확인할 것 |
|---|---|
| Time Profiler | 게임 tick·Frame 조립·UI·글자 준비·이미지 decode/업로드·GL 제출/표시 대기·오디오의 CPU 스택과 비용. 심볼 없는 스택은 미해석으로 남김. |
| Allocations + Leaks / Game Memory | 살아 있는 Heap/VM·peak·반복 후 잔류량·누수 검사 횟수/검출 수·객체/텍스처/오디오 수명. 누적 할당량을 현재 메모리나 누수량으로 부르지 않음. |
| Game Performance / Animation Hitches | 표시 콜백 간격과 긴 프레임, 처리시간 p50/p95/p99/max, 목표 시간 초과 수와 연속 지연. CPU 처리시간과 화면 표시 간격을 따로 기록. |
| Metal System Trace 등 실제 GPU 트랙 | 대상 프로세스에 귀속된 GPU 제출/완료·대기·과도한 draw/업로드·화면 채움 비용. GLES 앱에서 수집되지 않는 GPU 카운터는 미수집으로 표기하며0사용률로 쓰지 않음. |
| Power Profiler / thermalState | 사용 가능한 에너지/전력 지표와 열 상태 변화 시점, 초기/후반 프레임 분포. thermalState를 실제 섭씨 온도라고 표현하지 않음. |
| 추적기를 끈 대조 | 같은 Release·저장/입력으로 별도 프레임 집계. debugger와 Allocations/Leaks 계측 비용을 일반 플레이 성능에서 분리. |

타이틀→슬롯→기지/사령부/상점→전장 이동/경로 재지정/점프/락온·해제/전투→기지 복귀를 한 반복으로 정의한다. 캐시 준비 뒤10회 반복에서 돌아온 같은 화면의 살아 있는 메모리·자원 수를 비교한다. HUD 편집·적용/취소·재실행·앱 비활성/복귀도 포함한다. 원작의 의도된 지연 준비 증가와 수명이 끝나도 남는 증가를 구분한다.

발열은 별도로 일반 플레이15분을3회 기록해 준비/초기/후반 부하와 열 상태를 비교한다. 메모리·CPU·GPU·전력 추적을 무조건 한꺼번에 붙이지 않고 같은 시나리오의 분리된 실행으로 계측 간섭을 줄인다. 짧은 trace 성공으로 장시간 발열·누수 없음이나 무드랍을 선언하지 않는다.

### 통과와 보고

- 사용자 두 경로 재지정 순서에서 유효한 더블탭 점프가 실제 발동해야 한다. 락온 성공과 줌 변화는 각각 판정한다.
- 전체 출력 영역은 고정되고 가변 줌·HUD/버튼 위치·입력·원본 에셋 조립이 함께 맞아야 한다. 검은 레터박스와 버튼/건물 위치 불일치를 허용하지 않는다.
- 메모리 누수 검출0은 기록 길이·검사 횟수·상태 범위와 함께 쓴다. 반복 때 살아 있는 메모리가 계속 늘면 호출 스택/소유자를 찾아 고친다. 측정 전 임의 MB 기준을 만들어 준수로 처리하지 않는다.
- 60Hz의 한 프레임 예산은 약16.67ms다. 처리시간과 표시 간격의 초과 수·연속 지연을 숨기지 않는다. 원작 대기·OS 스케줄링·계측 비용은 실제 근거로 분리한다. 기준을 낮추거나 FPS 제한을 바꿔 통과시키지 않는다.
- CPU/GPU·열 상태의 수정 전후 비교와 장시간 후반 악화 여부를 보고한다. 객관적 자료 없이 발열 해결로 표시하지 않는다.
- 원본 `.trace`·export·입력/환경 명세·실패 기록은 HM2 전용 로컬 증거 폴더에 보존한다. 기존 비공개 보고 목록에는 요약·그래프·범위·미해결을 연결한다. 원본 IPA·사용자 저장·기기 식별자·서명 비밀은 게시하지 않는다.

## 재사용한 HeavyMach1 근거

읽기만 수행했다. 다른 저장소의 소스·문서·빌드·사용자 변경은 수정하지 않았다.

- [HeavyMach 1 Instruments 기록](https://github.com/gelb2/HeavyMach/blob/bd0aa73ad0bf765f3e54c1800c89b8d550a5e1e9/docs/instruments-validation-20261024.md): CPU/메모리/GPU trace와 추적기 없는 대조, 실제 누수 검사·긴 프레임·미수집 GPU 지표 구분.
- 현행 작업 사본 `HeavyMach/build/pr24-review`의 [apple-platform-progress-20261026.md](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/docs/public/apple-platform-progress-20261026.md), [wireless-device-workflow.md](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/docs/wireless-device-workflow.md), [profile_apple.py](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/tools/profile_apple.py): 실제 iPhone/iPad 누수 기록과 무선 추적 절차. 과거 짧은 기록은 HM2 측정 결과로 재사용하지 않는다.
- 같은 작업 사본의 [HUDLayout.hpp](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/Sources/Core/HUDLayout.hpp), [HUDConfiguration.hpp](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/Sources/Core/HUDConfiguration.hpp), [HMHUDEditor.inc](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/Sources/iOS/HMHUDEditor.inc), [game-button-incident-20261101.md](https://github.com/gelb2/HeavyMach/blob/ca75b1b66174686503ab0aebccc4577ae9dd866e/docs/reviews/game-button-incident-20261101.md): 공통 그림/입력 변환, 편집 초안·APPLY 저장, 게임 렌더러 사용 기준. HM1 코드를 HM2 원작 구현이라고 부르지 않는다.
