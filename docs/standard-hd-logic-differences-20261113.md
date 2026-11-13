# 일반판·HD판 원본 네이티브 로직 비교

2026-11-13 정적 분석 기록이다. 일반판 2.0.1 ARMv7와 HD 1.0.3 ARMv7를 조사했다. 원본 게임을 새로 실행하거나 복원 코드를 변경하지 않았다.

**일반판에는 이동 경로·표적과 연동하는 가변 카메라 배율이 있다.** 따라서 기존 화면에서 잰 기체 크기 하나를 고정 배율로 삼으면 원작의 다른 전투 상태를 재현하지 못할 수 있다. HD 에셋을 공유하는 방향은 유지할 수 있지만, 표시 배율과 터치 역변환은 화면 배치와 별도로 설계해야 한다.

전체 함수 목록과 클래스 메타데이터는 인덱스로 만들었다. 전체 함수의 의미와 두 판본의 대응 관계를 모두 확정한 상태는 아니다. 화면 판정·충돌·생성·BOT의 남은 분기와 저장 호출 사건·음향 수명·전체 UI 대응은 추가 추적이 필요하다. 후속 독립 조사에서 저장 형식과 음향의 주요 경로, 선택한 UI 구조의 대응을 추가로 확인했다.

## 입력과 조사 범위

| 항목 | 일반판 2.0.1 | HD 1.0.3 |
|---|---:|---:|
| 비교 실행 파일 | 일반판 fat 실행 파일에서 추출한 ARMv7 slice | HD 원본 ARMv7 실행 파일 |
| 크기 | 447,376 bytes | 414,720 bytes |
| SHA-256 | `0fffcfc09cb797c04c70aa0a7b6d748adf7888d7b6583954b9a71fe1f448512a` | `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090` |
| Ghidra 함수 본체 | 1,077 | 1,300 |
| 외부 함수 / thunk | 301 / 307 | 333 / 348 |
| 전체 함수 목록 | 1,685 | 1,981 |
| 익명 `FUN_` 본체 | 934 | 96 |
| 복구한 클래스 메타데이터 | 길이 검증한 C++ RTTI 39개, 플랫폼 ObjC 7개 | ObjC 80개 |
| ObjC 메서드 / ivar | 메서드 133개. C++ 멤버 타입 전체는 미복구 | 메서드 1,194개 / ivar 763개 |
| 핵심 경로 추적 단계의 일반판 pseudo 조회 | 96 / 1,077 | 해당 없음 |
| 후속 전체 export | pseudo 1,077 + assembly 1,077. 누락·empty 0 | 기존 pseudo 1,300 + assembly 1,300 |
| 기존 HD pseudo·assembly 파일 | 해당 없음 | ObjC 1,194 + native 106. 각 파일 존재와 해시를 확인 |

함수 분모에는 게임 코드와 플랫폼 코드가 함께 들어간다. 핵심 경로 추적 단계에서 일반판 96개를 조회했다. 이어 나머지 981개를 포함한 전체 1,077개 본체의 pseudo와 assembly를 저장소 밖에 보존하고 파일 해시·주소·누락을 검사했다. 이 전체 export와 HD 1,300개 export는 두 원본의 함수 의미 동등성을 모두 대조했다는 뜻이 아니다.

일반판 본체의 소유 메타데이터를 분류하면 ObjC 메서드 133개, RTTI vtable에 연결되는 나머지 진입 124개, 미분류 820개다. 미분류 함수를 함수명 추정으로 라이브러리·엔진·게임에 강제 배정하지 않았다.

일반판에서 C++ RTTI·vtable과 일부 ObjC 메타데이터를 확인했다. HD에서는 게임 클래스의 ObjC selector·ivar 메타데이터를 확인했다. 이 차이로 개발 당시 전체 소스가 별도로 관리됐다고 단정하지 않는다. 원본 소스 저장소는 확보하지 않았다.

일반판 vtable의 30개 고유 주소는 현재 Ghidra 함수 본체 목록 밖에 있다. 일부는 실제 작은 stub이다. 예를 들어 `0x23ba4`는 REA 명령 해석에서 `70 47` / `bx lr`로 확인됐다. 나머지 주소가 모두 같은 stub이라고 단정하지 않는다.

## 대응 판정 기준

- **역할 대응 확인:** RTTI·vtable·직접 호출·필드 흐름으로 같은 역할의 경로를 확인했다. 함수 전체의 의미 동등성 판정과 구분한다.
- **후보:** 호출 또는 수식은 연결했으나 클래스·필드·분기 전체 대응이 덜 됐다.
- **미대응:** 이번 조사에서 일반판 대응을 연결하지 못했다. 다음 확인 지점을 적었다.

아래 16개 조사 항목 중 역할 대응 확인은 7개, 후보는 9개, 미대응은 0개다. 저장·음향은 후속 캐시 조사로 주요 대응을 찾아 후보로 올렸다. 미대응 행이 0이라는 숫자는 각 항목에 조사할 대응점이 생겼다는 뜻이며, 함수 전체의 의미·모든 분기·모든 이벤트가 확정됐다는 뜻이 아니다. 전체 1,077개와 1,300개 사이의 함수 대응률로 환산하지 않는다.

