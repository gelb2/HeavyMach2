# HeavyMach 2 Desktop

macOS의 Tauri 앱은 검증한 정식 PWA를 그대로 묶어 실행한다. PWA 빌드의 `asset-manifest.json`에서 판본과 파일별 길이·SHA-256을 확인한다. Mac 앱은 번들에서 파일을 읽고 서비스 워커 등록을 생략한다. 이 앱의 창은 기본 1,200 × 800pt이며, 최소 크기는 800 × 540pt다.

## 빌드

1. [웹 빌드 안내](../../Web/README.md)를 따라 저장소의 `build/web-normal/dist/`에 정식판을 만든다.
2. 이 폴더에서 아래 명령을 실행한다. Node.js, Cargo, Xcode 명령줄 도구가 필요하다.

```sh
npm ci
npm run build
```

현재 Rust 대상을 기준으로 `src-tauri/target/release/bundle/macos/HeavyMach 2 Desktop.app`을 만든다. macOS 14 이상을 대상으로 로컬 실행에 쓸 임시 서명을 적용한다. 런처 아이콘은 검증용 임시 이미지다.

## 저장

앱은 `~/Library/Application Support/com.gelb2.heavymach2.desktop/progress/`에 진행 기록과 HUD 설정을 저장한다. 복구코드를 적용할 때 웹과 같은 게임 코드가 기존 진행을 `BeforeRecovery.sav`에 보관한 뒤 `HeavyMach2.sav`를 바꾼다. 파일은 같은 폴더에서 임시 파일에 먼저 쓰고 디스크에 기록한 뒤 교체한다. 앱을 닫을 때 진행 중인 저장 요청이 끝날 때까지 기다린다. 브라우저 저장은 각 브라우저 프로필에 남으며, 복구코드로 Mac 앱에 옮길 수 있다.

## 검사

```sh
npm test
cargo test --manifest-path src-tauri/Cargo.toml --locked --no-default-features --lib
python3 -m unittest discover -s tests -p 'test_*.py'
```

웹 산출물 없이도 저장 검사와 호스트 연결 검사를 실행할 수 있다.
