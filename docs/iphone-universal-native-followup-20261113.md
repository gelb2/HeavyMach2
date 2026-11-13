# iPhone 유니버설 설계를 위한 네이티브 후속 조사

2026-11-13. 일반판 2.0.1 ARMv7와 HD 1.0.3 ARMv7의 로컬 분석 기록이다. 새 게임 코드·에셋·앱 빌드를 만들지 않았다. 원본·포팅판·시뮬레이터·실기기를 실행하지 않았다.

추가로 확인한 것은 **일반판 카메라 배율의 실제 그리기 범위와 입력 역변환, HD 화면 캐시의 갱신 순서, 탄환 point 분기의 호출 순서, 적 잔해·BOT의 제거 경로, 기존 목록에서 빠진 vtable 진입 30개**다. 복원판은 HD 규칙을 쓰는 공통 코어·공통 에셋을 유지한다. 일반판의 다른 규칙을 iPhone 전용 게임 코드로 복제하지 않는다.

## 조사 입력과 실제 수행

| 항목 | 범위 |
|---|---|
| 일반판 원본 SHA-256 | `0fffcfc09cb797c04c70aa0a7b6d748adf7888d7b6583954b9a71fe1f448512a` |
| HD 원본 SHA-256 | `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090` |
| 재사용한 전체 목록 | 일반판 Ghidra 본체 1,077개·HD 1,300개. 양쪽 pseudo·ASM 기존 캐시 |
| 이번 참조 파일 재검사 | 일반판 43개 함수 + HD 38개 함수의 162개 pseudo·ASM 파일 |
| 누락 vtable 진입 추가 조회 | 30개 진입의 REA 명령 관측 459회. 28개는 해독, 2개는 Ghidra 해독 실패 |
| 해독 실패 보완 | 설치된 Apple LLVM 21.0.0의 Thumb 해석으로 `0xb318..0xb988`, `0xb988..0xbbfc` 주소 구간을 외부에 보존 |
| REA 종료 | 새 증거 bundle·snapshot을 외부에 저장하고 정상 종료 |

기존 1,077개는 Ghidra가 함수로 등록한 본체의 분모다. 실제 모든 진입의 완전한 분모로 취급하지 않는다. 30개 추가 진입에는 반환 stub 외에 실제 게임 함수도 있었다. 전체 export·명령 관측·역할 대응·전체 의미 동등성은 서로 다른 결과다. 이 문서는 전체 의미 동등성을 완료했다고 보고하지 않는다.

원문과 추가 ASM은 저장소 밖 `/Users/sokol/Documents/Codex/HeavyMach2/Analysis/Phone201/iphone-universal-followup-20261113/`에만 저장했다. 저장소에는 주소·필드·요약·해시·증거 ID만 기록한다. [기계 판독 근거](evidence/iphone-universal-native-followup-20261113.json)와 [앞선 조사](standard-hd-logic-differences-20261113.md)를 함께 읽는다.

## 1. 일반판 줌은 월드 layer에 적용되고 HUD 앞에서 해제된다

확인된 호출은 world tick `0x5af50` → 목표 배율 계산 `0x57124` → 경계 계산 `0x5a418` → manager 배율 저장 `0x4e07c` → 실제 draw `0x4ef90`이다. 배율 저장만으로 시각 효과를 추측한 상태에서 더 나아가 OpenGL 소비까지 연결했다.

draw `0x4ef90`은 matrix를 보존하고 manager `+0x68/+0x6c`로 `glTranslatef`, `+0x0c`로 `glScalef`를 호출한다. layer를 순회하다 manager `+4`의 경계에 도달하면 matrix를 복구한다. 전투 화면 진입 `0x64e8`은 이 경계를 **9**, 기지 화면 진입 `0x7004`는 **−1**로 설정한다. 이 전투 경로에서 layer 0..8은 배율을 받고 9 이후 HUD는 같은 배율을 받지 않는다. `0x57ac8`의 적은 layer 3, `0x58738`의 BOT은 layer 5로 등록된다.

일반판 월드 좌표 함수 `0x4bebc`와 manager 입력 `0x4e0e4` → world 입력 `0x5a1dc`도 연결했다. 기호를 `p=월드 위치`, `c=카메라 중심`, `h=half-screen`, `o=화면 offset`, `s=배율`로 놓으면, 연속 좌표의 수학적 요약은 다음과 같다.

