# iOS·iPadOS 소스 복원 첫 구현

2026-11-10. 원본 HD 데이터 reader·정적 파츠 조립·Apple GPU 출력부터 구현했다. **현재 실행 화면은 조립 검사용 미리보기다. 플레이 가능한 게임이나 전체 기능 치트 앱의 완성본은 아니다.** 배경은 검은 단색이며 맵·HUD·게임 입력·전투·레벨업·진행·저장·음향은 아직 연결하지 않았다.

## 구현과 원작 근거

| 소비 코드 | 확인한 원작 경로·구현 범위 |
|---|---|
| `Core/Data/Chunk` | HD QChunk loadChunkWithFp `0x3db10`. LE size/type/payloadLength·payload·childCount·재귀 children. 원본 stale size와 unknown bytes를 보존해 원본3개 MCB의 encode 결과를 바이트 비교 |
| `Core/Data/DataTable`, `DarwinRandom` | QTable initWithFile `0x3e38c`, loadBuffer `0x3e4a4`, getDataFrom `0x3e778`. EncTbl 해독·원문 바이트 보존·헤더/행 규칙·누락/빈 값·중복 헤더 overwrite 처리. first-key findRow는 원본 API가 아닌 명시적 검사 편의 함수 |
| `Core/Data/Models` | readPartsChunk `0x165f8`, decodeChunk `0xf508`, getSocketPos `0x1c7e4`, refreshRotationPos `0x1ca2c`. 원본 파츠·모델 그래프, 소수점 소켓, 부모 연결·레이어·z·정적 본체/무장 방향 |
| `Core/Presentation/AssemblyPreview` | 같은 reader·조립 경로의 여러 사례를 한 Frame에 합성. 자원 준비와 프레임 합성 분리. 원작 개별 Tile2D 이미지 fallback의 첫 출력. atlas·그림자·컨트롤러 애니메이션은 미완료 |
| `Assets/Image`·`Platform/Apple/HM2Renderer` | HM1/MD와 같은 stb2.28·CgBI/PNG/JPEG 준비·GLES1·재사용 texture/context·POT padding/content UV·원작 linear 필터·일반 RGBA premultiplied blend·GPU sprite quad·좌표 변환. UIKit 인게임 컨트롤·매 프레임 디코드는 없음 |
| `Platform/Apple/HM2Application`, `Apple/iOS/project.yml` | 같은 소스의 Source/Validation 타깃·별도 bundle ID. iPhone 가로·iPad 세로. CADisplayLink는 현재 정적 조립 표시 예약만 담당하며 HD 게임 tick 스케줄러를 복원한 상태가 아님 |

