# 다음 원본 소비자 구현 계획 — GameOver·완료 임무·점수

2026-11-11 사용자 변경: 현재는 iOS·iPadOS 정상동작과 동시 치트 검증에 집중한다. 랭킹5009는 실제 서비스 연결 대신 기존 알림 형식으로 “현재 랭킹 서비스를 이용할 수 없습니다.”를 표시한다. 아래 점수/실서비스 어댑터 계획은 보류하며 원본 분석은 역사적 근거로 보존한다. 다른 플랫폼 확장·설치 전달·배포는 현재 범위에서 제외한다.

작성일: 2026-11-10. 이번 작업은 확보된 HD 원본 분석과 현재 소스의 읽기 전용 대조다. 앱 실행·게임 기능 구현·외부 점수 전송은 하지 않았다. Native112 실패 조사는 범위에 넣지 않았다.

원본 실행 파일 SHA256: `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`. 기존 `help-gameover-reference/spec.md`와 그 함수/ASM 근거를 재사용했다. 원본 소스코드를 확보했다는 뜻이 아니다. 원본 바이너리 분석의 정적 사실과 복원 구현 계획을 구분한다.

## 1. GameOver: 원본 종료 단계를 먼저 연결

### 확인한 원본 규칙

- Player.onDamage(0xbf58)는 방어·RNG를 적용하고 상위 피해 처리 뒤 실제 actorHP를 working slot +0x7c에 복사한다. 기존 공통 피해·장비·입자·음향을 유지한다.
- GobHvMach.onDamage(0xf3d0)는 HP를 0으로 제한하고 onCrash(0xf0f8)를 호출한다. onCrash는 SFX21, Efx_ExplodeNPC의 기존 풀/애니메이션16/scale1, 타깃 정리와 machState2를 처리한다. 적 사체·적 drop 경로를 Player에 붙이지 않는다.
- GobHvMach.tick(0xcfa4)의 unpaused 처리에서 state2는 processCrash(0xccf8)를 거쳐 virtual onDestroy를 호출한다. 임의 사망 대기 시간을 추가하지 않는다. show가 false면 tick은 조기 종료한다.
- Player.onDestroy(0xb024)는 machState3을 쓴 뒤 World.setGameOver(0x1104c)를 호출한다. World는 이미 gameState3이면 반환한다. 처음이면 setGameState3과 원본 changeStateTime 기록 뒤 AlertId2 알림을 요청한다. 이 함수에는 save/heal/setPause1이 없다.
- 알림 제목은 `Destroy Your Mach.`, 본문은 Description의 `DestroyMach`다. KR 원문 `매크가 파괴되었습니다.|기지로 돌아가서 매크를 수리합니다.`는 기존 Description 표시 규칙으로 줄바꿈한다. CancelBtn=false, OK 하나다.
- GameMain.alertView callback(0x5468)은 일치하는 alert만 처리한다. AlertId2를 버튼 index 일반 처리보다 먼저 분기해 현재 location(+0x2d)을 lastBase(+0x55)로 바꾸고 changeScreen3을 요청한 뒤 alertId를 0으로 지운다.
- 기존 공통 문 전환을 거쳐 Base에 들어간다. Base는 유효하지 않은 장소를 Base001로 보정하고 lastBase 갱신, 실제 baseHP+body bonus 회복, BOT savedHP 3개를 0으로 초기화한 뒤 기존 정상 Base 저장을 한다.
- gameState3은 pause가 아니다. pause가 아니면 world clock은 증가할 수 있고, processMyMach만 gameState0 조건을 받으며 camera/children 처리가 남는다. OS 알림에 따른 실제 앱 lifecycle 영향은 아직 실행 대조하지 않았다.

### 현재 소비 차이

CombatScene.damage는 HP0에서 baseCrash→state2→hooks.playerDied를 실행한다. Session.playerDied는 dirty만 만든다. tickPlayer에는 state2→processCrash→state3/World GameOver 소비자가 없다. setGameState는 현재 값 대입만 한다. 지금 playerDied에 알림을 바로 붙이면 원본 onDestroy 단계보다 빨라진다.

