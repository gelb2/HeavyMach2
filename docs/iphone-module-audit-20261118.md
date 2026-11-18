# 아이폰 포팅 모듈의 부작용 조사 — 2026-11-18

## 수정과 시뮬레이터 검증

분석 브랜치의 `f555584`에서 `fix/iphone-runtime-regressions-20261118`을 분기했다. 제품 파일 10개를 네 변경으로 나눴다. 하단 HUD에 접근하는 방향을 판단하는 데 이전 Y 좌표가 필요해, 조사 때 추산한 9개에 `CameraPresentation.hpp`를 추가했다.

| 문제 | 구현과 확인 |
| --- | --- |
| 점프 | Apple 입력에서 이전·현재 위치를 실제 캔버스로 변환한다. 같은 좌표의 이동도 UI 처리까지 전달하고, 월드 처리에서만 생략한다. 두 시뮬레이터에서 차이가 −1.42×10⁻¹⁴인 입력으로 첫 탭과 경로를 보존하고, 이어지는 탭에서 점프 1회·SP 100 소비·포인터 해제를 확인했다. |
| Skyway 레이어 떨림 | 화면 채움 배경의 가로 중심을 닫힌 구간에 제한한다. 가로 여유가 0이면 구간은 화면 중앙 한 점이다. 실제 기지 버튼으로 게이트 1에 진입하고 300프레임씩 캡처했다. iPhone 배경 이동의 최대값은 3.170pt에서 0.000031pt로 줄었다. iPad는 0.153687pt였고, 수정 전후 프레임 0과 170의 RGB 픽셀이 모두 일치했다. |
| 이동 중 뷰포트 급변 | 기존 거리 기반 줌에 기본 HUD의 목표 보정만 더한다. 하단으로 이동할 때는 다음 위치를 예상해 먼저 올리고 HUD로 인한 한 틱 배율 상승을 약 0.008로 제한한다. 맵을 덮는 최소 배율은 즉시 보장한다. 정지 상태의 표적 줌과 명시적 iPad의 기존 하한 계산을 보존한다. 실제 맵 78곳의 37,440틱에서 iPhone 배율 변화 0.01 초과가 19회에서 0회로 줄었다. 시험한 82개 포털에 이동 입력으로 진입했다. |
| 창 변경 뒤 빈 상점·인벤토리 | 같은 화면·게임 페이지에서 표시 행 수가 바뀌면 기존 첫 칸 또는 열려 있던 상세 항목으로 내부 페이지를 다시 계산한다. 기존 소스에서 재현된 빈 페이지가 수정 후 사라졌다. 상품·인벤토리·정비 창의 상세와 드롭 좌표, 콜드 상점과 크기 변경 뒤 GPU 첫 프레임을 확인했다. |

C++ 검사 76개, Source Native 검사 iPhone 5개와 iPad 4개를 통과했다. 실제 OS 입력으로 경로를 한 점 또는 여러 점으로 교체한 두 사례를 기기마다 각각 다섯 번 실행했다. 두 기기에서 모두 10회씩 통과했다. 치트빌드는 각 126조건·2,100프레임을 표시했고 버튼 실패는 0건이었다. 치트의 두 경로 교체 조건에서도 점프 1회와 SP 100 소비를 확인했다. CPU가 가정한 소형 iPad 창과 일반 창의 카메라 결과 6,000틱은 수정 전후 값이 일치했다. [실행 수치와 로그 해시](evidence/iphone-runtime-regressions-20261118.json)

첫 카메라 후보의 표적 줌·동시 실행 실패와 초기 iPad Skyway 시험의 잘못된 버튼 좌표를 보존했다. 최종 후보는 세로 선행 보정을 하단 이동에만 적용했고 기지 버튼의 실제 화면 좌표로 시험했다. 기존 메뉴 화면 검사에서 이전 후보를 가리키는 오류도 남았다. 이번 상점 첫 프레임은 새 후보의 GPU에서 검사했다.

Source Release 앱은 두 시뮬레이터에서 타이틀 기동을 확인했다. 전투와 메뉴 상호작용은 Source Debug 회귀와 치트빌드에서 검사했다. 사용자 실기기에서의 손 입력과 장시간 플레이는 별도 확인이 필요하다.

## 최초 재현과 계획