| 경로 | 일반판 근거 | HD 근거 | 판정과 범위 |
|---|---|---|---|
| 카메라 추적·배율 | `GobWorld` RTTI → vtable `0x67a18` slot 3 → tick `0x5af50` → `0x57124` | `GobWorld::processCamera` `0x11cac` | 역할 대응 확인. 일반판 동적 배율, 추적 완화 상수의 차이를 확인 |
| 배율 저장·카메라 경계 | `0x5a418` → manager `0x4e07c`, world `+0x4b0..+0x4bc` | HD 카메라 경계 설정·다른 scale 호출자 전체 미추적 | 후보. HD 게임 전체에 줌이 없다고 판정하지 않음 |
| 터치 수신 목록 | EAGLView touch `0x337c/0x353c/0x36fc` → manager `0x4e0e4` | EAGLView touch `0x3bf8/0x3acc/0x39a0` → `QobManager::onTap:State:ID:` `0x3a23c` | 역할 대응 확인. phone phase 1/2/3, HD 0/1/2. 일반판 수신 좌표는 화면 중심 기준. 전체 수신 순서는 미확정 |
| 월드 터치 역변환 | world vtable slot 5 `0x5a1dc` → 위치/유닛 아이템·일반 이동 | `GobWorld::onTap:State:ID:` `0x10f84` | 역할 대응 확인. 일반판 역배율 계산·HUD 경계 분기를 확인 |
| 적 터치 선택 | `0x168a8`의 Rad reader → info `+0x20`, `0x59cf0` → `0x54fb4` | Rad reader `0x1689c` → `setRadius` `0x19d28`, `onTap_EasyMove` `0x12698` | 역할 대응 확인. reader 보정과 선택 수식 차이를 확인. 실제 화면 경계 비율은 미검증 |
| 범위피해 | 탄환 `0x8910` → world `0x550d0` → `0x5504c` | `GobBullet::setCollide:` `0x71d0`의 `dmgRange != 0` 분기 | 후보. 일반판 이 함수에는 화면 필터가 없음. 모든 공격에 확대하지 않음 |
| 점 충돌·즉발피해 | weapon tick `0x23d70`의 type 0/6은 `0x550d0` 즉시 호출. 다른 탄환은 `0x8c64` → `0x8584` → `0x8910` → `0x550d0` | `checkBulletCollideX:Y:` `0x10ddc`, `checkEnemyBulletCollideX:Y:` `0x10dac`, `setCollide:`의 range 0 분기 | 후보. 일반판 공통 피해서 Range 0 전용 분기가 보이지 않음. HD 점 충돌과의 전체 대응은 미확정 |
| 화면 판정·그리기 등록 | QobBase 계열 `0x4c5e4`, QobVisual `0x51c4c`, manager `0x4edc4` | QobBase tick `0x36d98` → `isInCam` `0x37750` → `_isInScreen`; getter `0x3689c` | 후보. 일반판 전체에 대응 판정이 없는지는 미확정 |
| 적 AI 진입 | `GobHvM_Enemy` vtable slot 3 `0xc584` | `GobHvM_Enemy::tick` `0xa330` | 역할 대응 확인. 양쪽 진입 본문에 화면 gate는 보이지 않음. 이동·공격 helper별 판정은 별도 |
| 적 표적 획득 | `0xc584` → `0xb128` | `processAI_Attack` `0xa58c` 등 | 후보. 일반판 표적 없음·약 0.5초 주기·거리제곱 40,000 조건을 확인. HD 전체 대응 미완료 |
| 생성·재생성 | 맵 reader `0x58038`의 kind 4 → `0x57ac8` 적 생성. 퀘스트 추가 생성 `0x57bb8` | `readSpawner` `0xff18` → `addEnemySpawner` `0x10a00` → `GobEnemySpawner::tick` `0x7e20` | 역할 대응 확인. 맵 소비·생성 시점이 다름. HD 재생성에는 거리·카메라 조건이 있음. 일반판 전역 재생성 부재는 미확정 |
| BOT·수명 | 생성 `0x58738` → BOT tick `0xa240`, 비행 `0xaab8`, 직무 `0xa6ec/0xa9ac`, 회복 `0xad54` | BOT tick `0x8f04`, 비행 `0x95ac`, 직무 `0x927c/0x8db0`, 회복 `0x8e28` | 후보. 해당 tick 경로의 시간·직무·HP 회복을 연결. 화면 밖 소멸·전체 수명·저장 재개는 미확정 |
| 게임 시간·pause | host `0x50ec` → manager `0x4e3cc`, world tick `0x5af50` | host `0x299c`, world tick `0x110d8`, `time` `0x14864` | 후보. 전역 누적 시간은 양쪽 float32·같은 상수지만 tick 전후 순서가 다름. 월드 시간은 phone float32/HD float64. 전체 소비자는 미확정 |
| 저장 | writer `0x17f80`, reader `0x17df4`, 직접 호출 후보 6개 | writer `0x17580`, reader `0x16d8c`, selector 호출 후보 5개 | 후보. 파일명은 같으나 형식·seed 위치·XOR 범위·쓰기·난수 재설정이 다름. 사건별 전체 호출 대응은 미확정 |
| 음향 | OpenAL init `0x4965c`, cache `0x49aec`, effect play `0x49ab8/0x49ad0`, BGM `0x49a90` | init `0x412d0`, cache `0x423a8`, effect play `0x42494/0x424b0`, BGM `0x424d8` | 후보. 조사한 일반 효과음 재생은 기존 context/source를 사용. BGM stop→dispose→load→start 순서 대응. 중단·배경 전환·loop 전체 대응과 실제 음향은 미확정 |
| 화면 좌표·공통 시각 계층 | QobBase 화면 위치 `0x4bebc`, GuiGame `0x2e480`, DlgMission `0x2885c`, GuiHelp `0x3abb0` | QobBase/Tile2D, GuiGame `0x29c9c`, DlgMission `0x22b1c`, GuiHelp `0x2b2c4` | 역할 대응 확인. 선택한 HUD 부모·hit bounds·도움말·임무창·전환 구조 차이를 확인. 전체 UI 노드와 callback 대응은 미확정 |