- 월드 표시: `screen = h + s × (p − c − o)`.
- 월드 입력: `p = (screen − h) / s + c + o`.

이는 원본의 연산 재배치 허가가 아니다. `0x4bebc`의 fixed-position 반올림은 GL 배율 적용 전에 수행된다. 반올림을 배율 뒤로 옮기거나 `(1−d)+1`을 임의로 재결합하면 같은 부동소수점 결과를 보장하지 않는다. 카메라 배율은 앞선 조사대로 목표 `clamp((1−d)+1,1,1.6)`와 현재값 접근 단계가 분리돼 있다.

HD의 실제 기본 경로는 `QobCamera` half-screen·offset과 `QobBase::screenPosX/Y` `0x37958/0x37870`, 월드 입력 `0x12698`, manager draw `0x3a39c`로 연결된다. 해당 화면 좌표는 `p−c+h−o`, 월드 터치는 그 역변환을 사용한다. HD `processCamera` `0x11cac`과 이 경로에 일반판의 layer-prefix 줌은 보이지 않는다. HD 전체 pseudo 1,300개의 `_glScalef` 텍스트 검색도 0건이었다. 이 검색만으로 모든 간접 GL 호출과 수동 정점 변환에서 줌이 절대 불가능하다고 단정하지 않는다.

**설계 조건:** 공통 HD 표현에서 월드 밀도·HUD 배치·입력 변환을 별도로 정의한다. 일반판 화면 한 장의 기체 크기와 HD 기체 크기를 고정 비율 하나로 연결하지 않는다. 원본 일반판의 배율·표적·경로 상태가 함께 기록된 측정만 비교 기준으로 사용한다.

## 2. HD 화면 캐시는 gameplay 읽기 뒤에 갱신되는 경로가 있다

HD 전체 캐시의 `isInScreen` 명시적 참조는 다음 7개 본체에서 확인했다.

| 본체 | 소비/쓰기 |
|---|---|
| `QobBase::init` `0x36f04` | 초기 캐시를 1로 설정 |
| `QobBase::tick` `0x36d98` | show·visual·remove 조건 뒤 `isInCam` 결과를 캐시에 쓰고 그리기 등록 |
| getter `0x3689c` | 캐시 반환 |
| `QobImage::tick` `0x38e50` | 기존 캐시를 atlas 준비 조건으로 읽음 |
| `GobHvMach::processMove` `0xd6d8` | 항공 예외 뒤 지상 이동을 제한 |
| `GobWorld::checkBulletCollideX:Y:` `0x10ddc` | 적 HP·화면 캐시·point 검사 |
| `GobBullet::setCollide:` `0x71d0` | 범위피해의 적/BOT 순회에서 검사 |

`GobHvMach::tick` `0xcfa4`는 상태에 따라 `processMove` 또는 `processCrash`를 부른 뒤 마지막에 `QobBase::tick`으로 간다. `QobImage::tick`도 atlas 조건에서 기존 캐시를 읽고 마지막에 base tick을 부른다. 따라서 **해당 객체의 gameplay/atlas 읽기와 자신의 캐시 갱신 순서**가 확인됐다. 새 프레임 시작에 모든 화면 캐시를 먼저 갱신하면 이 순서를 바꿀 수 있다. 초기값 1도 무시하지 않는다.

HD world tick `0x110d8`은 pause일 때 월드 시간·주인공 처리를 건너뛰지만 `processCamera`와 base tick은 호출한다. spawner tick `0x7e20`에는 별도의 pause 분기가 없고 world time·거리·카메라 조건이 있다. pause를 이유로 월드 트리 전체를 일괄 반환하는 변경은 이 호출을 없앨 수 있다. 실제 pause 중 생성 여부는 도달한 시간·카메라 조건을 함께 주는 동시 시나리오로 확인해야 한다.

일반판의 `+5` 자기/부모 활성 flag와 HD의 `_isInScreen`은 구분한다. 일반판 setter 호출 150회/65개 함수의 모든 객체 alias까지 판정한 것은 아니다. register-offset·넓은 store·간접 호출·디컴파일 제외 블록의 전체 연결은 여전히 남는다.