아이폰 시뮬레이터에서 경로를 한 점 또는 여러 점으로 바꾼 뒤 점프가 간헐적으로 실패했다. UIKit 좌표의 미세한 수치 차이를 입력 어댑터가 이동으로 전달했고, 실제 캔버스 좌표가 그대로인데도 첫 탭의 기록이 지워졌다. 복원판 아이폰의 이동 중 뷰포트 급변과 창 높이 변경 뒤 빈 상점·인벤토리 페이지도 재현했다. Wasteland Base에서 Skyway로 나간 뒤에는 배경 가로 위치가 튀면서 발판과 배경의 상대 위치가 떨렸다. [진입 경로와 GPU 근거](#wasteland-base에서-skyway로-나간-뒤의-떨림)

## iOS size class와 실제 분기

정식판의 [HM2SourceView::updateDisplay](../Sources/Platform/Apple/HM2SourceView.mm#L107)와 치트판의 [HM2PreviewView::updateDisplay](../Sources/Platform/Apple/HM2Application.mm#L58)는 `bounds`, `safeAreaInsets`, `userInterfaceIdiom`을 받는다. 저장소의 현행 제품·검사 코드와 제품 소스 이력에서 `horizontalSizeClass`, `verticalSizeClass`를 읽거나 그 변경을 구독하는 경로를 찾지 못했다. Apple도 size class와 interface idiom을 각각 별도의 trait로 정의한다. [Apple UIKit 문서](https://developer.apple.com/documentation/uikit/uitraitcollection)

공통 C++의 [DisplayConfiguration::surface](../Sources/Core/Presentation/DisplayLayout.cpp#L20)가 `min(폭, 높이) < 600pt`를 자체 `compact`와 `focusZoom`에 넣는다. [CameraPresentation::configure](../Sources/Core/Presentation/CameraPresentation.cpp#L20)는 이 두 값을 보고 줌 정책을 고른다. [menu::phone](../Sources/Core/Presentation/MenuArtwork.hpp#L8)은 명시적 phone과 소형 자동 프로필만 Phone 메뉴로 고른다. [menu::visibleRows](../Sources/Core/Presentation/MenuGridLayout.hpp#L15)는 캔버스 크기로 행 수를 계산한다. 이 때문에 세 분기는 서로 다른 경계에서 바뀐다.

같은 코어로 창 크기와 기기 프로필을 바꿔 확인했다. 아래의 작은 iPad는 명시적 tablet 프로필을 준 CPU 검사 입력이다. 실제 iPad 창의 크기나 UIKit trait를 계측한 값은 없다.

| 입력 | 공통 `compact` | 카메라 | 메뉴 |
| --- | --- | --- | --- |
| iPhone 912×420 → 912×550 | 둘 다 켜짐 | 둘 다 `compactFocus` | Phone 2행 → 3행 |
| iPad 1024×1366 | 꺼짐 | `legacyHD` | HD |
| iPad 500×700 | 켜짐 | `compactFocus` | HD |
| iPhone 500×700 | 켜짐 | `compactFocus` | Phone 4행 |
| iPad 599×820 → 600×820 | 켜짐 → 꺼짐 | `compactFocus` → `fixedSurface` | 둘 다 HD |

창 변경이 UIKit size class 변경과 함께 일어날 수 있다. 현재 게임이 받는 직접 신호는 창 크기·안전 영역이다. 이 값이 바뀌면 `GameRuntime::setDisplay → CombatScene::setDisplay`에서 포인터를 정리하고 카메라를 초기화한다. 이 재구성은 이동 중 HUD 경계에서 발생한 배율 급변과 별도로 계측한다. 점프의 재현 사례에서는 첫 탭에 들어온 같은 캔버스 위치의 `move`가 기록을 지웠다. 상점 빈 페이지는 공통 `compact` 값이 그대로인 두 크기에서도 재현됐다. iPad 앱의 현재 plist는 `UIRequiresFullScreen=true`다. 작은 tablet의 CPU 분기 관측과 실제 기기에서 그 창을 사용할 수 있는지는 검증 범위가 다르다. [화면 정책 검사](evidence/iphone-display-policy-20261118.txt)

## 수정 계획

### 점프 입력 — 플랫폼 어댑터와 월드 전달 분리

- [HM2SourceView::input:phase:](../Sources/Platform/Apple/HM2SourceView.mm#L180)는 UIKit 터치의 현재 위치와 이전 위치를 기존 `GameRuntime::logicalPoint`로 변환한다. 이전 위치를 변환해 같은 캔버스 점인지 확인하는 부분만 Apple 어댑터의 작은 함수로 분리한다. `move`이고 두 투영이 유효할 때만 비교한다. 기존 raw 좌표 완전 일치 필터와 화면 밖에서의 종료·취소 처리는 유지한다.
- [GameRuntime::input](../Sources/Platform/Apple/HM2GameRuntime.mm#L102)에 현재 이벤트의 판정값을 전달한다. 도움말·HUD·메뉴의 입력을 기존 순서대로 처리하고, 그쪽에서 받지 않은 같은 캔버스 위치의 `move`만 월드로 전달하기 직전에 거른다. [Button::input](../Sources/Core/UI/Button.cpp#L69)은 같은 위치의 `move`에서도 포인터가 캡처돼 있으면 콜백을 만든다. 이 콜백이 아이템 드래그와 상세·선택 상태에 쓰이는 것이 공통 UIKit 진입점에서 추가 이벤트를 일괄 제거하지 않는 근거다.
- 판정은 기기 이름이나 size class 없이 동일하게 적용한다. 현재 콜백의 좌표 두 개만 비교하고 포인터별 새 캐시를 두지 않는다. 유효한 이동과 종료·취소, 화면↔월드 역변환, 공통 `DashGesture`·`WorldNavigation`의 시간·SP·거리 기준은 기존 계약을 따른다. 앱 테스트는 raw 좌표와 투영 좌표, 월드 전달 결과를 각각 기록한다. 실패 좌표 쌍과 유효 이동, HUD 편집·아이템 드래그, 화면 밖 취소, 한 점·여러 점 교체 뒤 점프와 SP 100 소비를 양쪽에서 확인한다.

### 뷰포트 — 공통 카메라 안에서 보정만 바꾼다

- `CameraPresentation.cpp`의 파일 내부 함수로 목표 배율 계산을 분리한다. 기존 `requestedFocus`는 표적·경로와 플레이어 거리만 계산한다. HUD 보정 함수는 후보 배율, 기본 HUD bounds, 플레이어 위치를 받아 연속적인 목표 하한만 반환한다. HUD 좌우 경계까지 남은 거리로 보정 강도를 계산하고, 직전 프레임의 `worldScale`을 겹침 판정에 다시 넣는 경로를 끊는다. 맵을 덮는 최소 배율은 고정 제약으로 적용한다. `advance`가 목표와 현재 배율을 갱신하고 `project`가 기존 결과 형식으로 내보낸다.
- 새 보정의 선택은 공통 C++ 카메라의 기존 `compactFocus` 경계 한곳에서 한다. 명시적 phone과 소형 자동 프로필에 새 계산을 적용한다. 좁은 명시적 tablet도 `compactFocus`를 쓰므로 그 경로는 기존 HUD 하한 계산과 갱신 순서를 작은 함수로 보존한다. `legacyHD`와 `fixedSurface`는 기존 조기 반환을 사용한다. `DisplayConfiguration::compact`는 입력·배치와 `Screens::HUDState::selectProfile`의 저장 프로필까지 결정하므로 임계값 600과 프로필 판정은 그대로 둔다.
- `CombatScene::updateDisplay`는 표적·경로를 전달하고 결과를 `MapCamera`에 반영한다. `WorldNavigation::constrainCamera`의 위치·맵 경계, `GameSession::drawWorld/inputWorld`의 공통 정·역변환, `CombatScene::tick`의 가시성 갱신 순서를 보존한다. 기본 및 편집 HUD, 하단 포털, 방향 전환을 포함해 시작·이동·정지 구간의 HUD 가림과 맵 밖 노출을 각각 기록한다. 보간 때문에 가림이 생기면 HUD 접근 전에 목표를 올리는 구간을 조정하고 같은 경로를 다시 검사한다.

### 상점·인벤토리 — 내부 페이지 계산을 하나로 모은다

- `MenuGridLayout.hpp`의 순수 함수는 이전 페이지의 기준 칸, 새 페이지당 칸 수, 현재 게임 페이지의 유효 칸 수를 받아 새 내부 페이지를 반환한다. 기준 칸을 유효 범위로 제한한 뒤 새 칸 수로 나눈다. 빈 목록은 내부 페이지 0이다. `Screens::Impl`이 현재 게임 페이지의 상품 수와 인벤토리 25칸을 넘긴다.
- [Screens::refresh](../Sources/Core/UI/Screens.cpp#L1398)가 새 `Impl`을 조립할 때, 같은 기지·Phone 상점/정비 화면과 같은 게임 페이지를 유지하며 행 수가 달라진 경우에만 다시 계산한다. 상품 쪽은 분류도 같아야 한다. 보통 이전 화면의 첫 표시 칸을 기준으로 삼고, 상세창의 항목이 이전 페이지에서 보였다면 그 항목의 칸을 기준으로 상세창과 셀을 같이 유지한다. 상품 분류·게임 페이지 이동, Phone 메뉴 버튼, 거래 확인은 각자의 기존 경로를 사용한다.
- `phoneShop`·`inventory`·`buildDetail`·`phoneDropPoint`가 같은 `stockWindow/inventoryWindow`를 읽는다. HD 메뉴는 기존 조립 경로를 사용한다. `Screens::setDisplay`의 입력 정리, `resourceGeneration`과 정식·치트 렌더러의 첫 사용 텍스처 준비를 유지한다. 2행↔3행에서 끝 페이지·부분 페이지·빈 목록, 상세창 및 아이템 드롭, iPad의 버튼·거래·저장을 검사한다.

### Skyway — 배경 가로 위치를 고정한다

`MapPreview::compose`의 화면 채움 분기에서 배경의 가로 중심을 제한하는 순수 함수를 둔다. 논리 뷰포트 중심에서 배경에 남는 가로 여유의 절반만큼 이동을 허용한다. 여유가 0 또는 작은 음수면 허용 구간은 화면 중심 한 점이다. 현재처럼 폭 비교가 실패한 틱마다 제한을 건너뛰는 경우를 없앤다. 배경의 확대·세로 시차와 지형 반올림은 기존 경로를 사용한다.

소형 iPad 창도 `compact`에서 채움 경로를 사용할 수 있다. 기록한 줌 값으로 기존 C++ 배경 계산을 돌린 결과, 화면 방향에 따라 599×820에서는 보정 전후 위치가 같았고 820×599에서는 최대 5.898pt 차이가 났다. 이는 CPU 투영 결과다. 적용 여부는 `CombatScene::updateDisplay`에서 기존 `DisplayConfiguration.device`로 고른다. 명시적 tablet은 기존 경로를 사용하며, 명시적 phone과 소형 자동 프로필에 새 계산을 적용한다. `MapCamera`에는 계산을 고르는 표시 값만 넣는다. iPad의 일반 창과 강제 소형 창을 수정 전후 대조한다. 지형 중심을 연속 좌표로 옮긴 별도 GPU 실험은 두 기기에서 픽셀을 바꿨다. 배경 수정 뒤 같은 실기기 떨림이 남을 때 지형 쪽을 별도 조사한다.

### 수정 소스 범위 다시 산정

제품 파일 기준 추산은 9개다. iPad 소형 창의 기존 동작을 보존하기 위한 표시 값 전달 2개를 포함한다. 점프 입력, 카메라 목표 배율, 배경 위치, 메뉴 내부 페이지를 각각 독립 커밋으로 수정한다.

| 항목 | 수정 대상 파일 | 재현과 회귀 기준 |
| --- | --- | --- |
| 점프 | `HM2SourceView.mm`, `HM2GameRuntime.hpp`, `HM2GameRuntime.mm` | 실제로 변환 후 같은 좌표의 `move`만 월드 전달에서 거른다. iPhone 두 경로의 기존 실패와 iPad에서 점프 1회·SP 100 소비, HUD·메뉴 입력을 비교한다. |
| 뷰포트 | `CameraPresentation.cpp` | HUD 때문에 현재 배율을 즉시 올리는 경로를 목표값 보정으로 옮긴다. 포털 0의 급변과 전체 78곳의 이동·가시성·경계를 확인한다. |
| Skyway | `MapPreview.cpp`, `MapPreview.hpp`, `CombatScene.cpp` | Base003→Stage02_04 게이트 1에서 배경 가로 급변 60회가 0회가 되는지 확인한다. 지형·시차·맵 바깥 노출과 정식 iPad 프레임을 비교한다. |
| 상점·인벤토리 | `MenuGridLayout.hpp`, `Screens.cpp` | 페이지 계산을 분리한다. 창 높이를 바꾼 뒤 셀·상세·드롭과 첫 렌더를 확인한다. |

회귀 검사는 `WorldInputTests.mm`, `SessionRendererTests.mm`, `CameraPresentationTests.cpp`, `DisplayLayoutTests.cpp`의 기존 진입점을 확장한다. 치트 쪽은 메뉴·이동 시나리오의 필요에 따라 기존 동시 실행기의 설정과 관측을 확장한다. 제품 추산과 시험 파일 수를 구분한다.

### iPad 전후 검사와 변경 순서

`3140e0e`의 제품 소스와 기존 결과를 기준으로, 점프·배경·배율·메뉴 순으로 독립 커밋을 검사한다. Apple 입력의 변경 허용 범위는 같은 캔버스 좌표의 월드 move다. 고정 iPad와 좁은 명시적 tablet을 별도로 실행한다. 599/600pt, 기존 13인치 1024×1366 기준, 창 크기·인셋 변화, 소형/일반 화면 전환, iPhone·iPad 세션 동시 진행을 포함한다. 배율 변경 뒤 게이트 1의 배경도 다시 검사한다.

iPad는 같은 시작·입력·장비·seed에서 카메라 배율·중심·시야, 정점·레이어·clip, 입력 결과와 터치 해제, 표적·피해·시간·RNG·저장 결과를 비교한다. 타깃 검사는 기존 `SharedCameraPolicyAndIsolation`, `PhonePortalRoutesAndHUDInput`, `UniversalDisplayAndInput`, `ConcurrentTabletDisplay`, `WorldInputTests`, 첫 사용 상점 아이콘 검사에 연결한다. 표시·게임 상태를 새 기준으로 바꾸기 전 기존 iPad 결과와 달라진 사건의 원인을 확인한다. 이 절은 구현과 검사의 계획이다.

창 재구성의 Native 검사에서는 가로·세로 size class, 기기 분류, bounds, 인셋, `setDisplay` 호출을 따로 기록한다. 같은 bounds·인셋에서 trait만 바꾼 표본과 trait를 유지한 채 bounds를 바꾼 표본을 비교한다. 카메라 초기화·입력 정리가 어떤 변경에서 일어나는지 구분한다.

## 수정 후 검증과 전환 기준

수정 전 제품 소스 `3140e0e`와 수정 후보를 같은 맵·장비·시작 저장·seed·입력 순서에서 비교한다. 기존 [실패 로그와 측정값](evidence/iphone-module-audit-20261118.json)을 보존한다. 추가 회귀 검사에서 각 실패를 재현할 때는 기준 소스를 별도 작업 트리에서 실행한다. iPad 비교는 iPad의 수정 전후 조건끼리 한다. 실행마다 커밋·앱 대상·시뮬레이터와 OS·실제 캔버스/인셋·입력·저장 및 자원 해시·로그를 기록한다. Source 시험 저장과 치트 저장은 실행 UUID로 격리한다.

### 1. 공통 코드와 Apple 회귀 검사

| 항목 | 같은 입력과 합격 기준 |
| --- | --- |
| 점프 | 실패 로그의 raw 좌표 쌍으로 같은 캔버스의 `move`를 재현한다. 같은 좌표의 move는 UI에 전달될 수 있고, UI가 소비하지 않으면 월드 탭 기록을 지우지 않아야 한다. 한 점·여러 점 교체 뒤 각 더블탭에서 점프 1회·SP 100 소비·포인터 해제를 확인한다. 실제 캔버스 이동은 기존 경로점·탭 처리와 일치해야 한다. SP 부족·시간 경계·화면 밖 종료/취소·두 포인터·HUD 드래그·아이템 드롭도 검사한다. |
| 카메라 | `Stage02_04` 포털 0의 기존 경로와 포털 78곳의 각 480틱을 같은 조건으로 돌린다. 고정된 창의 이동 구간에서 기존 검사 기준인 한 틱 배율 차이 `>0.01`인 급변은 0회여야 한다. 맵 밖 노출·HUD 가림 없이 시야가 표적·경로에 따라 움직여야 한다. 기본·편집 HUD에서 포털에 실제로 진입하고, 표시 위치·역입력·표적 판정이 일치해야 한다. 창 변경으로 초기화되는 프레임은 이동 구간과 따로 판정한다. |
| 메뉴와 자원 | 상품 24개·인벤토리 25칸의 끝 내부 페이지에서 912×420↔912×550을 왕복한다. 2행에서 3행으로 바뀔 때 보이던 상품과 인벤토리 첫 칸이 새 페이지에 포함되고, 상세·선택·드롭이 같은 항목을 가리켜야 한다. 부분 페이지와 빈 목록도 확인한다. 실제 레벨 5 상점의 `Icon10113.png`는 콜드 캐시의 첫 표시에서 GPU 오류가 없어야 한다. 같은 페이지의 재방문과 예열 후 120프레임 동안 추가 읽기·업로드·저장 요청은 0이다. |
| iPad 보존 | 1024×1366, 좁은 tablet 599/600pt, 일반→소형→일반 전환을 비교한다. 같은 창에서 카메라 중심·시야·정점·클립과 유효 입력, HD 메뉴와 HUD, 표적·피해·시간·RNG·저장 결과가 기준과 같아야 한다. 같은 캔버스 위치에서 추가로 걸러진 월드 move는 별도 기록한다. |
| Skyway | 기지 출구 버튼으로 게이트 1에 진입해 같은 300프레임을 비교한다. iPhone 배경의 1pt 초과 이동은 0회여야 한다. 정상 iPad와 강제 소형 tablet의 배경·지형·오브젝트 정점, 시차, 클립을 기준과 대조한다. 두 기기의 GPU 결과에서 발판 위치와 맵 밖 노출을 확인한다. |

공통 검사는 `build/core`에서 원본 준비 자원을 `HM2_HD_RESOURCES`로 지정해 빌드한다. `ctest --test-dir build/core --output-on-failure`로 검사한다. 구현 중에는 `OriginalDashSession`, `SharedCameraPolicyAndIsolation`, `PhonePortalRoutesAndHUDInput`, `UniversalDisplayAndInput`, `OriginalScreensAndHUD` 등 변경 경로를 우선 돌린다. 통합 후보는 전체 호스트 검사를 한 번 실행한다. 원시 UIKit 좌표와 월드 전달 분기는 공통 코어만으로 검증할 수 없으므로 Source의 Apple XCTest에 실패 좌표 쌍을 고정한 검사를 연결한다.

### 2. 정식 앱 경로를 시뮬레이터에서 검증

`Apple/iOS/HeavyMach2.xcodeproj`에서 `HeavyMach2Source`의 Debug XCTest를 실행한다. iOS 26.5의 iPhone Air와 iPad Air 11인치를 각기 지정하며, 13인치 고정 카메라는 별도 iPad 조건으로 비교한다. 실제 UIKit 입력은 `HeavyMach2UITests/WorldInputTests`의 `testOSMultiplePathThenSinglePathThenDoubleTap`, `testOSMultiplePathThenMultiplePathThenDoubleTap`을 기기·경로별 5회씩 실행한다. 실행을 다시 시작할 때마다 새 시험 저장을 쓴다. 첫 실패와 입력 이력을 보관하며, 재시도 성공으로 앞선 실패를 지우지 않는다. OS가 깨끗한 탭만 보낸 표본은 위 고정 좌표 XCTest와 함께 판정한다.

기기마다 `HM2_TEST_SIM_ID`, 빌드 디렉터리 `HM2_TEST_BUILD`, 새 결과 경로 `HM2_TEST_RESULT`를 지정한다. 아래 명령을 각 기기에서 한 번씩 실행한다.

```sh
xcodebuild -project Apple/iOS/HeavyMach2.xcodeproj -scheme HeavyMach2Source \
  -configuration Debug -destination "platform=iOS Simulator,id=$HM2_TEST_SIM_ID" \
  -derivedDataPath "$HM2_TEST_BUILD" -resultBundlePath "$HM2_TEST_RESULT" \
  -parallel-testing-enabled NO -test-iterations 5 -test-repetition-relaunch-enabled YES \
  -only-testing:HeavyMach2UITests/WorldInputTests/testOSMultiplePathThenSinglePathThenDoubleTap \
  -only-testing:HeavyMach2UITests/WorldInputTests/testOSMultiplePathThenMultiplePathThenDoubleTap test
```

`HeavyMach2AppleTests/SessionRendererTests`에서 실제 GameRuntime의 창 크기 변경·첫 GPU 프레임, `testColdShopPagePreparesFirstUseIconsThroughGameRuntime`, `testPhoneBottomHUDAndOriginalPortalRoutes`를 확인한다. 축소한 창을 넣어 보는 검사는 실제 OS 창 전환과 분리해 기록한다. 휴대폰의 두 경로와 iPad를 각각 별도 `.xcresult`에 남긴다. Debug가 통과한 후보의 Source Release 시뮬레이터 빌드로도 타이틀→슬롯→전장·메뉴의 일반 플레이와 저장·재실행을 확인한다. 시험 저장 주입 경로는 Debug에서만 사용한다.

### 3. 치트빌드로 동시 실행과 상태 보존 검증

`HeavyMach2Validation`은 앱 스킴으로 빌드한다. 각 시뮬레이터에 `xcrun simctl install`로 설치하고 `xcrun simctl launch`로 `com.gelb2.heavymach2.validation`을 실행한다. `xcrun simctl get_app_container <기기 ID> com.gelb2.heavymach2.validation data`로 컨테이너를 찾고, 이번 실행에서 생성된 `Documents/ConcurrentValidation/<UUID>/report.json`을 가져온다. 화면을 사용하는 현재 실행기의 기준은 126개 상태·2,100프레임이다. 최종 프레임 수와 보고서의 `frames`·`presented`를 대조한다. 기존 케이스를 모두 유지하고 새 케이스를 추가하면 총수를 함께 기록한다. [치트 앱 빌드·실행 명령](public/build-and-install.md#치트-앱-실행과-결과)

- 기존 경로 교체 케이스 115·116과 표적 줌·유휴·HUD·중단 케이스에 상태 비교를 연결한다. HUD 경계 이동과 상점 끝 페이지 창 변경도 독립 세션에서 실행한다. 다른 전투 세션이 같은 프레임에 움직이고 공격하는지 `frameActivity`로 확인한다.
- 현재 `ConcurrentRuntime::setDisplay`는 모든 세션을 갱신한다. 메뉴 창 변경 케이스는 해당 시험 세션과 화면만 재구성한다. `pressButton`의 `revealButton`은 아이템이 있는 페이지를 먼저 열 수 있다. 빈 페이지 회귀는 실제로 보이는 페이지 버튼만 눌러 재현하고, 창 변경 직후 첫 프레임의 목록·상세·리소스를 확인한다.
- 시작 조건만 준비하고 입력·전투·거래·저장·GPU 준비는 제품 경로에서 일어나야 한다. 조건별 합격, 버튼 실패 수, 이동·발사·메뉴 활동의 겹침, 렌더 오류와 예열 후 자원 재사용을 기록한다. 치트의 공통 입력은 UIKit 원시 좌표를 거치지 않으므로 점프 어댑터의 합격 근거에는 Source XCTest를 함께 붙인다. 기존 [작업 순서](validation-build.md#작업-순서)에 따라 최종 HD 포팅판 대조는 iPad에서 일괄 처리한다.

### 롤백과 실기기 전환

- **실기기 검증 후보.** 변경한 결함의 고정 재현 테스트와 iPhone·iPad Source 검사가 모두 통과하고, 치트빌드의 기존·추가 케이스 및 iPad 전후 비교에 새 실패가 없어야 한다. 그 커밋의 서명된 Source Release를 후보로 삼는다. 기기 연결과 저장 백업을 확인한 뒤 iPhone에서 같은 손 입력과 이동을 확인하고 iPad에서 기존 동작을 재확인한다.
- **실패 또는 롤백.** 새 회귀가 확인되면 해당 커밋의 `.xcresult`·프레임·`report.json`과 기준 비교 결과를 보존하고 실기기 전환을 보류한다. 원인을 수정한 뒤 해당 검사를 다시 실행한다. 되돌릴 경우 회귀를 낸 구현 커밋만 `git revert`로 취소하고 같은 iPad 검사와 나머지 수정의 회귀 검사를 다시 실행한다. 상점 크래시 수정 `1420cc1`과 기존 증거는 유지한다. 되돌린 결함은 미해결로 기록한다.
- **실행 환경 오류.** 부팅 실패나 결과 파일 누락은 미검증으로 기록한다. 원인을 확인한 뒤 같은 후보의 해당 실행을 다시 수행한다.

이 계획 보강에서 확인한 범위는 기존 CTest 등록, Xcode 스킴과 테스트 경로, 시뮬레이터 목록, 치트 실행기의 종료·결과 파일 경로다. 수정 후보의 새 검증은 구현 후 실행한다.

## Wasteland Base에서 Skyway로 나간 뒤의 떨림

`MapInfo.mcb`에서 Wasteland Base `Base003`의 Skyway 출구가 `Stage02_04` 게이트 1로 이어짐을 확인했다. iOS 26.5의 iPhone Air·iPad Air에 각각 격리 저장을 만들어 실제 기지 버튼 2005를 두 번 눌렀다. 화면 전환 후 두 기기의 시작 위치는 `(403,419)`였다. 정식 `GameRuntime::input`으로 이동을 세 번 지시했고 월드가 모두 받았다. iPhone과 iPad의 입력은 각 시야에 맞게 제한했으며, 실제 도달 지점은 달라질 수 있다. 각 300프레임에서 정식 그리기 경로의 GPU 결과와 렌더 명령을 저장했다.

| 관측 | iPhone 912×420 | iPad 820×1180 |
| --- | ---: | ---: |
| 배경 가로 중심 이동이 1pt를 넘은 프레임 사이 | 60 | 0 |
| 배경 가로 이동 최대 | 3.170pt | 0.154pt |
| 진단용 배경 중심 제한 적용 후 1pt 초과 | 0 | 0 |
| 지형 반올림 오차의 최대 틱 변화 X/Y | 0.906/0.860pt | 0.944/0pt |
| 배율 변화가 한 틱에 0.01 초과 | 0 | 0 |

배경은 `Stage02_04BG.jpg`, 격자 발판은 투명 영역이 있는 `Stage02_04.png`에 그려져 있다. `b63f588`에서 추가한 [MapPreview::compose의 화면 채움 분기](../Sources/Core/Presentation/MapPreview.cpp#L37)는 배경 폭이 뷰포트 폭 이상일 때만 중심을 제한한다. 수학적으로 같은 폭도 부동소수점으로 계산하면 조금 작아질 수 있다. iPhone의 26→27→28프레임에서 배경 X는 456→452.830→456pt로 움직였고, 지형 X는 402.708→403.282→403.858pt였다. 27프레임의 논리 폭 차이는 −0.000061이었다. 이때 제한을 건너뛰는 분기가 300프레임 중 50회 나타났다.

동일한 27프레임에서 지형·배율·UV를 그대로 두고 배경 X만 456pt로 제한한 GPU 비교를 생성했다. 발판은 같은 위치였고 그 밑 배경만 이동했다. 세 RGB 채널 중 하나라도 값 차이가 2를 넘은 픽셀은 1,630,926개였다. 동일한 명령을 반복한 비교는 예열 후 두 기기 모두 바이트가 일치했다. 처음 실행의 콜드 렌더에서는 바이트 일치 검사에 실패해 첫 로그를 보존했고, 예열 후 대조 실행으로 다시 검사했다. [실행 근거](evidence/skyway-transition-20261118.json)

이 결과로 사용자 경로의 레이어 상대 떨림에 기여하는 배경 분기를 확인했다. 게이트 0의 HUD 배율 급변은 기존 별도 결함이다. 물리 iPhone의 연속 프레임과 전체 미세 떨림 해소는 추가 검증 범위다. GPU 비교는 진단용 렌더 명령에서만 수행했다.

## 카메라와 레이어

상점 수정 `1420cc1`을 포함한 분석 브랜치 `audit/iphone-original-logic-20261116`의 `3140e0e`를 사용했다. 준비된 원본 맵과 HD 공통 전투 코드로 포털 시작점 78곳을 각 480틱 실행했다. 두 화면 프로필은 같은 시작점·저장 형식·난수 시드를 사용했다. 입력은 해당 프레임의 카메라로 지정한 뒤 캔버스 안으로 제한했다. 프로필에 따라 투영된 입력 위치와 이후 이동 궤적은 달라질 수 있다.

| 화면 프로필 | 프레임 | 한 프레임 배율 변화가 0.01 초과 | 최대 변화량 |
| --- | ---: | ---: | ---: |
| iPhone 912×420 | 37,440 | 19 | 0.092919 |
| iPad 1024×1366 | 37,440 | 0 | 0 |

- **이동 중 뷰포트 급변.** `Stage02_04` 포털 0에서 경로를 따라 이동할 때, 0부터 센 프레임 228에서 플레이어 x가 −326.78에서 −326.06으로 옮겨갔다. 그 사이 배율이 0.907081에서 1로 바뀌었다. 논리 뷰포트는 약 1005.4×463.0에서 912×420으로 줄었다. 출력 캔버스는 912×420이었다.
- **발생 코드.** `b63f588`의 [CameraPresentation::advance](../Sources/Core/Presentation/CameraPresentation.cpp#L52)는 직전 배율로 화면 하단에서 플레이어가 기본 HUD와 겹치는지를 계산한다. 조건을 넘으면 새 하한을 보간값에 즉시 적용한다. 진단용 실행 파일에서 그 즉시 적용만 빼고 같은 검사를 반복하자 19번이 0번으로 줄었다. 이 실험은 배율 급변의 원인 분리용이다. HUD 겹침과 맵 경계의 제품 수정 검사는 별도로 필요하다. [CombatScene::tick](../Sources/Core/Simulation/CombatScene.cpp#L728)이 배율을 카메라 경계와 가시성 갱신에 사용한다. [GameSession](../Sources/Core/Session/GameSession.cpp#L732)은 그 배율로 월드를 그리며, 화면 터치를 월드로 되돌릴 때도 같은 변환을 사용한다.
- **원본 일반판.** REA로 ARMv7의 `0x5af50→0x5a418→0x57124`를 연결했다. 현재 배율을 직전 목표에 1/100씩 접근시키고, 배율과 화면 절반 크기로 경계를 계산한 뒤 새 목표와 위치를 갱신한다. 하단 경계는 `−256 + (화면 반높이 − 40) / 배율`이다. 이 경로에서 현재 복원판의 HUD 사각형 교차 검사와 즉시 하한 적용은 확인되지 않았다.
- **지형 좌표 반올림.** 별도 배경을 쓰는 `Stage02_04`–`Stage02_08`에서 지형 중심과 반올림 전 월드 원점의 차이는 연속 프레임에서 iPhone 최대 0.966, iPad 최대 1.316 캔버스 단위 변했다. 즉시 배율 하한을 뺀 진단에서도 최대치는 유지됐다. [MapPreview](../Sources/Core/Presentation/MapPreview.cpp#L34)의 지형 반올림은 iPhone 도입 전부터 있었다. 원본 일반판 `0x4bebc`의 공용 fixed-position 경로도 투영 전에 반올림한다. 포팅 이후 compact가 추가한 배경 위치 계산은 [게이트 1 GPU 검사](#wasteland-base에서-skyway로-나간-뒤의-떨림)에서 대조했다. [원본 좌표 분석](iphone-universal-native-followup-20261113.md)

## 경로 재지정 뒤 점프

### 시뮬레이터의 UIKit 입력

현재 소스의 Debug 앱과 격리된 XCTest 저장을 사용했다. iPhone Air와 iPad Air 11인치의 iOS 26.5에서 `WorldInputTests`의 한 점·여러 점 교체를 실행했다. 계측 전 iPhone은 한 점 교체 2회 통과·1회 실패, 여러 점 교체 1회 통과·1회 실패였다. iPad는 각 교체가 2회씩 통과했다. 성공 검사에서는 두 번째 begin에서 점프 시작과 SP 100 소비를 확인했다. 이 작은 검사 표본으로 실기기의 실패율을 추정하지 않는다.

실패는 첫 탭의 `begin→move→end`에서 시작했다. 한 점 교체 뒤 첫 begin은 탭 시각을 3.4362초로 기록했다. 같은 포인터의 move에 캔버스와 월드 좌표는 그대로였고 경로점도 1개를 유지했지만, 탭 시각은 0으로 바뀌었다. 첫 begin에서 두 번째 begin까지 월드 시간은 약 0.0166초였다. UI·도움말이 두 탭을 가로채지 않았다. 두 번째 begin은 월드에 전달됐고 SP는 180 그대로였으며 점프 시작 수는 증가하지 않았다. 여러 점 교체 뒤 실패에도 같은 캔버스 좌표의 move가 탭 시각을 지우는 순서가 남았다.

별도 작업 트리에서 `HM2SourceView`에 로그만 추가해 같은 실패를 다시 재현했다. 첫 탭의 move에서 UIKit의 raw x는 현재 339.66666666666669, 이전 339.66666666666674였다. 차이 약 −5.68×10⁻¹⁴는 두 좌표를 변환한 캔버스에서 0이었다. `bc3323a`의 raw `CGPoint` 정확 일치 비교는 이 move를 통과시켰다. 같은 포인터에서 `CombatScene::input→DashGesture::pathDrag`가 첫 탭 기록을 지우는 것을 확인했다. 같은 앱에서 성공한 입력의 첫 탭에는 move가 도착하지 않았다. 물리 iPhone이 같은 raw 이벤트를 발생시키는지와 사용자 실패 빈도는 미확인이다.

### 이벤트를 통제한 CPU 대조

`Stage01_01`에서 기존 장비 `12001`과 메모리 저장 사본을 사용했다. 세 경로점→한 경로점 또는 세 경로점→새 세 경로점 뒤에 실제 `Screens`와 `GameSession` 입력을 보냈다. 두 탭 간격은 월드 시간 약 0.166초였고 두 번째 탭 직전 SP는 180이었다. 화면 두 프로필에서 같은 입력 순서와 간격을 사용했다. 보이는 빈 땅을 골라 각 프로필의 월드 좌표는 달랐다.

| 첫 탭 뒤 이벤트 | iPhone 한 점 / 여러 점 | iPad 한 점 / 여러 점 | SP |
| --- | --- | --- | --- |
| begin→end | 성공 / 성공 | 성공 / 성공 | 180→80 |
| begin→0.25 월드 단위 move→end | 실패 / 실패 | 실패 / 실패 | 180 유지 |

작은 move는 새 경로점을 추가하지 않았고, `DashGesture::pathDrag`가 첫 탭 시각을 0으로 바꿨다. `CombatScene::input`이 경로점 추가 거리를 검사하기 전에 이 함수를 호출한다. 원본 일반판 `0x59cf0`과 HD `0x12698`에도 같은 순서가 있다. 두 원본의 더블탭 기준은 0.5초 미만·SP 100 이상이다. 일반판 ARM의 move 추가 기준은 좌표 차이 제곱 1024 초과이고 복원판의 HD 기준은 3600 초과다. 입력에 쓰는 좌표 단위가 다르다. 원본 일반판의 점프 효과음 슬롯은 19이고 HD 복원판은 14다.

원본과 현재의 경로 드래그 규칙을 보존할 수 있는지 판단할 때, 원시 좌표 차이와 게임에 전달된 이동을 각각 확인해야 한다. 이 분석에서는 발동 조건이나 입력 정책을 바꾸지 않았다.

## 상점·인벤토리와 창 변경

`ca424ab`에서 `Screens::Impl`은 같은 화면을 재구성할 때 내부 페이지 번호를 복사한다. 페이지당 행 수는 새 캔버스 높이로 다시 계산한다. 이 둘을 함께 갱신하지 않아 유효 범위 밖 페이지가 생긴다. 실제 `Screens`에 상품 24개·인벤토리 25칸을 넣고 912×420에서 각각 다음 버튼을 두 번 누른 뒤 912×550으로 변경했다. 상품 8개·인벤토리 5칸이 보이던 페이지는 각각 0개·0칸이 됐다. 이전 버튼을 누르자 12개·10칸이 다시 보였다. 재현한 빈 화면은 가격·구매·저장에 쓰는 게임 페이지 번호를 바꾸지 않았다. 상점 텍스처 준비 문제를 고친 뒤에도 남는 페이지 상태 결함이다.

UIKit에서 캔버스나 안전 영역이 바뀌면 `GameRuntime::setDisplay`가 기존 포인터를 해제하고, `CombatScene::setDisplay`가 경로 입력 캡처·발사 상태를 정리하고 카메라를 다시 초기화한다. 창 크기가 유지된 상태에서도 안전 영역이 달라지면 이 분기를 탄다. 장시간 플레이에서 이 이벤트가 발생해 사용자의 점프 실패와 연결되는지는 확인하지 않았다.

## 모듈과 에셋의 확인 범위

아이폰 도입 직전 `0fa7928`과 비교한 새 소스 13개·기존 변경 소스 48개를 근거 JSON에 파일별로 기록했다. 아래는 새 모듈의 실제 호출 경로와 검증 범위다.

| 모듈 | 사용 경로 및 관측 | 남은 검증 |
| --- | --- | --- |
| CameraPresentation | CombatScene이 이동 경로·적 표적·기본 HUD bounds로 카메라를 갱신한다. 이동 중 배율 급변을 재현했다. | 같은 경로의 GPU·실기기, 가시성·터치 결과 |
| DisplayLayout | GameSession의 월드 표시와 역입력, Screens의 HUD·메뉴 변환과 클립에 들어간다. 두 프로필의 이동·점프를 실행했다. | 모든 창 크기와 안전 영역 전환, 실제 드롭·클립 |
| HUDLayout | Screens는 편집 배치를 표시하고 입력에 쓴다. 카메라 보호는 기본 배치를 참조한다. | 편집 배치에서 월드 입력·가시성, 중단과 회전 |
| MenuArtwork | Screens의 표지·메뉴, MapLoading의 그림 선택에 쓰인다. 타이틀 시간과 HD 로딩 그림·레이어의 계약 불일치는 이전 보고에 남아 있다. | 게임 화면 전환·음향 타이밍과 원본 포팅판 비교 |
| MenuGridLayout | Screens의 상품·인벤토리 행 수와 입력 좌표를 계산한다. 창 높이 변경에서 빈 페이지를 재현했다. | 실제 창 변경, 전체 거래·장비·BOT 상세 |
| SharedAssetPreparation | GameSession·CombatScene가 무기 테이블로 공용 타일을 준비한다. Apple 런타임이 준비 프레임을 GPU에 보낸다. 상점의 첫 사용 오류는 별도 수정했다. | 전환·복귀·해제 뒤 차가운 캐시와 모든 첫 사용 |
| ResourceName | PreparedTiles, Apple 리소스 읽기, 렌더러에서 이름을 검사한다. | 맵·장비·효과·음향 전체 자원 참조와 누락 처리 |
| HM2HUDStorage | 두 Apple 런타임에서 compact/regular JSON을 읽고 명시적 Apply에서 원자 저장한다. | 손상 파일·쓰기 오류·프로필 이동·재실행 결합 |
| HM2Profiling | Apple 콜백과 Runtime의 tick·프레임 생성·present를 계측한다. | 실제 기기 프레임 변동·열 상태 |
| HUDLayoutExercise | 치트 실행기의 HUD 편집·저장 검증을 보조한다. | 정식 경로의 입력·GPU 준비와 대응 |

REA에서 추가로 읽은 함수는 일반 입력 `0x59cf0`, 카메라 경계 `0x5a418`, 위치·유닛 아이템 입력 `0x59720`·`0x59994`, 경로 진행 `0x58ce4`다. 경로 진행의 첫 점 제거 조건은 x와 y 차이가 각각 16 미만이며, 제거 뒤 다음 점을 목적지로 쓴다. 현재 `WorldNavigation::process`의 대응 조건도 확인했다. 두 아이템 입력 함수는 dossier를 보존했다. 효과별 정밀 비교는 남았다. 원본 파일과 전체 기계어·의사코드·스냅샷은 기존 로컬 분석 폴더에 보관했다.

공용 전투·저장·진행·자원 검사 78개를 실행해 모두 통과했다. 먼저 실행된 자동 배경·화면 훅은 해당 시점에 시뮬레이터 실행 파일이 없어서 ERROR였고, Phone UI 훅은 후보 근거가 오래돼 ERROR였다. 원본과 제품의 적합성 검사 통과로 계산하지 않는다. [검사 근거](evidence/iphone-module-audit-20261118.json)

원본 포팅판의 신규 런타임 실행, 실제 iPhone/iPad 동시 플레이, 모든 무기·적/BOT·진행·음향·저장·중단 조건의 원본 대조는 미완료다. 연결 검사에서 등록된 물리 iPhone Air와 iPad Air 13인치의 기기 터널은 모두 `unavailable`이었다. 분석은 기존 브랜치에서 실행했다. 제품 코드·에셋·물리 기기 앱·사용자 저장의 변경은 없었다.

소유자 비공개 사이트에 진단을 게시하고 기존 누적 목록에 연결했다. 사이트 소스 `8dc1a39`와 배포 `appgdep_6afd8343f9b88191966eb6230bf590f6`의 `succeeded`를 확인했다. [비공개 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/iphone-code-asset-diagnosis-20261116.html#module-audit-20261118)
