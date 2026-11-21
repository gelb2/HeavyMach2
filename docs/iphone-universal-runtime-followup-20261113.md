# iPhone 유니버설 설계: 시간·저장·음향 후속 조사

2026-11-13. 일반판 2.0.1과 HD 1.0.3의 기존 REA ASM·pseudo 캐시를 조사했다. 정식판·치트판의 게임 규칙은 두 기기 모두 HD를 따른다. 일반판의 저장·난수·시간·음향 구현을 iPhone 전용 코드로 옮기지 않는다.

이 문서는 정적 조사와 다음 개발·검사 기준이다. 앱 실행·빌드·실기기 조작·게임 소스 변경은 하지 않았다. 원작과 복원판의 전체 의미 동등성을 판정한 문서가 아니다.

## 이번에 확인한 범위

| 항목 | 이번 결과 |
|---|---|
| 캐시 검색 분모 | 일반판 1,077개 본문, HD Objective-C 1,194개와 native 106개 본문 |
| 해시로 연결한 상세 근거 | 고유 함수 217개, pseudo·ASM 파일 434개. 모든 파일에 원본 native SHA·주소·export evidence ID를 연결했다. |
| HD 월드 시간 | `time` 호출 53회/31함수, `_gameTime` 직접 참조 5함수 |
| HD 전역 프레임 시간 | 참조 16함수. producer·timestamp 저장·소비를 구분한다. |
| 일반판 시간 후보 | literal `+0x48` 후보 83함수. 그중 월드 시간과 연결한 역할은 producer 포함 30개다. 다른 구조체의 scale·index도 같은 offset을 쓰므로 83개를 시간 소비자로 세지 않는다. |
| 직접 저장 caller | 일반판 6함수, HD 5함수를 사건·조건에 연결했다. |
| HD 음향 callback | 원본 실행파일 SHA와 `__DATA` 매핑을 확인하고 pointer 4개를 독립 검증했다. |

[전수 검색·호출 목록과 근거 JSON](evidence/iphone-universal-runtime-followup-20261113.json)에 행별 주소·횟수·해시·근거 ID를 남겼다. 이 목록은 모든 간접 호출과 alias를 해석한 비율을 뜻하지 않는다.

## 시간: 전역 프레임·월드·실시간을 분리한다

전역 프레임 시간은 양쪽 모두 `float32` 증가값 `0x3c87fcb9`, 즉 `0.016599999740719795`를 쓴다. 일반판 manager `+0x3c` 갱신과 HD `renderScene 0x299c`의 전역 갱신 순서는 다르다. 일반판 `drawFrame 0x50ec`은 별도로 `CFAbsoluteTimeGetCurrent`의 double 값을 manager `+0x44`에 기록한다. 이 값을 월드 시뮬레이션 delta로 동일시하지 않는다.

월드 시간은 **일반판 `0x5af50`의 `+0x48` float32, HD `0x110d8`의 `_gameTime` float64**다. 실제 더하기 명령은 일반판 `0x5afc4 vadd.f32`, HD `0x11108 vadd.f64`다. HD getter `0x14864`는 8바이트를 복사한다. 같은 증가값을 써도 누적 정밀도는 같지 않다.

HD의 소비 필드도 전부 double인 것은 아니다. 무기 `fireTime`, BOT HP 회복 시각, 플레이어 아이템 획득 시각, HUD 사거리 안내 시각 등은 double 월드 시간을 읽고 float timestamp로 내린다. 월드 누적기만 보고 모든 timer 필드를 double로 바꾸면 원본의 반올림·비교 순서를 바꾼다.

HD의 31개 소비 함수는 무기 발사·재장전·AI, 탄환과 공습, 적/BOT 대기·회복·재생성, 아이템 조회·표시, trail 수명, 메시지·사거리 안내를 포함한다. `_gameTime` 직접 참조는 tick·초기화·getter·쉬운 이동 입력·플레이어 처리다. 전역 시간은 별도로 title·base UI·버튼·fade·particle·camera shake·월드 상태 전환 timestamp에 쓰인다. 함수별 목록은 JSON에 보존했다.

**개발 기준:** HD 월드 누적기와 각 timestamp 필드의 정밀도를 공통 코어에 유지한다. 월드 pause gate가 참이면 월드 시간을 멈춘다. 전역 UI 시간과 host 실시간을 별도 값으로 관리한다. `0.0166f`를 임의로 `1/60`이나 실제 경과 시간으로 치환하지 않는다.

