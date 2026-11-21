# 기능별 완료 근거

[정본](canonical.md) · [치트 검증 순서](validation-build.md) · [미해결 항목](todo.md)

코어 검사는 알고리즘, 실제 앱 검사는 호스트와 연결된 동작을 증명한다. 각 행에서 관측한 ID·기기·조건을 확인하고 빈 근거는 후속 항목으로 남긴다.

| 기능과 원본 진입점 | 현재 소비자와 검사 | 실행 관측과 남은 대조 |
| --- | --- | --- |
| 타이틀·슬롯·기지·포털. GuiSelectSlot·GuiBase와 Base001~006 | [Screens](../Sources/Core/UI/Screens.cpp), [WorldGates](../Sources/Core/Simulation/WorldGates.cpp), [버튼·HUD 검사](../tests/ScreensTests.cpp) | [정식 두 기기 설치·iPhone 사용자 진입·시뮬레이터 포털 검사](white-mountain-dual-release-20261120.md). 실제 iPad 출입과 방향·시설 전수 대조는 TODO. |
| 이동 경로·대시·카메라. 원본 지면 탭과 SP100 | [GameSession](../Sources/Core/Session/GameSession.cpp), [대시](../Sources/Core/Simulation/Dash.cpp), [카메라 검사](../tests/CameraPresentationTests.cpp) | [2,100프레임·OS 입력](iphone-module-audit-20261118.md). 진행 중 실기기 장시간 프레임 비교는 남아 있다. |
| 사격·피해·VFX와 음원. WeaponInfo 213행, PtWpn 110종 | [WeaponController](../Sources/Core/Simulation/WeaponController.cpp), [CombatScene](../Sources/Core/Simulation/CombatScene.cpp), [데이터와 포구](weapon-data-restoration-20261110.md) | [동시 검증](iphone-module-audit-20261118.md), [설원 무장](white-mountain-implementation-20261119.md). 모든 하체·공격 분기와 원작 동등성은 미확인. |
| 상점·정비·인벤토리. 구매/판매 2102/2103과 75칸 저장 | [Screens](../Sources/Core/UI/Screens.cpp), [Equipment](../Sources/Core/Progression/Equipment.cpp), [장비 검사](../tests/EquipmentTests.cpp) | [iPhone 원본 좌표·패널](iphone-shop-reference-coordinates-20261119.md) · [갤럭시 구매](../Android/검증-20261120.md) · [마지막 페이지](iphone-inventory-last-page-20261120.md). |
| 퀵아이템·공습과 BOT. 특수공격 02006 포함 | [QuickItems](../Sources/Core/Progression/QuickItems.cpp), [Bot](../Sources/Core/Simulation/Bot.cpp), [공습 검사](../tests/AirStrikeTests.cpp) | [실제 피해·레이저 음향·보정](campaign-and-ipad-followup-20261120.md). 전체 BOT 입수·복귀·장시간 환경은 남아 있다. |
| 사령부·임무·보상. 수락 5004, 완료 5007 | [Progression](../Sources/Core/Progression/Progression.cpp), [완료 임무 검사](../tests/CompletedMissionSessionTests.cpp) | [완료 흐름](completed-missions-restoration-20261111.md)과 [화이트마운틴 의뢰](white-mountain-implementation-20261119.md). E-10 알림과 수집 항목은 TODO. |
| 도움말·게임오버·중단 | [Help](../Sources/Core/UI/Help.cpp), [수명 검사](../tests/LifecycleSessionTests.cpp), [게임오버 검사](../tests/GameOverSessionTests.cpp) | [게임오버 실행](gameover-restoration-20261111.md). 기기의 실제 음향 중단·복귀와 성능은 별도. |
| 저장. 28,204바이트, 3개 슬롯, 복구코드 HM2-1 | [SaveData](../Sources/Core/Data/SaveData.cpp), [RecoveryCode](../Sources/Core/Data/RecoveryCode.cpp), [저장 검사](../tests/SaveTests.cpp) | [저장 이동의 플랫폼별 근거](save-transfer.md). |

개별 ID는 [원본 테이블 인벤토리](evidence/full-validation-inventory-20261110.json)의 아이템·무기·임무·맵·기체·기지 버튼과 [Screens::coverage](../Sources/Core/UI/Screens.cpp)의 화면·콜백 명세에서 확인한다. 원본 함수별 흐름과 검사 공백은 [초기 검증 목록](full-validation-development-plan-20261110.md)과 [원본 함수 검사](hd-original-function-audit-20261111.md)에 있다. 추가한 설원의 [맵·포털](../Resources/Campaign/white-mountain.json), [적·의뢰](../Resources/Campaign/encounters.json), [무장·BOT](../Resources/Campaign/items.json), [실행 기록](campaign-and-ipad-followup-20261120.md)을 함께 대조한다. 기존 결과는 후보 빌드·기기·입력이 같을 때 재사용한다.
