# iPhone·iPad 런타임과 자원 수명 점검

2026-11-14 사용자 발열·생명주기 피드백에 따른 코드 점검과 수정 기록이다. 아래 수정은 공통 구현을 모은 뒤 실행 검사할 대상이다. 이 문서 작성 시점에는 새 앱·시뮬레이터·실기기·Instruments를 실행하지 않았다. 발열 원인이 확정되거나 해결됐다는 보고가 아니다.

## 확인한 문제와 수정

| 항목 | 확인한 코드/원작 근거 | 반영한 수정 |
|---|---|---|
| 백그라운드 복귀 후 메뉴/정지 없음 | 기존 Scene delegate는 inactive/active를 전달했으나 GameSession은 실행 여부와 입력만 바꿨다. 복귀할 때 다시 active가 되어 전투를 계속했다. | 공통 GameSession의 정지/기존 메뉴 유지 처리는 루트 담당자가 수정한다. Apple Scene/audio interruption은 독립 상태로 묶어 실제 전면 상태이며 중단이 끝났을 때만 active를 전달한다. active 복귀가 메뉴를 닫거나 전투를 자동 재개하지 않는다. |
| 반복 Scene/오디오 콜백 | willResignActive와 didEnterBackground가 연속으로 같은 inactive를 전달한다. | SourceView와 HM2Audio는 같은 상태의 반복 요청을 무시한다. OS 오디오 중단이 끝나도 Scene이 백그라운드면 활성화하지 않는다. AVFAudio는 중단 통지에만 사용하며 효과음/BGM 재생기를 대체하지 않는다. |
| 화면/월드 자원 수명 | 원본 GuiBase/GuiTitle은 별도 ResMgr_Tile을 소유하고 소멸 때 해제한다. MapLoading은 clearObjects→clearTileResMgr→setStage 순서를 사용한다. 이전 복원판의 화면/글자/GPU 캐시는 세션 동안 누적됐다. | Screens의 화면/숨겨진 GuiGame 소유 자원과 GameSession의 살아 있는/retired World 준비 목록을 모은다. 원래 자원 소유가 끝난 경계에서만 GPU 항목을 해제한다. 프레임마다 보이는 그림만 보고 캐시를 버리지 않는다. |
| 글자 캐시 | 원본 Texture2D(Text)0x42fa4는 UIFont·CGBitmapContext·UIGraphicsPushContext·NSString drawInRect 경로로 A8을 만든다. 현행 글자 준비에도 같은 종류의 경로가 있다. | 살아 있는 Text 노드가 shared bitmap을 소유한다. 캐시만 소유한 항목은 소유 경계에서 제거하고 관련 GPU 글자도 해제한다. 글자 ID는 단조 증가해 삭제 후 충돌을 피한다. UIKit 게임 버튼으로 바꾸지 않는다. |
| 효과음 로드/재사용 | 원본 SoundManager.getSound0x423a8은 파일명 dictionary miss 때만 로드하고 다음 호출은 SoundID를 재사용한다. 이전 정식 런타임은 시작할 때57 WAV를 모두 준비했다. | 정식/치트의 무조건57개 디렉터리 준비를 제거했다. 전역 메타데이터와 실제 화면 생성이 참조하는 순서로 같은 prepareEffect/prepareSound 캐시를 요청한다. 첫 이름 요청에서만 source/buffer를 준비하고 이후 사격은 재사용한다. |
| 사운드 컨텍스트 | OpenALEffects는 생성자에서 context를1개 만들고 소멸자에서 해제한다. play는 기존 source의 position→stop→play를 사용한다. setActive는 장치 pause/resume이다. | 컨텍스트를 사격·Scene 변화마다 만들거나 지우는 코드는 추가하지 않았다. 실제 첫 로드와 이후 반복 재생·중단/복귀의 context/source/buffer/read 수를 Native 검사와 최종 trace로 확인한다. |
| BGM 큐 | HD0x424d8은 같은 파일명을 다시 요청해도 이전 AudioQueue를 닫고 새로 연다. | 명시적 playMusic 호출의 이 동작은 유지한다. 백그라운드 pause/resume은 큐를 다시 여는 동작과 구분한다. OpenAL context와 BGM queue 수를 혼용하지 않는다. |

