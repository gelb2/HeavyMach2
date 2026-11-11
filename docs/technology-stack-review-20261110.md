# 세 작품의 기술스택 대조와 HeavyMach 2 구현 기준

2026-11-10. HeavyMach 1·MachDefense의 main 소스와 빌드 설정, HeavyMach 2 원본 HD1.0.3의 실행파일·음원을 읽었다. 두 기존 게임의 파일은 수정하지 않았다. 신규 복원 코어·앱 빌드는 아직 만들지 않았다.

## 검토 결론

**HeavyMach 1·MachDefense에서 사용한 기술스택으로 HeavyMach 2를 복원할 수 있다.** 공통 게임 코어는 C++20, Apple 연결은 Objective-C++, 인게임 표시는 GPU 스프라이트·파츠·글꼴 렌더링, 효과음은 OpenAL 경로를 사용한다. Android·웹/PWA·Tauri도 두 작품에서 사용한 플랫폼 구성을 따른다. 새로운 게임 엔진이나 런타임 언어를 추가할 필요를 발견하지 못했다.

현재 세 작품의 구현이 이미 같다는 뜻은 아니다. HeavyMach 2 복원 소스는 아직 없다. 기존 두 작품도 BGM·플랫폼 출력·게임 규칙에 차이가 있다. 공통 스택과 원작별 동작을 함께 유지한다. 같은 스택을 맞추려고 HeavyMach 1의 저장 길이·시간·메크 조립 형식·음향 상수를 HeavyMach 2에 대입하지 않는다.

검토 기준 커밋:

- HeavyMach 1 main: `bd0aa73ad0bf765f3e54c1800c89b8d550a5e1e9`.
- MachDefense main: `0aa03aa7c4adf53d6aa8b33c6d4e193105298d6e`.
- HeavyMach 2 계획·포팅판 대조: `96d8160`. 당시 미커밋 호환 실행기 작업은 보존했다.

확인한 파일·Git blob·SHA-256·원본 함수 주소는 [조사 명세](evidence/technology-stack-review-20261110.json)에 기록한다. 여기서 main은 읽은 로컬 Git 참조다. 이번 검토에서 원격 배포나 기존 게임의 실행 검사를 다시 수행한 것은 아니다.

## 공통 스택과 적용 범위

| 영역 | HeavyMach 1·MachDefense 실제 구현 | HeavyMach 2 적용 |
|---|---|---|
| 게임 규칙·데이터·메뉴·HUD | 두 작품 모두 C++20 게임 코어·CMake. HM1 Apple 메뉴/HUD에는 Objective-C++ 구현이 있고 웹에 대응 C++ 구현이 있음. MachDefense는 메뉴/HUD도 공통 C++ Frame으로 출력 | HD 원본의 reader·파츠·입력·전투·진행·저장을 C++20으로 복원. 메뉴·HUD는 MachDefense처럼 같은 코어에서 표시·입력 처리. 두 기존 작품의 UI 소스가 이미 하나라고 보고하지 않음 |
| iPhone·iPad | Objective-C++/Xcode, EAGL/OpenGL ES1 계열 출력, 원작 이미지·글꼴·버튼을 GPU로 표시 | 같은 구성의 유니버설 앱. UIKit은 창·터치·키보드·OS 입력 연결에 한정. 메크·탄환·인게임 버튼·게이지를 UIKit 컨트롤로 만들지 않음 |
| 효과음 | OpenAL Soft1.22.2의 재사용 컨텍스트·버퍼·source. 두 저장소의 vendor 버전 확인 | 같은 OpenAL 계열과 PCM 준비·재생 구조 사용. HD source 위치·재시작·캐시 규칙을 연결 |
| BGM | MachDefense Apple은 AudioFile/AudioQueue. Android는 별도 음악 어댑터. 효과음과 분리 | HD 원본의 AudioQueue BGM을 Apple에서 복원. 다른 플랫폼은 같은 곡 전환·루프·중단 계약을 기존 방식의 출력 어댑터로 연결 |
| Android | 기존 Kotlin Activity/JNI 호스트 + NDK C++20, EGL/GLES1, OpenAL Soft | 게임은 C++ 코어에 유지. Kotlin은 두 작품에 이미 있는 호스트 역할에 한정. Kotlin으로 게임·HUD·전투를 별도 구현하지 않음 |
| 웹·PWA | Emscripten/WebAssembly C++20, WebGL2, OpenAL API 연결, JS 플랫폼 호스트 | 같은 코어·표시 명령을 사용. Three.js나 별도 JS 게임 엔진을 추가하지 않음. 브라우저 출력의 WebAudio/저장 API는 플랫폼 연결에 한정 |
| Tauri Mac | Tauri2 Rust 호스트 + 같은 웹 산출물. 두 Cargo 설정에서 같은 Tauri 본체2.12.5 확인 | 게임을 Rust로 재작성하지 않음. PWA와 같은 코어·자원·게임 UI를 감쌈 |
| native Mac | HeavyMach 1은 AppKit/Objective-C++와 legacy OpenGL 경로도 보유 | 필요할 때 그 Apple 어댑터 구성을 재사용. MachDefense의 동일 native Mac 경로까지 이번에 확인했다고 하지 않음 |
| 분석·오프라인 준비 | Python 자원 도구, 셸/CMake 빌드, Rust·SDL 원본 비교 실행기 | 기존 역할만 유지. Python을 게임 루프·사격·음향·저장의 실행 의존성으로 넣지 않음. Ruby 런타임·새 게임 프레임워크를 추가하지 않음 |