SessionHost·Apple NativeRequest에는 일반 알림만 있고 AlertId2 응답이 게임에 돌아오는 경로가 없다. SourceView의 일반 OK는 native queue를 닫을 뿐이다. 기존 이름/삭제 요청과 VisitIndiean 처리를 훼손하지 않고 별도 응답 경계를 추가해야 한다.

### 최소 구현과 검사

1. 실제 Player crash phase와 World gameState3의 멱등 처리를 연결한다. 원본의 render-time changeStateTime 기록이 필요한 후속 소비자도 확인하고 임의 world-time 대체를 하지 않는다.
2. Session에서 타입이 있는 GameOver 요청/확인을 제공한다. 요청 token·Session/slot 수명을 검사하고 중복·이전 세션 응답을 무시한다. 기존 알림 큐를 사용하며 가짜 UI 버튼·추가 save·강제 pause는 넣지 않는다.
3. 확인 후 lastBase→공통 door3→기지 회복·기존 save에 연결한다. 실패한 파일 저장은 기존 원자 저장/오류 경로를 재사용한다.
4. 원본 형식 시작 저장의 낮은 양수 HP와 실제 적 발사/피해를 사용한다. HP0·slotHP0→state2→원본 actor tick에서 state3→world3→알림 1회를 관측한다. 사망 결과를 직접 주입하지 않는다.
5. pause 중 crash 단계 보류, 반복 피격/중복 응답, lastBase가 Base001 이외인 경우, 기지 회복의 body bonus/BOT savedHP, 저장 후 재시작을 검사한다. 실제 RNG/풀/SFX 순서와 원본에 없는 저장 0회를 별도로 기록한다.

## 2. 완료 임무 창과 5007: 보상 지급 시점 보존

### 확인한 원본 규칙

- Command init(0x2614c)은 curState=-1/state=-1 뒤 state0을 요청한다. tick(0x26558)은 curState가 달라질 때만 상태를 소비한다. state0은 완료 임무 검사, state1은 임무 목록, state2는 별도 Hall of Fame다.
- refreshCompletedMission(0x269ac)은 이전 완료창을 제거하고 accepted 목록의 원래 순서로 첫 requireComplete && completePlace==현재 location(+0x2d)인 임무를 고른다. 없으면 state1만 설정한다. 목록 생성은 다음 상태 tick이다.
- completeMission(0x279a8)은 SFX11→창/제목/OK 준비→원본 completeActions 순서대로 보상 표시 및 실제 효과→MissionManager.completeMission 순서다. 보상은 창 생성 시 지급한다.
- type0은 Cr, type1은 EXP, type2는 아이템 추가, type3은 아이템 차감이다. 표시 셀은 0/1/2만 사용한다. type3은 표시 셀 번호를 늘리지 않는다.
- 아이템 추가 실패는 TakeItemFail 알림을 요청하고도 다음 action과 최종 임무 완료를 계속한다. 전체 보상을 취소하거나 자동 재시도하는 거래로 바꾸지 않는다.
- Pop5007은 refreshCompletedMission→SFX0이다. 다음 적격 임무가 있으면 그 SFX11이 OK의 SFX0보다 앞선다. 다른 버튼 phase에서 claim하지 않는다. 완료창/5007 자체에 saveDataFile 호출은 없다.

### 원본 GPU 표시 규칙

GameMain.setScreen_CommandCenter는 root를 (384,512)에 둔다. 완료창은 MissionCompletedBG.png 512×256, tile0, 명시 layer9다. 상위 Command 일반 layer와 popup layer를 혼동하지 않는다.