## 그림 출력과 남은 차이

정식/치트 Apple 런타임은 이제 각 공통 GameSession의 orderedStartupSounds 순서로 prepareEffect를 호출한다. GuiTitle의 새 소유가 생성될 때에는 원본0x32340대로 BGM 요청 뒤 Title_TitleIn.wav→Title_Bang.wav→Title_TapToStart.wav를 준비하며 재생하지 않는다. 같은 title의 scalar refresh에는 반복하지 않는다. 실제 HD 준비 파일의 읽기 전용 조사에서 WeaponInfo213행+22 SFX는235요청·고유46개다. 아래 추가 조사에서 확인한 GameMain의 Loading_Open→Loading_Close를 합치면237요청·고유48개이며, title3요청을 합친 시작 참조는 고유50개다. 디렉터리57 WAV를 자동으로 모두 SFX에 올리지 않는다. STAGE1/2/3와 Title은 BGM이며 `SFX_GameOver.wav` 등의 나머지 요청 경계는 별도로 추적한다. 참조 집합 조사와 실제 실행·디코딩 성공 결과는 구분한다.

추가 원본 조사에서 GlobalInfo.openWeaponInfo0x14bc4가 WeaponInfo의 원래 행 순서대로 fireSound를 getSound에 요청하고, GlobalInfo.init0x15ef4가22개 공통 SFX도 명시적으로 준비하는 것을 확인했다. 따라서 파일명 캐시의 lazy miss는 “정상 게임이 모든 효과음을 첫 실제 사격 때만 읽는다”는 뜻이 아니다. 디렉터리의57 WAV를 무조건 훑는 이전 동작과 전역 메타데이터가 실제 참조하는 소리를 원래 순서로 요청하는 동작을 구분한다. 같은 파일명 준비 API인 prepareEffect를 전역 참조 목록과 연결하고, 나머지 최초 요청·이후 재사용을 따로 검사한다.

첫 Native 동시 검사는 시작48개와 종료50개의 read/decode/source/buffer 차이 때문에 이전 전구간 동일 assertion에서 실패했다. 이 실패를 남긴다. 이후 원본 `GameMain.init0x53b0→start0x4f0c`를 읽어 GlobalInfo 뒤 Loading_Open→Loading_Close를 getSound에 요청한 사실을 확인했다. 기존 동일 SHA의 CFString 인덱스에서0x5e08c는 `Loading_Open.wav`,0x5e09c는 `Loading_Close.wav`였다. 원본의 명시적 시작 준비가 빠졌으므로 공통 런타임 시작 목록에 이 순서를 연결했다. 실패 때 늘어난 두 파일의 실제 이름은 기존 총계만으로 확정하지 않으며 후속 이름 기록으로 대조한다.

HM2Audio에는 실제 filename-cache miss 때만 이름·순서·성공과 read/decode/source/buffer 생성·삭제 변화, source/context ID를 메모리에 남기도록 추가했다. 캐시 hit/재생마다 기록하거나 파일을 쓰지 않는다. 동시 보고서는 준비 전후 이름과 추가 miss를 내보내고, Native 검사는 각 고유 첫 요청에서 read/decode/source/buffer가 각각1회인지 확인한다. 반복 재생에서 총 자원 증가량은 증명된 새 이름의 miss 수와 같아야 한다. 컨텍스트1개·같은 ID·source/buffer 삭제0·반복 cache hit의 새 읽기0 기준은 유지한다. 이 보강과 GameMain 소리 준비를 반영한 다음 Native 실행은 아직 하지 않았다.

인게임 화면·버튼은 공통 C++ UI/VisualLayer가 생성한 Sprite/atlas를 HM2Renderer의 GLES1으로 출력한다. 확인한 Apple 코드에서 UIKit UIButton/UIImageView로 게임 버튼을 새로 만드는 경로는 찾지 못했다. UIKit은 창·OS 터치·이름/확인 알림을 담당한다. 이 정적 범위를 전체 구현 감사·실행 완료로 확대하지 않는다.

