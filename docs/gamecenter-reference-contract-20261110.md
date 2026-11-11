# GameCenter 인증·오류 콜백 추가 조사

2026-11-10. HD 원본 SHA256 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`의 로컬 정적 분석이다. 기존 근거를 먼저 사용하고 부족했던 함수·참조·선택된 block metadata/명령만 REA Ghidra로 추가 확인했다. 실제 앱·게임계정 인증·점수 업로드는 실행하지 않았다. 제품 소스·정본 문서·Native112 조사는 변경하지 않았다.

## 기존 계획에서 정정할 점

`GlobalInfo.reportScore`가 읽는 전역값을 기존 계획에서 ‘Game Center 가용 flag’라고 불렀다. 이번 writer 대조로 이 해석을 정정한다. **이 값은 Mach-O 암호화 정보의 cryptid 조건**이다. OS 지원 여부와 인증 상태는 각각 별도 경로다. 상위 plan.md/consumer-matrix.json은 이번 읽기 전용 소유 범위 밖이므로 직접 수정하지 않았으며, 이 후속 명세가 해당 용어를 보완한다.

| 구분 | 실제 위치·함수 | 원본의 의미 |
|---|---|---|
| 점수 제출 전역 gate | `PTR 0x51374 → byte 0x5ddac` | main(0x242c)이 LC_ENCRYPTION_INFO의 cryptid가 nonzero이면1, 나머지는0을 기록 |
| OS 지원 검사 | GameCenterManager.isGameCenterAvailable(0x44b30) | GKLocalPlayer 클래스가 있고 systemVersion compare(options0x40,`4.1`) 결과가 -1이 아니면 true |
| 인증 상태 | GameCenterManager 객체 `self+4`, offset 변수0x5c16c의 값4 | authenticationChanged(0x44a58)가 localPlayer.isAuthenticated 결과로1/0 기록 |

세 값을 합치거나 하나를 다른 값의 별칭으로 사용하면 원본과 달라진다. reportScore 원본은 self+4의 isAuth나 isGameCenterAvailable을 직접 검사하지 않는다.

## 1. 전역 gate의 정확한 writer

main/FUN_0000242c는 dladdr로 자기 실행 이미지의 header를 얻고 Mach-O load command들을 순회한다. command==0x21을 찾으면 그 구조체 +0x10을 읽는다. nonzero면 byte0x5ddac=1, 못 찾거나0이면0이다. 이어 autorelease pool 생성→UIApplicationMain→pool release다. ASM의 실제 byte 쓰기는0x2486이다.

로컬 Apple SDK `mach-o/loader.h`는 0x21을 LC_ENCRYPTION_INFO로 정의한다. encryption_info_command의 다섯 번째 uint32가 cryptid다. 선택한 SDK 정의와 파일 SHA는 `sdk-encryption-info-excerpt.txt`, `local-decoder-verification.json`에 있다. 원본 참조 파일의 현재 cryptid는0이다. 따라서 이 참조 바이너리의 원래 reportScore 분기는 전역 gate에서 생략된다. 이는 복호화 참조 파일의 메타데이터에 관한 정적 결과이며 실제 서비스 실행 성공/실패 관측이 아니다.

0x5ddac에 대한 Ghidra 직접 참조는 main의 주소계산0x2480/byte쓰기0x2486, reportScore의 읽기0x15bd0, pointer0x51374다. 조사한 직접 writer는 main 한 곳이다. 간접 포인터의 모든 가능한 쓰기까지 증명했다는 뜻은 아니다. 암호화 상태를 검사하는 개발 의도는 이 코드만으로 단정하지 않는다. 원본 binary의 cryptid를 바꾸거나 이 gate를 인증 flag로 조용히 대체하지 않는다. 복원판의 서비스 정책은 원본 사실과 별도로 정해야 한다.

## 2. 지원 검사와 인증 호출 순서

GameMain.init(0x53b0):

1. QobBase.init→GameMain layer3→start(기존 데이터/World/UI 준비).
2. scrChangeState=8.
3. GameCenterManager.sharedMgr→isGameCenterAvailable.
4. true일 때 authenticateLocalPlayer를 먼저 호출한다.
5. 그다음 registerForAuthenticationNotification을 호출한다.

isGameCenterAvailable(0x44b30)은 NSClassFromString(`GKLocalPlayer`)와 UIDevice.systemVersion compare(`4.1`,options0x40)을 사용한다. 클래스가 없거나 비교 결과가 -1이면 false다. 네트워크·계정·서버·리더보드 category를 검사하지 않는다. false이면 위 두 인증 관련 호출만 생략되며 별도 게임 오류창/소리/저장 호출은 없다. 이 함수는 byte0x5ddac을 쓰지 않는다.

authenticateLocalPlayer(0x44afc)는 GKLocalPlayer.localPlayer→authenticateWithCompletionHandler(block0x5deb8)를 호출한다. 인증 완료를 기다리거나 결과를 게임 저장에 기록하지 않는다. 순서는 인증 요청→알림 observer 등록이며, 원본에 없는 ‘먼저 observer 등록’으로 바꾸어 분석 결과를 보고하지 않는다.

registerForAuthenticationNotification(0x44aa8)은 defaultCenter.addObserver(self,authenticationChanged,GKPlayerAuthenticationDidChangeNotificationName,nil)이다. notification 이름은 외부 심볼 pointer0x5138c를 통해 전달한다. observer selector는 selref0x5ce38이며 실제 참조0x44ad0을 확인했다.

## 3. 인증 성공·실패·취소

인증 completion block0x5deb8의 invoke pointer는0x44841(Thumb bit 포함), 실행 주소는0x44840이다. **첫 명령 bx lr로 즉시 반환한다.** 뒤 nop는 padding이다. 오류 인자를 읽거나 알림·재시도·save·SFX·isAuth 쓰기를 하지 않는다. 성공·실패·취소를 이 callback 안에서 구분하지 않는다.

별도로 authenticationChanged(0x44a58)가 통지를 받으면 localPlayer→isAuthenticated를 읽고 low byte가0이면 self+4=0, 아니면 self+4=1을 쓴다. isAuth getter(0x4485c)는 이 byte를 반환한다. offset metadata0x5c16c=4와 두 쓰기/한 읽기 참조를 확인했다. 이 함수에도 게임 알림·소리·저장·점수 자동제출은 없다.

따라서 인증 성공/실패/취소의 원본 게임 처리 계약은 다음과 같다.

- completion 호출만으로는 게임 상태가 바뀌지 않는다.
- 인증 변경 notification이 실제 도착하면 그때 OS의 isAuthenticated 값만 반영한다.
- 취소가 어떤 NSError인지, 그때 notification이 오는지, OS가 어떤 인증/오류 화면을 보이는지는 이 앱의 빈 callback으로 판별할 수 없다. OS 실행은 하지 않았으므로 미확인이다.
- callback에서 성공 값을 직접 넣거나 취소를 게임 실패 알림으로 바꾸는 것은 원본에 없는 처리다.

GameCenterManager.sharedMgr는 alloc/init을 사용하며 조사한 해당 클래스 메서드 목록에는 별도 init override가 없다. 명시 isAuth 초기화 쓰기는 찾지 못했다. 객체 최초0은 Objective-C allocation의 zero 초기화를 전제로 한 추론이며 별도 실제 실행값 관측이 아니다.

## 4. 점수 제출 completion과 오류

기존 GlobalInfo.reportScore(0x15bc0)의 조건은 **byte0x5ddac true && working slot 존재 && signed EXP(+0x70)>0**이다. category는 HM2HD_LEADERBOARD, 값은 현재 EXP다. 원래 조건에 인증 상태 검사는 없다.

GameCenterManager.reportScore(0x44978)는 GKScore.alloc→initWithCategory→autorelease→setValue→reportScoreWithCompletionHandler(block0x5def4) 순서다. score는 Objective-C ARM ABI의 r2/r3 두 word에서 보존하고 category는 호출자 stack 인자다. 앞서 decompiler가 C++ thiscall처럼 표시한 인자 이름으로 값의 의미를 바꾸지 않는다.

block0x5def4의 invoke pointer는0x4484d, 실행 주소0x4484c다. **이 본문도 bx lr로 즉시 반환한다.** 제출 성공/실패 분기, 게임 알림, 로그 함수, 수동 retry, 로컬 미전송 queue, 저장, best 변경을 하지 않는다. OS SDK/서버 내부의 캐시·재시도까지 없다는 뜻은 아니다. 그 내부 동작은 앱 바이너리의 범위 밖이다.

기존 5009의 reportScore→showLeaderboard→SFX0 순서와 SaveExit의 save→best 갱신→reportScore 순서는 그대로다. callback이 서버 성공 여부를 게임에 통지하거나 추가 save를 요청하는 경로는 없다. 5009의 OS 화면 표시 시도는 reportScore의 gate 결과와 별개다.

## 5. 익명 block 검증 방법과 한계

Ghidra 기본 분석은 0x44840/0x4484c를 함수로 정의하지 않았다. procedure_pseudo_code/assembly 조회 4회는 실패했고 오류 JSON을 보존했다. 이 실패를 decompile 성공으로 표현하지 않는다.

REA read_bytes로 읽은 두 global block의 invoke pointer와 선택된 28byte를 확인한 뒤, 같은 SHA 파일을 LLVM의 명시 `thumbv7-apple-ios` 모드로 독립 해독했다. 두 주소의 첫 명령은 모두 0x4770=bx lr, 다음0xbf00=nop다. `completion-thumb-decode.log`, 도구 버전, 명령/exit0은 `local-decoder-verification.json`에 있다. 실제 callback을 실행한 것은 아니다.

원본 앱에 대한 새 함수/명령 annotation, patch, 바이너리 쓰기는 하지 않았다. 이번 추가 REA 세션은 close했고 최종 open=false/cleanup_pending=false를 보존했다.

## 6. 구현·검사에 반영할 최소 기준

1. 서비스 지원 여부, 실제 인증 상태, 원본 metadata gate를 별도 값으로 정의한다. 기존 계획의 ‘가용 flag=true’ 한 변수로 구현하지 않는다.
2. 원본 init의 지원 검사와 authenticate→observer 순서를 CPU host spy로 검사한다. 지원 false일 때 게임 진행/저장/음향에 변화가 없어야 한다.
3. 원본 두 callback은 no-op임을 보존한다. 성공·오류·취소 인자를 넣어도 게임 수치/저장/SFX/알림이 추가되지 않아야 한다. 인증 상태 변경은 notification 경로로만 검사한다.
4. 5009 호출 순서와 reportScore의 gate/slot/signed EXP 조건, SaveExit의 save-first를 별도 검사한다. 복원판의 metadata gate 정책을 정하기 전 암호화 파일과 인증 상태를 혼동한 테스트 성공을 만들지 않는다.
5. 최신 OS API 대응·카테고리 등록/entitlement·실제 계정·네트워크 오류 화면은 후속 플랫폼 작업이다. 원본에 없는 오류 UI를 임의로 만들지 않는다. 서버 요청 호출과 실제 서버 수락을 구분한다.

## 조사 질문별 상태

| 질문 | 결과 |
|---|---|
| 기존 점수 gate writer | 확인: main0x242c,LC_ENCRYPTION_INFO.cryptid→0x5ddac |
| 별도 지원 검사 | 확인: GKLocalPlayer 클래스 + systemVersion4.1 compare |
| 인증 요청·observer 순서 | 확인: GameMain.init에서 요청 후 observer등록 |
| 인증 상태 writer·offset | 확인: notification handler0x44a58,self+4 |
| 인증 completion 성공/실패/취소 분기 | 확인: 분기 없음,즉시 반환 |
| 점수 completion 오류 UI/재시도/저장 | 확인: 앱 callback에 없음,즉시 반환 |
| OS 자체 인증·오류·취소 UI/notification 발생 양상 | 미확인: 실제 OS/계정 실행 안 함 |
| 실제 서버 수락·현재 OS/category 가용성 | 미확인: 네트워크·인증·업로드 안 함 |

원본 함수와 Evidence ID는 `finding-ledger.json`에, 모든 산출물 SHA와 실패/종료 상태는 `manifest.json`에 기록했다.