- 제목: 실제 missionName, TrebuchetMS-Bold 16pt, 256×20, align1, RGB(100,240,255), local(0,42).
- OK: Common_Btn_OK.png split1×2, tile0/release1, ID5007, local(62,-74), pushDepth/releaseHeight/releaseDelay=0.
- visible cell k의 기준: x=(k%2)*100-84, y=(k/2)*-52-10.
- Cr: Icon00001.png, 기준 위치, `%dCr`, 텍스트 x+60.
- EXP: Icon00010.png, 기준 위치, `%d`, 텍스트 x+50.
- 아이템: 원본 format은 정확히 `Icon%@.png`이며 strItem을 넣는다. 아이콘은 기준+(8,8), `%d` 수량 텍스트는 x+60. 임의 `Icon_` 접두사를 추가하지 않는다.
- 보상 텍스트: TrebuchetMS-Bold **13pt**, Cr 100×14 / EXP·아이템 60×14, align0, 흰색. 높이14와 font size13을 구분한다. 기존 decompiler의 CGFloat/CGSize 인자 재배치 표기만으로 font size14라 읽으면 틀린다.
- 위 수치는 확보 ASM와 원본의 선택한 float literal을 대조했다. 0x27f10=42, 0x27f88/0x281a8=13, 0x27eac=100, 0x27eb0/0x28198=60, 0x27ef0=20이다. 명령의 height14와 title16은 ASM 즉시값이다. 창에 상점 상세 펼침 애니메이션을 임의로 붙이지 않는다.

### 현재 소비 차이와 최소 구현

Screens.command는 CmdCenterBG+임무 목록+5009/Exit를 바로 만든다. 완료 popup/5007과 Command phase0 소비자가 없다. Session은 5007 Pop을 unsupported로 센다.

Progression.claimMission은 SFX11, action 순서, per-action missionReward, 실패 후 진행, 최종 finish를 이미 구현했다. 이 executor를 재사용하고 UI에서 보상 효과를 중복 구현하지 않는다. Session은 현재 missionReward를 받지 않는다. 팝업/title 준비를 먼저 하고, 기존 executor의 실제 순서에서 reward presentation을 수집하는 연결 경계가 적합하다. SFX11 중복 재생은 금지한다.

현재 public claimMission은 lastBase를 넘긴다. 원본 자동 완료 검사는 current location이다. Command가 유효한 기지에 있으면 두 값이 같을 수 있지만, 새 sweep은 실제 current location을 명시해서 검사한다. 무조건 같은 값이라고 가정하지 않는다.

1. Session에 원본 Command state/curState와 현재 완료창의 실제 mission identity·ordered rewards를 둔다. acceptedDefinitions의 원래 순서를 사용한다.
2. 실제 claim 결과를 받은 popup snapshot을 공통 PreparedText/TileReader/Button/VisualLayer로 그린다. 5007 키에 owner/mission identity를 보존한다. 버튼이 보상을 지급하는 별도 경로를 만들지 않는다.
3. 적격2개+미달+다른기지 임무 fixture에서 첫 tick 1개만 보상, 각 Pop5007에 다음 1개, 최종 다음 tick 목록을 검사한다. phase/중복 입력·화면 rebuild로 중복 지급되지 않아야 한다.
4. 실제 Cr/EXP/수량/finished state, 부분 아이템 실패 뒤 후속 보상/차감, EXP 레벨업 SFX7, SFX11/0 순서와 저장0을 검사한다. 원본 타이틀/아이콘/텍스트/UV/layer를 CPU와 Native GPU에서 따로 비교한다.

## 3. 점수와 GameCenter 5009: 플랫폼 결과 분리

### 확인한 원본 규칙

- Command Pop5009(0x25a2c): GlobalInfo.reportScore→DefaultAppDelegate.showLeaderboard→SFX0. 점수 제출이 생략돼도 showLeaderboard 호출은 남는다.
- reportScore(0x15bc0)는 원본 cryptid metadata gate true && working slot 존재 && signed32 EXP(+0x70)>0일 때만 실행한다. 점수는 best(+0x74)가 아닌 현재 EXP다. category는 정확히 HM2HD_LEADERBOARD다.
- GameCenterManager.reportScore(0x44978)는 GKScore 생성/category/value 지정 뒤 비동기 제출을 요청한다. 호출·요청·서버 수락·오류를 같은 성공으로 세지 않는다.
- showLeaderboard(0x2560)는 구형 GKLeaderboardViewController를 생성하고 nonnil이면 delegate와 기존 GL view controller에 animated 표시를 연결한다. 종료 delegate(0x2530)는 animated dismiss다.
- inspected 5009 경로는 Command state2를 요청하지 않는다. 별도 setHallofFame를 5009용 임의 로컬 랭킹으로 대체하지 않는다.
- SaveExit1031은 dialogs close→pause1→SFX0 뒤 isSave일 때만 **먼저 save**, signed best<signed EXP면 best 갱신→reportScore, 이어 changeScreen1이다. best를 바꾼 뒤 추가 save는 없다. 원본 순서를 유지한다.

