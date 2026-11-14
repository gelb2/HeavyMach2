# 이동 경로 재지정 후 점프 미발동 조사

사용자의 최신 재현 순서는 **여러 경로점 지정→새 경로점 하나 또는 새 여러 경로점 지정→더블탭에서 점프 미발동**이다. 다른 손가락 접촉이라는 앞선 추정은 현재 원인으로 사용하지 않는다. 원본 HD EasyMove·Apple 터치 전달·공통 입력 코드를 정적으로 대조한 뒤, root가 별도의 공통 코어 probe16조건을 실행했다. 앱·시뮬레이터·실기기 입력은 이번 조사에서 실행하지 않았다. 이 문서 작성 담당은 게임 코드를 수정하지 않았다.

## 확인한 원작 순서

HD `GobWorld.onTap_EasyMove:State:ID:`0x12698의 의사 코드와 ASM을 함께 읽었다. 의사 코드의 CGPoint/SEL/인수 표시는 ARM ABI 때문에 어긋나므로 아래 상태/ID는 ASM과 함께 판정했다.

- 상태는 진입0x126b4에서 `[sp+0xa4]`→r10으로 읽고, ID는 이후 `[sp+0xa8]`에서 읽는다. state0/1/2가 begin/move/end다.
- 유효한 지면 begin은 기존 moveTapID가0이어야 한다. Player와 거리 제곱이900보다 작으면 이동 ID만 잡고 점프 판정 전에 반환한다. 살아 있는 적을 탭하면 표적 선택 분기로 간다.
- 지면 분기에서는 경로 재설정/절단/추가를 먼저 처리한 뒤 이전 tapDashTime을 검사한다. 이전 값이0이 아니고 world gameTime과의 차이가0.5초 미만이며 SP≥100이면 점프 속도 설정→이동 ID0→SP100 소비→SFX14 순서다. 성공하면 이전 탭 시각을 다시 쓰지 않는다. 실패 분기에서 현재 double gameTime을 float로 저장한다.
- owned move이며 pathCnt>0이면 **이동 거리를 검사하기 전에** tapDashTime을0으로 쓴다. 작은 move도 같다. ASM0x12dac..0x12dd2가 ID/경로 조건과 시각 초기화,0x12e24..0x12e30이 그다음 거리 제곱3600 검사를 보여준다.
- end는 같은 ID일 때 마지막 경로점과 거리 제곱900을 비교해 추가점을 처리한 뒤 moveTapID와 attackTapID를0으로 쓴다. ID가 달라도 마지막 두 ID 정리는 실행한다(ASM0x12fd8..0x13076).

현재 `CombatScene::input`, `WorldNavigation::inputWorld`, `DashGesture`의 위 핵심 begin/move/end 순서는 이 선택 계약과 대체로 일치한다. 현재 정적 자료만으로 **경로 개수/재지정 자체가 점프를 막는 확정 결함**은 찾지 못했다. 이전 경로 상태나 원작의 감도를 임의로 지우거나 move 시각 초기화를 없애는 수정을 근거 없이 제안하지 않는다.

## Apple 터치 전달

원작 EAGLView의 began0x3bf8/moved0x3acc/ended0x39a0은 각 changed UITouch를 순회해 좌표와0/1/2 상태, 같은 UITouch 객체 ID를 delegate에 즉시 전달한다. moved ASM0x3b94..0x3ba6에서 state1과 touch ID를 스택에 놓고 바로 호출하는 것을 확인했다. delegate0x25fc는 방향 좌표를 바꾸고 QobManager0x3a23c로 전달한다.

원작이 터치를 display tick까지 큐잉하거나 작은 move를 합쳐 없앴다는 근거는 이 경로에 없다. 현재 `HM2SourceView`도 각 changed UITouch를 즉시 전달하며 Y변환은 renderer의 logicalPoint와 공통 표시 역변환을 거친다. UIKit callback 횟수나 물리 손 동작까지 동등하다는 실행 검증은 아직 아니다.

### 좌표 양자화·작은 move 필터 확인

EAGL moved0x3acc, delegate0x25fc, QobManager0x3a23c, World.onTap0x10f84의 ASM을 연결해 읽었다.

