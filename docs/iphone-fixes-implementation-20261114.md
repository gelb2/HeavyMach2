# iPhone 수정 진행 — 2026-11-14

## 설치 후 피드백과 후속 코드 — 아직 iPhone 미설치

사용자는 `bc3323a` 설치 후 백그라운드 복귀 때 허브 버튼과 게임 전체가 멈춘다고 보고했다. 기존 Scene handler가 오디오 중단 flag로 display link·입력까지 함께 막는 경로를 확인했다. 종료 알림 누락과 늦은 중단 알림의 새 Native 검사3개는 수정 전 양 시뮬레이터에서 실패했다. Apple은 Begin에 대응하는 Ended가 보장되지 않는다고 설명한다. [공식 중단 처리](https://developer.apple.com/library/archive/documentation/Audio/Conceptual/AudioSessionProgrammingGuide/HandlingAudioInterruptions/HandlingAudioInterruptions.html).

후속 코드에서는 Scene 활성과 오디오 사용 상태를 분리했다. 오디오가 중단돼도 foreground 메뉴·문 전환은 갱신한다. 전투 중단은 기존 공통 GameSession의 pause/menu를 사용하며, 오디오가 돌아왔다는 이유만으로 전투를 재개하지 않는다. 활성화가 필요한 상태와 중단 사건 순서를 추적해 background Ended·늦은 suspension·활성화 도중 새 Begin을 처리한다. 실제 foreground/Ended 경계에서만 기존 OS 오디오 세션을 재활성화한다. OpenAL 컨텍스트·효과음 source/buffer·원본 AudioQueue BGM을 다른 재생기로 바꾸지 않았다.

| 확인 | 결과와 범위 |
|---|---|
| 음향·Scene 중단 Native | 양쪽에서 음향5개·Scene 순서7개 통과 |
| 실제 공통 Runtime 무음 UI | 양쪽에서 Base→Shop→Base의 실제 버튼·문 전환, 원래 저장1회·저장 decode 일치, SFX 재생 요청0·기존 컨텍스트1개 유지 통과 |
| 실제 공통 Runtime 전투 | 무음 중 렌더 시간은 진행하고 월드 시각/위치/HP는 정지. 음향 복귀 뒤에도 실제 Resume1030 전까지 정지 유지 통과 |
| 실제 OS Home·전장 복귀 | 양쪽 각각3회 정지 메뉴·렌더 콜백·수동 재개 통과 |
| 실제 OS Home·허브 복귀 | 허브에서 상점 진입까지 작동했으나 문 전환 완료의15초 기준은 양쪽 실패. 전체 허브 복귀 검사 통과로 처리하지 않음 |
| iPhone 사용자 정지 해결 | 후속 코드를 아직 실제 iPhone에 설치하지 않아 미확인 |

허브 실패의 추가 진단에서는 Source의 `applicationActive=1`·`audioOutputActive=1`, 진행한 callback740/770회·Session renderTime 약12.28/12.78초를 확인했다. 문은 phase7이고 topY1399.72/1232.31로 이동 중이었다. 정지된 앱이라고 판정하지 않는다. 실제 Simulator Time Profiler에서는 GLRendererFloat의 소프트웨어 래스터 경로를 확인했다. 요청20초 중 테스트 종료까지2.55초만 기록했으므로 장시간 성능·발열이나 실기기 원인으로 확대하지 않는다.15초 실패는 그대로 보존하며 시간 기준을 늘려 통과시킨 것으로 보고하지 않는다.

실제 iPhone의 기존 프로세스에는 이름과 PID로 Time Profiler 부착을 시도했으나 Instruments가 대상을 찾지 못해 trace가 생성되지 않았다. 실기기 메모리·누수·발열 측정 결과는 아직 없다. 별도의 게임오버 OS 검사는 iPad에서 통과했으나 iPhone의60초 전투 관측 안에는 파괴 알림이 나타나지 않아 남아 있다.

로컬 증거는 `.local/evidence/foreground-freeze-20261114/`의 `lifecycle-red-batch`, `lifecycle-fixed-batch`, `foreground-separated-batch`, `hub-diagnostic-batch`, `hub-clock-diagnostic-batch`, `hub-simulator-time-profile.trace`와 해당 export다. 화면·입력·원래 실패·사용자 저장은 보존했다. 일반판 초기/대기 줌 비교와 시뮬레이터/실기기 성능·허브 검사·아이폰 정지 재검증은 [TODO](todo.md)에 남긴다.

## 저장 슬롯에서 바로 전장으로 진입한 이유

공통 `GameSession::action`의 슬롯 시작은 저장된 위치 문자열이 `Base`로 시작하면 기지, 그 외에는 저장된 맵/게이트의 전장을 요청한다. iPhone 전용 저장 형식·슬롯 선택·화면 전환 분기는 없다. 이 분기는 iPhone 개발 전 [원본 슬롯 진입 복원](source-entry-restoration-20261110.md)에 이미 연결돼 있었다.

공통 SaveData decoder로 읽은 최신 설치 전/직후 iPhone 저장은 둘 다 `Stage01_01`, 마지막 기지는 `Base001`, 게이트0이었다.11월13일 보관 iPad 백업은 `Base002`, 마지막 기지도 `Base002`, 게이트1이었다. 세 파일 모두 체크섬이 유효하다. 저장 위치가 달라 두 진입 결과가 달랐으며, 설치 때 위치를 바꾼 것은 아니다. 현재 실시간 iPad 저장이 이 백업과 같다고 주장하지 않는다. 원본 저장은 수정하지 않았다. 로컬 근거는 `slot-entry-save-analysis.json`이다.

공통 코드와 Apple 빌드를 수정하고 첫 양 시뮬레이터 검사 뒤 후속 검사를 실행했다. 후속15개 중 iPhone14개·iPad14개가 통과했고 게임오버 자동 검사 수정·재검증이 남는다. 사용자의 새 설치 지시에 따라 게임 코드 `bc3323a`의 정식 Release를 실제 iPhone에 설치·실행하고 기존 저장을 보존했다. Instruments는 아직이다. 사용자 발열과 실제 손 입력의 경로 재지정 후 점프 실패를 해결했다고 판정하지 않는다. [수정·측정 계획](iphone-fixes-and-profiling-plan-20261114.md), [남은 TODO](todo.md)를 함께 따른다.

## 실제 iPhone 정식 Release 설치

사용자는 남은 검사보다 설치를 먼저 진행하도록 후속 지시했다. `bc3323a`의 게임 코드를 Source Release로 빌드했다. 현재 미커밋 변경은 Release에 포함되지 않는 UI 검사 코드다. CLI의 `errSecInternalComponent`와 Xcode의 팀 미선택 오류를 기록했고, 기존 개인 개발 팀을 선택한 Xcode Build For Profiling의 Release 서명을 사용했다. 임시 프로젝트 설정은 보존한 원래 파일 바이트로 복원했다.

설치 전 앱 데이터7파일·7,312,065바이트를 백업했다. 서명 유효성, 실행파일/dSYM UUID, HD 자원1,479개와 준비 자원의 전체 해시 일치, 테스트 플러그인·동시 치트·UI 검사 환경 코드 제외를 확인했다. 기존 앱에 업데이트 설치한 직후 진행 저장28,204바이트를 다시 읽어 백업과 완전히 같음을 확인했다. 앱 실행과 실제 기기 프로세스 존재도 확인했다. 저장을 초기화하거나 옛 진행으로 덮어쓰지 않았다.

근거는 로컬 `.local/evidence/iphone-release-bc3323a-20261114/`의 `backup-before-manifest.json`, `signed-release-verification.json`, `packaged-resource-manifest.json`, `install.json`, `save-preservation.json`, `launch.json`, `result.json`이다. 원본 저장·앱 번들·서명 자료는 커밋하거나 보고 사이트에 올리지 않는다. 남은 게임오버 자동 검사와 Instruments를 이 설치로 완료 처리하지 않는다.

## 반영한 코드

- iPhone/iPad가 비활성화·백그라운드·음향 중단에 들어가면 게임을 정지하고 기존 메뉴를 유지한다. 복귀만으로 전투를 재개하지 않는다. 맵 교체 중에는 새 World가 생길 때 정지를 이어받고 전환 뒤 메뉴를 표시한다. Scene 활성 상태와 오디오 중단 상태를 따로 추적한다.
- 작은 화면에서 최소 줌이 원래 목표 범위 전체를 막던 문제를 수정했다. 유한 맵을 덮는 기본 배율과 가변 줌을 분리했다. 전체 화면 출력과 HUD는 고정한다. 이 변경을 원본 일반판 픽셀 비례와의 최종 동등성으로 주장하지 않는다.
- HUD를 게이지·무장·조작3그룹으로 나누고 원본 그림·버튼을 함께 움직인다. 실제 알파와 버튼 영역을 배치 범위로 사용한다. 메뉴에서 편집·적용·취소·기본값을 제공하며, 적용 때만 진행 저장과 별도의 설정 파일에 기록한다.
- 원본의 전역 무기 준비, 실제 Player/적/BOT 파츠 부착, 아이템·수리 효과 최초 생성, 별도 화면 manager와 World 해제 경계를 연결했다. StageClearBG는 전역 manager에서 재사용한다. 도움말 manager의 고유 정체성도 준비·해제 변경 신호에 포함했다.
- 원래 데이터가 참조하는 순서로 효과음을 준비하고 이름 캐시를 재사용한다. 전역 초기화46개 뒤에 GameMain.start의 문 열림/닫힘2개를 요청하며, 타이틀까지 고유50개다. 첫 Apple 실패 뒤 누락된 문 효과음의 원래 준비 순서를 복원했다. 사격마다 컨텍스트를 만들지 않는다. 명시적 BGM 교체와 장치 중단/복귀를 구분한다.
- 끝난 화면·적 이름 Text와 GPU 자원을 소유 종료 경계에서 해제한다. 살아 있는 월드와 실제 관측 snapshot은 유지한다. 최근256개 입력은 메모리에 기록하며 프레임별 파일·DB 쓰기를 추가하지 않았다.

## 확인한 C++ 결과

독립 검토에서 찾은 고정 효과·공습/파티클 그림의 공유 수명 차이와 HUD 내부 scissor의 소수점 경계 중복을 수정했다. 이동 경로3개 그림은 원작의 World 소유가 맞아 유지했다. 아래 코어 결과와 Apple 컴파일은 이 후속 변경을 포함한다. 실제 앱·시뮬레이터·Instruments 결과는 아직 아니다.

| 검사 | 관측 |
|---|---|
| 일반 동시 검사 | 120조건·1,200프레임, 무기110종·5계열, 같은 프레임5계열 발사, 버튼 실패0 |
| 휴대폰852×393 | 120조건·2,100프레임, 무기110종·5계열, 버튼 실패0 |
| 실제 iPhone 화면 기준912×420 | 120조건·2,100프레임, 무기110종·5계열, 버튼 실패0 |
| 자원 요청 | 최신912조건은 최초 준비read/exists1,647/1,559회, 바이트 캐시의 원시 miss178/178회. 새 소유자/최초 준비 요청은 별도 기록. 같은 소유자의 반복 read/exists0, 원시 miss와 준비/반복 총량 일치 |
| 생명주기 | 휴대폰·태블릿10회 중단/복귀와 실제 맵 교체6조건 통과. 비활성 중 시간/RNG 정지, 복귀 메뉴와 명시적 재개 확인 |
| 기존 기능 | 적·BOT 장기전, 수리6종·공습3종, 도움말, 맵 전환 관련 코어 검사 통과. 공격·피해·HP·XP·수량·저장·RNG 기준 유지 |

위 입력은 공통 코어/실제 Screens 경로의 합성 입력이다. UIKit의 실제 손가락 이벤트나 실제 전화 중단 관측으로 확대하지 않는다. 코어 전체75개 실행에서74개가 통과했다. 대시 최초 효과 그림1회 읽기를 전체 읽기0으로 요구하던 나머지 검사를 수정한 뒤 해당1개도 통과했다. 실제 SP100 소비·RNG·36회 대시 이동·그리기 읽기0 기준은 유지했다. 별도 ASan/UBSan11개 검사도 모두 통과했다. Apple build-for-testing은 새 검사의 문자열 수치 변환과 C++ 모듈 대신 명시적 오디오 헤더 import를 고친 뒤 성공했다. Native·OS 입력 실행은 다음 단계다.

## 실패 원인과 보존한 증거

- 최초 전체 검사에서는75개 중5개가 실패했다. 원본처럼 실제 적·아이템·수리 아이콘 생성 때 처음 읽는 자원까지 전체 구간 읽기0으로 요구한 옛 검사를 좁혔다. 새 파일명·실제 준비 목록·소유 세대를 함께 검사하며 같은 자원의 반복 읽기를 허용하지 않는다.
- 도움말 메뉴를 새로 여는 frame182/case111의4read/4exists를 자원 변경 신호가 놓쳤다. 고유 GuiHelp manager의 생성/해제를 신호에 포함한 뒤 반복 요청0을 확인했다.
- 휴대폰1,800프레임에서는 Stage03_04의 원본 시체 제거 예정 시각이 아직 오지 않았다. 별도2,100프레임 진단에서852조건은1,852,912조건은1,830프레임에 원래 시각대로 제거됐다. 전체120조건의 공통 관측 구간을2,100으로 늘렸고 시체 수명·HP·시간·RNG는 바꾸지 않았다.
- 좁은 화면에서는 빠른 총이 미사일의 최초 재장전보다 먼저 적을 파괴해4종, 이후2종의 미사일 발사가 빠졌다. 원래 재장전이 자연히 끝난 뒤 실제 다중 적 드래그 입력을 사용해110종 발사를 확인했다. 장비·공격 컨트롤러·결과를 강제로 바꾸지 않았다.
- 기존 실패 로그와 수정 전 정식 Release/dSYM·사용자 저장을 보존했다. 로컬 근거는 `.local/evidence/iphone-fixes-20261114/`의 `core-accounted-tests`, `concurrent-help-owner-tests`, `phone-window-*`, `phone-guide-*`, `phone-queue-*`, `boundary-ledger-tests`, `airstrike-boundary-tests`, `owner-epochs-tests` 로그다. 최종 자원 수명 변경을 포함한 근거는 `core-assembled-tests`(74/75), `dash-first-effect-tests`(1/1), `sanitized-assembled-tests`(11/11), `sanitized-assembled-raw-tests`, `phone-final-912-2100`, `apple-assembled-build-import-fixed`다. 두 번의 Apple 컴파일 실패 로그도 보존한다.

## 다음 검사와 미해결

첫 실행의 실패 항목을 아래 원인별로 수정한 뒤 양 시뮬레이터에서 한 묶음으로 다시 검사한다. 이어 독립 치트 Release와 정식 Release의 Instruments 메모리·누수·CPU·프레임·가능한 GPU/열 상태와 추적기를 끈 대조를 기록한다.

일반판 배경 그림은 조사만 했다. 타이틀과 로딩 그림은 서로 다르며 기지 버튼 표도 HD와 다르다. 현대의 긴 화면에 대한 잘림·비율·배치 기준을 확정하지 않은 채 일반판 기지 데이터 전체를 가져오지 않았다. [배경·버튼 조사](iphone-background-asset-research-20261114.md)에 차이와 선택 조건을 남겼다. 전체 캠페인·임무, 가로 왼쪽 등 기존 미완료 TODO도 닫지 않는다.

## 첫 Apple 실행과 후속 수정

`9144809` 빌드의 첫 실행은 iPhone24/33, iPad25/33 통과다. 원본 로그와 xcresult는 `.local/evidence/iphone-fixes-20261114/final-apple-batch/`, 요약은 같은 상위 폴더의 `iphone-first-batch-summary.json`과 `ipad-first-batch-summary.json`에 있다. 두 기기의 실제 Home→앱 복귀3회는 정지·메뉴 유지·명시적 재개·저장 불변을 통과했다. 주입한 Scene/음향 중단 검사와 실제 OS Home 동작을 구분한다. 전화 수신 실측은 아니다.

| 실패 | 확인한 원인과 후속 변경 | 현재 한계 |
|---|---|---|
| iPhone 경로 재지정 후 더블탭 | 같은 canvas 좌표의 moved가 원작 탭 시각을 지웠다. Apple raw current/previous CGPoint가 정확히 같을 때만 전달하지 않는 호환 처리를 추가했다. 양수 이동과 원본 공통 Dash/Nav 규칙은 유지한다. | raw 좌표 진단을 포함한 새 OS 실행 전이다. 물리 손 입력80% 실패 해결은 미확인이다. |
| iPad 최초 경로 종료 | 첫 drag가 지도 밖에서 시작했고 실제 Base001 게이트를 지나 필드 전환을 시작했다. World 정지 중 end가 거절된 원본 분기를 확인했다. 같은 필드 내부에서 게이트를 지나지 않는 검사 경로를 선택한다. | 게이트·원본 월드 입력 규칙을 바꿔 점프 검사를 통과시키지 않는다. |
| 효과음48→50 | 원본 GameMain.start가 요청하던 Loading_Open/Close 준비가 빠졌다. 원래 전역 요청235회/고유46개 뒤 Open→Close를 추가해237회/고유48개, 타이틀 포함50개를 준비한다. | 새 Native 실행 전이다. 이름별 최초 준비·캐시 재사용·동일 컨텍스트를 추가 검사한다. |
| HUD 기본 위치의 RGB 차이 | 같은 그림을3개 삼각형 strip으로 그릴 때 RGB1단계 차이가 생겼고 alpha 차이는0이었다. 저장된 사용자 배치가 기본 위치와 같으면 원래1sprite 경로를 쓰며 실제 편집/이동은3개 절편을 유지한다. | 원래 픽셀 차이0 기준을 그대로 재검사한다. |
| 자원·도움말 검사 | 최초 실제 효과 준비와 끝난 적 이름 해제를 전체 읽기/업로드/텍스트 개수 불변으로 예상한 옛 검사를 수정했다. Core의 이름별 최초1회 읽기와 GPU의 실제 새 image ID별 준비를 함께 검사한다. 도움말4개 그림과 메뉴 텍스트 수명을 별도로 센다. | owner 불변 tick과 모든 draw의 읽기/디코드/업로드0, 살아 있는 이미지 재업로드0 기준을 유지한다. |
| 정지 중 SP 검사 | 원본은 정지 중에도 SP를 회복한다. cancel 순간 SP 불변과 이후30tick의 실제 회복을 나눠 검사한다. | 월드 시각·이동·선택·dash 불변은 그대로 요구한다. |
| iPhone 게임오버·공습 조건 | 게임오버 입력은 적2대를 탭한 뒤60초 기다렸으나 한 대가 죽고 Player가 멈췄다. 같은60초 안에 실제 OS 이동/재조준으로 전투를 이어간다. 공습1종의 목표가 추가 화면 여백 때문에 제외돼 실제 HUD/입력 경계 안 후보를 사용한다. | HP·피해·재장전·RNG·성공 결과를 강제하지 않는다. 기존 사망/알림/기지 복귀와 공습 피해 기준을 재검사한다. |

120조건 동시 실행 자체는 양쪽에서 끝났으나 위 실패가 있어 전체 통과로 보고하지 않는다. 당시 iPhone의 tick/GPU 제출 p95 약145.76ms는 다른 프로젝트 시뮬레이터와 메모리 압박이 함께 있었던 스트레스 실행이다. 정식판 실기기 프레임 성능으로 쓰지 않는다. 후속 공통 Screens/Session/phone/tablet4개 검사는 모두 통과했고 Apple 경계의 새 수정은 재빌드·실행이 남는다.