남은 정적 확인은 일반판의 register-offset·간접 alias와 manager 실시간 필드의 전체 후속 소비자다. literal offset이 겹친 53개 함수를 “미구현 시간 기능 53개”로 취급하지 않는다. 다른 객체의 scale·index 후보까지 포함한 검색 잔여다.

## 저장: 직접 caller 11개를 사건에 연결했다

| 사건 | 일반판 | HD | 확인한 조건 |
|---|---|---|---|
| 유효 슬롯 선택 | `0x1a0b0` | `0x17be0` | working slot·성장/HP 처리를 마친 뒤 저장 |
| 기지 화면 진입 | `0x6b78` | `0x5644` | 기지 이름 보정·화면 구성 뒤 저장 |
| 슬롯 삭제 수락 | `0x3de68` | `0x5468` | 유효 슬롯의 사용 flag를 지우고 표시를 갱신한 뒤 저장 |
| 전투 시스템 메뉴 | `0x2ca98`, ID `0x408` | `0x281e0`, ID `0x407` | HD는 `world.isSave`가 참일 때 저장. ID를 같은 값으로 합치지 않는다. |
| 맵 설정 완료 | `0x5a5dc` | `0x13778` | HD는 `_isSave`가 참일 때 저장 |
| 게임오버 진입 | `0x55cf4` | 같은 직접 writer caller 없음 | 일반판은 state가 4가 아니면 4로 바꾸고 저장한 뒤 파괴 알림을 구성 |

HD의 임무 수락·완수·보상/EXP 관련 진입 5개도 확인했다. 그 본문은 기존 5개 외의 직접 `saveDataFile` caller가 아니다. 따라서 임무 보상을 받는 매순간 디스크에 쓴다고 가정하지 않는다. 실제 보존은 위 저장 사건과 이어서 검사해야 한다.

일반판 무기 tick `0x23d70`, 탄환 초기화 `0x8c64`, 탄환 tick `0x8584`, 충돌 `0x8910`에서 **직접 immediate BL/BLX 그래프**를 탐색했다. 도달한 기존 본문 수는 각각 317·74·49·45개이며 `save 0x17f80`으로 이어지는 경로는 없었다. 이 검사는 virtual call·function pointer·Objective-C dispatch를 포함하지 않는다. 이를 전체 사격 경로의 부재 증명으로 확대하지 않는다.

HD checksum도 원작 ASM에서 확인했다. 저장 `0x17580`은 plaintext offset `4..0x6e27`의 바이트를 부호 있는 값으로 더한다. `0x1764e sxtb`로 시작하고 `0x1766a sxtab`로 누적한 뒤 `0x17690`에서 offset `0x6e28`에 32비트 합계를 쓴다. 로드 `0x16d8c`는 복호화한 바이트를 `0x16e2a sxtab`로 더하고 `0x16e34/0x16e36`에서 마지막 word와 비교한다. 합계가 같을 때만 상태를 복사하며, 다르면 invalid-save 분기로 간다. 복원 C++에서 원작을 역으로 추정한 근거가 아니다. 손상 저장의 실제 실행 검사는 하지 않았다.

**개발 기준:** 두 기기는 HD의 28,204바이트 저장 구조·부호 있는 바이트 합계 checksum·seed/XOR 변환·쓰기 조건을 공유한다. 일반판의 26,344바이트 형식을 iPhone 규칙으로 추가하지 않는다. 총알·프레임마다 DB나 파일에 쓰지 않는다. 임무 수락·보상 처리와 디스크 flush를 같은 사건으로 임의 통합하지 않는다.

## 난수: 저장·테이블 로드가 게임 RNG를 소비한다

정적 본문의 직접 API 횟수는 일반판 `random` 75회/34함수, `srandom` 5회/4함수다. HD는 `random` 91회/34함수, `srandom` 5회/4함수다. 이는 실행 횟수가 아닌 본문에 존재하는 호출 지점 수다.

HD 맵 진입 `0x13778`과 저장 `0x17580`은 `mach_absolute_time`으로 seed를 다시 잡는다. 일반판은 초기화 `0x52e8`에서 host 시간으로 seed를 잡고, 저장 `0x17f80`은 기존 RNG의 `random`에서 저장 seed를 고른다. 양쪽 EncTbl reader는 `0xb27f7`로 seed를 잡아 테이블을 푼다. load/save도 `random/srandom`을 사용한다. 전투·AI의 random 호출과 별개 RNG라고 가정할 수 없다.