- EAGL은 locationInView의 X/Y float를 받아 Y만 bounds 높이에서 뺀다. moved ASM0x3b7a..0x3ba6에서 float 이동·`vsub.f32`·상태1/UITouch ID 전달을 확인했다.
- delegate는 방향에 따른 축 교환/빼기만 한다(0x262c..0x267a). QobManager는 좌표 R2/R3를 스택에 보존하고 receiver 호출 때 복원한다(0x3a24a/0x3a24c,0x3a308..0x3a30c). World.onTap도 좌표 레지스터를 보존해 EasyMove를 호출한다(0x10f8e/0x10f94,0x10fe6..0x10ff0).
- 이 경로에는 좌표를 정수/픽셀 격자로 내리는 floor/round/fixedPos, 이전 위치와 같은 move를 버리는 비교, 작은 이동 거리 필터를 확인하지 못했다. 렌더링의 fixedPos와 입력 좌표를 혼동하지 않는다.
- 현재 경로는 CGPoint→drawable/viewport 정규화→float canvas→공통 world 배율 역변환이다. `HM2Renderer::logicalPoint`와 `GameSession::inputWorld`에도 정수 좌표 양자화가 없다. 배율과 프레임 경계 반올림의 차이는 실제 수치로 별도 확인해야 하지만, “원작 어댑터가 작은 move를 걸렀는데 복원판이 정밀 좌표를 전달해서 실패한다”는 가설을 뒷받침하는 코드 근거는 찾지 못했다.

따라서 좌표 반올림이나 임의 최소 이동 거리를 추가하는 수정은 제안하지 않는다. 실제 OS 입력 기록에서 어떤 moved 이벤트가 왔고 어느 경로가 소비했는지 먼저 확인해야 한다.

원작 QobManager는 역등록 순서로 receiver를 순회하며 true를 돌려준 receiver에서 멈춘다. 현행 GameRuntime은 Help→Screens→World 순서로 consumed를 확인한다.

중앙 receiver 등록도 확인했다. GobWorld.init0x11208과 QobButton.init0x381a8은 각각 setUiReceiver(true)를 호출한다. QobBase.setUiReceiver0x3699c→QobManager.addUiReceiver0x3a37c의 pending add 배열을 거쳐 다음 onTap 진입에서 목록에 반영된다. child 트리를 재귀 순회하는 전달로 추정하지 않는다. World.onTap0x10f84는 pause가 꺼지고 Player가 존재하며 HP>0일 때만 EasyMove를 호출한다. 메뉴의 Pop 알림이 먼저 pause를 켰다면 World까지 end가 전달돼도 EasyMove의 정리는 실행되지 않을 수 있다.

## 확인한 전달 차이와 한계

원작 QobButton0x37ccc는 자신의 end를 처리하고 Pop/Cancel 알림을 보낸 뒤 false를 반환해 다음 receiver까지 end가 전달될 수 있다. 현행 `Button::input`도 false를 반환하지만 `Screens::input`은 호출 전 `capturedPointer`를 저장해 둔 `captured` 값으로 true를 반환한다. 따라서 캡처한 UI 버튼의 end는 World까지 전달되지 않는다.

이는 **receiver 전달의 코드 차이**다. 사용자가 보고한 단일 손가락 경로 재지정 뒤 실패의 직접 원인이라는 증거는 없다. 지면에서 시작한 손가락이 단순히 버튼 위를 지나는 것만으로 현재 버튼이 새로 캡처하는 코드도 아니다. UI/월드 소유권 보존과 기존 취소 검사를 함께 보지 않고 `captured`를 제거해서는 안 된다.

현재 GameSession은 화면 바깥 또는 HUD 아래에서 끝난 월드 end를 cancel로 바꿔 정리한다. 원작 EasyMove의 y<100 빠른 반환과 같은 코드라고 부르지 않는다. 이 정리가 실행됐는지, UI가 먼저 소비했는지를 실제 재현에서 따로 기록해야 한다.

## 공통 코어 probe16조건 결과

root가 `.local/evidence/iphone-path-jump-investigation-20261114/probe.cpp`를 현재 공통 라이브러리에 연결해 실행했다. 기존 DashSession의 시작 장비/진행 fixture와 실제 `GameSession::inputWorld`를 사용했으며 SP·점프 성공·tapTime을 강제로 쓰지 않았다. HD 기본/phone912×420, 단일/여러 경로 재지정, 작은 move 유무,31tick 대기 유무의16조건이다. `probe.log`에서 유효 지면 후보 부족이나 실행 제한은0건이었다.

| 조건 묶음 | 결과 |
|---|---|
| 첫 탭에 move 없음8조건 | 모두 점프1회·SFX14 1회. 경로 재지정만으로 실패하는 현상은 이 입력에서 재현되지 않음. |
| 첫 탭에0.01point move 포함8조건 |6조건은 tapTime이0으로 지워지고 점프0회. 이는 확인한 원작 owned move 규칙과 일치. |
| 나머지 작은 move2조건 | 단일 경로 재지정 직후 대기 없는 경우로, 첫 탭 begin에서 이미 점프가 발동하고 SP180→80. 그 뒤 move에는 소유 이동 포인터가 없어 tapTime이 지워지지 않음. |