## 일반판 카메라: 확인된 수식과 호출

일반판 `GobWorld` 타입 이름은 `0x61164`, typeinfo는 `0x67a3c`다. `0x67a18`의 vtable slot 3이 tick `0x5af50`을 가리킨다. tick은 `0x57124`를 호출한다. 이 연결은 문자열 이름만의 추정과 구분된다.

`0x57124`는 world `+0x4a8`의 플레이어와 그 표적 `+0xdc`, 또는 이동 경로 끝을 사용한다. 플레이어와 표적/이동점 사이의 중점을 목표 카메라 위치로 정한다. y에는 −40 좌표 보정이 포함된다. 목표 위치와 플레이어 사이의 차이를 x/90, y/60으로 정규화하고 큰 절댓값을 `d`로 삼는다. 함수 안의 목표 배율 수식은 `clamp(2 − d, 1.0, 1.6)`이다. 결과를 world `+0x4c0`에 쓴다.

ARM Thumb 명령 `0x5721e/0x5722a`는 나눗셈, `0x57244/0x57248`은 `1−d+1`, `0x5724c..0x57268`은 범위 제한, `0x5727a`는 결과 저장이다. literal 주소 `0x57458/0x5745c/0x57460`의 원본 4 bytes는 각각 90.0, 60.0, 1.600000023841858이다. 부동소수점 근삿값과 UI 픽셀 수를 혼동하지 않는다.

`clamp(2−d, …)`는 수학적 요약이다. 원본 명령의 `(1−d)+1` 부동소수점 연산 순서를 구현에서 바꿔도 된다는 뜻은 아니다.

tick은 manager `+0x0c`의 현재 배율을 읽고, world `+0x4c0`의 목표 배율로 차이/100만큼 접근한다. 배율 적용 함수 `0x5a418`은 역배율로 half-screen을 환산하여 world `+0x4b0/+0x4b4/+0x4b8/+0x4bc`의 카메라 경계를 바꾼다. 이어 `0x4e07c`가 manager `+0x0c`에 배율, `+0x68/+0x6c`에 화면 보정값을 쓴다. 카메라 위치는 manager `+0x50/+0x54`다.

HD `processCamera` `0x11cac`에서도 표적/이동 경로의 중점, 카메라 추적, 배경 0.94 시차를 확인했다. 해당 함수는 `easyOutTo:Div:Min:`에 50과 0.5를 전달하고 x −128..128, y −94..0으로 제한한다. **이 함수 본문에는 일반판의 거리 기반 목표 배율 수식이 없다.** 다른 HD 함수의 모든 scale 소비까지 조사한 결론은 아니다.

일반판 목표 위치 추적에는 /10과 최소 차이 0.2가 보인다. HD와 일반판의 각 필드가 같은 논리 좌표 단위를 쓴다는 전체 증명이 없으므로, 이 숫자 비율을 실제 이동 속도 비율로 발표하지 않는다.

이 차이들이 기기 대응 때문에 생겼는지, 2.0.1과 1.0.3의 버전 차이 때문에 생겼는지는 확정하지 않았다. 이번 기록은 원본 관측이며 일반판 자동 줌을 iPhone 전용 코드로 복제하자는 채택안이 아니다. 복원판은 HD 기준 공통 코드에서 출발한다.

## 시간: 전역·월드·실시간을 구분

일반판 `drawFrame` `0x50ec`은 `CFAbsoluteTimeGetCurrent`의 double을 manager `+0x44`에 저장한 뒤 manager tick `0x4e3cc`와 draw `0x4ef90`를 부른다. `0x4e3cc`은 장면·이벤트 처리 **뒤에** manager `+0x3c`의 float32 누적 시간을 늘리고 `+0x40` delta를 쓴다. HD `renderScene` `0x299c`은 전역 float32 시간을 늘린 **뒤에** 게임 tick과 그리기를 호출한다. 이 두 전역 누적 시간의 증가 상수는 같다.

월드의 pause 대응 시간은 별도다. 일반판 world tick `0x5af50`은 pause `+0x41`이 거짓일 때 world `+0x48` float32를 늘린다. `0x5afbc`가 literal `0x5b0d4`를 읽고 `0x5afc4`가 `vadd.f32`를 수행한다. HD world tick `0x110d8`은 `_gameTime`을 **float64**로 읽고 `0x11108`의 `vadd.f64`로 더한다. HD `time` `0x14864`의 8-byte 반환 복사도 이 차이를 뒷받침한다. 양쪽 월드 시간의 자료형이 같다고 주장하지 않는다.

일반판 world literal과 HD renderScene literal `0x2a00`의 raw bytes는 `b9 fc 87 3c`, bits는 `0x3c87fcb9`, float32 값은 약 0.016599999740719795다. HD world tick에서는 이 float32 값이 정확히 표현되는 double 증가량을 사용한다. **전역 누적 상수·float32 형식은 같음**, **월드 누적 형식과 전역 처리 순서는 차이**, **모든 소비자의 시간 의미 동등성은 미확정**으로 기록한다. 정확한 1/60으로 바꾸거나 복원 공통 시간 정책을 이 조사만으로 수정하지 않는다.