**개발 기준:** 공통 HD 규칙에서 RNG 호출 순서와 저장/맵 전환의 seed 경계를 보존한다. 기기별 RNG 구현을 만들거나 테이블 decoder가 전역 RNG에 주는 영향을 설명 없이 바꾸지 않는다. 재현 검사는 seed뿐 아니라 사건 순서·호출 count도 기록한다.

## 효과음: cache ID 재사용과 library loop를 구분한다

양쪽 전체 캐시에서 `alGenSources`를 직접 부르는 본문은 각각 1개다. 일반판 `getSound 0x49aec`의 filename map miss는 `0x49930`으로 간다. HD `SoundManager::getSound 0x423a8`의 dictionary miss는 `0x41c20 → 0x40cf4`로 간다. hit는 기존 ID를 재사용한다. 원작의 이 경로는 filename마다 한 source ID를 반환한다.

일반판 `0x482d8`과 HD `0x40aa8`은 같은 ID의 기존 source를 stop한 뒤 nonloop 경로에서 play한다. 이 경로에 round-robin source pool 선택은 보이지 않는다. 같은 파일을 여러 번 발사하는 것과 서로 다른 파일들을 동시에 재생하는 것은 별도 조건이다. 여러 총이 발사된다는 사실만으로 원작이 같은 효과음을 여러 source에서 중첩했다고 판단하지 않는다.

별도의 loop branch는 있다. attack buffer를 queue하고 worker를 시작한 뒤, processed attack buffer를 기다려 unqueue하고 `AL_LOOPING`을 켠다. 다만 공통 load 경로에서 HD `0x41c20`은 loop flag 0을 constructor에 전달하고, 일반판 `0x49930`도 flag를 0으로 초기화한다. library에 loop 코드가 있는 것과 게임이 그 branch를 실제로 사용하는 것은 구분한다. flag의 다른 alias 쓰기와 engine 파괴 시 worker 수명은 아직 전부 닫지 않았다.

**개발 기준:** source·context 생성과 PCM decode를 사격 경로에 넣지 않는다. 원작보다 많은 동일 ID 중첩 재생을 만들었다면 source pool은 복원의 동일 동작이 아닌 확장으로 기록한다. 효과음을 AVAudioPlayer/AVAudioEngine으로 바꾸지 않는다. BGM loop를 효과음 loop와 혼용하지 않는다.

## BGM callback pointer 4개를 닫았다

원본 HD SHA `5feb7ae5…f5090`의 `__DATA`는 VA `0x51000`, file offset `0x50000`이다. 이 매핑으로 4바이트 word를 읽고 Thumb bit를 제거한 주소를 cache ASM·근거 ID와 연결했다. REA 메모리 읽기 ID로 포장하지 않고 로컬 파일에서 얻은 파생 근거로 기록했다.

| pointer VA | 원본 word | 대상 | 역할 |
|---|---|---|---|
| `0x5deb0` | `0x416d5` | `0x416d4` | AudioQueue output callback |
| `0x5deac` | `0x41f3d` | `0x41f3c` | queue isRunning listener |
| `0x5dea8` | `0x40ca9` | `0x40ca8` | pthread entry |
| `0x5deb4` | `0x41579` | `0x41578` | effect loop worker |

output callback은 EOF의 다음 파일·loop·정지·형식 변경 분기를 갖는다. `isRunning` listener는 형식 전환 정지가 끝난 조건에서 기존 queue를 dispose하고 queue·buffer를 다시 만든 뒤 prime/start한다. 이 BGM 전환을 매 사격의 OpenAL context 생성과 합치지 않는다.

buffer 설정에는 0.5초 크기 요청, `0x4000` 최소·`0x10000` 최대와 packet-size 예외, 짧은 파일 1개/streaming 3개 buffer가 있다. 이 수치는 실제 지연 측정값이 아니다. EOF·format transition helper 전부의 ABI·rounding 경계와 callback 중단 경쟁은 후속 확인 대상이다.

## 앱 중단·재개: 아직 실행으로 확인하지 않은 경계

원본 undefined-symbol table은 일반판 ARMv7 266개, HD 262개다. 양쪽에 `AudioSession`, `AVAudioSession`, `AudioQueuePause`, `alSourcePause` import가 없고 전체 pseudo에서도 해당 직접 호출을 찾지 못했다. 동적 함수 조회·selector 호출·OS 동작까지 없다는 뜻은 아니다.

