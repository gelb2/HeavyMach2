# HeavyMach 2 코드 구조

[문서 목록](../README.md) · [개발 과정](development-history.md) · [빌드와 설치](build-and-install.md)

게임 규칙과 화면 배치는 C++20 공통 코어에 둔다. iPhone·iPad는 Xcode 프로젝트에서, Android는 Gradle과 NDK에서 같은 코어를 빌드한다. 웹은 그 코어를 Wasm으로 컴파일한다. Tauri Mac은 웹 빌드 파일과 Rust 호스트를 앱에 묶는다.

## 코드와 데이터

| 위치 | 역할 |
| --- | --- |
| [공통 코어](../../Sources/Core) | 이동·탄환·적·임무·성장·저장. 파츠 조립, 카메라, HUD와 메뉴 배치도 공통이다. |
| [Apple 호스트](../../Sources/Platform/Apple) · [Xcode 프로젝트](../../Apple/iOS/HeavyMach2.xcodeproj) | iPhone·iPad의 터치와 화면, OpenGL ES, OpenAL 효과음, AudioQueue 배경음을 연결한다. |
| [Android 호스트](../../Sources/Platform/Android) · [Gradle 프로젝트](../../Android) | Kotlin과 JNI가 C++ 호스트에 입력을 전달한다. EGL·OpenGL ES와 오디오 장치, 앱 전용 파일을 연결한다. |
| [웹 호스트](../../Sources/Platform/Web) · [웹 셸](../../Web/Shell) | Wasm과 WebGL 2를 연결한다. 설치·오프라인 자원·브라우저 입력·IndexedDB를 관리한다. |
| [Tauri Mac](../../Desktop/Tauri) | 웹 화면을 앱 창에서 실행한다. Rust 호스트가 사용자 파일과 진행 백업을 관리한다. |
| [캠페인 확장 데이터](../../Resources/Campaign) · [공개 빌드 자원 도구](../../tools/prepare_build_resources.py) | 설원과 추가 장비의 데이터를 담는다. 공개 클론은 고정된 PWA에서 해시를 확인해 캠페인 자원을 준비한다. |

## 화면과 입력이 만나는 곳

1. 호스트가 표면 크기, 안전 영역, 기기 유형과 입력을 게임에 전달한다.
2. `DisplayLayout`이 표시 영역을 계산하고, `Screens`가 메뉴 버튼의 그림·위치와 터치 판정에 쓸 배치를 정한다.
3. `GameSession`이 이동·전투·아이템·임무를 진행한다. 렌더러는 이 상태와 준비한 텍스처로 전장과 HUD를 그린다.
4. 효과음 요청은 준비한 OpenAL 버퍼와 소스를 사용한다. 진행 저장은 게임이 저장을 요청한 시점에 호스트가 처리한다.

[표시 배치 코드](../../Sources/Core/Presentation/DisplayLayout.cpp) · [화면과 버튼 코드](../../Sources/Core/UI/Screens.cpp) · [세션 코드](../../Sources/Core/Session/GameSession.cpp) · [시간·저장·음향 기준](../runtime-contract.md)

## 정식판·치트판과 저장

정식판과 치트판은 게임 로직을 공유하며 진행 저장을 분리한다. 치트판은 장비·임무·시작 지점 같은 검사 조건을 준비한다. 복구코드는 공통 코덱에서 생성·검사한다. Apple·Android는 앱 전용 파일을, PWA는 오리진별 IndexedDB를, Tauri는 앱 데이터 폴더를 쓴다. 원작 비교용 실행과 자원 재생성 절차는 [호환 실행기](../../Compatibility/README.md)에 있다.

[복구코드와 플랫폼별 확인](../save-transfer.md) · [치트 앱과 검증 범위](../validation-build.md) · [화면·에셋 기준](../design-and-asset-rules.md)