호스트 수명 처리도 다르다. 일반판은 resign-active/active 콜백 `0x2cd0/0x2cf4`에서 displayLink를 중단·재시작한다. HD의 대응 콜백 `0x24d8/0x24dc`은 반환한다. HD 초기화 `0x2858`에는 OS 버전에 따른 NSTimer/displayLink 분기가 있다. 원작 콜백 차이를 두 종류의 게임 코드로 옮기지 않고, 공통 호스트 정책에서 pause·저장·음향 중단·재개를 명시해야 한다. 이번 조사에서 실제 백그라운드 전환을 실행하지 않았다.

## 저장: 파일명은 같고 형식은 다름

후속 캐시에서 두 writer·reader를 확인했다. 아래 offset은 저장 파일 시작 기준이다.

| 항목 | 일반판 | HD |
|---|---:|---:|
| writer / reader | `0x17f80` / `0x17df4` | `0x17580` / `0x16d8c` |
| 저장 크기 | 26,344 bytes (`0x66e8`) | 28,204 bytes (`0x6e2c`) |
| 첫 필드 | offset 0에 magic `0x0132db78` | offset 0에 난수 seed |
| seed | offset 4 | offset 0 |
| XOR 시작 | offset 8 | offset 4 |
| 끝 checksum | offset 26,340 | offset 28,200 |
| 쓰기 | `fopen wb` → `fwrite` → `fclose` | `NSData writeToFile:atomically:YES` |

양쪽은 원본 payload byte를 signed로 더한 checksum을 쓴다. 일반판 load에는 magic이 다른 이전 형식을 위한 별도 분기가 있다. 같은 `HeavyMach2.sav`라는 파일명으로 호환성을 판단할 수 없다. 복원판이 기기에 따라 저장 형식을 바꾸거나 일반판 파일을 HD 파일로 그대로 읽어서는 안 된다. 이번 조사에서 사용자 저장을 열어 실행하거나 변환하지 않았다.

난수 순서도 다르다. 일반판 초기화 `0x52e8`은 `mach_absolute_time`으로 seed를 정한다. 일반판 save `0x17f80`은 기존 `random()` 상태에서 seed를 뽑고 `srandom(seed)` 후 XOR을 수행한다. HD save `0x17580`은 저장할 때 먼저 `mach_absolute_time` → `srandom(low32)` → `random()%0xffff`를 거쳐 seed를 정하고 다시 `srandom(seed)`한다. 저장 이벤트가 이후 게임 난수열에 미치는 효과를 실행으로 확인한 것은 아니다. HD 공통 난수 순서를 기기별로 바꾸는 근거로 쓰지 않는다.

전체 캐시의 직접 호출 검색은 일반판 writer 호출 후보 6개, HD selector 호출 후보 5개를 찾았다. 슬롯·기지·스테이지·UI 흐름이 포함된다. 간접 함수 포인터와 selector까지 닫힌 호출 그래프는 아니므로 “어떤 사격에서도 저장 호출이 절대 없다”는 전역 증명으로 표현하지 않는다.

## 음향: 초기화·재사용·BGM 순서

조사한 양쪽 OpenAL 초기화는 device 열기 → context 생성 → current 지정 순서다. 일반판 `0x49b3c` → `0x47b00` → `0x4965c`, HD `0x4229c` → `0x40504` → `0x412d0`을 연결했다. 22050f 주파수 인자도 같다. cache는 일반판 `0x49aec`의 문자열 map, HD `0x423a8`의 dictionary로 다르지만, hit이면 sound ID를 재사용하고 miss에서 source·PCM buffer를 준비한다.

일반 효과음은 일반판 `0x49ab8/0x49ad0`, HD `0x42494/0x424b0`에서 위치 설정과 기존 source 정지 후 비반복 `alSourcePlay`로 이어진다. **이 조사한 재생 경로 안에는 context 생성·파괴나 PCM 재로드가 없다.** 엔진 종료 경로 `0x49398/0x42114`는 따로 있다. loop worker·source pool 선택·모든 호출 빈도를 대조한 결론은 아니다.

BGM의 상위 순서는 양쪽에서 이전 곡 stop 요청 → 기존 queue/state dispose → bundle 경로 결정 → load → start다. 일반판 `0x49a90`은 `0x478c0(1)` → `0x47908` → 경로 변환 → `0x47a64` → `0x478ec` 순으로 호출한다. HD `0x424d8`도 `0x40134(1)` → `0x40310` → 경로 변환 → `0x40400` → `0x40160` 순서다. AudioQueue 시작 helper `0x488d4/0x40f50`은 `AudioQueuePrime(queue,1,0)`이 성공해야 `AudioQueueStart(queue,0)`을 부른다.

버퍼 설정에는 양쪽 0.5초 요청, 최소 `0x4000`·최대 `0x10000`, 짧은 파일 1개/스트림 3개 버퍼 구성이 보인다. 연산 순서와 형식 전환 helper 전체가 동등하다고 판정하지 않는다. HD callback pointer의 모든 연결, 실제 오디오 형식과 rounding, 중단·재개·지연·다성음은 남은 확인이다.

HD `setPause` `0x42268`은 사운드 cache를 비우며 해당 본문에 `AudioQueuePause`는 없다. `setBackground` `0x42364` → `0x40604`는 효과음·stream 엔진을 정리한다. 대응 일반판 고수준 사건의 연결은 미확정이다. 메서드 이름만으로 실제 queue pause를 확인했다고 말하지 않는다.

