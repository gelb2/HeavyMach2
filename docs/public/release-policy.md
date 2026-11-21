# HeavyMach 2 버전과 배포

[설치형 PWA](https://gelb2.github.io/HeavyMach2/) · [GitHub Releases](https://github.com/gelb2/HeavyMach2/releases) · [빌드와 설치](build-and-install.md) · [제작자와 외부 코드](../../CREDITS.md)

## 버전과 브랜치

1. `develop`에서 작업 브랜치를 만든다. 검사를 마친 작업을 PR로 `develop`에 병합한다.
2. 릴리스할 `develop`을 PR로 `main`에 병합한다. 버전 태그 `v0.1`, `v0.2` 등은 해당 `main` 병합 커밋에 붙인다.
3. GitHub Release에는 변경 사항과 실제 확인한 기기·기능 범위를 적는다. 첨부한 산출물의 SHA-256을 함께 올린다. 기존 태그를 보존하고 후속 수정에는 새 버전을 붙인다.

첫 버전은 `v0.1`이다. Mac은 Apple Silicon용 정식 앱 ZIP을, Android는 정식·치트 APK를 분리해 제공한다. iPhone·iPad는 소스에서 빌드하고 본인의 개발 팀으로 서명한다. [v0.1 다운로드와 변경 사항](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1)

## PWA와 Mac 산출물

- HeavyMach 1·MachDefense와 같이 GitHub Pages는 별도 `gh-pages` 브랜치의 루트에서 서비스한다. 정식 웹 빌드의 `dist/` 전체와 `.nojekyll`을 배포한다. 버전과 소스 커밋은 `build.json`에 기록한다. 앱 실행 시 파일별 길이와 SHA-256을 확인한다.
- Mac 앱은 동일한 정식 웹 파일을 번들에 넣는다. `.app`을 macOS의 `ditto --keepParent`로 압축해 GitHub Release에 첨부한다. ZIP을 내려받아 풀고 서명과 실행을 확인한다. [다운로드와 처음 열기](build-and-install.md#tauri-mac-앱-받기)
- 원본 IPA·ARM 실행파일, 개인 저장과 서명 키는 로컬에 보관한다. PWA·앱 산출물에는 컴파일된 복원 코드와 게임 실행에 사용하는 준비 자원을 담는다. [원본 관리](../source-provenance.json) · [제작자와 외부 코드](../../CREDITS.md)

출시 방식 참고 [HeavyMach 1 Releases](https://github.com/gelb2/HeavyMach/releases) · [MachDefense Releases](https://github.com/gelb2/MachDefense/releases)