판본은 HD1.0.3, IPA SHA-256 `7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2`다. 원본 실행파일 SHA-256은 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`다. 원본 분석은 로컬 Ghidra12.1.4로 수행했고 원본 개발 소스를 확보했다고 표현하지 않는다.

HD의338개 파츠·78개 모델을 읽었다. 이 중77개는 선택 레코드의 declared size가 재귀 길이와 맞는다. 나머지 원본 Dummy 모델은49/91바이트로 달라 원작의 모델 사본 범위에서 child가 누락된다. raw tree는 보존하되 그 모델의 정적 pose 요청은 명확하게 거부한다. 원본을 조용히 고쳐 정상 모델로 바꾸지 않았다.

원본 E_Tank_01의 각도0·scale1 기준은 body 중심 `(0,0)`, weapon 중심 `(13.5,-1.5)`다. 무장 이미지를 본체에 합치거나 유닛 이름으로 조립 정의를 생성하지 않았다. Player의 원본 모델에는 무기가 없으며 초기 장비는 별도 setParts 경로다. 이 정적 모델을 장비를 갖춘 Player의 복원으로 계산하지 않았다.

## 실제 빌드·검사

- Xcode27.1·AppleClang21·CMake4.4.4·XcodeGen2.45.4를 사용했다. Apple Debug 시뮬레이터 빌드이며 실기기 설치·Release 배포는 하지 않았다.
- C++7검사를 `ctest --parallel 7`으로 실행했다. MCB3개 원본 바이트 왕복,135개 테이블,1276개 원본 PNG/JPEG decode, Darwin 난수5개 seed×5000회, 잘린 데이터·잘못된 경계·NUL·빈 칸·소수 소켓·조립 사례를 검사했다.
- 네 본체/무장 방향 조건을 같은 Frame에 합성했다.1200회 실제 pose/Frame 재합성 중 resource read 수가 증가하지 않았다. 게임의1200 tick이나 전투 검증으로 계산하지 않는다.
- iPhone Air/iOS26.5와 iPad Air11-inch(M4)/iPadOS26.5의 Apple XCTest를 동시에 실행했다. 각 결과·로그를 분리했다. 각 기기에서8개 sprite·네 사분면 출력,123회 GPU 출력, texture2개 재사용, 같은 context 유지와 원본 body texture의 일반/상하 반전1:1 픽셀 비교를 통과했다. RGB 허용차1을 넘는 픽셀 채널은0개였다. 최종 검사 구간은 두 기기 사이에 약0.681초 겹쳤다.
- Source 미리보기와 Validation 미리보기를 시뮬레이터에 직접 설치·실행했다. 실행 뒤 캡처에서 각각 원본 한 조립과 네 조립 사례를 확인했다. 처음 찍힌 흰 시작 전환 화면은 실행 성공 이미지로 쓰지 않고 보존했다.

이 단계는 원작과의 전체 화면 동등성·최종 기기별 배율·최종 프레임 성능의 통과가 아니다. 현재 timing은 시뮬레이터에서 대기 없이 반복한 합성·submit/present 비용이다. 실제60Hz cadence·전투 성능·원작 대비 성능·실기기 결과와 구분한다.

## 독립 검토와 수정

검토에서 macOS의 대소문자 무시 파일시스템에 가려진 CMake `Tests/` 대 `tests/` 경로 오류를 찾았다. 실제 Git 경로에 맞췄다. 재사용 고지를 추가했고, 비검정 픽셀 수만 보던 검사를 원본 texture의1:1 RGB 비교로 보강했다. display-link가 view를 강하게 붙잡는 순환 참조도 weak proxy와 invalidate로 없앴다. 후속 읽기 검토에서 새 필수 결함은 찾지 못했다.

첫 Apple 검사 빌드는 XCTest target의 UIKit·OpenGLES 링크 누락으로 실패했다. 해당 target에 framework를 명시해 재빌드했다. 실패 로그와 수정 뒤 성공 로그를 함께 보존한다. SDK의 Scene 전환·전체 방향 지원 안내와 테스트 창 appearance 경고는 남아 있다. 실제 제품의 창·생명주기 확장 단계에서 정리한다.

이미지 준비도 기존 두 작품이 사용하는 동일 stb2.28 소스로 맞췄다. 이 과정에서 이전 픽셀 oracle가 디코더의 첫 행을 GL framebuffer의 아래 행으로 간주한 오류를 잡았다. 별도 CPU 비교에서 ImageIO와 stb의 직접행 차이는 허용차 내0개, 반전행 차이는5109개였고 GPU 실패와 일치했다. 원작 `glOrthof 0x277a`, Tile2D의 vertex/UV `0x445f2–0x44786`가 양의Y위·아래 vertex=contentV를 확인했다. 이 근거로 oracle의 행 변환을 고쳤고 RGB 허용차1은 유지했다. 원작 `setAntiAliasTexParameters 0x426a0`에 맞춰 필터도 linear로 맞췄다. ImageIO는 독립 검사에만 남겼다.

## 치트 앱 범위와 다음 구현

[동시 검증 기준](validation-build.md)을 적용한다. XP·장비·진척도 허들을 건너뛰고 여러 기능을 한 전투에서 함께 검증하며, 배타적인 화면/저장 사례는 독립 인스턴스에서 병렬 실행한다. 네 정적 파츠 사례를 그 전체 기능 검증으로 대신하지 않는다.

다음은 원본 atlas/그림자·실제 Player 초기 장비 연결, 맵·카메라·이동/선택·전투와 HUD/버튼이다. 이어 원작 성장·아이템·임무·저장·OpenAL/AudioQueue를 연결하고 실제 복합 치트 시나리오에 편입한다. 기능별 완료 범위는 [TODO](todo.md)에 유지한다.

## 근거·보존·재현

[요약 명세](evidence/initial-source-restoration-20261110.json), [기존 코드 재사용·라이선스](source-reuse.md), [모델 사용 이력](model-usage-history.md)을 연결한다. raw 분석·테이블·이미지·앱·xcresult는 로컬 전용이다. IPA·실행파일·원작 자원을 Git에 추가하지 않았다. HM1·MachDefense 파일과 기존 포팅 앱/사용자 저장도 수정하지 않았다.

로컬: `.local/evidence/initial-restoration-20261110/`. `reader/`의 원본 함수·구조·texture/UV 명세, `core-tests-stb.log`, Apple build 로그, 실패와 최종 `parallel-uv/` 결과, `*-running.png`를 보존한다. `tools/prepare_hd_assets.py`는 원본 해시 확인 뒤 자원1478개/70894554바이트를 변경 없이 `.local/prepared/HD`에 준비한다. `build/apple` 하나에서 현재 두 산출물을 재사용했다. 이전 파생 빌드를 별도 복제하지 않았다.

```sh
python3 tools/prepare_hd_assets.py
cmake -S . -B build/core -DHM2_HD_RESOURCES="$PWD/.local/prepared/HD"
cmake --build build/core -j 4
ctest --test-dir build/core --parallel 7 --output-on-failure
xcodegen generate --spec Apple/iOS/project.yml
xcodebuild -project Apple/iOS/HeavyMach2.xcodeproj -scheme HeavyMach2Source -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' -derivedDataPath build/apple build-for-testing CODE_SIGNING_ALLOWED=NO
xcodebuild -project Apple/iOS/HeavyMach2.xcodeproj -scheme HeavyMach2Validation -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' -derivedDataPath build/apple build CODE_SIGNING_ALLOWED=NO
```

빌드 뒤 생성된 `.xctestrun`과 서로 다른 simulator ID를 `tools/test_apple_parallel.py --xctestrun ... --iphone ... --ipad ... --output .local/evidence/<새 경로>`에 전달한다. 도구는 기존 결과 디렉터리를 덮어쓰지 않는다. 제품 실행은 준비된 자원만 사용하며 원본 ARM 명령 해석기를 포함하지 않는다. 새 clone의 자원 준비·권리 명세·패키지 독립 재현 검사 전체는 후속 단계다.