## 3. point·즉발·범위피해를 한 경로로 합칠 수 없다

HD 탄환 tick `0x6464`에서 `_collideCheck`가 참이면 `_toPlayer`로 충돌 helper를 고른다.

| 경로 | 실제 확인한 순서와 조건 |
|---|---|
| 주인공 쪽 탄환의 point 검사 | `0x10ddc`: 적 목록 → HP>0 → `isInScreen` → `isInPtX:Y:` |
| 적 탄환의 point 검사 | `0x10dac`: 주인공 `isInPtX:Y:`. 해당 본문에는 앞 행의 HP·화면 검사와 BOT 순회가 없음 |
| point hit 뒤 | tick에서 `setCollide(1)` → 맞은 대상의 `onDamage(dmg)` → hit 효과 |
| 비행 종료 거리 | tick에서 `setCollide(0)` |
| impact Range 0 | `0x71d0`의 별도 적 순회·반지름·고정피해 분기. 화면 검사가 없음 |
| impact 범위피해 | 같은 함수의 발사자별 분기. 적/BOT 순회에는 화면 검사. 주인공 처리는 별도 |

일반판 weapon tick `0x23d70`의 type 0/6 즉발과 비행 탄환 `0x8c64` → `0x8584` → `0x8910`은 공통 `0x550d0/0x5504c` 피해 경로로 이어진다. 이 helper는 반지름+Range 기반 감쇠를 쓰며 HD처럼 같은 함수 안에서 Range 0을 별도 고정피해 분기로 나누지 않는다. 이 차이의 원인이 기기인지 판본인지는 확정하지 않는다. 공통 복원 규칙은 HD를 따른다.

**남은 활성 조건:** HD `GobBullet::init` `0x5fd0`은 `_collideCheck=0`으로 초기화한다. setter `0x5f94`는 존재하지만 전체 캐시에서 이 selector의 호출부를 찾지 못했다. 원본 raw metadata에서도 `setCollideCheck:` 문자열 `0x46e9c`를 가리키는 32-bit 값은 `__objc_const`의 메서드 선언 1개였고 `__objc_selrefs` 항목은 없었다. 이는 직접 정적 경로에서 켜는 곳을 찾지 못했다는 근거다. 외부 호출·동적 selector 생성·alias 쓰기의 전역 부재 증명은 아니다.

따라서 point hit와 impact를 조합한 실제 피해량, 특정 무기에서 이 flag가 켜지는지, 추가 `onDamage`가 실제로 중복되는지를 실행 전 확정하지 않는다. 소스에 존재하는 분기와 플레이 도달 조건을 구분한다. 이 flag를 켜는 새 정식 UI나 무기 규칙을 임의로 만들지 않는다.

## 4. 적 생성·잔해 제거·BOT 사망

일반판 맵 reader `0x58038`의 kind 4는 short 좌표를 ×0.5한 뒤 `0x57ac8`에서 적을 즉시 생성한다. 퀘스트 추가 적 `0x57bb8`은 별도 직접 호출자다. HD는 `readSpawner` `0xff18` → `addEnemySpawner` `0x10a00` → spawner tick `0x7e20`의 시간·개수·x/y 거리 각각 1024 미만·재생성 시 카메라 밖 조건을 사용한다. 일반판에 다른 재생성 경로가 전혀 없다고 주장하지 않는다.

추가 확보한 Enemy 진입 `0xb318`은 base crash `0x10ce4`를 부른다. ASM `0xb3ae..0xb3d6`은 world time `+0x48`을 읽고 **현재 시간+3+난수 항×2**를 float32 deadline `+0x104`에 저장한다. 기존 process-crash `0xbd7c`는 이 deadline을 world time과 비교한 뒤 vtable `+0x34` → `0xe4f8` → `0x4c2cc`로 제거를 요청한다. `0x4c2cc`가 remove byte `+7`을 세우고 트리 `0x4c5e4`가 이후 제거한다.