### 현재 소비 차이와 미확인 범위

5009 버튼 그림은 있지만 Session Pop은 unsupported다. GameCenter host·Native 요청·Apple GameKit 어댑터/인증/entitlement 연결을 찾지 못했다. SaveExit에는 기존 save→best 갱신 순서가 있고 reportScore 자리가 미구현 계수로 남아 있다. 새 제출 때문에 저장 순서를 바꾸지 않는다.

후속 원본 조사에서 점수 gate는 인증 상태가 아닌 LC_ENCRYPTION_INFO.cryptid 조건임을 확인했다. OS 지원 검사와 notification 기반 인증 상태는 별도다. 인증/제출 completion은 즉시 반환하며 앱 자체 오류 UI·재시도·저장은 없다. [추가 조사 명세](gamecenter-reference-contract-20261110.md)를 따른다. 앱 등록/category 소유권·현재 OS API 가용성·실제 계정 인증·서버 수락 역시 미확인이다. 서비스 계약을 구현하기 전에 필요한 함수만 추가 REA하고 해당 OS 공식 SDK 근거를 확인한다. 이번 작업은 네트워크나 계정에 접속하지 않았다.

### 최소 구현과 검사

1. 플랫폼 경계를 score OS support/authentication/original metadata gate/report(category,signed score)/show/dismiss로 나눈다. core는 원본 조건·순서만 결정한다. 임무/HP/Cr/저장과 서비스 결과를 섞지 않는다.
2. 지원하지 않는 플랫폼은 unsupported 결과를 명시하고 게임 상태를 유지한다. 가짜 로컬 순위·허위 서버 성공·임의 오류 문구를 만들지 않는다. 원본 인증/에러 경로를 더 확인하기 전 정확한 실패 UI를 완료로 보고하지 않는다.
3. 가능한 Apple 어댑터는 현재 SDK에 맞는 실제 서비스를 사용하되 원본 category/EXP를 보존한다. 비동기 callback은 Session/app 수명을 검사한다. 인게임 UIKit 버튼을 추가하지 않고 원본의 OS 서비스 화면만 플랫폼에서 연다.
4. CPU host spy로 metadata gate false/slot 없음/EXP0·양수·signed음수, exact category/score, 5009의 report→show→SFX0, show 생략 조건의 부재를 검사한다. SaveExit isSave와 best 비교3종, save-before-best 및 추가 저장0을 검사한다.
5. 미지원/인증실패/제출오류/닫기/오래된 callback에서 게임 값이 변하지 않는지 검사한다. Native UI 표시와 실제 서버 수락은 별도 결과로 기록한다. 실제 사용자 계정 점수 업로드를 CPU fixture로 가장하지 않는다.

## 4. 작업 순서와 완료 기준

우선 GameOver 원본 crash phase와 안전한 알림 응답을 연결하고 실제 기지 복귀·저장을 검증한다. 그다음 완료 임무의 자동 sweep/popup을 기존 executor에 연결한다. 점수 플랫폼은 추가 인증/오류 근거를 좁혀 확보한 뒤 독립 경계부터 구현한다. 서비스 사용 가능 여부와 무관하게 5009 원본 호출 순서는 별도 검사할 수 있다.

각 기능은 기존 동시 전투 검사와 같은 실제 코어를 소비하는 독립 조건으로 추가한다. 순차 메뉴 클릭만으로 전체 게임 기능 완료라고 하지 않는다. 현재 진행 중인 Native112 진단·다른 작업자의 수정은 건드리지 않는다. 정상 앱·치트 앱·실기기·모든 플랫폼 완료는 구현과 실제 검증 후 각각 기록한다.

## 근거 파일

세부 파일/함수/현재 소스 해시는 `consumer-matrix.json`, `manifest.json`에 있다. 원본 분석 파일은 기존 로컬 evidence를 참조하며 바이너리/에셋 원본 덤프를 복사하지 않았다. `selected-popup-facts.json`은 이번에 확인한 선택된 수치/문자열뿐이다.