세 저장소는 독립적으로 유지한다. 재사용할 코드는 출처 커밋·파일·라이선스·변경점을 명시해 HM2 안에 둔다. 다른 작품 checkout을 실행 중 참조하거나 그 폴더에 HM2 코드를 저장하지 않는다. 공통 기술스택이라는 요구를 세 게임의 원작 데이터·밸런스까지 합치는 것으로 해석하지 않는다.

## 그대로 가져오면 실패하는 부분

### PCM 형식

두 원본 IPA의 해시를 검증하고 WAV의 RIFF fmt 헤더를 읽었다. 일반판2.0.1은56개, HD1.0.3은57개다. 아래는 HD의 원본 형식이다. 파일 변환·재압축을 하지 않았다.

| PCM | 채널 | 샘플레이트 | HD 파일 수 |
|---|---|---|---|
| 16비트 | 모노 | 22050Hz | 45 |
| 8비트 | 모노 | 22050Hz | 1 |
| 16비트 | 모노 | 44100Hz | 3 |
| 16비트 | 스테레오 | 22050Hz | 2 |
| 8비트 | 스테레오 | 22050Hz | 3 |
| 16비트 | 모노 | 48000Hz | 3 |

별도로 STAGE3.mp3가 있다. WAV와 MP3의 실제 게임 사용 목록·전환 조건은 소비 함수와 연결해야 한다.

HeavyMach 1의 `HMOpenALAudio.mm`에는 모노 PCM16 전용 디코더가 있다. 이를 그대로 복사하면 HD의8비트·스테레오 파일6개를 처리하지 못한다. MachDefense의 `MDAudio.mm`·`PCM.hpp`·`OpenALEffects.cpp`는8/16비트·모노/스테레오와 원본 샘플레이트를 구분한다. **그 형식 처리와 수명 관리 구조를 재사용 기준으로 삼되 HM2 원작의 재생 동작에 맞춘다.** HM1에 맞추려고 HM2 음원을 일괄 리샘플링·모노 변환하지 않는다.

MachDefense의 현재 play 함수는 stop 뒤 gain·위치를 설정하고 play한다. HD의 조사한 일반 효과음 경로는 위치 설정 뒤 stop→play이며 HM1의 현재 경로도 이 순서다. PCM 처리와 OpenAL 객체 수명은 재사용하되 MachDefense play 함수를 그대로 복사하지 않는다. HD의 순서와 루프 분기를 검사한다. 같은 OpenAL API를 쓴다는 사실만으로 재생 동등성을 판정하지 않는다.

### 렌더링과 시간

HD `EAGLView::initWithCoder:` (`0x3078`)는 `initWithAPI:1`을 호출한다. 기존 두 작품의 GLES1 출력 구조와 맞는다. HD는 RGB565 drawable 설정도 사용한다. 이것을 다른 작품의 RGBA framebuffer 설정과 무조건 같게 취급하지 않고 실제 색·알파·블렌딩 비교에 포함한다.

일반판 포팅 로그에는 API2 요청이 거절된 뒤 API1 경로가 나타난다. 일반판의 검은 지형 원인을 해결한 근거는 아니다. HD 기준 복원 렌더러를 일반판의 이 실패 화면에 맞추지 않는다. 셰이더 파일이 있다는 이유로 새로운 엔진을 고르지도 않는다.

HD `renderScene` (`0x299c`)은 시간에 float 비트값 `0x3c87fcb9`를 더하고 tick·drawVisual·swapBuffers를 호출한다. 이 값은 약0.0166이다. HeavyMach 1의 double 프레임 상수와 같다고 취급하지 않는다. 스케줄러·실제 시간 소비·중단 복귀를 추가 대조하고 게임 시간·표시 주기를 분리한다.

### 저장과 난수

HD `GlobalInfo::saveDataFile` (`0x17580`)은 인벤토리·임무·슬롯을 모으고28204바이트를 만들어 `HeavyMach2.sav`에 원자 쓰기한다. DB를 새로 도입할 이유가 없다. 함수 내부에는 `srandom/random` 호출과 바이트 변환도 있다. 저장이 게임 난수 상태에 주는 영향과 정확한 저장 호출 지점을 확인해야 한다. 다른 작품의 저장 포맷·난수 호출 순서·저장 이벤트 목록을 복사하지 않는다.

## HD 음향 경로에서 확인한 내용

Ghidra12.1.4의 정적 분석이다. 원본 개발 소스를 확보한 것은 아니다. float·구조체 인자는 ARM 호출 규약과 명령으로 확인해야 한다.

