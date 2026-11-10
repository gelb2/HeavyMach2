# 전장 진입·포탑·항공기·대시 복원

차량만 허용하던 CombatScene을 공통 EnemyActor에 연결했다. 포탑이 포함돼 진입하지 못하던37개 전장을 추가로 열었다. 기존68조건에 이37개 전장과 대시1조건을 더한106개 Session을 같은 프레임에서 갱신하고 그린다. 정상 Source 앱의 시작은 아직 보행이며 전체 게임 완성을 뜻하지 않는다.

## 원본 근거와 연결

HD 실행파일 SHA256은 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`이다. REA/Ghidra의 디컴파일·명령어와 원본 MachList·MapInfo·PartsInfo·WeaponInfo를 대조해 새 C++ 구현을 작성했다. 원본 소스코드를 확보했다는 뜻이 아니다. 원본 IPA와 ARM 실행파일은 로컬에 유지하며 Git·보고 사이트에 올리지 않는다.

- EnemyActor는 기존 EnemyVehicle을 재사용한다. 포탑·항공기도 같은 데이터 reader·부품 조립·무기·탄환·피해·드롭·EXP·사체 삭제 경로로 연결한다. 항공기는 지상 몸체 충돌 후보에서 제외하지만 탄환 공격 대상에는 포함한다. 피해 후 항공기의 moveToTarget=false와 차량/포탑의 true를 구분한다. 보스 표시는 원본 데이터를 보존한다.
- Dash 입력은 원본 World.onTap_EasyMove `0x12698`을 따른다. 적 선택·Player 캡처·퀵 아이템 처리 뒤의 유효한 지면 입력에서만 두 탭의 간격<0.5초와 실제 SP≥100을 검사한다. SP100을 소비하고 SFX14를 재생한다. 이동 포인터만 지우고 기존 경로는 유지한다. 작은 owned drag도 탭 시각을 지운다.
- Dash 이동은 setDashVel `0xc508`, processMove `0xd6d8`에 근거한다. 기본 보행→대시 속도×10→기존 지형 충돌 순서를 사용한다. 속도는 축별 Div8·Min0.01로 줄어든다. 무적·피해 감소·새 쿨다운을 만들지 않는다.
- 효과는 원본64×64 `Efx_Dash.png`와 기존 공통 particle pool을 사용한다. 이전 효과 위치와 거리 제곱>800일 때 기존 RNG/입자 할당을 거쳐 생성한다. 별도 그림·유닛별 이미지 조립·새 오디오 context를 추가하지 않았다.
- 원본 PtBtm23행은 같은 Player의 P_Foot_01…23과 두 발 소켓을 사용한다. 서로 다른 하체 controller23개가 있다는 근거는 없다. 남은 과제는23개 장비의 실제 이동·애니메이션·GPU·포팅판 대조다.

## 실행 결과

| 검사 | 확인한 결과 |
| --- | --- |
| C++ 전체 | 51개 통과. 대시·Scene·Session·기존 무기/저장/화면 회귀 포함 |
| 전체 Core ASan/UBSan | 106조건·1,200프레임 통과. 메모리 누수 검사는 포함하지 않음 |
| iPad 시뮬레이터 | 106조건 각각1,200프레임. Player 무기110개·5분류 실제 발사 |
| iPhone 시뮬레이터 | 같은106조건 각각1,200프레임. Player 무기110개·5분류 실제 발사 |
| 38개 field 최초 생성 | 차량385·포탑307·항공기3=695대. 나머지5개 Base에는 스포너가 없음 |
| 대표 전투 | Stage01_02 포탑, Stage02_07·Stage03_04 항공기의 실제 발사·피격·격파·삭제를 양 시뮬레이터에서 확인 |
| 대시 | 실제 지면 두 탭, SP109→9, SFX14×1, 이동36tick, 공통 입자4개·RNG4회, GPU Efx_Dash quad 확인 |
| 지형 경계 | 별도 Session에서 실제 원본 edge에 접근해 대시 중 벽 보정28회. 매 tick의 최종 위치를 기본 보행→대시→Terrain 결과와 대조 |
| 동시 음향 | 각 시뮬레이터에서 샘플 진행 중인 효과음 최대25개. context1개·기존 source/buffer57개 재사용 |
| 실행 중 준비 비용 | 준비 후 resource read/exists·PNG 업로드0. 효과음 context/source/buffer·파일 읽기·디코드 추가0 |
| GPU 반복 출력 | 같은 캐시 프레임을 기존 ordered/scissor 경로로 다시 그려 RGB 차이0 |
| PNG 전달 | 기기별4장, 총8장의 PNG decode RGB가 원시 framebuffer RGB와 모든 픽셀 일치 |

695는 최초 스포너 pass의 실제 생성 수다. 후속 생성에는 정상 respawn이 포함될 수 있다.38개 전장 진입·실행과 모든 맵 이동·모든 임무·최종 캠페인 완료는 구분한다. C++에서 측정한 EXP 조건 충족33개도 서로 다른 임무33종 완료를 뜻하지 않는다.

기존 구급상자6종·공습3종·상점·임무·저장 재시작 조건은 유지했다. 기존 메뉴11프레임은 모두 기존55전투의 활동과 겹친다. 새 전장의 회복·공습까지 합치면 버튼 조작은15프레임이며 기존55전투와의 겹침은14프레임이다. 나머지1프레임까지 겹쳤다고 보고하지 않는다.

이동/획득 조건58은 대시와 분리해 보행을 검사한다. 입력을 frame1·32·63…으로 보내 간격31tick(0.5146초)을 유지하고 dashStarts=0을 확인했다. 실제 코인3개·9Cr와 아이템3개를 획득했다. 이전68조건의 아이템2개 결과와 섞지 않는다. 새 대시는 독립 조건105에서 실제로 실행한다.

iPad CPU+제출 p95는62.55ms, iPhone은53.80ms였다. 같은 Mac의 시뮬레이터·개발 환경에서 측정한 값이며 실기기60fps 합격 기준이 아니다. 공통 HD canvas와 검증용 분할 화면은 최종 iPhone 가로 HUD·화면 비례가 아니다.

## 발견한 검사 문제

첫106검사는 신규 worldQuick 버튼을 기존 메뉴11프레임에 더해 합계가 달라 실패했다. 기존68조건의11/11 겹침은 별도로 유지해 검사한다. 실패 로그는 보존했다. 대시 벽 표본의 첫 시도는 벽이 없는 gate 통로여서 보정0으로 실패했다. 원본 TerrainEdge에서 실제 벽을 고르는 유효 표본으로 바꿨고 보정>0 기준은 유지했다.

기존 PNG export는 불투명 CAEAGLLayer의 framebuffer를 premultiplied alpha로 해석했다. 화면 자체가 아니라 보고 이미지의 가장자리 색이 달라질 수 있었다. 위아래 방향만 뒤집고 RGB를 그대로 보존하는 opaque PNG로 고쳤다. 원시RGBA도 XCTest 첨부에 남겼다. 이전 캡처와 실패 증거는 보존한다.

## 다음 작업

1. 정상 Source 시작을 실제 GameSession 메뉴·전투에 연결한다. 현재 보행 시작점과 자동106조건 Validation을 혼동하지 않는다.
2. bot의 전장 AI·공격·획득을 원본 소비 절차에 연결하고 독립 상태와 복합 전투에 함께 추가한다.
3.23개 하체 장비의 실제 보행·부품 접합·GPU·포팅판 대응을 검사한다. 별도 controller를 추정해 만들지 않는다.
4. 모든 임무 종류·60개 임무의 수락/조건/보상, 기지와 전장 이동·게임오버·도움말·isSave 조건을 연결하고 기능/버튼 대응 목록을 채운다.
5. 일반판 iPhone·HD판 iPad 포팅판과 표시·입력·공격·음향을 같은 조건으로 대조한다. iPhone 가로·iPad 세로를 먼저 검증한다. 그 뒤 추가 방향·플랫폼으로 진행한다.

전체 게임·전체 치트 앱·원작 동등성·실기기·develop/main 병합은 아직 완료하지 않았다.

근거: `.local/evidence/combat-runtime-20261110/`의 `core/full51-final-ctest.log`, `core/concurrent106-final.log`, `dash/integration-verification.json`, `concurrent106-summary.json`, `capture106-rgb-verification.json`, `enemy-actors/scene-integration/`, `review/dash-integration-review.md`, `review/concurrent-106-review.md`. 원본·개인 저장·실패 로그는 보존한다.