## 선택한 원본 UI 구조의 추가 비교

UI 후속 조사는 36개 고유 함수의 pseudo·assembly 72개 파일을 읽은 정적 비교다. 아래 구조를 확인했지만 상점·인벤토리·모든 버튼과 callback의 전수 대응은 끝내지 않았다.

| 영역 | 확인한 차이 | 공통 HD 설계에서 지킬 점 |
|---|---|---|
| 입력 phase·기준점 | phone begin/move/end=1/2/3, HD=0/1/2. phone manager는 half-screen을 빼 중심 기준 좌표를 전달. 호스트는 bounds에서 크기를 구하므로 코드만으로 480×320 고정값을 가정하지 않음 | 원본 숫자 phase를 서로 그대로 넘기지 않는다. 공통 입력 enum과 가역 좌표 변환을 사용 |
| Retina·tile 크기 | phone `0x520c0`과 HD `0x3f56c`는 조건부 @2x 탐색. 별도 explicit 경로는 실제 @2x 파일을 찾지 않아도 forRetina를 지정할 수 있음. phone TileSplit `0x4b850`은 logical width/height를 반으로 하지만 HD `0x43ea8`에는 그 조건부 반감이 없음. draw에는 양쪽 forRetina scale 반감이 있음 | 실제 파일 픽셀·분할·논리 tile 크기·draw scale을 분리한다. retina 계수를 중복 적용하지 않음 |
| HUD 부모·hit bounds | phone `0x2e480`의 무기 1022/1023은 배경의 자식이며 bounds 80×52. HD `0x29c9c`는 GuiGame 직속·64×64이며 사거리 표시 자식이 있음. 메뉴 부모도 다름 | HD 조립 계층에서 표시와 hit 영역을 함께 이동. phone 위치를 일괄 확대하지 않음 |
| 임무창 | phone `0x2885c`은 QobImage 자체 MissionBG와 닫기 자식. HD `0x22b1c`은 QobBase 아래 위·중간·아래 배경 및 아래 배경의 OK/닫기 자식 | HD 구성·버튼 동작을 유지하고 공통 GPU 배치로 재배열. phone 임무 완수 기능 부재로 해석하지 않음 |
| 도움말 | phone `0x3abb0/0x3afb4/0x3affc`는 JPG 5페이지·page 0..4·중심 x −112/+112·phase 1. HD `0x2b2c4/0x2b1f0/0x2b0a8`은 PNG 3페이지·양수 x 204/564·phase 2 | HD 3페이지를 공통 기준으로 명시. 가로 화면에서 같은 파일의 읽기 크기·확대/이동·잘림 정책을 정하고 검증 |
| 닫힘·열림 전환 | phone `0x5f8c`, HD `0x45e8`은 phase와 효과음·phase 4 화면 교체·divisor 5/6/10·center scale 1.5/0.9/1 역할이 대응. 시작 좌표와 문턱은 다름 | HD tick 수식·순서·효과음을 유지한다. viewport를 덮는 geometry/clip/UV와 진행 시간 수식을 구분 |

phone HUD의 무기 이미지 자식 scale 0.5와 hit bounds는 별도 소비다. HD `QobButton::setBoundWidth:Height:` `0x37aa8`은 scale 필드로 bound rectangle을 계산한다. draw만 옮기고 hit 영역을 남기거나 비대칭 scale을 디컴파일 선언만으로 재해석하면 입력이 어긋날 수 있다. 전체 노드의 parent·absolute position·clip·hit edge 계약은 추가로 연결해야 한다.

## 입력·선택

일반판 EAGLView 터치는 manager `0x4e0e4`로 전달된다. manager는 half-screen `+0x60/+0x64`를 먼저 빼고 UI 수신 목록을 순회한다. world 입력 `0x5a1dc`는 y < −114를 배제하고, `1 / manager.scale(+0x0c)`를 곱한 뒤 카메라 중심·offset을 더한다. pause와 플레이어 HP도 검사한다. 그다음 위치 아이템 `0x59720`, 유닛 아이템 `0x59994`, 일반 이동·선택 `0x59cf0`으로 분기한다.

일반판 `0x54fb4`는 world 적 목록 `+0x78`에서 HP `+0x60 > 0`인 대상을 찾는다. 객체 `+0x3c`가 가리키는 정의의 `+0x20` 값을 읽고 1.5를 곱한 뒤 제곱하여 거리제곱과 비교한다. HD `onTap_EasyMove`는 `info.radius × info.radius`와 비교한다.

후속 전체 캐시에서 reader까지 연결했다. 일반판 `0x168a8`은 `MachList.tble`을 열어 `Rad`를 정수로 읽고 **float32 변환 → ×0.5 → int32 변환** 후 정의 `+0x20`에 저장한다. ASM `0x16c60/0x16c64/0x16c68/0x16c6c`가 이 순서를 확인한다. `0x13d28`이 같은 정의 맵에서 이름으로 찾아주고, 공통 기체 초기화 `0xefe8`이 인스턴스 `+0x3c`에 저장한다. 선택 함수는 이 포인터의 `+0x20`을 읽는다.

