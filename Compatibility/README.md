# HeavyMach 2 원본 포팅 앱

일반판 2.0.1·비교용 1.1.0과 HD 1.0.3의 원본 ARMv7 코드를 Rust CPU 인터프리터로 실행하는 내부 비교 앱이다. 각 앱은 서로 다른 원본·기기 설정·bundle ID·저장을 사용한다. 현재 C++ 소스 복원과 최종 일괄 대조 순서는 [작업 순서](../docs/validation-build.md#작업-순서)를 따른다.

초기 비교 기록에서 일반판 1.1.0은 시뮬레이터에서 이름 입력·저장 생성·기지·전장 진입까지 확인했다. 2.0.1은 초기화 중복 호출 수정 후 타이틀 렌더링을 확인했으며 회전 처리를 검증 중이다. HD 1.0.3은 저장 생성·임무 수락·전장·이동을 확인했다. 전투·저장 복구·음향 검증은 진행 중이다. 빌드 성공을 게임 정상 동작으로 취급하지 않는다.

## 준비와 빌드

macOS Apple Silicon, Xcode, XcodeGen, CMake, Rust 1.99.1이 필요하다. 원본 IPA는 `docs/source-provenance.json`의 외부 경로에서 읽는다. 다른 게임 checkout에 의존하지 않는다.

```sh
python3 Compatibility/bootstrap.py
rustup target add --toolchain 1.99.1 aarch64-apple-ios aarch64-apple-ios-sim
Compatibility/build-runtime.sh aarch64-apple-ios-sim
python3 Compatibility/package_reference.py phone --target aarch64-apple-ios-sim
python3 Compatibility/package_reference.py hd --target aarch64-apple-ios-sim
python3 Compatibility/package_reference.py phone110 --target aarch64-apple-ios-sim
xcodegen generate --spec Compatibility/ios/project.yml
```

현행 작업은 HD의 iPad 기준을 사용한다. 사용자가 요청한 실제 iPad의 HD 포팅판 설치와 스테이지 이동·전투 관측은 [확인 기록](../docs/hd-original-function-audit-20261111.md)에 보존한다. 일반판 2.0.1의 후속 실행·상점 자원·좌표 비교는 [iPhone 상점 기록](../docs/iphone-shop-reference-coordinates-20261119.md)에 있다. [호환 코드 제3자 고지](THIRD-PARTY-NOTICES.md)를 따른다. Xcode의 DerivedData 출력은 반드시 저장소 밖 `~/Documents/Codex/HeavyMach2/ReferenceBuilds/` 아래로 지정한다. 패키징 도구는 이 범위 밖의 원본 포함 출력을 거부한다. 서명 팀은 해당 Mac의 기존 개발팀을 명령 또는 Xcode 설정으로 지정한다.

원본 IPA는 읽기 전후 해시를 검사하며 수정하지 않는다. 원본을 포함한 호스트 앱은 로컬 기기에만 설치한다. Git/LFS/CI artifact/릴리스/보고 사이트에 올리지 않는다. 보고서에는 검사 결과와 허용된 화면 캡처만 사용한다.

`heavymach.patch`는 HeavyMach 1에서 검증한 호스트 수명·회전·링크와 MoleWorld 게임별 후킹 비활성화 변경의 출처 보존본이다. 내부 패키지 이름 `HeavyMach.ipa`와 옵션 파일명은 이 패치의 진입점 계약이며, 각 앱에는 명세에 맞는 HeavyMach 2 원본만 들어간다. 실제 적용 파일 `heavymach2.patch`는 이 기준 변경과 HM2 호환 수정의 합본이며 고정한 upstream에 한 번 적용한다. `rust-sdl2.patch`는 별도로 고정한 Rust SDL 연결 저장소에 적용한다. 원작 명령·게임 함수를 건너뛰지 않는다.