CoreGraphics가 존재한다는 사실 자체는 위반 근거가 아니다. 원본의 글자 텍스처 준비도 CoreGraphics를 사용한다. 현재 글자 준비는 캐시 miss/내용 갱신에서 호출되며 그리기 메서드에는 rasterizer가 없다. 실제 준비 횟수·캐시 잔류·프레임 부하는 마지막 계측으로 확인한다.

원본 Texture2D.initWithImage0x42be4는8비트 컬러 또는 알파가 있는 그림에서 format1을 선택하고, 낮은 비트의 불투명 그림에는 RGB565 분기가 있다. 따라서 RGBA8 출력만으로 모든 활성 이미지에 불필요한 두 배 메모리를 쓴다고 단정할 수 없다. 원본1024 POT 제한·@2x 선택·픽셀 형식 전체 지원과 현재 공통 decoder의 범위 차이는 기존 그래픽 감사와 함께 추적한다. 현재 metadata 준비는 PNG IHDR를 읽고 GPU 준비는 같은 원본 인코딩을 다시 읽어 디코딩한다. 이 초기 읽기 횟수까지 원작과 같다고 주장하지 않는다. 반복 프레임의 새 로드/디코딩/업로드0과 원본 소유 수명 준수는 각각 확인한다.

동시 치트의 임시 메뉴 예열 화면도 소멸하면 자원을 해제한다. 이전의 세션 전체 캐시를 유지하기 위해 소멸한 예열 화면을 가짜로 살려 두지 않는다. 동시에 살아 있는 실제 조건과 보고서용으로 보존한 source-frame snapshot은 명시적 소비자로 유지한다. 경계의 재준비 비용과 준비가 끝난 동일 프레임 출력 비용을 나눠 보고하며, 과거 전체 구간 업로드0 가정과 실패 기록은 보존한다.

## 입력 종료와 진단 기록

원본 QobManager는 보여지는 receiver를 역순 호출하고 true에서 멈춘다. QobButton의 end는 false지만 원본 World는 paused 상태에서 EasyMove를 호출하지 않는다. 버튼의 반환값 하나를 근거로 모든 UI 이벤트를 World에 전달하지 않는다.

현행 GameRuntime에는 World에서 시작한 포인터의 종료를 UI/도움말이 소비할 때 해당 기존 World 소유만 cancel하는 좁은 정리를 추가했다. 메뉴 뒤로 새 이동/적 선택을 전달하지 않는다. 이는 별도 소유권 결함을 막는 경계 처리다. 사용자가 보고한 경로 재지정 후 점프 실패의 원인으로 확정한 것은 아니다. 원작의 작은 move가 탭 시각을 지우는 규칙과0.5초/SP100 조건은 바꾸지 않았다.

첫 실제 OS doubleTap 검사에서는 같은 canvas 위치의 move가 첫 탭 시각을 지워 두 경로 조건 모두 실패했다. 이후 Apple 경계에 raw UITouch current/previous CGPoint가 정확히 같은 moved만 제외하는 호환 정책을 추가했다. 이는 원본 EAGLView에는 없는 추가이며 양수의 작은 이동은 모두 원본 규칙으로 전달한다. DEBUG에는 제외 수와 실제 raw 좌표·시각을 남긴다. 아직 이 수정 뒤 앱 실행 결과는 없으며 사용자 물리 손의 실패율 개선을 확정하지 않는다. [입력 조사](iphone-path-jump-investigation-20261114.md)의 원본 근거·첫 실패·호환 수정 절을 함께 본다.

입력 최근256개는 메모리의 고정 배열에 기록한다. phase·포인터·화면/월드 좌표·Player 위치·경로 수·이전 탭 시각·World 시간·SP·점프 시작 수와 UI/overlay/World 소비 여부를 포함한다. 기록 때문에 파일·DB·콘솔을 매 프레임 쓰지 않는다. 끝난 뒤 명시적으로 조회한 결과와 실제 OS 입력을 연결해 실패 경로를 좁힌다.