HD reader `0x1689c`은 CFString `0x5e8ac`(`Rad`)로 읽은 정수를 `MachInfo::setRadius:` `0x19d28`에 그대로 전달한다. 이 setter도 값을 그대로 저장한다. 일반판의 맵 reader `0x58038`은 kind 4 레코드의 short x/y에도 ×0.5를 적용한다. 따라서 반지름 보정만 떼어 “선택 범위가 HD보다 1.5배”라고 말하면 좌표 단위 변환을 빠뜨린다. 확인한 수식은 일반판 저장 반지름 `trunc(Rad×0.5)`, 선택 임계 반지름 `저장값×1.5`, HD 선택 임계 반지름 `Rad`다. 실제 화면 픽셀 경계는 동적 줌·투영까지 같은 상태로 측정해야 한다.

## 화면 판정·피해·AI

HD의 `isInScreen`은 존재가 확정됐다. `QobBase::tick` `0x36d98`이 show·visual·remove를 검사하고 `isInCam` `0x37750`의 결과를 `_isInScreen`에 저장한다. `isInCam`은 카메라·boundRect·화면 좌표를 사용하고 경계에 60의 여유를 둔다. getter `0x3689c`가 이 값을 돌려준다.

HD에서는 이 값이 그리기 외에도 쓰인다. `GobHvMach::processMove` `0xd6d8`의 항공 분기는 화면 검사보다 먼저 빠지고, 나머지 경로는 화면 밖이면 반환한다. `checkBulletCollideX:Y:` `0x10ddc`는 HP→`isInScreen`→`isInPt` 순으로 검사한다. `GobBullet::setCollide:` `0x71d0`의 범위피해는 적과 BOT 순회에서 화면 값을 검사한다. **같은 함수의 `dmgRange == 0` 분기는 화면 검사를 하지 않는다.** 이를 모든 피해의 공통 조건으로 합치면 원본 분기를 잃는다.

일반판에서는 `0x8910`이 `0x550d0`을 호출한다. `0x550d0`은 발사자 측에 따라 적 목록 또는 플레이어/BOT 목록을 순회하고 `0x5504c`의 거리·반지름 피해 계산으로 이어진다. 이 본문에는 HD 범위피해와 같은 화면 필터가 보이지 않는다. 일반판의 모든 탄환·즉발·이동·AI가 화면 밖에서 동일하게 동작한다는 결론은 아니다.

일반판 weapon tick `0x23d70`의 type 0/6 분기는 효과를 만들고 `0x550d0`을 즉시 부른다. 다른 탄환 분기는 `0x8c64`로 탄환을 초기화하고, tick `0x8584`의 비행 종료 조건 → `0x8910` → `0x550d0`으로 이어진다. `0x5504c`는 `기체 저장 반지름 + 무기 정의의 short +0x0c`를 피해 반지름으로 사용하고 `min(1, 1−distance/radius+0.5)`를 피해에 곱한다. 무기 reader `0x15128`에서 `+0x0c`가 원본 `Range` 소비값임을 확인했다. 이 일반판 공통 피해 경로에는 HD의 Range 0 별도 고정피해 분기가 보이지 않는다. 소수점·정수 변환과 원본 reader의 추가 보정도 포함되므로 복원판 공통 피해식을 이 수식으로 즉시 바꾸지 않는다.

일반판 기본 트리 순회 `0x4c5e4`와 시각 등록 `0x51c4c`에서 HD처럼 `isInCam` 결과를 공통 캐시에 저장하는 흐름은 찾지 못했다. 함수명 자체도 대부분 제거돼 있다. 따라서 “일반판에는 isInScreen이 없다”는 전역 결론은 아직 내리지 않는다. 다음 확인은 일반판 모든 시각 flag·카메라 경계 참조와 지상 이동·탄환 후보를 묶어 추적하는 것이다.

일반판 적 tick `0xc584`와 HD 적 tick `0xa330`의 진입 본문은 pause·HP와 기체 유형/상태로 AI helper를 부른다. 두 진입 본문에서 화면 gate는 확인되지 않았다. HD의 지상 이동 화면 조건과 적 tick 진입 조건을 혼용하지 않는다.

후속 캐시에서 일반판 기체 tick `0x10f08` → 이동 분기 `0x10230` → 유형별 이동 `0xfc20/0xf97c/0xf800/0xf714`를 연결했다. `0x10230`은 기체 유형 `+0x50`을 바로 분기하며 HD `0xd6d8`의 공통 화면 gate와 같은 검사는 해당 분기 앞에 보이지 않는다. 상위 active flag와 그리기 판정까지 모두 같다는 결론은 아니다.

추가 ASM 추적으로 일반판의 상위 flag 의미를 좁혔다. `0x4bcfc`는 객체 byte `+5`를 읽고, 참이면 parent `+0x38`을 재귀 검사한다. 이 함수는 좌표·카메라·화면 크기를 읽지 않는다. 기본 트리 `0x4c5e4`는 이 검사에 통과하면 vtable `+0x0c`의 tick을 부르고, remove byte `+7`이 거짓인 자식의 vtable `+0x1c`를 호출한다. 이 경로의 `+5`는 부모를 포함한 표시/활성 flag이며 HD의 좌표 기반 `_isInScreen` 캐시와 같은 값으로 취급할 수 없다.

기체 tick의 ASM `0x10f20`은 자기 `+5`, `0x10f9e`는 world pause `+0x41`, `0x10fac`는 기체 상태 `+0x54`를 읽는다. 살아 있는 이동 상태에서는 `0x10fc2`가 `0x10230`을 부른다. 그 함수는 `0x1024e`에서 유형 `+0x50`을 읽고 `0x1029a/0x102a8/0x102b6/0x102c4`에서 네 이동 helper로 분기한다. HD처럼 이 공통 유형 분기 앞에서 화면 캐시를 검사하는 조건은 없다. BOT도 pause·직무 처리를 거친 뒤 같은 기체 tick을 사용한다. **이 공통 진입의 gate 차이는 확인했지만, 모든 상위 flag 쓰기와 모든 이동 helper 내부의 활성 조건까지 닫은 결론은 아니다.**