HD Enemy `onCrash` `0x982c`도 crash deadline을 정하고 spawner의 `setEnemyDead`를 부른다. `processCrash` `0xa998`은 deadline 경과 후 `GobHvMach::onDestroy`로 간다. 해당 deadline·제거 본문에는 화면 캐시 조건이 없다. 지상 이동의 화면 gate를 잔해 제거에도 공통 적용하는 변경은 원본 경로를 합치는 것이다. 일반판 world time은 float32, HD는 float64이며 HD deadline은 float32로 저장하므로 수치 형식까지 분리해 보존한다.

BOT 사망은 일반판 `0xa3f0`과 HD `0x94f8`에서 base crash → 저장 슬롯의 활성/HP 값 정리 → destroy 역할이 대응한다. 일반판은 vtable `+0x34`를 거쳐 위 remove 흐름으로 간다. HD `onDestroy` `0xc7d0`도 `QobBase::remove` `0x37218`로 가서 remove flag를 세우고 자식에도 제거를 전달한다. 이 본문에서 “카메라 밖에 있으면 BOT을 제거한다”는 분기는 찾지 못했다. 이는 BOT의 맵 전환·저장 재개·모든 간접 상태까지 동등하다는 뜻은 아니다.

## 5. 누락 vtable 진입 30개를 보완한 결과

REA exact instruction 조회로 28개 진입을 해독했다. 가상 table 위치와 각 명령의 Evidence ID를 보존했다. 동일한 byte를 함수 본체가 아니라 파일 offset으로 재해석해 얻은 pseudo는 사용하지 않았다.

- `0x23ba4/0x7924/0x84bc`는 즉시 반환 명령이다.
- `0x7928/0x792c`는 0 반환, `0x7930`은 byte `+6` 저장 후 반환이다.
- `0x39e14`는 GuiCommandCenter slot 3의 실제 상태 전환 tick이다. `+0x54/+0x58` 비교와 `0x37d6c/0x39b7c` 호출을 가진다. UI 담당자에게 별도 외부 ASM을 전달했다.
- 나머지는 클래스별 가상 wrapper·소멸·초기화 역할을 가진다. table slot과 관측 명령을 보존했으며 모든 역할 이름을 추정해 붙이지 않았다.

Enemy `0xb318`(slot 12)과 `0xb988`(slot 11)은 Ghidra Listing이 해독하지 못했다. 원본 가상 포인터 `0xb319/0xb989`의 Thumb bit와 REA `read_bytes`의 실제 prologue를 확인하고 설치된 LLVM을 Thumb 모드로 사용했다. `0xb318`은 base crash·보상/드롭 쪽 호출, `0xb988`은 base damage `0x106e4` → 표적 설정 `0x10a58` 및 선택 대상 HP 표시 갱신을 가진다. **두 함수는 반환 stub이 아니다.**

LLVM 산출물은 주소 구간의 해석이며 전체 예외 CFG·타입·고수준 함수 의미를 인증한 결과가 아니다. literal pool과 예외 처리 블록이 함께 있으므로 선형으로 읽은 모든 행을 실행 코드라고 합산하지 않는다. 28개 명령 walk 역시 관측 그래프이며 디컴파일·함수 전체 동등성으로 바꾸지 않는다. 새로운 Ghidra 함수 정의나 원본 바이트 수정은 하지 않았다.

## HD 공통 구현에 채택할 조건

1. 모든 플랫폼에서 HD 게임 규칙·에셋 reader·파츠 조립·저장·음향 경로를 공통으로 쓴다. 일반판 관측은 화면·입력 설계의 비교 근거로 쓴다.
2. 월드 표시와 HUD를 같은 배율로 늘리지 않는다. 표시 viewport·카메라·고정 UI 좌표·hit 영역을 연결하고 역변환을 같은 데이터에서 만든다. 실제 파일 픽셀 크기와 논리 크기를 구분한다.
3. HD `_isInScreen` 초기값·갱신 시점·소비자 예외를 유지한다. 화면 비율을 바꿀 때 이동·피해·생성 조건도 달라지는지 별도 증거로 확인한다. 표시 범위 변경만으로 활성 범위의 동등성까지 확인됐다고 간주하지 않는다.
4. point hit·Range 0·범위피해·적 탄환·항공 이동의 예외를 그대로 분리한다. 원본에서 활성 출처를 못 찾은 flag는 정식 기능으로 임의 노출하지 않는다.
5. 시간·난수·피해의 자료형과 연산 순서, deadline 저장 형식, 원본 update 순서를 보존한다. 원본에 보이는 순서를 편의상 재정렬하지 않는다.

