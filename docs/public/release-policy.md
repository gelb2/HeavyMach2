# HeavyMach 2 버전과 배포

[설치형 PWA](https://gelb2.github.io/HeavyMach2/) · [GitHub Releases](https://github.com/gelb2/HeavyMach2/releases) · [빌드와 설치](build-and-install.md) · [제작자와 외부 코드](../../CREDITS.md)

## 버전과 브랜치

1. `develop`에서 작업 브랜치를 만든다. 검사를 마친 작업을 PR로 `develop`에 병합한다.
2. 릴리스할 `develop`을 PR로 `main`에 병합한다. 버전 태그 `v0.1`, `v0.2` 등은 해당 `main` 병합 커밋에 붙인다.
3. GitHub Release에는 변경 사항과 실제 확인한 기기·기능 범위를 적는다. 첨부한 산출물의 SHA-256을 함께 올린다. 기존 태그를 보존하고 후속 수정에는 새 버전을 붙인다.

첫 버전은 `v0.1`이다. Mac은 Apple Silicon용 정식 앱 ZIP을, Android는 정식·치트 APK를 분리해 제공한다. iPhone·iPad는 소스에서 빌드하고 본인의 개발 팀으로 서명한다. [v0.1 다운로드와 변경 사항](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1)

`v0.2`는 휴대폰 PWA의 가로 방향과 홈 복귀 배경음을 수정했다. `main` 병합 커밋 `18319f5`에 태그를 붙이고 `gh-pages`의 `2eea699`로 배포했다. v0.2 배포 당시 Mac과 Android 다운로드는 `v0.1`을 사용했다. [v0.2 릴리스](https://github.com/gelb2/HeavyMach2/releases/tag/v0.2) · [갤럭시 설치 검사](../pwa-lifecycle-audit-20261121.md)

`v0.3`은 휴대폰 PWA의 전체 화면과 Safari 홈 화면의 상태바 영역을 수정했다. `main` 병합 커밋 `bc7ab55`에 태그를 붙이고 `gh-pages`의 `e40314f`로 배포했다. 공개 HTTPS 파일 16개의 크기와 SHA-256을 대조했다. [v0.3 릴리스](https://github.com/gelb2/HeavyMach2/releases/tag/v0.3) · [전체 화면 검사](../pwa-lifecycle-audit-20261121.md#홈-화면-전체-화면-개선)

`v0.3`은 갤럭시의 설치형 PWA를 전체 화면으로 연다. [v0.3 PWA 릴리스](https://github.com/gelb2/HeavyMach2/releases/tag/v0.3)

`v0.4`는 Android 정식판의 저장 이동 메뉴를 배포한다. 기존 정식판과 같은 앱 ID와 서명을 사용한다. iPhone·iPad는 본인의 개발 팀으로 `v0.4` 소스를 빌드한다. Mac ZIP과 Android 치트판은 `v0.1`, 설치형 PWA는 `v0.3`에서 받는다. [v0.4 Android 정식 APK와 SHA-256](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4) · [복구코드 검증](../save-transfer.md#호스트별-확인-범위)

## PWA와 Mac 산출물

- HeavyMach 1·MachDefense와 같이 GitHub Pages는 별도 `gh-pages` 브랜치의 루트에서 서비스한다. 정식 웹 빌드의 `dist/` 전체와 `.nojekyll`을 배포한다. 버전과 소스 커밋은 `build.json`에 기록한다. 앱 실행 시 파일별 길이와 SHA-256을 확인한다.
- Mac 앱은 해당 릴리스의 정식 웹 파일을 번들에 넣는다. `.app`을 macOS의 `ditto --keepParent`로 압축해 GitHub Release에 첨부한다. ZIP을 내려받아 풀고 서명과 실행을 확인한다. [다운로드와 처음 열기](build-and-install.md#tauri-mac-앱-받기)
- 원본 IPA·ARM 실행파일, 개인 저장 원본과 서명 키는 로컬에 보관한다. PWA·앱 산출물에는 컴파일된 복원 코드와 게임 실행에 사용하는 준비 자원을 담는다. [원본 관리](../source-provenance.json) · [제작자와 외부 코드](../../CREDITS.md)

## v0.1 공개 배포 확인

`v0.1`은 [PR #26](https://github.com/gelb2/HeavyMach2/pull/26)으로 `develop`에 반영하고 [PR #27](https://github.com/gelb2/HeavyMach2/pull/27)의 `main` 병합 커밋 `29a4e30`에 붙였다. GitHub 로그아웃 상태에서 저장소, 한·영 README, 개발자의 변, 이전 회고, 문서 목록, 빌드·설치, 개발 과정, 트러블슈팅, 갤러리 등을 포함한 20개 문서의 렌더링을 확인했다. 갤러리의 PNG·GIF·MP4 8개를 공개 주소에서 받아 Git의 파일과 해시를 대조했다. [공개 저장소](https://github.com/gelb2/HeavyMach2) · [문서 목록](../README.md)

| 배포 | 직접 확인한 범위 |
| --- | --- |
| PWA | `gh-pages`의 `271e42f` 배포에서 파일 15개의 길이와 SHA-256을 대조했다. 맥미니 Chrome에 정식판을 설치해 독립 창에서 새 슬롯을 만들고, 오프라인으로 다시 열어 저장 28,204바이트가 그대로인지 확인했다. [공개 PWA](https://gelb2.github.io/HeavyMach2/) |
| Tauri Mac | 공개 ZIP을 다시 받아 SHA-256과 압축을 푼 앱의 서명을 확인했다. 맥미니의 820×900pt 창에서 새 슬롯을 만들고 Ruin Field Base와 상점을 왕복했다. 검증용 홈 폴더에 저장을 생성했다. [Mac ZIP](https://github.com/gelb2/HeavyMach2/releases/download/v0.1/HeavyMach2-Tauri-Mac-v0.1-arm64.zip) |
| Android | Gradle에서 정식·치트 APK와 일반 앱 단위 검사를 통과했다. GitHub에서 두 APK를 다시 내려받아 로컬 서명 검사본과 대조했다. [정식·치트 APK](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1) |

위 표는 v0.1 배포 당시의 결과다. 이후 갤럭시에 공개 Android 정식 APK v0.4를 설치해 기존 슬롯과 **저장 이동 → 코드 입력** 화면을 확인했다. 정식 에뮬레이터에서는 복구코드 적용, 복원 전 백업, 재실행을 확인했다. 실제 갤럭시의 시험용 슬롯 교체와 Mac 정식 앱의 음향·사령부 검사는 남아 있다. [저장 이동의 기기별 확인 범위](../save-transfer.md#호스트별-확인-범위) · [현재 TODO](../todo.md#현재-체크--2026-11-21) · [변경 이력](../../CHANGELOG.md)

갤럭시 설치형 PWA에서는 v0.2의 홈 복귀 뒤 배경음 재개와 v0.3의 홈 화면 전체 화면 복귀를 각각 확인했다. [갤럭시 v0.2 검사](../pwa-lifecycle-audit-20261121.md#갤럭시-설치-앱) · [갤럭시 v0.3 검사](../pwa-lifecycle-audit-20261121.md#갤럭시-v03-설치-앱-확인)

레퍼런스 프로젝트의 버전 기록. [HeavyMach 1 Releases](https://github.com/gelb2/HeavyMach/releases) · [MachDefense Releases](https://github.com/gelb2/MachDefense/releases)
