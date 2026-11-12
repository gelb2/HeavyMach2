# 전투·진행·저장 복원 중간 결과

2026-11-10. HD1.0.3 실행파일을 REA/Ghidra로 분석하고 C++ 공통 코어에 옮겼다. 원본 소스를 확보한 것은 아니다. 아래 검사 결과는 모듈과 제한된 전투 장면의 결과다. 설치된 Source/Validation 앱의 전체 플레이·전체 치트 검증 완료를 뜻하지 않는다.

## 원본 분석 근거

분석 대상은 로컬 HD 실행파일 `original-executable`, SHA-256 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`다. REA의 Ghidra12.1.4 분석에서 함수·어셈블리·호출 관계·문자열을 읽었다. 부동소수점 인자와 명령어 순서가 디컴파일 표현에서 불명확하면 어셈블리를 함께 확인했다. 원본 실행파일과 전체 디컴파일 증거는 로컬에만 보관한다.

| 원본 동작 | 새 소비 코드 | 확인한 범위 |
| --- | --- | --- |
| 무기 reload/fire·포구·직접피해와 GobBullet | Simulation/WeaponController, WeaponEffects, ProjectileMotion, ProjectileImpact | 9종 무기 동작 경로, 원본 난수·반동·탄환/효과 분리 |
| QVisualLayer·QobImage·GobTrailEffect | Presentation/VisualLayer, Simulation/Trail, ProjectileVisual | non-atlas 우선, atlas 생성 순서, 20바이트 vertex/80바이트 quad, draw 시점 효과 |
| Enemy 차량·스폰·피격·사망 | EnemyVehicle, EnemySpawner, CombatScene | Stage01_01의 차량 적과 16스포너, 사망 후 부품/표적/스포너/보상 순서 |
| Player.tick `0xc028`, onDamage, levelUp `0xb1f8` | CombatScene, Progression, Equipment | actor HP 재생과 slot HP를 구분. 피해 때만 slot에 반영. 레벨업이 actor HP를 자동 회복시키도록 바꾸지 않음 |
| selectSlot `0x17be0`, initSlot `0x17e10` | SaveData, Session/GameSession | working slot 복사, 장비·임무 복원, addExp(0)1회, 선택 시 저장 |
| saveDataFile `0x17580` | Data/SaveData | 28204바이트, 미해석 바이트 보존, signed-byte 합, 동일 게임 RNG reseed/XOR |
| setScreen_Base `0x5644`, GuiBase.init `0x246d4` | Session/GameSession | 기지 이름·복귀 장소, HP 회복, bot HP 초기화, 기지 진입 저장 |
| GuiBase.openGateList `0x2593c`, setStage_LinkGate `0x13778` | Session/GameSession, CombatScene | gate No와 raw subchunk index 구분, 링크 gate의 signed-byte 소비, 원본 안쪽50 이동 |
| GobFieldItem init `0x81dc`·tick `0x8580`, QobBase.calcWorldPos `0x373bc` | Progression/FieldItem, Session/GameSession | 드롭·관성·획득·공유 난수, 부모 위치만 가산. 회전/크기/투명도는 자식에 자동 상속하지 않음 |
| SoundManager와 OpenAL/AudioQueue | Core/Audio, Platform/Apple/HM2Audio | 원본 WAV57개, 효과음 준비/재생 분리, source/buffer/context 재사용, BGM AudioQueue |

함수별 REA Evidence ID·전체 응답·입력 해시·실행 로그는 `.local/evidence/combat-runtime-20261110/`에 있다. 예: 기지 진입 `ev_31b9ad2806a9ec15869c77788e6cb3d25e9992c07f182f4a0e62e2db8cf09dda`, gate 목록 `ev_e2743e8f16ed5a361f084944ccd8e2595d4224961cd4c2d6d068bfa9ddca5279`, stage 전환 `ev_7c4882313a45877fcfae7f854cb2380d0940898a9b779a54c4fd1b9502e18973`.

## 실행해서 확인한 결과

- 코어 검사40개를 통과했다. 이후 Session 통합 검사를 추가했으며 관련 저장·전투 검사와 새 Session 검사를 통과했다. 최종 전체 검사 수는 후속 전체 실행 결과로 갱신한다.
- 실제 HD Stage01_01에서 1800 tick/draw를 실행했다. Player 발사63회, 적 발사12회, 피해10회, 적 격파3회와 사망 처리 뒤 제거를 확인했다. 이 장기 검사는 방어 hook을 사용한 제한된 전투 검사다. 정상 난이도 플레이로 보고하지 않는다. 반복 중 파일 read/exists 증가는0이었다.
- 원본 포팅 앱의 실제 저장 파일을 읽어 해석한 뒤 Darwin libc의 난수로 다시 계산했다. 28204바이트 전체가 일치했고 원본 파일이 변하지 않았음을 다시 읽어 확인했다.
- Apple 두 시뮬레이터에서 OpenAL 재생 진행, A8 글자와 효과 그림의 GPU 픽셀, atlas40개 quad를1회 indexed draw로 그리는 검사를 실행했다. atlas와 개별 sprite 비교의 픽셀 차이는0이었다. 이 결과는 전체 전투 GPU·전체 원작 대조 결과가 아니다.
- 새 Session 검사는 슬롯→기지→임무 수락→원본 gate 링크→실제 전투, 저장 이벤트, 실패한 저장의 기존 내용 보존과 독립 게임2개의 상태/RNG 분리를 확인했다. 전투 tick·사격 중 저장 요청과 파일 읽기 증가는0이었다.
- `GimmeCr` 슬롯은 원본 initSlot에서 level50/EXP100m으로 시작한다. selectSlot의 원본 addExp(0)이 한 번 더 레벨을 올려51이 된다. 처음 검사에서50으로 예상해 실패했고 원본 EXP 규칙에 맞춰 검사 기대값을51로 정정했다. 실패 로그는 보존했다.

## 남은 연결과 검증

설치된 앱은 아직 보행 장면이다. GPU 메뉴·실제 게임 상태·음향·원자 저장을 앱에서 연결하고 iPhone 가로/iPad 세로의 화면과 입력을 검증해야 한다. Session의 미해석 버튼은 계수에 남기며 구현 완료로 취급하지 않는다.

현재 통합 CombatScene은 차량 적이 있는 Stage01_01 범위다. 실제 전체43맵의 스포너695개는 차량385/포탑307/항공기 보스3개다. 포탑과 항공기 소비자를 복원하고 있다. 맵 스포너에 열차·적 bipod는 없으며, 없는 등장을 가정해 만들지 않는다. 다른 하체, bot 실제 행동, 대시·특수 공격·도움말·게임오버·맵 이동과 모든 버튼의 실제 상태 변경도 연결해야 한다. 새 메뉴 그림이나 버튼이 생성됐다는 이유로 기능 완료를 표시하지 않는다.

전장 좌우 무기 교체는 후속 검사에서 실제 부품/controller에 연결했다. 교체한 쪽만 원본처럼 새로 추가하고 reload하며, 반대쪽 탄 수·재장전 시각·반동·포구를 보존했다. 교체 후 양쪽 미사일의 실제 발사와 반복 중 파일 접근0을 확인했다.

GPU 검수에서 아이콘 누락을 발견했다. 처음에는 버튼에 가려진다고 추정했지만 해당 버튼 그림은 완전히 투명했다. 원인은 Ghidra의 정리된 CF 심볼 이름을 실제 파일명으로 오독한 것이다. 실제 문자열 `Icon%05d.png`와 `IconB%05d.png`에 밑줄이 없는데 구현은 밑줄을 넣고 있었다. 공통 경로를 고쳤고 아이템255개의 아이콘이 실제 파일로 연결됨을 검사했다. 두 시뮬레이터의 실제 HUD 퀵 아이콘 픽셀867개를 원본 PNG의 직접 GPU 출력과 비교해 차이0을 확인했다. 처음 관측과 잘못된 원인 추정도 로컬 기록에 보존했다.

Apple `GameRuntime`과 GPU 통합 검사를 추가했다. 타이틀→슬롯→이름 입력 요청→기지→필드의 공통 버튼 입력, 월드 터치 선택·실제 발사, 파일 원자 저장과 재실행을 두 시뮬레이터에서 실행했다. 반복 출력에서 자원 읽기·텍스처 업로드·효과음 context/source/buffer 생성 증가는0이었다. 손상된 파일은 덮어쓰지 않고 오류를 반환했다. 이 검사는 HD768×1024 canvas의 Apple 소비 경로다. iPhone 최종 가로 HUD·비례와 iPad 원작 전체 대조를 통과한 유니버설 앱으로 보고하지 않는다. Source 앱의 시작 화면은 아직 보행 장면이다.

게임판·기기 해상도의 전체 캡처는 로컬 XCTest 결과에 보존했다. 보고서용384×512 미리보기는 별도 GPU 표면에서 다시 그린 출력이다. 전체 캡처를 잘라내거나 크기를 바꾼 이미지가 아니며 원작 해상도 판정에 사용하지 않는다.

저장과 활성 전투는 같은 RNG를 사용하도록 연결했다. 초기 QTable 로드와 전체 앱 시작 과정까지 원본 전역 RNG 소비가 일치하는지는 확인하지 못했다. 전체 실행 재현과 원작 대조 전에 이 차이를 조사한다.

손상된 저장 길이/체크섬은 오류를 반환하고 기존 파일을 보존한다. 원본의 범위 밖 memcpy와 자동 초기화를 그대로 재현하지 않은 차이다. 기기별 최종 배율·가시성 경계·프레임 시간·출력 지연·다른 방향과 플랫폼·실기기 검사는 남아 있다. 일반판 포팅 앱의 검은 지형 오류도 남아 있다.

다음 작업은 [전체 치트 계획](full-validation-development-plan-20261110.md)의 미완료 소비자를 앱에 연결하고, 실제 버튼 입력과 복합 전투를 독립 인스턴스에서 함께 검사하는 것이다. 이 문서와 모듈 검사를 근거로 전체 치트 계획을 완료 처리하지 않는다.

## 실제 동시 검증 후속

[59조건 동시 검증](concurrent-validation-20261110.md)을 연결했다. 최종C++46개와 iPhone/iPad 각각1,200프레임을 통과했다. 무기110개 실제 발사, 실제 이동·코인/아이템 획득·레벨업·거래·임무 조건·저장 재시작을 같은 프레임 시계에서 확인했다. 효과음23개 동시 진행, context1개, warmread/exists/PNG업로드0을 관측했다. 선택적 화면 배치 최적화는 실제 픽셀 차이로 채택하지 않았으며 원래 GLES/scissor 경로를 유지했다. Source의 시작 화면은 보행 단계이고 전체 게임·모든 버튼·모든 임무·기기별 최종 표시는 여전히 미완료다.
