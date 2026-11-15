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

## 첫 iPhone OS 입력 실행에서 확인한 실패

통합 담당자가 실행한 첫 Native/UI batch에서 두 OS 경로 검사가 실제로 실패했다. 이 조사 담당자는 이미 끝난 iPhone xcresult에서 추출된 JSON을 읽었으며 새 앱·검사·시뮬레이터를 실행하지 않았다. iPad의 같은 실행 결과나 사용자의 물리 손가락 입력까지 이 결과로 대체하지 않는다.

| 실제 OS 조건 | 마지막 두 begin의 World 시간 간격 | 두 begin의 SP | Player와 실제 입력의 World 거리 | 결과 |
|---|---:|---:|---|---|
| 여러 경로→새 여러 경로→doubleTap |0.016600초|180→180|164.1295→164.9973|dash0→0|
| 여러 경로→새 단일 경로→doubleTap |0.033200초|180→180|106.7524→110.7821|dash0→0|

두 조건 모두 첫 탭이 `begin→move→end`였다. begin과 move의 canvas 좌표는 **정확히 같은(104,125)** 이고 World 시간도 같았다. 첫 begin이 tapTime을 저장한 직후, 같은 포인터의 move가 owned path1에서 tapTime을0으로 지웠다. 이어진 두 번째 begin은 이전 tapTime이0이므로 새 시각만 저장했고 점프하지 않았다. 두 번째 탭에도 같은 위치의 move가 있었다.

다중 조건의 첫 탭 시각은5.7269999, 단일 조건은4.6313999였다. 두 조건 모두 실제 end에서 이동 포인터가0으로 해제됐다. 모든 마지막 탭 이벤트는 World가 소비했고 UI/overlay는 소비하지 않았다. 최종 selectedEnemy0·paused0·systemVisible0이었다. 이 실행에서는 SP 부족,0.5초 초과,Player30 반경 캡처,끝나지 않은 이동 포인터가 실패 원인이 아니다.

이0거리 move에 tapTime을 지우는 현행 공통 규칙은 앞서 확인한 원본 EasyMove ASM과 일치한다. 원본 EAGLView moved0x3acc도 콜백으로 받은 changed touch마다 좌표와 state1을 전달하며 이전 좌표 비교나0거리 필터가 없다. 따라서 이번 결과만으로 원본의 작은 move 규칙을 버그라고 바꾸거나, 복원판이 그 규칙을 잘못 구현했다고 결론 내리지 않는다. 실제 물리 손가락에서도 이 이벤트가 발생하는지는 별도 기록이 필요하다.

현대 OS의 같은 위치 move를 호환 입력으로 제외하는 정책을 택한다면 원본에 없던 플랫폼 경계의 추가 규칙이라고 명시해야 한다. 거리 임계값을 신설하거나 공통 DashGesture를 바꾸는 방식과 구분한다. 그 경우에도 양수인 아주 작은 move는 원본대로 전달되는지,실제 drag·다중 터치·취소는 보존되는지 별도 검사해야 한다. 이번 분석에서는 입력 코드를 바꾸지 않았다.

근거는 `.local/evidence/iphone-fixes-20261114/iphone-first-batch-attachments/`의 다중 조건 `653DBE49-A1C6-4DF6-B13A-C932616A87F6.json`, 단일 조건 `A1B3B39B-DAD7-4E99-A679-7FDB8B382DB8.json`이다. 마지막 이벤트와 입력 거리 계산·원본 첨부 SHA는 같은 상위 폴더의 `os-dash-ring-analysis.json`에 보존했다. 원래 실패 log와 첨부는 수정하지 않았다.

## Apple 입력 호환 수정

위 실패 뒤 통합 담당자가 선택한 현대 Apple 호환 정책을 HM2SourceView에 추가했다. 실제 `UITouch.locationInView`와 `previousLocationInView`의 **raw CGPoint X/Y가 모두 정확히 같은 moved 이벤트만** 공통 입력으로 보내지 않는다. canvas float로 바꾼 뒤 비교하지 않으며 반올림·epsilon·거리 임계값을 쓰지 않는다. 두 축 중 하나라도 실제 차이가 있으면 아무리 작아도 원본 경로로 전달한다. begin/end/cancel 전달과 공통 DashGesture·WorldNavigation·시간·SP 규칙은 바꾸지 않았다.

이 정책은 원본 EAGLView에 없던 호환 추가다. 원본 자체가 같은 위치 move를 걸렀다고 보고하지 않는다. 첫 실패 기록은 변환 뒤 canvas 좌표만 같음을 증명했다. 수정 뒤 DEBUG 기록에는 마지막 raw current/previous와 UITouch timestamp·포인터·phase, 제외 수, 마지막 제외 raw 값을 넣어 실제 raw 차이가0인지 다시 확인한다. 실제 차이가 양수여서 여전히 실패한다면 이 필터의 통과 범위를 넓히지 않고 기록을 다시 판정한다.

진단 JSON은 기존처럼 실제 입력 배치가 끝난 시점에만 메모리에서 만든다. display callback은 캐시를 사용하며 Release에 진단 저장·로그·접근성 쓰기를 넣지 않았다. OS 경로 검사의 실제 경로 생성·점프 증가·100 SP 소비 기준은 유지했다. SDK가 같은 위치 moved를 발생시키지 않을 수도 있으므로 제외 수가 반드시 양수여야 한다고 검사하지 않는다. 제외가 관측되면 해당 raw 두 좌표의 정확한 일치와 timestamp를 검사한다.

