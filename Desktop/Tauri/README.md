# HeavyMach 2 Desktop

macOS의 Tauri 앱은 선택한 PWA 판본을 그대로 묶어 실행한다. PWA 빌드의 `asset-manifest.json`에서 판본과 파일별 길이·SHA-256을 확인한다. Mac 앱은 번들에서 파일을 읽고 서비스 워커 등록을 생략한다. 이 앱의 창은 기본 1,200 × 800pt이며, 최소 크기는 800 × 540pt다.

## 빌드

1. [웹 빌드 안내](../../Web/README.md)를 따라 저장소의 `build/web-normal/dist/`에 정식판을 만든다.
2. 이 폴더에서 아래 명령을 실행한다. Node.js, Cargo, Xcode 명령줄 도구가 필요하다.

```sh
npm ci
npm run build
```

현재 Rust 대상을 기준으로 `src-tauri/target/release/bundle/macos/HeavyMach 2 Desktop.app`을 만든다. macOS 14 이상을 대상으로 로컬 실행에 쓸 임시 서명을 적용한다. 해당 PWA 판의 512px 아이콘을 Finder와 Dock에서 사용하는 `icon.icns`로 묶는다.

치트판은 `build/web-validation/dist/` 웹 빌드를 만든 뒤 `npm run build:validation`으로 묶는다. 결과는 같은 번들 디렉터리의 `HeavyMach 2 Desktop QA.app`이다. 앱 ID는 `com.gelb2.heavymach2.desktop.validation`이다.

## 저장

앱은 `~/Library/Application Support/com.gelb2.heavymach2.desktop/progress/`에 진행 기록과 HUD 설정을 저장한다. [복구코드](../../docs/save-transfer.md)를 적용할 때 `BeforeRecovery.sav` 백업을 동기화한 뒤 `HeavyMach2.sav`를 바꾼다. 같은 폴더의 임시 파일을 동기화하고 원자적으로 교체한다. 교체 전 오류는 기존 저장을 유지한다. 교체 후 디렉터리 동기화 오류가 나면 세션에 실제 교체를 반영하고 화면에 별도로 알린다. 복원 전 백업 동기화에 실패하면 원래 진행의 교체를 중단한다. 앱을 닫을 때 진행 중인 저장 요청을 기다린다.

치트판은 `~/Library/Application Support/com.gelb2.heavymach2.desktop.validation/progress/`를 사용한다.

## 검사

```sh
npm test
cargo test --manifest-path src-tauri/Cargo.toml --locked --no-default-features --lib
python3 -m unittest discover -s tests -p 'test_*.py'
```

웹 산출물 없이도 저장 검사와 호스트 연결 검사를 실행할 수 있다.

세로 창 검증에는 앱을 닫고 `--portrait-check` 옵션으로 다시 시작한다. 두 판 모두 실제 820 × 900pt 창에서 동일한 화면과 입력을 검사할 수 있다.

```sh
open -a "src-tauri/target/release/bundle/macos/HeavyMach 2 Desktop.app" --args --portrait-check
open -a "src-tauri/target/release/bundle/macos/HeavyMach 2 Desktop QA.app" --args --portrait-check
```