재현 도구는 일반판 1,077개 ASM 전체에서 정확한 literal offset `#0x5`의 byte 읽기·쓰기와 setter `0x4bcf8` 직접 BL 호출을 색인한다. 직접 byte 쓰기는 6명령/6함수, 읽기도 6명령/6함수, setter 직접 호출은 150회/65함수다. 직접 쓰기에는 초기화 `0x4bcb0/0x4c7a0`, setter, particle 초기화·수명·시작 `0x50fe8/0x5108c/0x51274`가 포함된다. 이 숫자는 모든 object alias·register offset·넓은 store·간접 호출을 해석한 수가 아니다. 150개 호출 대상이 기체 자신인지 UI/파츠인지 모두 분류하지 않았으므로 “화면에서 멀어져도 flag가 절대 바뀌지 않는다”고 확장하지 않는다.

현재 vtable에서 확인한 30개 진입은 본체 목록뿐 아니라 전체 export의 명령 주소 목록에도 없다. `0xb318/0xb988`처럼 Enemy 가상 slot이 가리키는 주소도 포함된다. 앞서 별도 명령 증거로 확인한 `0x23ba4`의 반환 stub을 나머지에 일반화하지 않는다. 다음 정적 확인은 이 진입들의 실제 명령·분기 범위를 각각 확보하고, 150개 setter 호출 중 적/BOT 자신·부모를 바꾸는 호출을 구분하는 것이다. 그 뒤에야 화면 밖의 사망·충돌·수명 경로까지 더 좁힐 수 있다. 이번 추가 조사에서는 REA 세션을 새로 열거나 앱을 실행하지 않았다.

## 생성·BOT: 확인한 범위

일반판 맵 reader `0x58038`은 kind 4의 이름과 ×0.5 좌표를 `0x57ac8`에 전달한다. 이 함수는 적 생성자 `0xc718`, 공통 정의 초기화 `0xefe8`, 위치 설정, 최초 위치 `+0xf0/+0xf4` 저장을 거쳐 world `+0x78` 적 목록에 넣는다. 다른 직접 호출자 `0x57bb8`은 활성 퀘스트 액션을 읽어 추가 적을 생성한다. 이 관측에서 HD의 위치별 spawner와 같은 타이머를 일반판에 임의로 가정하지 않는다. 전체 indirect call과 디컴파일이 제거한 블록까지의 부재 증명은 끝내지 않았다.

HD `readSpawner` `0xff18`은 `addEnemySpawner` `0x10a00`으로 spawner를 만든다. `GobEnemySpawner::tick` `0x7e20`에는 checkTime+0.5, spawnTime+regenTime, regenCount, 플레이어와 x/y 각각 1024 미만의 거리 조건이 있다. 재생성 간격이 0이 아니면 `isInCam`이 거짓일 때만 생성하는 분기도 있다. 이 값은 HD 좌표이며 일반판 수치와 단순 비교하지 않는다.

일반판 BOT은 `0x58738`에서 만들고 world `+0x7c`에 등록한다. RTTI vtable slot 3의 `0xa240`은 pause가 아닐 때 비행 `0xaab8`, refType 0 공격 `0xa9ac` 또는 refType 2 아이템 수집 `0xa6ec`, 회복 `0xad54`를 부르고 공통 기체 tick으로 간다. HD `0x8f04`도 비행·직무·회복 후 공통 tick으로 간다. HD refType 1/3 직무 메서드는 기존 export에서 짧은 반환 함수다. 이 때문에 일반판의 분기 생략만 보고 두 직무 전체가 누락됐다고 단정하지 않는다.

일반판 회복 `0xad54`와 HD `0x8e28`은 HP가 0보다 크고 최대 HP보다 작을 때 게임시간으로 약 5초 간격의 HP+1을 처리한다. 이 BOT 진입·비행·회복 본문에는 카메라를 벗어나면 소멸하는 분기가 보이지 않는다. 사망·맵 전환·저장 재개를 포함한 전체 수명 정책의 동등성은 아직 확정하지 않았다.

## 유니버설 설계에 주는 기준

1. 공통 HD 에셋·공통 게임 코드를 유지한다. 카메라·화면 배치·입력 변환을 데이터 정책으로 분리한다. 기기마다 별도 게임 소스나 별도 iPhone 에셋 경로를 만드는 근거로 쓰지 않는다.
2. 기존 기체·HUD 측정값에 카메라 상태를 추가한다. idle, 경로 끝, 표적 거리, 현재/목표 배율을 함께 기록해야 같은 조건의 표시 크기를 대조할 수 있다. 이번 조사에서는 새 실행 측정을 하지 않았다.
3. 터치는 표시와 같은 카메라 변환의 역함수로 처리한다. 배율·화면 offset·HUD 고정 영역을 한꺼번에 확대하는 설계를 피한다.
4. HD의 화면 캐시가 이동·충돌에도 쓰이므로, 넓어진 iPhone 화면만으로 적 활성 범위나 피격 조건이 바뀌는지 별도로 판단해야 한다. 표시 화면과 시뮬레이션 활성 범위를 같은 것으로 정하기 전에 미확정 호출을 마친다.
5. 패키지 차이는 이 네이티브 수식과 구분한다. 별도 패키지 조사에서 확인한 하체 23개의 SP 최대량·회복 파라미터, 일반판에서 주석 처리된 저격 무기 4종은 UI 크기 차이와 성격이 다르다. KR 미션 60개가 같은 사실만으로 네이티브 시간·카메라·음향 로직이 같다고 판단하지 않는다. 상세 근거는 [패키지 비교](universal-package-differences-20261113.md)를 따른다.