| 원본 함수 | 확인한 구조 |
|---|---|
| sharedMgr `0x42540`, init `0x4229c`, 초기화 경로 `0x40504` → `0x412d0` | 싱글턴과 음향 관리 객체 준비. 하위 초기화에서 alcOpenDevice·alcCreateContext·alcMakeContextCurrent 호출 |
| getSound `0x423a8` | 이름으로 캐시 조회. 없을 때 파일 경로와 음원 ID를 준비해 사전에 저장. 같은 이름 요청마다 새 파일을 준비하는 구조가 아님 |
| play:X:Y: `0x42494` → 위치 `0x407e4`·재생 `0x40aa8` | source 위치 설정 뒤 재생 경로. 일반 분기는 기존 source를 stop→play. 별도 루프 분기에는 버퍼 큐 처리도 있어 단순 재생 하나로 전체를 대체하지 않음 |
| playBGM `0x424d8` → 큐 준비 `0x41d04`, 버퍼 `0x41a20`, 시작 `0x40f50` | AudioFile/AudioQueue를 사용하는 BGM 경로. AudioQueuePrime·Start를 호출. 효과음 사격 경로와 분리 |
| setBackground `0x42364`, dealloc `0x42318` | 음원 사전 정리·관리 객체 해제 경로가 있음. 전투 중 재사용과 앱 중단/종료 수명을 구분해야 함. 현대 앱 복귀 정책까지 검증됐다는 뜻은 아님 |
| QobButton::draw `0x383c4` | 비활성/누름/해제 상태에 따라 타일·위치·배율·alpha를 고르고 drawTile 경로로 전달. 인게임 버튼을 플랫폼 UIButton으로 대체할 근거가 아님 |

따라서 효과음을 AVAudioPlayer/AVAudioEngine으로 임의 대체하지 않는다. 원작이 실제로 쓰는 BGM AudioQueue와, OpenAL 출력 장치에 필요한 OS 오디오 세션 연결은 보존한다. Apple 프레임워크 이름의 유무만으로 준수 여부를 판단하지 않고 **무엇이 사격을 재생하고 언제 생성·해제되는지** 검사한다.

## 구현 순서와 실패 기준

[실행 계약](runtime-contract.md)을 HM2 구현 기준으로 추가했다. 먼저 HD 데이터 reader·파츠 조립·표시 명령과 공통 코어를 연결한다. 같은 소스의 일반/치트 앱을 구성하고 기기별 표시·입력만 분리한다. 포팅판의 시뮬레이터 정상동작 확인 전 실기기 설치 금지는 유지한다.

| 방지할 문제 | 구현 전에 정할 계약 | 구현 후 실제 검사 |
|---|---|---|
| 사격마다 DB·파일 쓰기 | 전투 상태 변경은 메모리. HD 저장 호출 지점→저장 요청→플랫폼 완료 결과를 연결 | 연사·피격·드롭·이동·그리기 중 저장 요청과 실제 파일/IndexedDB 쓰기 계수. 원작 저장 이벤트에서만 증가. 실패·재시도·재실행 복구도 검사 |
| 효과음 지연·컨텍스트 반복 생성 | 음원 준비와 재생 분리. source·buffer·context의 활성 전투 중 수명 고정. 요청 순서·위치·동시 재생 규칙 보존 | 예열 뒤 반복 사격에서 파일 로드·PCM 디코드·새 context/source/buffer 생성/해제 증가0. BGM 전환·앱 중단은 별도 조건. 발사 프레임·재생 요청·실제 출력 샘플 진행과 지연을 비교 |
| UIKit 인게임 UI·매 프레임 객체 생성 | 그림·글꼴·HUD·버튼은 원작 자원과 공통 GPU 표시 명령·입력 경로 | 실제 뷰 계층과 렌더 명령 연결 확인. 연사·스크롤·회전 중 UI 객체/텍스처/프레임버퍼 재생성 계수와 프레임 시간 측정 |
| 플랫폼별 게임 재구현 | 같은 C++ 코어와 원작 데이터·일반/치트 분기 사용 | 소스 목록·패키지·동일 조건의 상태/그림/입력/전투·음향·저장 결과 대조. Android Kotlin/JS/Rust에서 게임 수식 중복 구현 금지 |

MachDefense의 `FrameIOTests.cpp`와 Apple `AudioTests.mm`, HeavyMach 1의 `HMAudioTests.mm`·실제 프레임 검사에서 관련 검사 구조를 확인했다. **이번에는 그 테스트를 다시 실행하거나 HM2에 이식하지 않았다.** 문서 키워드 검사·컴파일·mock 호출 횟수만으로 실제 사격·출력음·프레임·파일 쓰기를 통과시키지 않는다.

판별을 마친 범위는 공통 스택 사용 가능성, 재사용할 모듈의 역할, PCM 로더 차이와 HD의 위 정적 경로다. 저장 호출 전수·루프 음원 설정·실제 지연·프레임 시간·원작 재생 동등성은 구현 및 포팅판 대조에서 남은 검사다. 이 미확인을 이유로 새로운 프레임워크를 넣지 않는다.