실제 OS 입력 회귀 두 조건을 WorldInputTests에 추가했다. 독립 UUID 저장에서 실제 drag로 여러 경로를 만든 뒤 새 단일 경로 또는 여러 경로로 바꾸고, 실제 Equipment의 SP 충전과 이전 탭 간격을 기다린 뒤 XCUI doubleTap을 보낸다. 점프 시작 수 증가와 해당 실제 begin에서100 SP 소비, 마지막 포인터 해제를 확인한다. 결과나 tapTime/SP를 런타임에 주입하지 않는다. 공통 concurrent 초기 fixture를 그대로 helper로 옮겼으며 OS 시작 fixture만 저장 위치 문자열을 Stage01_01로 지정한다. 원작의 작은 move 규칙은 변경하지 않았다. 추가했지만 아직 빌드·실행하지 않았다.

DEBUG의 UUID UI-test 모드에서만 현재 점프/SP/경로/포인터 상태와 메모리256입력 JSON을 접근성 값에 제공한다. 입력 JSON은 실제 OS 입력 배치가 끝날 때만 만들어 재사용하며 파일에 쓰지 않는다. UI 검사는 이 기록을 XCTest attachment로 보존한다. 정식 Release에서는 fixture 생성과 접근성 값 생성 코드가 컴파일되지 않는다.

OS 입력 검사의 기기 방향은 iPhone 가로·iPad 세로로 나눈다. 각 검사의 시작과 종료에서 이 기준 방향을 설정하고 실제 canvas의 가로·세로 비례도 기기별로 검사한다. iPad에 iPhone 방향을 강제하지 않는다.

실제 백그라운드 회귀 `testOSBackgroundReturnKeepsMenuPaused`도 추가했다. 독립 UUID field에서 Home→앱 activate를3회 수행하고, 돌아온 뒤 시스템 메뉴/Resume1030 표시와12개 후속 display callback 동안 World 시간·Player 위치 정지 및 추가 저장 없음을 확인한다. 실제 Resume 터치 후에만 World 시간이 다시 증가해야 한다. 직접 setActive를 호출해 OS 전환을 대체하지 않는다. 전화 중단 재현은 이 검사의 범위에 포함하지 않으며 아직 실행하지 않았다.

## 독립 검토 후 보강한 자원 해제 경계

적 이름은 GameSession의 전체 presentationRevision을 바꾸지 않고 updateHUD에서 교체된다. 이전 조건대로 전체 UI/월드 재구성 때만 정리하면, 끝난 적 이름 bitmap/GPU가 다음 화면 전환까지 남았다. 또 UI 입력 callback 안의 중첩 synchronize 시점에는 Screens가 이전 Impl을 계속 보유해 아직 해제할 수 없는 Text가 있었고, callback 종료 뒤의 실제 해제를 별도로 관측하지 못했다.

Screens에 자원 소유 변경 generation을 추가했다. 글자 생성·메시지/지역명/적 이름의 교체와 해제, Impl의 마지막 참조 해제 때만 증가한다. 런타임은 이 값이 바뀌었을 때 정리하므로, callback 안에 아직 살아 있는 Text는 유지하고 바깥 입력/틱이 끝나 소유가 사라진 뒤 다시 해제한다. 같은 프레임의 무조건 전체 GPU 순회로 해결하지 않는다. 동시 치트도 같은 generation을 사용한다.

Native 회귀에는 실제 전장의 서로 다른 적 이름을 반복 선택해 이름 하나의 살아 있는 텍스처/bitmap만 남는지, 같은 이름 재선택 시 업로드를 재사용하는지, 시스템 메뉴를8회 열고 닫은 뒤 끝난 메뉴 Text가 남지 않는지 검사를 추가했다. 이 추가 검사도 아직 실행 결과로 판정하지 않는다.

## 마지막 검사와 계측 연결