## 남은 정확한 확인

| 미확정 | 다음 정적 확인 | 설계에서 보류할 결정 |
|---|---|---|
| 일반판 화면 판정 전체 | `0x4c5e4/0x51c4c/0x4edc4`와 draw 소비자의 flag·경계 읽기, `GobHvMach` 이동 helper의 전체 호출 | 화면 밖 AI·이동을 HD와 같은 gate로 고정할지 |
| 실제 화면 선택 반지름 | reader의 half·정수 변환과 선택 ×1.5는 연결 완료. 동적 줌·투영을 포함한 동일 상태의 화면 경계 확인이 남음 | iPhone hit radius 정책 |
| 점 충돌·즉발·범위피해 대응 | `0x23d70/0x8584/0x8910`의 분기를 weapon type·range와 연결 | 화면 확대가 무기 유효 범위를 바꾸는지 |
| 렌더 배율 전체 | manager `+0x0c/+0x68/+0x6c` 쓰기→투영/정점 소비와 HD scale setter 전체 caller | 동적 줌을 어떤 플랫폼 정책으로 보존할지 |
| 생성·BOT·수명 | 맵 초기 생성·HD 재생성·BOT tick은 연결 완료. 일반판의 다른 간접 생성 경로와 사망·맵 이탈·저장 재개 추적이 남음 | 적/BOT 활성 영역·동시 개체 수 |
| 시간·저장 | 전역/월드/실시간의 모든 소비자, 직접·간접 writer caller의 mission/reward/map exit 사건 대응 | 시뮬레이션 시간 형식·저장 시점과 난수 순서 |
| 음향 | loop worker/source pool, HD callback pointer, cache reset·background 사건 및 interruption/resume 연결 | 공통 재생기의 다성음·지연·수명 동등성 |
| 전체 UI | 선택한 36개 외에 인벤토리·상점·퀵슬롯·에어리프트·타이틀·슬롯·loading callback과 전체 parent/clip/hit 연결 | 고정 HUD·팝업·상점의 전체 재배치와 full-page raster 가독성 |

## 근거 보존과 재현

- 비교 메타데이터: [JSON](evidence/standard-hd-logic-differences-20261113.json).
- 재현 도구: `python3 tools/audit_universal_native_coverage.py`. 원본을 읽고 해시를 검사한다. 앱 실행·빌드·설치를 하지 않는다.
- 로컬 전체 인덱스: `.local/evidence/universal-native-audit-20261113/`의 `phone201-procedures.tsv`, `hd103-procedures.tsv`, `phone-rtti-vtables.json`, `hd-objc-inventory.json`, `hd-export-index.json`, `phone-query-index.json`, `coverage-summary.json`.
- 원문 pseudo·assembly·REA snapshot은 저장소 밖 `/Users/sokol/Documents/Codex/HeavyMach2/Analysis/`에만 보존했다. 일반판 핵심 bundle은 `Phone201/universal-audit-20261113/rea-evidence-bundle.json` 112건이다. 전체 bundle은 같은 폴더의 `rea-full-export-bundle.json` 2,164건이며, `full-export/functions/`의 1,077쌍과 `full-export/export-manifest.json`에 파일 해시·주소·증거 ID를 보존했다. 세션은 정상 종료했다.
- 일반판 주소 `0x23ba4`에는 Ghidra 함수가 없다. 숫자 주소 요청이 파일 offset으로 대체 해석되어 무관한 `0x24b50` pseudo가 반환된 1건을 대응 근거에서 제외했다. 명령 직접 확인으로 stub만 인정했다.
- 후속 독립 조사 A는 `.local/evidence/universal-runtime-native-review-20261113/`의 `findings.json`·`verification.json`이다. 월드 시간 정정을 반영한 9개 결론에 함수 참조 99회·파일 참조 198회가 있으며 고유 함수 97개·고유 파일 194개다. 입력 SHA는 `06fad3c2f8373ad9bd69822dd45ee34e092028093f586c302f2fc66095e63a68`이다. 후속 B는 `.local/evidence/universal-original-ui-review-20261113/findings.json`의 6개 결론·고유 함수 36개·파일 72개다. 통합 도구는 참조 파일의 주소 시작·SHA를 다시 검사하고 두 입력 JSON 자체의 해시도 기록한다. 캐시 읽기 검토를 새 앱 실행이나 전체 의미 동등성 검사로 표현하지 않는다.
- 표시/활성 flag 후속 인덱스는 `.local/evidence/universal-native-audit-20261113/phone-activation-flag-index.json`이다. 1,077개 ASM에서 literal-offset 접근과 직접 호출을 다시 셀 수 있다. 이 검색 분모와 읽기·쓰기·호출 분모를 전체 의미 해석률로 바꾸지 않는다.
- 디컴파일의 `undefined`, `unaff`, `extraout`·도달 불가 경고와 ARMv7 부동소수점 호출 규약 문제를 보존했다. FP 인자/반환이 C 선언에서 어긋날 수 있어 중요한 배율은 명령과 literal까지 확인했다. 원본 소스를 확보했다고 표현하지 않는다.
