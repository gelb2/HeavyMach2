# 원본 자원 준비 범위와 수명 조사

2026-11-14. 기존 HD 1.0.3 REA 함수 export와 현재 공통 C++ 소비 코드를 대조했다. 원본 실행파일 SHA256을 다시 읽어 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090` 일치를 확인했다. 앱·시뮬레이터·실기기는 실행하지 않았다. 아래는 조사 결과와 수정 제안이며, 수정·실행 통과 보고가 아니다.

## 확인한 원본 절차

| 자원 | 원본 소비 경계 | 확인한 범위와 수명 |
|---|---|---|
| 무기 bullet/effect/explode | `GlobalInfo.init 0x15ef4` → `openWeaponInfo 0x14bc4` | WeaponInfo.tble의 모든 행을 원래 순서로 읽으면서 `ResMgr_Tile.sharedMgr.getTile`을 호출한다. 전체 무기 그림 준비는 원본에도 있다. 전역 정보/공유 캐시 수명이다. |
| 무기 사격 효과음 | 같은 `openWeaponInfo 0x14bc4` | 각 행의 FireSound에 `.wav`를 붙여 `SoundManager.getSound`를 호출한다. getSound의 내부 캐시는 첫 요청에서 로드하지만, 이 첫 요청은 실제 사격보다 앞선 전역 정보 준비에서도 발생한다. |
| 전역 효과음 22종 | `GlobalInfo.init 0x15ef4` 후반 | 무기 정보 준비 뒤 고정 SFX를 추가로 요청한다. ID 요청 순서는 `0,1,2,3,4,5,6,10,11,12,13,7,8,14,9,15,16,17,18,19,20,21`이다. 파일명 대응은 공통 `SoundIDs.hpp`에 있다. |
| 부품 정보 전체 | `GlobalInfo.readPartsChunk 0x165f8` → `PartsInfo.setPartsName 0x19f64` | 부품 타입·타일 수·소켓·이름과 `.png` 파일명을 저장한다. 이 두 함수는 전체 부품 그림을 읽지 않는다. |
| 실제 부착 부품과 그림자 | `GobMachParts.setPartsInfo 0x1c36c` | PartsManager의 부품 타일을 조회한다. 없으면 해당 부품의 이름으로 World의 tileResMgr에서 컬러와 alpha 타일을 요청한다. 타일 수 0은 PartsDummy 경로다. |
| PartsManager의 목록 준비 | `addMach 0x35e78` → `decodeChunk 0x35d4c` → `loadPartsTiles 0x36414` | 선택 모델의 chunk를 읽어 body/other 목록을 만든다. loadPartsTiles는 이 목록을 순회한다. 전체 PartsInfo 카탈로그를 순회하는 함수로 해석하면 안 된다. 이 경로가 모든 모델에 대해 호출된다는 근거는 이번 범위에서 확인하지 않았다. |
| 맵의 적 생성 지점 | `readSpawner 0xff18` → `addEnemySpawner 0x10a00` → `initWithMachName 0x7cfc` | 모델명·위치·공격 여부·생성 시각을 저장한다. 이 단계에서 모델의 부품 그림을 읽는 호출은 없다. 실제 생성은 spawner tick의 addEnemyMach와 makeMach/setPartsInfo로 이어진다. |
| 아이템 정보 전체 | `openItemInfo 0x17108` → `ItemInfo.setIcon 0x18dfc` | 아이콘 번호를 정수로 저장한다. 전체 아이콘 그림 준비는 확인되지 않는다. |
| 필드에 생긴 아이템 | `GobFieldItem.initWithItem 0x81dc` | 해당 아이템의 `Icon%05d.png`와 필드 효과를 sharedMgr에서 요청한다. 아이템 생성 시에 필요한 파일을 준비한다. |
| 파일명 캐시 | `ResMgr_Tile.getTile 0x3f56c` | dictionary hit는 기존 Tile2D를 반환한다. miss는 파일을 찾고 이미지를 읽어 Tile2D를 만든 뒤 dictionary에 저장한다. 매 draw에서 다시 파일을 읽는 구조로 해석하지 않는다. |

`GlobalInfo.init`의 관련 순서는 MapInfo → PartsInfo → MachInfo → ItemInfo → WeaponInfo → Description → Inventory → ItemSpawner → MissionList → 고정 SFX다. 모든 그림을 실제 사용 순간에만 처음 읽는다고 설명해도 원본과 달라진다.

무기 행의 준비 입력은 `BltTile` → `Blt_<값>.png`, 인식된 EffType의 `EffTile` → `<값>.png`, `ExplodeEff` → `Efx_<값>.png`, `FireSound` → `<값>.wav`다. 비어 있는 원본 표식은 기존 Weapons reader와 동일하게 제외한다. Rail 효과는 가로 4칸, 다른 확인된 효과는 1칸이다. `Weapons::all()`은 키 정렬 map이므로 원래 행 순서가 필요하면 `Weapons::raw()`의 행을 사용한다.

## 현행 코드와 다른 점

조사 시점의 `CombatScene` 생성자는 모든 무기 그림, 모든 Models 부품/그림자, 맵 spawner의 모델 부품을 먼저 준비한다. `GameSession::prepareTiles`는 모든 아이템 아이콘을 World pool에 준비한다.

전체 무기 그림 준비에는 원본 근거가 있다. 다만 원본은 전역 shared 캐시이며 현재 준비 위치는 새 World마다 생성되는 pool이다. 전체 부품/아이템 그림을 World 생성 때 한꺼번에 읽는 현행 범위는 위에서 확인한 원본 부착·생성 경계보다 넓다. 이것이 사용자 발열의 실제 원인이라고 확정하지 않았다.

## 공통 코드 수정에 필요한 경계

1. 무기 그림의 준비 목록과 사격 효과음 요청 목록을 전역 정보 소유에 연결한다. 정상판과 동시 치트판이 같은 목록을 쓴다. World가 종료돼도 이 전역 소유 목록을 GPU 보존 목록에 포함한다. 전역 효과음을 디렉터리 전체 57 WAV와 혼동하지 않는다.
2. 부품 준비는 초기 Player 부착, 실제 적 생성, BOT 생성, 장비 교체의 새 mount 검증 단계에서 공통 prepare 경로로 수행한다. 전체 카탈로그 루프만 지우면 현행 addEnemy/BOT/equipWeapon의 `lookup`이 실패한다. 이 소비 경계를 먼저 연결한다.
3. 필드 아이템은 실제 addFieldItem에서 해당 아이콘과 필드 효과를 준비한 뒤 bounds를 만든다. 임무 배치 아이템과 적 드롭도 같은 경로를 쓴다.
4. 새 파일 cache miss는 자원 준비 세대를 갱신한다. 기존 Runtime 준비 단계가 draw 전에 새 GPU 자원을 반영한다. 준비된 항목은 동일 World 소유가 끝날 때까지 재사용한다. draw/collect/매 사격에 파일 읽기나 디코딩을 넣지 않는다.
5. 변경 뒤에는 World 교체의 전역 자원 재사용, 처음 등장하는 적·BOT, 처음 착용하는 무기, 처음 떨어진 아이템, 재요청 시 추가 읽기 없음, World 해제 뒤 전역 자원 유지와 World 자원 해제를 검사한다.

## 스포너 초기 시각 확인

원본 `GobEnemySpawner.initWithMachName 0x7cfc`는 checkTime을 `-1.0`으로 초기화한다. 현행 `EnemySpawner.hpp`도 HEAD부터 `checkTime=-1`이다. 초기값 불일치라는 앞선 추정은 잘못됐으며 수정 대상이 아니다. `tests/SpawnerTests.cpp`에는 첫 GameClock tick의 생성, time 0의 미생성, 정확한 0.5 경계와 그다음 시각의 생성 검사가 이미 있다. 이 조사 때문에 소스를 되돌리거나 가짜 변경 전 실패를 만들지 않았다.

원본 `GobWorld.tick 0x110d8`은 pause 시 gameTime 증가/processMyMach를 건너뛰지만 processCamera와 QobBase 자식 tick은 계속한다. 스포너는 pause 값을 직접 보지 않고 고정된 World 시각과 checkTime/spawnTime을 비교한다. 새 맵을 만들고 처음 자식 tick을 실행하는 시점의 RNG 사용을 곧바로 전투 진행으로 판정하면 안 된다. 초기 적 생성 수·RNG 변화·공격/피해·월드 시각을 함께 측정해야 한다.

## 이번 수정 후보에 연결한 코드와 검사

조사 뒤 공통 `SharedAssetPreparation`에 원본 WeaponInfo 행 순서의 그림 준비와 FireSound/22 SFX 요청 목록을 구현했다. `PreparedTiles`에는 전역 타일을 읽는 fallback과 준비 세대를 추가했다. World의 prepare는 항상 자기 pool에 쓰므로 공유 타일의 split/origin을 덮어쓰지 않는다. fallback 조회는 파일을 읽지 않는다.

`CombatScene`은 Session의 공유 pool을 전달받는다. 독립 Scene 검사는 자체 공유 pool을 소유한다. 전카탈로그·전체 spawner 모델의 부품 준비를 제거하고 실제 적/BOT 생성과 Player 장비 부착에 공통 prepare를 연결했다. `GameSession`의 필드 아이템 생성은 해당 아이콘과 효과만 공유 pool에 준비한다. 새 자원 세대는 플랫폼 준비 단계가 draw 전에 반영하도록 노출한다.

추가·수정한 검사는 아직 이 조사 담당자가 실행하지 않았다. 실행은 통합 담당자가 한 번에 수행한다.

- `PreparedTileTests`: 전역 fallback 조회의 추가 읽기 없음, World split/origin과 전역 타일 분리, 공유 항목 추가 시 준비 세대 변화.
- `SharedAssetPreparationTests`: 키 정렬과 다른 원본 행 순서, 중복 그림 캐시, Rail/다른 효과 split, FireSound 뒤 고정 22 SFX 순서.
- `CombatSceneTests`: 적 최초 생성의 합법적인 준비와 이후 반복 IO 0을 분리한다. 미부착 무기는 준비 목록에 없어야 한다. 처음 착용한 `10012`의 PNG는 한 번 읽고 alpha는 같은 준비 경로로 만든다. 재착용/그리기는 추가 읽기가 없어야 한다. 서로 다른 두 World가 같은 전역 bullet 타일을 유지한다. 기존 공격·피해·RNG·사망 기준은 유지한다.
- `PartShadowTests`: 별도 전체 337종 alpha 픽셀 검사는 유지한다. Scene은 최초 Player/BOT에 실제 부착된 그림자 집합만 준비해야 하며, 실제 생성된 적 부품이 이후 집합에 들어와야 한다. 각 draw의 파일 읽기 0을 별도로 검사한다.

통합 검사 `core-scoped-tests.log`에서 수리 아이콘 `Icon01001.png`/`Icon01006.png`의 미준비 예외가 나왔다. 전카탈로그 예열을 제거하면서 수리의 실제 생성 경계도 연결해야 했으나 빠져 있었다. 원본 `GobHvMach.useRepairItem 0xc7e4`는 pool에서 particle을 얻은 뒤 해당 아이템 아이콘을 sharedMgr.getTile로 요청한다. 이 순서를 따라 QuickItemsHost/Scene의 명시적 `prepareRepairEffect`를 연결했다. pool이 가득 차면 아이콘 준비도 생략하며 HP·수량·사운드·RNG 처리 순서는 바꾸지 않았다. `BotSceneTests`에는 미준비 상태의 실제 BOT 수리에서 해당 아이콘을 1회 준비하고, 후속 Player 수리에서 같은 타일/세대를 재사용하는 검사를 추가했다. 이 후속 수정의 실행은 아직 하지 않았다.

그다음 `core-accounted-tests.log`에서는 EnemyActor/BOT 장기전과 AidKit의 옛 전구간 IO 0 assertion이 실패했다. 이를 단순한 예열로 통과시키지 않도록 `tests/ResourceBoundaryLedger.hpp`를 추가했다. 테스트 ResourceAccess가 기록한 read/exists의 이름을 실제 준비 목록과 비교한다. 처음 요청한 PNG는 이전 준비 목록에 없어야 하고 새 목록에 들어가야 하며 실제 resourceRevision도 바뀌어야 한다. 같은 action 안의 중복 읽기와 준비 목록 밖의 읽기는 실패한다. epoch가 바뀌지 않은 tick/input과 모든 draw는 추가 읽기/exists 0을 요구한다. 준비 경계의 요청 총량·파일명, 반복 action 수, draw 수를 따로 출력한다.

EnemyActor 38개 필드의 첫 생성과 장기전, BOT의 장기전·같은 pass/게이지·사망 제거, Session의 600프레임 전투와 AidKit 6종에 이 관측을 연결했다. AidKit은 해당 아이콘의 처음 준비 1회와 실제 재수리의 추가 준비 0을 따로 검사한다. 공격·HP·XP·수량·RNG·사망·저장 조건은 유지한다. 이 검사 보강은 아직 실행하지 않았으며 이전 실패 로그도 보존한다.

기존 전카탈로그 예열을 전제로 한 초기부터 전체 구간 IO 0 검사는 원본 최초 부착 준비와 충돌한다. 최초 생성/교체 경계에서 필요한 읽기는 측정하고, 이미 준비한 파일의 반복 요청·draw·사격에서 추가 읽기가 없는지를 검사한다. 게임 결과 실패 기준을 낮추는 변경과 혼용하지 않는다. 기존 통합 실패 로그는 보존하며 과거 실패를 지우기 위해 수정 전 소스로 되돌려 실행하지 않는다.

## 도움말·맵 전환의 자원 소유 보강

실제 동시 검사에서 frame182/case111의 메뉴 도움말 생성은 KR 페이지3개와 HelpHelp를 새 GuiHelp 전용 manager에 준비했다. 파일명 바이트 캐시에 있어도 논리적 read/exists는 각각4회였다. GameSession의 자원 변경 신호가 World와 전역 그림만 관찰해 이 새 소유자를 놓쳤다. 원본의 도움말 생성을 없애지 않고 고유 manager ID의 생성/해제를 자원 세대에 포함했다. 포인터 주소 재사용도 구분한다. 동시 검사에는 해당4개 요청이 실제 소유자 변경과 함께 기록되는지 검사를 추가했다.

원본 `GuiMapLoading.init0x2da14`는 `ResMgr_Tile.sharedMgr.getTile`을 사용한다. `dealloc0x2d9cc`는 링크와 이미지 노드를 해제하며 공유 그림 캐시를 비우지 않는다. MapLoading의 StageClearBG를 GameSession의 공통 전역 manager에서 최초 준비·재사용하도록 연결했다. `MapLoadingTests`에서 첫 전환 read/exists1회, 노드 소멸 후 타일 유지, 다음 전환 추가 read/exists0을 확인했다. 배경 픽셀·전환 순서·fade90+90프레임은 유지했다.

위 변경 뒤 `OriginalMapLoading`, `OriginalHelpSession`, `LifecyclePauseAndMenu`3개 코어 검사를 통과했다. 근거는 로컬 `iphone-fixes-20261114/owner-epochs-tests.log`다. 전체 동시 검사와 Native GPU·실제 기기·Instruments 결과는 아직 아니다.

## 고정 VFX·이동 마커의 소유와 최초 요청

마지막 정적 점검에서 고정 VFX의 World-local 선로딩도 원본과 달랐다. `GobWorld.init0x11208`은 고정 8종을 sharedMgr에 준비한다. World 종료 후에도 이를 전역 목록에 남기도록 수정했다. 나머지 고정 파일은 아래의 원본 소비 지점에 연결했다. 이 배치의 소스·검사 작성은 끝났으며 아직 새 배치를 빌드·실행하지 않았다.

| 파일 | 원본 manager·최초 요청 | 이번 소비 경계 |
|---|---|---|
| 고정 8종: Explode02/Ground/Boss/NPC, Fire, Damage, Smoke, RailgunEx | `World.init0x11208`의 sharedMgr | 최초 World 초기화 때 공유 준비. 다음 World는 같은 타일/그림을 재사용한다. |
| `Target.png` 일반 표적 | `GobHvMach.init0xcd8c`의 World.tileResMgr, 2×1 | World의 타일을 유지한다. 공습의 공유 표적과 합치지 않는다. |
| `Target.png` 공습 표적 | `AirStrike.setAttackPos0x58b4`의 sharedMgr, particle 성공 뒤 2×1 | 공습 marker의 실제 particle 할당 성공 뒤 공유 준비한다. |
| `MovePath.png`, `DestPos.png`, `TargetPath.png` | `World.refreshWorldObject0x10174`의 `_tileResMgr` | World 소유를 유지한다. 12개 line/marker는 3개 파일을 같은 World manager에서 재사용한다. header의 잘못된 매개변수명 sharedCache만 worldCache로 고쳤다. |
| `PickItem.png`, `HPGauge.png` | `Player.init0xbcdc` sharedMgr. Gauge는 screen7에서 1×2. `Bot.init0x8fd4`도 공유 Gauge | Player/BOT 생성 때 공유 준비한다. |
| `Airplane.png` | `AirStrike.initWithItem0x5758` sharedMgr | 실제 공습 생성 때 준비한다. |
| `Efx_Dash.png` | `processMove0xd6d8`, particle 성공 뒤 sharedMgr, 1×1 | 실제 Dash particle을 얻은 뒤 준비한다. |
| `Efx_LevelUp.png` | `levelUp0xb1f8`, particle 성공 뒤 sharedMgr, 1×1 | 실제 레벨업 particle을 얻은 뒤 준비한다. |
| `Efx_Nuclear.png` | `GobBullet.setCollide0x71d0`, 핵무기 분기의 particle 성공 뒤 sharedMgr | 실제 핵 충돌 효과 생성 때 준비한다. |
| `BulletShell.png`, `Efx_Trail04.png`, `Efx_Trail02.png` | `Weapon.tick0x1cf38`의 Gun/Sniper 효과 생성 분기 sharedMgr. Shell은 2×1 | 해당 실제 효과 생성 순서에서 명시적으로 공유 요청한다. cache miss 때만 읽고 후속 요청은 재사용한다. |

사용자의 사격별 DB/파일 저장 금지와 원본의 최초 효과 그림 읽기를 혼동하지 않는다. 기존 `emit` 주석의 일괄적인 파일 IO 금지를 고쳤다. 이번 변경은 원본이 처음 요청하는 그림을 앞서 전부 예열하는 방식이 아니다. 효과를 그리는 draw/collect에서는 여전히 파일을 읽지 않는다. GPU 반영은 준비 세대 변화로 draw 전에 처리한다. HP·RNG·시각·particle pool 순서는 바꾸지 않았다. 준비된 테스트용 `WeaponEffectResources` 포인터와 원본 nil 동작도 유지한다.

`WeaponEffectsTests`에는 pool 여유에 따른 요청 유무·Gun/Sniper 요청 순서, 첫 공유 준비와 후속 요청의 추가 읽기 0을 추가했다. `CombatSceneTests`에는 실제 공습 평면/marker의 처음 2개 공유 요청, 두 Target manager의 다른 타일 주소, 후속 공습의 추가 준비 0을 추가했다. 원본 두 manager가 같은 `Target.png`를 따로 소유하므로 파일명만 합친 이전 ledger로는 정상 첫 공유 요청도 중복 읽기로 오판한다. 공통 ledger에 전역 준비 목록을 함께 대조하도록 보강했다. 새 shared 소유·실제 자원 세대가 함께 생긴 경우만 인정하며 같은 shared 소유자의 재읽기는 계속 실패한다.

## 원본 근거 위치와 한계

전체 함수 원문은 저장소 밖의 `/Users/sokol/Documents/Codex/HeavyMach2/Analysis/HD103/method-evidence/functions/`에 보존돼 있다. 위 주소의 `.pseudo.c`와 필요한 `.asm`을 사용한다. 이 경로의 `open.json`도 위 실행파일 SHA를 기록한다. 원본 IPA·실행파일·이미지·전체 디컴파일 본문을 이 문서에 복사하거나 외부로 전송하지 않았다.

기존 REA export를 재사용한 정적 조사다. 확인한 함수 체인 밖의 모든 간접 호출을 완전하게 배제했다는 주장은 하지 않는다. 원본 최초 로드 시간과 실제 발열·physical footprint는 마지막 Native/동시 치트/시뮬레이터/Instruments 실행에서 별도로 측정한다.
