# 퀵 아이템 복원 진행

2026-11-10. 구급상자6종과 실제 공습3종을 기존59조건에 더해68조건을 함께 실행했다. C++48개와 두 시뮬레이터 각각1,200프레임을 통과했다. 일반 빌드와 전체 코어 ASan/UBSan도 통과했다. 전체 게임 복원 완료는 아니다.

## 원본에서 확인한 절차

HD1.0.3 실행파일 SHA256 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`을 REA로 분석했다. 버튼6200..6204 Push는 아이템을 선택한다. 실제 소비는 그다음 월드 begin 입력에서 발생한다. 구급상자는 Player를 먼저 검사하고, 아니면 실제 BOT 목록에서 거리제곱900 미만의 가장 가까운 대상을 고른다. 등록된 BOT 코드만으로 수리 대상을 만들지 않는다.

대상이 있으면 공통 particle pool 할당·원본 아이콘 효과→실제 actor HP와 원본 슬롯 메모리·게이지→수량1 감소→퀵 슬롯 갱신→SFX13→선택·메시지 해제 순서다. HP가 가득 차도 원본은 소비한다. pool이 가득 찬 때는 효과만 생략하며 HP·수량 처리는 계속한다. 이 경로에 새로운 파일·DB 저장이나 cooldown을 만들지 않는다.

실제 AidKit01001..01006의 수리량은 각각120·240·500·최대HP의30%·50%·100%다. 자연 회복은 actorHP만 올릴 수 있으므로, 수리 직전의 실제 actorHP를 기준으로 계산하고 수리 뒤 슬롯HP가 같은 값인지 따로 확인한다.

독립 검토에서 float 좌표 덧셈 순서 차이로 정확히30 거리의 입력이 소비되는 오류를 재현했다. 원본은 `((cameraCenter-halfViewport)+cameraOffset)+screen` 순서를 사용한다. QuickItems 입력에서 해당 순서를 보존하고 strict900 경계 회귀를 추가했다. 다른59조건의 공통 카메라는 이 수정 때문에 바꾸지 않는다.

## 실제 연결과 동시 검사

`QuickItems` 독립 모듈의 원본 데이터·HP·수량·공통 particle·RNG·callback·거리 경계 검사를 수행했다. Session/Scene의 실제 HP·공통 pool·게이지·수량·음향에 연결하고 실제 앱에서 확인했다.

기존59조건의 seed·장비·입력은 유지한다. 구급상자6종을 각각 독립 상태로 추가하고, 공습3종을 더해68조건을 함께 실행했다. 새6조건만 원본 형식 시작 저장에 양수의 낮은HP·해당 구급상자5개·퀵 등록을 준비한다. `selectSlot→enterField`로 들어가며 기지 방문이 HP를 채우는 원본 규칙은 바꾸지 않는다.

240프레임에 공통 GPU 퀵 버튼을 누르고241프레임에 실제 Player 위치로 월드 입력을 보낸다. 실제 actorHP·슬롯HP·5→4 수량·선택 해제·SFX13·공통 pool 할당과 다른 전투의 동시 활동을 관측했다. 결과 HP나 소비 성공을 실행기가 쓰지 않는다. 원본 Player의 HPGauge 두quad와 실제 선택 메시지도 같은 GPU 출력에서 확인했다.

## 실제 공습 소비자

SpAtk02004·02005·02007은 GobAirStrike actor와 실제 공통 GobBullet 생성 경로가 필요하다. 단순 요청이나 수량 감소를 공격 성공으로 보고하지 않는다. 시작 지연은 원본 데이터의1.5·1.0·2.0초이며 공격 RNG·속도·가속·4초 종료 분기를 보존했다. PartsController의 muzzle flash 경로를 재사용하면 원본에 없는 효과·난수가 추가되므로 공통 탄환 생성 소비자를 분리해 controller와 공습이 공유한다.

## 실행 결과

같은 frame clock에서55개 장비 전투·상점·임무·저장 재시작·자연 이동/획득·구급상자6종·공습3종을 계속 갱신하고 모든 월드와 UI를 그렸다. Player 장비110개가 발사됐고 메뉴/아이템 조작11프레임 모두 다른 전투 활동과 겹쳤다.

수리 직전 실제 actorHP456에서 각 원본 수리량을 적용해576·696·956·988·1344·1776을 관측했다. 최대HP는1776이다. 여섯 조건 모두 슬롯HP가 결과와 같고 수량5→4·공통 입자1·SFX13 한 번·선택 해제를 확인했다.

공습 조건은 Player 무기를 발사하지 않는다. Bomb10발·Missile10발·Nuclear1발과 공습 탄환 출처에서 실제 적HP 감소를 확인했다. 이번 동시 조건에서는 각200HP가 줄었다. 별도 Session fixture에서는 Nuclear가 두 적의400HP를 줄인 결과도 보존했다. 총 damageCalls나 요청 횟수만으로 공습 피해를 판정하지 않는다.

원본 MessageBox512×64, 실제 TrebuchetMS-Bold16·512×24 A8 글자·레이어9를 연결했다. double 현재worldTime에서 float 저장 시각을 빼는 순서, 등장·종료·pause·10초/0 sentinel·재요청을 검사했다. Native에서는 실제 글자 픽셀, 게이지2quad, 선택 메시지와 비행기/표적 표시를 확인하고 당시 source Frame을 그대로 GPU에 다시 그린 캡처를 보존했다. crop/이미지 재생성은 하지 않았다.

두 시뮬레이터의68조건 검사는 각1,200프레임을 통과했다. 기기별 context는1개다. 준비 뒤 자원read/exists·PNG업로드는0이었다. 같은 프레임 재출력의 RGB 차이는0이었다. p95 CPU+GPU제출 시간은 iPad55.09ms·iPhone45.62ms이며 같은 Mac에서 두 시뮬레이터를 함께 실행한 값이다. 실제 기기 또는60fps 합격을 뜻하지 않는다.

별도 Validation 앱도 양 시뮬레이터에서68조건·1,200프레임 후 멈추고 결과 파일을 저장했다. SFX9 요청3회·SFX10 요청9회·SFX13 요청6회, 세 실제 WAV의 재생 샘플 진행을 확인했다. 소스 시작 화면은 아직 보행이며, 전체 게임과 모든 버튼·맵·임무는 미완료다.

## 발견한 실패와 수정

strict30 수리 경계의 float 덧셈 순서 오류를 수정했다. 메시지 그림은 공통 VisualLayer가 명시 정점/UV로 전달하므로 검사도 실제512×64 정점 범위·UV를 확인한다. 비어 있는 Sprite.size/rows 필드를 크기 증거로 사용하지 않는다. 공습 테스트를 자원 인자 없이 등록했던 CMake 오류는 실제 자원을 받는 단일 검사로 고쳤다. 실패 로그를 유지했고 최종48개는 건너뛰지 않고 통과했다.

## 다음 과제와 한계

실제 Scene은 아직 차량만 허용한다. 포탑이 있는 나머지37개 필드는 진입할 수 없으며, 독립 복원된 EnemyActor 소비자와의 통합이 다음 캠페인 진행 과제다.38개 field·5개 base의 데이터695스포너는 차량385·포탑307·항공기3개다. 이 수치는 전체 맵 실행 통과 수가 아니다.

Dash·실제 BOT 행동, 모든 하체의 장착/움직임 확인, 전체60임무·캠페인, 포팅판 대조와 iPhone 최종 표시·방향/플랫폼 확장은 남는다. 원본 AirStrike는 gameplay pause 중에도 이동/가속과 열린 발사 분기를 계속한다. 이를 멈추도록 임의 변경하지 않았다. 앱 비활성은 전체 Scene을 멈춘다.

근거: 로컬 `core/full48-ctest.log`, `core/concurrent68-final.log`, `core/final68-sanitized-ctest.log`, `apple-concurrent68-final-ipad.xcresult`, `apple-concurrent68-final-iphone.xcresult`, `concurrent68-final-summary.json`, `standalone68-verification.json`, `quick-items/`, `field-message/`, `airstrike/`, `review/`다. 원본 IPA/ARM 실행파일은 업로드하지 않았다.