HD/phone 쌍의 점프 횟수와SFX14 결과는 같은 조건에서 일치했다. 사용자 실패율80%를 재현하거나 반증한 결과는 아니다. 합성한0.01point move는 실제 손가락에서 관측한 값이 아니다.

이 fixture는 `selectSlot`/`enterField`를 직접 호출한 뒤180tick을 진행한다. 초기 transition·첫 도움말을 정상 앱 UI 순서로 모두 마쳤는지는 이 probe가 검증하지 않는다. Help/Screens를 거치지 않고 World 입력 API에 직접 전달하므로 실제 UIKit·화면 UI 소유권·사용자 진행 상태의 재현으로 쓰지 않는다. 최종 정식 앱과 동시 치트의 해당 경로에서 실제 입력을 다시 확인해야 한다.

## 원인 확정을 위한 다음 실행 기록

위 코어 probe의 한계를 보강해 아래 두 순서를 실제 앱 전달 경로로 재현한다. 내부 점프 성공값·SP·탭 시각을 강제로 쓰지 않는다.

1. 여러 경로점을 그린 뒤 손을 떼고 새 단일 경로를 찍는다. 이어 두 번의 지면 탭을 보낸다.
2. 여러 경로점을 그린 뒤 손을 떼고 다른 여러 경로점을 그린다. 이어 두 번의 지면 탭을 보낸다.

각 이벤트에서 다음 값을 전후로 남긴다.

- begin/move/end/cancel·pointer ID·입력 canvas/변환 후 월드 좌표·world gameTime.
- Help/Screens/World 중 소비자와 버튼 캡처 여부.
- movePointer·pathStart/pathCount·tapTime·선택 적 ID·Player와 거리 제곱.
- 실제 SP·dashStarts·점프 속도·사운드14 증가량.

같은 입력을 다음 조건으로 구분한다. 이들은 원인 후보이며 확인 결과가 아니다.

| 조건 | 구분하려는 분기 |
|---|---|
| begin/end만 있는 두 탭 | 경로 교체만으로 실패하는지 |
| 첫 탭에 작은 owned move 포함 | 원작/현행의 tapTime 초기화가 실제로 발생하는지 |
| 첫/둘째 탭이 Player30 world 반경 안팎 | Player 캡처 때문에 점프 검사까지 가지 않는지 |
| 경로 재지정과 각 탭 사이의 시간·SP 변화 | 최종 두 탭 전에 이미 점프가 발동해 SP를 소비했는지, 실제0.5초 조건을 넘겼는지 |
| 실제 end/cancel이 UI나 화면 경계에서 소비됨 | stale movePointer 때문에 다음 begin이 빠르게 반환하는지 |

원인을 확인하기 전에 감도·허용 시간·최소 이동 거리·SP 소비를 바꾸지 않는다. C++의 합성 입력 성공을 실제 UIKit 손가락 입력 통과로 보고하지 않는다. 최종 실제 Source 입력과 동시 치트 검사에서 사용자 재현 순서를 포함해야 한다.

## 근거 위치

- 원본 HD export: 로컬 `Analysis/HD103/method-evidence/functions/`의0x12698,0x3bf8,0x3acc,0x39a0,0x25fc,0x3a23c,0x37ccc 의사 코드/ASM. 원본 파일과 함수별 Evidence ID는 같은 폴더의 export-manifest에 연결돼 있다.
- 공통 소비: `Sources/Core/Simulation/CombatScene.cpp`, `WorldNavigation.cpp/.hpp`, `Dash.cpp/.hpp`, `Sources/Core/Session/GameSession.cpp`, `Sources/Core/UI/Button.cpp`, `Screens.cpp`.
- Apple 소비: `Sources/Platform/Apple/HM2SourceView.mm`, `HM2GameRuntime.mm`.
- 기존 검사: `tests/DashTests.cpp`, `tests/DashSessionTests.cpp`. 기존 작은 owned move 시각 초기화 검사는 보존한다. 이번 사용자 순서의 실행 결과를 기존 검사로 대신하지 않는다.

현재 다른 담당자가 같은 공통/UI/Apple 코드를 수정 중이다. 이 문서는 읽은 시점의 선택 계약 비교이며 이후 diff와 실행 결과를 다시 연결해야 한다.