- Native 검사에 Scene/오디오 중단 순서, normal lazy 이름 캐시,20회 반복 inactive/active에서 context/source/buffer 보존, GPU 소유 종료/재진입, World 포인터 종료를 메뉴가 소비한 경우를 추가했다. 추가만 했으며 아직 실행 통과로 표시하지 않는다.
- 실제 HD 리소스와 같은 HM2Renderer에서 기본 HUD와 세 조각의 사용자 배치 HUD를 원래 위치에 놓은 결과를 픽셀 단위로 비교하는 Native 검사를 추가했다. 912×420·857×393 및667×375의1.5배 drawable에서 RGB·alpha 차이와 기본 프레임의 재출력 안정성을 따로 검사한다. 편집 도구나 월드 그림은 비교에 포함하지 않는다. 아직 실행하지 않았으므로 분할 경계의 틈이나 중복 합성이 없다고 판정하지 않는다.
- 실제 시스템 메뉴의 도움말 버튼으로 GuiHelp를8회 만들고 닫으며 고유 소유 ID, 네 이미지의 준비·해제, 닫은 뒤 기존 GPU 항목 수 복귀를 검사하는 Native 회귀를 추가했다. 스칼라 synchronize 반복에는 새 업로드·삭제가 없어야 한다. 이 검사도 아직 실행하지 않았다.
- TextTests에 살아 있는 Text의 bitmap 유지와 캐시만 남은 bitmap의 경계 해제를 추가했다. TextTests 타깃 컴파일은 성공했지만 작업 중 사용한 CTest 정규식이 검사 이름과 맞지 않아 실행된 검사는0개였다. 최종 공통 검사에서 실제 실행해야 한다.
- Runtime signpost는 GameTick·FrameBuild·GLSubmitAndPresent와 DisplayCallback을 구분한다. 게임 tick·RNG·음향/저장 사건을 바꾸지 않고, Instruments가 기록할 때 사용한다.
- `resourceMetrics()`의 residentTextureBytes는 텍스처 POT 크기×형식으로 계산한 값이며 프로세스 physical footprint가 아니다. 살아 있는 GPU 항목·삭제·업로드·디코딩·글자 항목·HUD 적용 저장 수를 함께 기록한다.
- `tools/profile_hm2_apple.py`는 이미 실행 중인 명시적 Source/Validation 프로세스에 붙는다. 빌드/설치/실행·입력·저장 변경은 하지 않는다. 정확한 바이너리 SHA와 dSYM UUID를 확인하고 HM2 `.local` 아래의 새 디렉터리에 trace/목차/실패 로그를 보존한다. Python 문법만 확인했으며 새 trace 수집은 하지 않았다.
- 치트120조건과 시뮬레이터를 마친 뒤 실제 Release에서 Allocations/Leaks·Time Profiler·프레임·가능한 GPU/전력/열 트랙을 기록한다. 자세한 기간·대조 조건·통과 기준은 [측정 계획](iphone-fixes-and-profiling-plan-20261114.md)을 따른다.

## 원본 근거 위치

원본 IPA·ARM·전체 분석 원문은 로컬에만 둔다. 이번 점검은 기존 REA export를 재사용했다.

- HD 실행파일 SHA256: `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`.
- `Analysis/HD103/native-evidence/functions/0x42fa4.pseudo.c`, `0x42be4.pseudo.c`: 글자와 이미지 Texture2D 준비. 디컴파일 ABI의 잘못된 인수 타입은 원본 소스 선언으로 취급하지 않는다.
- `Analysis/HD103/method-evidence/functions/0x423a8.pseudo.c`, `0x3f56c.pseudo.c`: 파일명 캐시 miss/재사용.
- 같은 method export의 GuiBase init/dealloc0x246d4/0x24664, GuiTitle0x32340/0x322e8, MapLoading0x2da14/0x2d730/0x2d9cc: 소유 수명과 단계.
- `.local/evidence/ipad-ui-feedback-20261110/runtime-fidelity/`의 기존 audio/graphics audit·manifest·context probe. 기존 성공은 이번 수정 뒤의 새 성공으로 쓰지 않는다.