## 남은 조사와 마지막 일괄 검증 조건

- [ ] 일반판 show setter 150회/65함수의 대상이 적/BOT 자신·부모·파츠·UI 중 무엇인지 연결한다. register-offset·넓은 store·간접 쓰기와 제외 블록을 함께 확인한다. 현재 인덱스만으로 전역 부재를 주장하지 않는다.
- [ ] 두 Enemy ASM의 예외 분기·modifier·드롭·보상 소비를 호출자/데이터와 연결한다. 추적한 base crash/damage 역할을 함수 전체 동등성으로 바꾸지 않는다.
- [ ] HD `_collideCheck`를 실제로 켜는 도달 경로가 있는지 확인한다. 필요하면 마지막 검증 단계의 내부 계측에서 flag writer·weapon ID·호출 지점을 함께 기록한다. 정식 UI를 새로 만들지 않는다.
- [ ] 기존 일반판 측정의 카메라 현재/목표 배율·표적·경로 상태를 보강한다. 지금은 별도 원본 실행을 하지 않았다.
- [ ] iPad HD 포팅판 기준에서 화면 안/밖 경계, 항공/지상, pause/비pause, point flag 0/1, Range 0/양수, 플레이어/적/BOT 피격, initial/regen/보스 생성, 적 잔해 deadline·BOT 사망을 동시에 구성한다. 한 world-step 추적에 순서·자료형·상태 변화·난수 소비·사운드 이벤트를 함께 기록한다.
- [ ] 공통 정식판·치트판의 구현을 마친 뒤 포팅판·정식판·치트판을 마지막에 일괄 대조한다. 이후 iPhone·iPad는 같은 공통 코드에서 각 viewport의 표시·입력 회귀를 확인한다. 구현 중 기능 하나마다 포팅판/기기를 오가는 절차로 되돌리지 않는다.

## 금지할 오류

- iPhone 전용 게임 로직·별도 원본 에셋 계열을 추가하거나 일반판 규칙과 HD 규칙을 기기마다 나누는 것.
- 화면 전체 확대, HUD까지 월드 줌 적용, draw와 다른 hit 변환, retina 계수 중복 적용.
- `isInScreen`을 전 프레임 공통 선행 계산으로 옮기거나 모든 이동·피해·생성에 같은 gate를 붙이는 것.
- 원본 helper의 조건을 이름만 보고 합치거나, 못 찾은 setter·버튼·활성 조건을 임의로 만드는 것.
- crash deadline·world time·RNG·FP 연산 순서를 편의상 바꾸는 것.
- 사격별 DB/파일 쓰기, 사격마다 음향 context 생성·해제, 원본 효과음/BGM 절차를 대체하는 것.
- 함수명·ABI·byte 차이·캐시 파일 개수를 의미 동등성 검사나 실제 플레이 검사라고 보고하는 것.
- 원본 IPA·실행 파일·전체 pseudo/ASM을 저장소나 외부 보고 사이트에 올리는 것.

## 보존 위치

추가 REA 최종 bundle은 외부 `rea-evidence-final.json` 2,624개 기록이다. 이전 일반판 snapshot에서 재사용한 기록도 포함하므로 이 수를 새 함수 수로 부르지 않는다. 최종 snapshot은 `rea-analysis-final.json`이다. LLVM 구간 원문과 `llvm-span-manifest.json`도 같은 외부 폴더에 있다.

재현 명령은 `python3 tools/audit_iphone_native_followup.py`다. 원본 SHA·기존 export manifest·파일 SHA·명령 Evidence ID·LLVM 구간 SHA를 다시 검사하고 정적 검색 인덱스를 만든다. 앱 실행과 코드 의미 동등성을 자동 판정하는 도구가 아니다.

메타데이터 인덱스는 `.local/evidence/iphone-universal-native-followup-20261113/`에 있다. `cached-function-reference-index.json`, `vtable-entry-walk-index.json`, `vtable-entry-verification.json`, `hd-collidecheck-selector-static-index.json`, `verification.json`을 재생성할 수 있다. 조사·코드 채택·실제 실행 완료를 구분해서 기록했다.