HD `SoundManager::setPause 0x42268`은 dictionary를 release하고 null로 만든다. `setBackground 0x42364`와 `dealloc 0x42318`은 engine 파괴로 간다. 전체 캐시에서 no-argument `setPause`, `setBackground`, `removeAllSounds` selector caller는 찾지 못했다. 월드의 `setPause:`는 다른 selector다. 메서드 이름만 보고 실제 background audio 정책이 완성돼 있다고 말하지 않는다.

일반판 앱 resign/active callback은 displayLink를 stop/start한다. HD 같은 callback은 return한다. 이 차이가 실제 OS에서 효과음·BGM·게임 시간에 주는 결과는 이번에 실행하지 않았다.

**개발 기준:** 공통 Apple adapter에서 interruption·route change·background·resume 사건을 명시한다. game state와 cache ID를 보존하거나 재구성하는 시점, source/context 수명, BGM queue restart 순서를 함께 검사한다. world pause를 audio pause로 대신하지 않는다.

## 구현 뒤 필요한 동시 검사와 마지막 원작 대조

이번 조사만으로 아래 검사를 통과했다고 기록하지 않는다. 정식판과 치트판은 같은 HD 코어를 쓴다. 서로 배타적인 화면은 독립 상태에서 동시에 검사하고, 전투 안에서 함께 성립할 조건은 같은 clock에서 함께 실행한다.

| 검사 묶음 | 기록할 값과 기대 |
|---|---|
| 시간·일시정지 | world double, float timestamp, frame/UI clock을 각각 기록. pause 동안 world 누적 0, UI clock 정책은 별도. 긴 시간 뒤 reload·BOT 회복·AI·toast 경계를 확인 |
| 전투·디스크 쓰기 | 서로 다른 무기·탄환·공습·BOT을 동시에 실행. 사격·프레임별 파일/DB write 0. 저장 사건별 writer count와 호출 이유를 기록 |
| 임무·보상·저장 | 수락·이미 충족된 수집·격파·보상·기지/맵/메뉴 전환을 독립 상태에서 함께 검사. 게임 종료/재시작 뒤 지급·수락·완료 상태 보존 확인 |
| 동일 ID·서로 다른 ID 음향 | cache hit/miss, PCM decode, source/context 생성 수, stop/play ID·시각을 기록. warmed battle에서 생성/재decode 0. 동일 ID 재발사와 다중 ID 재생을 분리 |
| BGM queue | 짧은 파일/긴 stream, 정상 EOF·다음 파일·loop·형식 변경·전환 중 stop을 분리. callback이 해제된 queue/buffer를 참조하지 않아야 함 |
| 중단·재개 | 실제 background/foreground, OS interruption·route 변경을 확인. 복귀 후 중복 clock·source/context 누수·stale sound ID·BGM 중복 재생 없어야 함 |

정적 미확정은 위 실행 전에 가능한 범위까지 추적한다. 구현과 동시 치트 검사를 마친 뒤 마지막 일괄 대조에서 원본 HD/iPad를 기준으로 게임·저장·음향을 확인한다. 일반판/iPhone은 표시·입력 기준으로 사용한다. 한 기능마다 두 앱을 번갈아 빌드·플레이하며 개발하지 않는다.

## 근거와 재현

- [기존 시간·저장·음향 관측](standard-hd-logic-differences-20261113.md), [패키지 차이](universal-package-differences-20261113.md)를 보존한다. 이 후속은 기존 문서를 덮어쓰지 않는다.
- 영구 재현 도구: [tools/audit_iphone_runtime_followup.py](../tools/audit_iphone_runtime_followup.py). 도구에는 메타데이터 parser·숫자·주소·검색 규칙·요약 관측만 있으며 원본 함수 본문이나 에셋은 없다. 원본·캐시를 수정하지 않는다.
- 로컬 결과·검사: 같은 폴더의 `findings.json`, `verification.json`.
- 재현 명령: `python3 tools/audit_iphone_runtime_followup.py`. 다른 로컬 경로에서는 `--analysis-root`, `--reference-root`, `--output`을 지정한다.
- 원본 IPA·실행파일·에셋·전체 pseudo 본문을 이 문서나 JSON에 복사하지 않았다. 원본은 저장소 밖에서 읽었다. 조회한 원본과 cache 파일의 SHA, 함수 주소, evidence ID만 연결했다.