이 수정은 아직 새 빌드·앱·OS 검사로 확인하지 않았다. 사용자가 보고한 물리 손 입력의80% 실패까지 해결됐다고 보장하지 않는다.

## 첫 iPad OS 경로 실행의 별도 실패

첫 batch의 iPad 두 경로 검사는 doubleTap 전, 최초 여러 경로 drag 종료에서 이동 포인터가 남아 실패했다. 이는 앞의 iPhone0위치 move와 다른 분기다. 실제 이벤트와 원본 MapInfo를 함께 읽었다.

최초 drag begin은 World(-476,-587.5), Player(-275,-412)였다. Player가(-298.0628,-432.1370)에 도달한 뒤 World 입력이 true에서 false로 바뀌고 World 시각이 멈췄다. Stage01_01의 원본 gate0은(-275,-462)→Base001이다. 당시 Player와의 각축 차이23.0628/29.8630은 WorldGates의 각축30미만 조건을 모두 만족한다. 최종 working location은Base001, paused1·doorPhase2·mapLoadingStep−1·systemVisible0·helpVisible0이다.

이 상태는 실제 기지 복귀 게이트에 들어가 문이 닫히는 단계다. 백그라운드 복귀 메뉴나 생명주기 중단으로 판정하지 않는다. 마지막 end의 실제 ring에는 overlay=false·UI=false·World=false와 이동 포인터 전후 동일 값이 남아 있다. SourceView와 GameRuntime의 inactive 분기는 기록 전에 반환하므로 이 end 기록은 두 어댑터가 활성인 상태에서 전달됐음을 보여준다.

소비 순서는 `WorldGates.tick→GameSession.linkGate→pause=true/working location Base001/기지 화면 전환 요청→CombatScene.input의paused 빠른 반환`이다. 원본 `GobWorld.onTap0x10f84`도 pause일 때 EasyMove를 호출하지 않으므로 end의 포인터 정리까지 실행하지 않는다. 이번 잔류를 공통 입력의 원본 규칙 불일치로 고치지 않는다. 현재 Apple 어댑터의 기존 World 소유 종료 취소는 UI/overlay가 소비한 end만 다루고 이 paused World 거절 분기는 포함하지 않는다. 취소 보강 여부는 해당 소유 경계로 따로 판단해야 한다.

OS 경로 검사의 지면 후보는 화면 안의 Player/적 거리를 확인했지만 지도 경계 밖과 실제 게이트 전환을 제외하지 않았다. 전환은 정상 게임 동작이며 사용자의 입력이 잘못됐다는 뜻은 아니다. 경로 재지정과 doubleTap을 같은 필드에서 검사하려면 원본 지도 경계와 게이트 위치를 반영한 내부 경로를 선택해야 한다. 게이트 동작을 끄거나 World·Player 상태를 강제로 바꾸는 방법은 사용하지 않는다. 이 분석에서는 추가 코드를 수정하지 않았다.

근거 첨부는 `.local/evidence/iphone-fixes-20261114/ipad-first-batch-attachments/1F89BEF6-BCFC-44E9-87EC-253BBF51EE08.json`과 `26CDC401-BC1C-4737-A9CC-3929B6C3CEEB.json`이다. 원본 MapInfo 준비 파일 SHA는`ab697612198efce5f9d9e9d8a085fc9f0a51cca683489c57b7edc3ade6b74bc8`이며 동일 공통 chunk 형식을 읽어 gate 좌표·링크를 확인했다. 첫 수락/거절/마지막end와 거리·첨부SHA는 상위 폴더의 `ipad-os-path-gate-analysis.json`에 보존했다.

## 같은 필드에서 검사할 경로 선택 보강

이 분석 뒤 통합 담당자는 paused World의 일반 종료 취소를 추가하지 않고 OS 검사의 경로 후보만 수정하도록 결정했다. World·게이트·Nav 동작은 그대로 둔다.

Stage01_01의 지도 범위±512와 검증된 세 게이트 좌표/각축30 범위를 검사 helper에 연결했다. 후보는 지도 가장자리에서160 World만큼 안쪽인±352 영역에 있어야 한다. Player에서 첫 후보로 가는 선분과 drag 선분이 실제 게이트 사각형을 통과하면 제외한다. drag 길이는180~320 World로 골라 원작의60거리 경로점 추가를 실제로 여러 번 거치면서, iPad의 화면 전체 overscan을 가로지르지 않도록 했다. 이 수치는 테스트의 목적지 선택이며 게임의 이동 거리·속도·게이트 범위를 바꾸지 않는다.

초기 drag, 경로 재지정, doubleTap 전후에 working location Stage01_01·화면field·doorPhase8·mapLoading없음·paused0을 함께 검사한다. 기존의 실제 다중 경로 생성·단일/다중 재지정·점프 증가·100 SP 소비 검사는 유지한다. GameOver나 다른 조건의 후보 선택은 수정하지 않았다.

첫 batch의 네 초기 상태 JSON에 새 후보 계산만 적용한 정적 대조에서는 iPhone 두 조건18개, iPad 두 조건167개 후보가 남았다. 선택된 drag는 iPhone291.973 World, iPad307.350 World이며 Player 진입 선분과 drag 선분의 게이트 교차는0이다. 이 계산은 저장된 초기 상태의 기하 대조이며 새 OS 실행·게임 진행·최종 통과 결과가 아니다. 근거는 `.local/evidence/iphone-fixes-20261114/os-path-safe-candidate-static-check.json`에 보존했다. 수정 뒤 앱·빌드는 실행하지 않았다.
