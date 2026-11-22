# HeavyMach 2 TODO

[현재 검증](#현재-검증) · [남은 작업 순서](#남은-작업-순서) · [보류](#보류) · [원문과 항목별 대조](todo-audit-20261122.md) · [기능별 근거](feature-verification.md)

2026-11-22 감사 기준은 `main`의 `e5e41b0`이다. 체크 표시는 해당 줄에 적은 범위의 결과가 있을 때만 쓴다. 사용자 플레이 관측은 기기와 확인한 구간을 함께 적는다. 아래 예상 시간은 기존 빌드와 시험 저장이 준비된 뒤의 작업 시간이다. 실패를 수정하거나 기기 연결을 기다리는 시간은 별도다.

<a id="현재-체크--2026-11-21"></a>

## 현재 검증

### 공개 배포와 빌드

- [x] `v0.1` **배포**. `main` `29a4e30`에 태그를 붙였고 Mac 정식 ZIP과 Android 정식·치트 APK를 게시했다. PWA의 첫 공개 파일은 Pages `271e42f`에 배포했다. [PR #27](https://github.com/gelb2/HeavyMach2/pull/27) · [다운로드·검사](public/release-policy.md#v01-공개-배포-확인)
- [x] `v0.2` **PWA 수정**. 양쪽 가로 방향 요청과 홈 복귀 음악 재개를 연결했다. [구현 `7935d0d`](https://github.com/gelb2/HeavyMach2/commit/7935d0d)
- [x] `v0.2` **PWA 배포**. Pages `2eea699`에 배포했다. [Pages 파일](https://github.com/gelb2/HeavyMach2/commit/2eea699)
- [x] `v0.3` **PWA 수정**. 휴대폰 전체 화면과 Safari 홈 화면의 상태바·뷰포트 처리를 수정했다. [PR #63](https://github.com/gelb2/HeavyMach2/pull/63)
- [x] `v0.3` **PWA 배포**. Pages `e40314f`에 배포했다. 현재 공개 `build.json`도 `v0.3`을 반환한다. [배포 파일](https://gelb2.github.io/HeavyMach2/build.json)
- [x] `v0.4` **Android 정식 빌드**. `NormalRelease`로 arm64-v8a·x86_64 APK를 빌드했고 v0.1과 앱 ID·서명을 대조했다. [PR #71](https://github.com/gelb2/HeavyMach2/pull/71)
- [x] `v0.4` **Android 정식 배포**. APK와 SHA-256을 게시하고 다시 내려받아 갤럭시 설치 파일과 대조했다. Apple은 태그의 소스를 각 개발 팀으로 서명해 설치한다. [릴리스](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4) · [Apple 설치 방법](public/build-and-install.md)
- [x] **공개 클론에서 빌드**. 공개 자원 1,576개를 준비한 새 클론에서 공통 검사 29개와 Apple·Android·PWA·Mac 빌드를 확인했다. 시뮬레이터·에뮬레이터·Mac에서 확인한 실행 범위는 별도로 기록했다. [공개 클론 실행 기록](public/validation.md#공개-클론에서-빌드와-실행) · [PR #35](https://github.com/gelb2/HeavyMach2/pull/35) · [PR #37](https://github.com/gelb2/HeavyMach2/pull/37)

<a id="네이티브-정식판-복구코드와-설치--2026-11-22"></a>

### 저장 이동

- [x] **공통 구현·소스 반영**. 슬롯 세 개의 검사, 이전 저장 백업, 원자적 교체와 세션 갱신을 Apple·Android·웹·Mac에 연결했다. [코덱·호스트](save-transfer.md#개발과-확인) · [PR #26](https://github.com/gelb2/HeavyMach2/pull/26)
- [x] **Apple 정식 빌드·실기기 설치**. iPhone Air·iPad Air에서 재실행했고 설치 전후 저장 파일을 대조했다. [설치·검사](save-transfer.md#호스트별-확인-범위) · [PR #70](https://github.com/gelb2/HeavyMach2/pull/70)
- [x] **iPhone 시뮬레이터의 Apple 런타임**. 공개 코드 두 개, 잘못된 코드 거부, 검증 중 저장 보존, 교체 전 백업과 새 런타임의 세 슬롯 일치를 확인했다. OS 복구창을 처음부터 끝까지 조작한 결과는 아래 2차에 둔다. [확인 범위](save-transfer.md#호스트별-확인-범위)
- [x] **Android 정식 에뮬레이터**. 두 공개 코드의 적용·재실행, 잘못된 코드 거부, 취소, 교체 전 백업을 확인했다. v0.4 후보에서는 v0.1에서 갱신할 때 기존 저장이 일치했고 공개 iPad 코드로 교체·백업을 확인했다. [검사 기록](save-transfer.md#호스트별-확인-범위) · [v0.4 검사 범위](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4)
- [x] **갤럭시 v0.4 설치·열기**. 기존 슬롯을 유지한 상태에서 정식판의 `저장 이동 → 코드 입력`을 열었다. 슬롯 교체는 2차에서 별도 시험 저장으로 확인한다. [공개판 확인](public/validation.md#공개-배포판과-기기-검사)
- [x] **코드 공개와 웹·Mac 왕복**. 레벨 22 예시와 실기기 iPhone·iPad에서 만든 코드 두 개를 공개했다. Mac Chrome 설치 PWA에서 예시와 두 공개 코드를 적용했다. Tauri에서는 별도 세로 창 검증 후보에서 시험 코드와 백업을 대조했다. [코드와 적용 기록](save-transfer.md) · [PR #65](https://github.com/gelb2/HeavyMach2/pull/65) · [Tauri 검사](pwa-device-validation-20261120.md#tauri-mac-세로-창)

<a id="상점-원본-phone-표시-일치--사용자-추가-지시-2026-11-15"></a>
<a id="작업-브랜치의-현재-상태--2026-11-19"></a>
<a id="iphone-점프-실패원본-포팅판-전체-로직-대조--사용자-지시-2026-11-16"></a>

### iPhone 상점·점프·화면과 iPad HUD

- [x] **상점 텍스처 수정·시뮬레이터 검사**. `Icon10113.png` 첫 사용 오류를 재현해 Apple 런타임에 프레임 자원 준비를 연결했다. iPhone 4/4와 iPad 3/3을 기록했다. [커밋 `1420cc1`](https://github.com/gelb2/HeavyMach2/commit/1420cc1) · [현상과 검사](shop-texture-fix-20261116.md)
- [x] **상점 후보 설치와 제한된 실기기 조작**. `f8e82d8` 정식판을 iPhone·iPad에 설치해 상점 다음 페이지와 상세를 열었다. iPhone BUY·Equip 겹침을 이 후보에서 캡처했다. 이후 수정한 크기·거래 영역과 사용자 제보의 재시험은 2·3차에 둔다. [설치·캡처](dual-release-installation-20261119.md)
- [x] **가짜 빈칸·구매 판정**. Phone 상점·장비의 20·5칸 배경을 수정해 시뮬레이터에서 여섯 화면을 확인했다. 사용자 저장 사본에서 75/75칸일 때 LS-102가 거절됐고 한 칸을 비우면 구매됐다. [저장·화면 검사](iphone-inventory-last-page-20261120.md) · [아이콘·정보창 측정](iphone-shop-reference-coordinates-20261119.md)
- [x] **점프 입력·뷰포트·Skyway·메뉴 페이지 수정**. `20ccf0d`, `e57a623`, `5b07e44`, `ab41cda`를 반영했다. iPhone/iPad 시뮬레이터의 입력·GPU와 공통 검사를 확인했다. [커밋·실행·이전 실패](evidence/iphone-runtime-regressions-20261118.json) · [실행 설명](iphone-module-audit-20261118.md)
- [x] **이동 수정 후보의 실기기 조작**. `f8e82d8` 정식판에서 Wasteland 출구·Skyway 이동·경로 교체 뒤 점프·기지 복귀를 검사했다. 반복 손 입력 성공률·연속 프레임은 따로 남긴다. [실기기 기록](dual-release-installation-20261119.md)
- [x] **iPad 세로 출력 회귀 수정·설치**. 메뉴 검은 여백과 작은 전장/HUD를 공통 코드에서 수정했다. 치트·GPU 후보를 검사하고 11월 16일 두 실기기에 설치·시작과 저장 보존을 확인했다. [실기기 기록](dual-release-installation-20261116.md) · [시뮬레이터 기록](evidence/ipad-field-regression-20261115.json)
- [x] **클래식 HUD의 iPad 모델 검사**. Air 13형·mini·9세대·Pro 11형 시뮬레이터에서 기본 세로 HUD와 이동·메뉴를 검사했다. 9.7인치 조건은 별도 화면 모의다. [커밋 `f5c827c`](https://github.com/gelb2/HeavyMach2/commit/f5c827c) · [모델별 결과](campaign-and-ipad-followup-20261120.md#ipad-모델별-hud)

<a id="화이트마운틴과-미등장-콘텐츠-구현--사용자-지시-2026-11-19"></a>

### 화이트마운틴과 추가 콘텐츠

- [x] **원본 조사·신규 캠페인 구현**. 원본 6종에서 미등록 설원 그림 6장과 미등장 콘텐츠 조건을 조사했다. 기지·설원 6개·보스 4건·추가 적·무기 25종·레이저 공습·Picker D~F를 공통 데이터와 코어에 연결했다. [조사 명세](evidence/white-mountain-feasibility-20261119.json) · [PR #21](https://github.com/gelb2/HeavyMach2/pull/21)
- [x] **포털·보스·지형·안내 수정 및 치트 검사**. Desert 01·SnowPass 출구, 보스 거리와 접근 출현, 설원 글꼴·버튼, iPhone 지형 소실을 고쳤다. 두 시뮬레이터에서 포털 14방향·보스 4건·장비 29건을 확인했다. [출구 `75743aa`](https://github.com/gelb2/HeavyMach2/commit/75743aa) · [지형 `58b0938`](https://github.com/gelb2/HeavyMach2/commit/58b0938) · [실행 명세](evidence/white-mountain-dual-release-20261120.json)
- [x] **적 배치·공습·특수공격 진열 검사**. 일반 적 14~20기, 공습 피해 보정과 고정 가격 4종, Lv22의 레이저 공습 `02006` 진열·구매를 공통 검사와 치트 앱에서 확인했다. [배치 `90b0e6c`](https://github.com/gelb2/HeavyMach2/commit/90b0e6c) · [전투·구매 기록](campaign-and-ipad-followup-20261120.md)
- [x] **Apple 실기기 설치와 iPhone 사용자 관측**. 두 기기에 같은 Release와 기지 6곳 방문을 반영한 iPad 저장을 넣었다. 사용자는 iPhone에서 화이트마운틴이 열린다고 확인했다. [두 기기 설치 및 관측 범위](white-mountain-dual-release-20261120.md)
- [x] **공개 갤러리**. 첫 14장과 후속 설원·비행체·LS-D·Picker D 캡처를 공개했다. LS-D 희귀 드롭은 치트 시드 고정 조건이다. [PR #103](https://github.com/gelb2/HeavyMach2/pull/103) · [PR #109](https://github.com/gelb2/HeavyMach2/pull/109) · [갤러리](public/gameplay-gallery.md#화이트마운틴-기지와-설원)

<a id="공유-복구코드와-웹mac-후속-작업--2026-11-20"></a>
<a id="android-구현과-실기기-확인--2026-11-20"></a>

### Android·PWA·Mac

- [x] **Android 공통 코드 연결·기기 검사**. Apple과 Android 호스트를 나눴다. 기존 갤럭시 정식·치트에 iPad 슬롯 사본을 적용했고, 사용자는 레이저·스킬·일반 전투를 짧게 확인했다. 에뮬레이터 치트의 설원·상점·공습 27건과 동시 126조건·2,100틱은 별도 기록이다. [PR #23](https://github.com/gelb2/HeavyMach2/pull/23) · [기기와 에뮬레이터 기록](../Android/검증-20261120.md)
- [x] **Galaxy 설치형 PWA v0.2**. 홈 아이콘 실행과 홈 복귀 뒤 가로 화면·음악 위치 재개를 확인했다. [실기기 기록](pwa-lifecycle-audit-20261121.md#갤럭시-설치-앱)
- [x] **Galaxy 설치형 PWA v0.3**. 홈 아이콘 실행 및 홈 복귀 뒤 주소창·상태바 없는 전체 화면을 확인했다. 270도 손 회전과 이 버전의 실제 슬롯 교체·음향은 아래 순서에 남긴다. [실기기 기록](pwa-lifecycle-audit-20261121.md#갤럭시-v03-설치-앱-확인) · [PR #67](https://github.com/gelb2/HeavyMach2/pull/67)
- [x] **Mac 설치형 PWA와 iPhone 시뮬레이터**. Mac Chrome의 독립 창에서 오프라인 재실행과 저장을 확인했다. iPhone 17 Pro 시뮬레이터는 별도 후보를 홈 아이콘으로 열어 정식 첫 기지·상점·사령부·전장과 치트 상점 왕복을 확인했다. [장치별 실행](pwa-device-validation-20261120.md) · [PR #25](https://github.com/gelb2/HeavyMach2/pull/25)
- [x] **PWA 분류·런처 수정과 모의 검사**. Windows 터치 데스크톱과 WebKit 터치 포인터를 수정했다. 다운로드 바이트·검증·재시도·설치 안내를 연결했고 선택·복사·스크롤이 경로 입력을 가로채지 않는 조건을 확인했다. Mac Safari의 Dock 후보 실행과 서버 종료 뒤 재실행을 확인했다. [작동 범위](pwa-device-validation-20261120.md) · [Wasm 경계](pwa-wasm-audit-20261120.md)
- [x] **Tauri Mac 정식 실행과 두 번들 빌드**. v0.1 공개 ZIP을 받아 서명과 정식 820×900pt 창의 슬롯·저장·상점 왕복을 확인했다. 별도 정식·치트 빌드와 치트 기지 진입을 확인했다. 검은 화면·정비소·데스크톱 첫 도움말 수정은 검증 후보 기록에 남겼다. [공개판 검사](public/release-policy.md#v01-공개-배포-확인) · [후속 실행](pwa-device-validation-20261120.md)
- [x] **에셋 연결 및 게임 음향 경로**. 세 작품의 소비 경로, 웹 그림 1,359개·음원 58개와 네이티브 번들을 대조했다. PWA·Tauri 효과음은 공통 PCM·OpenAL을 사용하고, 설치형 Chrome 치트 교전에서 출력 버퍼를 확인했다. [구현 `f03ad93`](https://github.com/gelb2/HeavyMach2/commit/f03ad93) · [대조와 실행](code-document-fixes-20261121.md)
- [x] **앱 아이콘과 공유 이미지**. iPad 정식 아이콘과 iPad 시뮬레이터 치트 아이콘을 확인했다. 모바일 일러스트를 PWA와 Mac 앱 아이콘에 연결했다. GitHub와 PWA의 iPad 공유 이미지도 별도로 게시했다. [앱 아이콘](app-icons.md) · [공유 이미지 `81f1392`](https://github.com/gelb2/HeavyMach2/commit/81f1392) · [Pages `8d11a10`](https://github.com/gelb2/HeavyMach2/commit/8d11a10)

<a id="github-문서와-개발자의-변--2026-11-20"></a>

### 문서·글감과 초기 구현 기록

- [x] **한·영 README·설치·검증·개발자의 변·갤러리**. 헤비멕1·맥디펜스를 비교해 README 순서·배지·원본 출처·작성자 문장·문서 색인을 반영했다. 사용자 플레이 PNG·영상·GIF와 검증 자료를 게시했고 교체 지시를 받은 GIF를 제거했다. [비교와 공개 범위](github-documentation-review-20261121.md) · [문서 정리 계획 PR #97](https://github.com/gelb2/HeavyMach2/pull/97) · [버전 대조 PR #105](https://github.com/gelb2/HeavyMach2/pull/105) · [영문 6편 PR #107](https://github.com/gelb2/HeavyMach2/pull/107)
- [x] **글감 보존·게재와 원문 위치**. 회고 원문과 후속 글감을 보존했고, 현재 공개 원고·트러블슈팅·개발 과정에 반영한 범위를 기록했다. [글감 목록](developer-background.md) · [삼부작 소회 반영 `b9efa0b`](https://github.com/gelb2/HeavyMach2/commit/b9efa0b) · [원문과 항목별 대조](todo-audit-20261122.md)
- [x] **문서·코드 대조 29건 반영**. 31건 조사 중 제목 잘림과 미사용 Phone 분기는 3차에서 각각 처리한다. [대조표](code-document-audit-20261121.md) · [반영 근거](code-document-fixes-20261121.md)
- [x] **기본 전투·성장·저장·메뉴 연결**. 공통 코어의 무기·전투·BOT·퀵아이템·저장, 도움말·완료 임무·게임오버·랭킹 이용 불가 안내는 각 소비 코드와 실행을 확인했다. 작성 당시 미연결로 남은 기록은 [기능별 근거](feature-verification.md)와 [항목별 대조](todo-audit-20261122.md)에 연결했다.
- [x] **한·영 README에서 플레이 영상과 GIF 바로 보기**. 공개 GitHub의 두 README 본문에서 Chrome으로 MP4 3편 재생과 GIF 2편의 움직임을 확인했다. Mac mini 영상에는 게임 음향이 들어 있다. 공개 첨부 3개는 저장소의 MP4와 해시가 일치한다. [공개 PR #144](https://github.com/gelb2/HeavyMach2/pull/144) · [갤러리](public/gameplay-gallery.md)
- [x] **한·영 공개 문서 문체 정리**. README의 사격·파츠·광고 문단과 플레이·설치·검증·개발 과정 등 17개 파일을 고쳤다. 개발자의 변과 인용은 기존 원문과 대조했다. [공개 PR #146](https://github.com/gelb2/HeavyMach2/pull/146)
- [x] **헤비멕2 글쓰기 지침 연결**. 헤비멕1·맥디펜스의 지침을 바탕으로 로컬 스킬과 참고 문서를 추가하고 `AGENTS.md`에서 연결했다. [글쓰기 스킬](../.agents/skills/write-project-text/SKILL.md) · [공개 PR #148](https://github.com/gelb2/HeavyMach2/pull/148)
- [x] **문서 검사에서 반복 문구 차단**. 기존 GitHub `documentation` 검사에서 한·영 공개 글의 사격·파츠·광고 정책 문구와 검증 면책 문구를 잡는다. 정리 전 문서에서 9건을 찾았고 수정본과 인용은 검사를 통과했다. [검사 사용법](../CONTRIBUTING.md#문서-변경-확인) · [공개 PR #150](https://github.com/gelb2/HeavyMach2/pull/150)
- [x] **이전 시리즈 복원 회고 연결**. 헤비멕2에 있던 한·영 회고 사본을 삭제하고, README·문서 목차·개발자의 변·Sass 자료에서 각 작품의 원문으로 연결했다. [헤비멕1 개발자의 변](https://github.com/gelb2/HeavyMach/blob/main/docs/RESTORATION.md) · [맥디펜스 개발자의 변](https://github.com/gelb2/MachDefense/blob/main/docs/RESTORATION.md)

<a id="현재-남은-일--2026-11-16-검토"></a>
<a id="남은-수정조사검사"></a>

## 남은 작업 순서

### 1. 지금 있는 빌드로 Mac에서 먼저 확인

예상 각 15~45분. 공개 파일의 버전·해시와 로컬 후보의 커밋을 먼저 기록한다. 저장을 바꾸는 검사는 공개 코드나 별도 테스트 슬롯을 쓴다.

- [ ] **iPad 시뮬레이터 정식 복구**. 테스트 앱 실행에 실패한 후보를 새 시뮬레이터에서 열고 두 공개 코드의 복구창·취소·교체·재실행·재내보내기·백업을 확인한다. [실패와 현재 범위](save-transfer.md#호스트별-확인-범위)
- [ ] **iPhone 시뮬레이터 시스템 복구창**. 같은 절차를 실제 붙여넣기·미리보기 버튼으로 실행한다. 별도 Apple 런타임 직접 호출 통과는 위에 기록했다.
- [ ] **Tauri 정식 세로 창**. 사령부 임무 목록·상세·복귀와 전장 실제 음향·홈 복귀를 확인한다. [정식 시작·상점 기록](public/release-policy.md#v01-공개-배포-확인)
- [ ] **Tauri 치트 세로 창**. 치트용 별도 저장에서 상점·사령부·복구·재실행을 확인하고 Finder·Dock의 설치 아이콘도 기록한다. [두 번들 기록](pwa-device-validation-20261120.md)
- [ ] **PWA 재현 조건 고정**. 834×1194 세로 기지 제목, Safari 탭 874×338의 Shop·Exit, iPad Safari에서 전장 복귀 포털을 각각 재현한다. 포털은 같은 저장과 포털을 사용해 탭·홈 화면 실행·직접 터치를 가른다. [보고된 세 경로](pwa-device-validation-20261120.md) · [공통 표시 경계](code-document-fixes-20261121.md#수정-결과)
- [ ] **웹 자동 검사 신뢰도**. 시작 타임아웃에 런처 상태·콘솔·뷰포트·실패 파일을 기록한다. WebKit 모의의 863×360→915×412 실패는 실제 OS 크기 변경과 가른다. Web Locks 없는 탭·설치 앱 동시 실행의 저장 충돌도 시험한다. [실패와 동시 쓰기 기록](pwa-device-validation-20261120.md)

### 2. 현행 정식판 실기기에서 재현과 정착 확인

1차에서 확인한 메뉴와 저장 절차를 사용한다. 예상 기기당 30~75분. 앱 교체가 필요한 경우 설치·프로필 백업·재실행과 기능 확인을 따로 기록한다.

- [ ] **iPhone Air Native 복구창 왕복**. 실제 앱에서 시험 저장으로 복사·붙여넣기·오류·취소·교체·백업·재실행을 확인한다. [설치와 현재 범위](save-transfer.md#호스트별-확인-범위)
- [ ] **iPad Air Native 복구창 왕복**. iPhone과 같은 절차를 iPad에서 따로 확인한다.
- [ ] **갤럭시 정식 APK v0.4 슬롯 실제 교체**. 테스트 슬롯에 두 공개 코드 적용과 교체 전 백업·재실행을 확인한다. 에뮬레이터 통과, 실기기의 메뉴 진입과 각각 구분한다. [v0.4](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4)
- [ ] **iPhone 상점**. 같은 저장 사본·기지·상품 순서에서 `Icon10113.png` 첫 표시와 오류 로그, Shop·Equip 여섯 페이지의 가짜 빈칸, LS-102 구매·판매·저장, BUY·상세·Equip 겹침을 확인한다. 과거 정식 후보에서 본 상태와 최종 수정 후보를 분리한다. [상점 오류](shop-texture-fix-20261116.md) · [인벤토리](iphone-inventory-last-page-20261120.md)
- [ ] **iPad 상점**. Lv22 특수공격 탭의 레이저 공습 `02006` 진열·구매·퀵 등록을 확인한다. 레이저 건 8종의 저장 사본 구매 결과와 별도로 기록한다. [레이저 공습 후속](campaign-and-ipad-followup-20261120.md#특수공격-탭의-레이저-공습)
- [ ] **iPhone 화이트마운틴**. 사용자 첫 입장 확인에 이어 Desert 01·SnowPass 양방향과 기지 사령부 임무를 직접 확인한다. [실기기 후보와 프로필](white-mountain-dual-release-20261120.md)
- [ ] **iPad 화이트마운틴**. 첫 입장, Desert 01·SnowPass 양방향과 기지 사령부 임무를 직접 확인한다.
- [ ] **iPhone 조작·회귀**. 가로 왼쪽과 비대칭 안전 영역, 연속 경로 재지정 뒤 더블탭 성공률, Wasteland→Skyway의 연속 프레임·줌·HUD를 확인한다. [기존 실기기 실행](dual-release-installation-20261119.md) · [GPU와 실패 기준](iphone-module-audit-20261118.md)
- [ ] **iPad 세로 회귀**. 같은 저장에서 점프·Skyway와 기본 메뉴/HUD·전장·저장을 다시 확인한다. Air 13형의 사용자 설정을 보존하고 mini·9세대·Pro 11형의 이전 시뮬레이터 범위도 후보별로 유지한다.
- [ ] **갤럭시 네이티브 장시간**. 정식과 치트의 시작 저장을 가르고 일반 전투·레이저·공습·상점·저장을 이어서 플레이한다. 실제 세션 길이·재개 상태와 프레임을 기록한다. [짧은 사용자 플레이](../Android/검증-20261120.md#ipad-사용자-저장과-갤럭시-플레이)
- [ ] **iPhone Safari 설치형 PWA**. 공개 주소를 홈 화면에서 열어 가로 상점·사령부·전장·경로·포털과 홈 복귀·음향·코드 복원·오프라인 재시작을 확인한다. [설치 절차](../Web/README.md)
- [ ] **iPad Safari 설치형 PWA**. 공개 주소를 홈 화면에서 열어 세로 상점·사령부·전장·복귀 포털, 홈 복귀·음향·코드 복원·오프라인 재시작을 확인한다. Safari 탭 결과를 별도 기록한다.
- [ ] **갤럭시 Chrome 설치형 PWA**. 공개 v0.3의 90도·270도 손 회전, 경로 입력·코드 복원·오프라인 재시작과 홈 복귀 음향을 확인한다. [현재 공개 범위](public/validation.md#공개-배포판과-기기-검사)
- [ ] **실기기 갱신·진행 표시·아이콘**. 위의 세 환경에서 다운로드 바이트와 검증·실패·재시도, 기존 홈 화면 아이콘이 실제 갱신됐는지 기기별로 기록한다.
- [ ] **PWA 실제 창 변화와 지원 환경**. 접이식 Android 접힘·펼침, iPad 분할 화면, iPad·Android 태블릿 세로, 데스크톱과 지원 브라우저에서 시작·배치·터치·저장 결과를 기록한다. 미확보 환경은 검증 미완료로 둔다. [기기 분류 근거](pwa-device-validation-20261120.md)

### 3. 범위가 좁은 결함부터 수정

PWA 검사는 실제 설치 앱을 기준으로 한다. iPhone·iPad·Android가 쓰는 파일은 PWA 수정에서 보호한다. 공통 C++ 배치와 원작 버튼·글꼴을 사용한다. 임의 JS/DOM 버튼·별도 클릭 판정·가짜 화면 크기/배율·스크롤·검은 여백·일러스트 늘림 금지는 계속 적용한다. [판정 기준](pwa-wasm-audit-20261120.md)

각 결함의 해당 재현을 먼저 마치고, 결과가 있는 경로부터 한 번에 하나씩 고친다. 각 항목의 구현을 확인한 뒤 해당 APK·IPA 또는 Pages 반영과 기기별 검사를 나눠 기록한다. 예상 건당 30~120분과 재빌드·검사 20~60분.

- [ ] **iPad 세로 PWA 기지 제목**. 834×1194 설치형 PWA와 네이티브를 대조한다. 보호 파일이 필요한 수정은 위치·범위·영향을 기록해 네이티브 작업으로 넘긴다. [재현](code-document-fixes-20261121.md#수정-결과)
- [ ] **Android 세로 태블릿 시설명**. 1600×2560에서 원본 그림·글자·터치를 맞추고 네이티브 회귀를 확인한다. [Android 재현](../Android/검증-20261120.md)
- [ ] **Phone 메뉴 미사용 분기**. 실제 Phone 상점·정비·사령부 반환 뒤의 분기를 제거할 조건을 점검한다. 각 자원·좌표·버튼 동작의 네이티브 회귀를 확인한다. [소비 경로와 남은 수정](code-document-fixes-20261121.md#수정-결과)
- [ ] **휴대폰 상점 첫 도움말**. iPhone과 갤럭시 양쪽에서 HD 세로 안내와 버튼·하단 차이를 확인하고 실제 Phone UI와 일치시킨다. [Android 대조](../Android/검증-20261120.md)
- [ ] **네이티브 상점·허브·사령부의 남은 표시와 입력**. 원본 일반판 iPhone의 모든 분류·페이지와 24상품·25인벤·6행 에어리프트를 대조한다. 거래·가변 상세·보상/예외창·Help·배경, 추가 버튼 9200~9205와 시설 버튼을 측정한다. iPad HD판의 아이템 선택·SELL과 실제 HUD도 같이 회귀 검사한다. 세로/가로 스크롤 금지와 공통 버튼 경로를 유지한다. [원본 대조](iphone-shop-comparison-20261119.md) · [Phone 메뉴 16사례](phone-menu-pages-20261115.md)

<a id="타이틀스테이지-간-로딩-일러스트--사용자-추가-지시-2026-11-14"></a>

- [ ] **탭 투 플레이와 HD 로딩의 별도 수정**. iPhone 타이틀의 메크·하늘과 석상 노출, 11초/10초와 원작 타이밍, HD 전환 그림·최상단 정렬·필수 자원 오류를 각각 확인한다. 배너·저장·음향 사건과 iPad를 같은 후보에서 검증한다. [사용자 기준과 현재 구현](iphone-development-contract.md) · [자원 진단](iphone-code-asset-diagnosis-20261116.md)
- [ ] **임무 달성 알림과 아이템 지급**. E-10 격파의 달성 토스트·효과음을 직접 재현한다. 크롬 10개를 포함한 수락 지급·필드 수집·완료 보상과 저장을 임무별로 검증한다. 완료 임무의 신규/재저장 목록 순서도 원본에서 비교한다. [임무 구현과 남은 범위](completed-missions-restoration-20261111.md) · [제보](public/validation.md#현재-플레이에서-유의할-문제)


<a id="iphone-사용자-피드백배경-일러스트-분기-조사--2026-11-14"></a>

### 4. 실제 기기 생명주기·성능과 화면 전수

생명주기 재현은 현행 정식판에서 먼저 할 수 있다. 화면 변경의 전후 계측은 해당 수정 후보를 준비한 뒤 진행한다. 예상 동일 기기 두 번 실행과 전후 비교 1~3시간, 전체 장시간 플레이와 원작 대조는 5차다.

- [ ] **Apple 생명주기·음향**. 허브·전장·문 전환에서 홈·전화·시스템 중단 뒤 입력 취소·일시정지·명시적 재개·저장과 OpenAL 컨텍스트·BGM 위치를 확인한다. 같은 저장에서 슬롯 선택 뒤 기지/전장 진입 차이도 대조한다. [11월 14일 관측 원문과 대조](todo-audit-20261122.md#이전-iphone-화면과-생명주기) · [수명 검사](feature-verification.md)
- [ ] **실기기 성능**. iPhone 발열 제보를 포함해 iPhone·iPad 같은 장비와 경로에서 CPU·메모리·누수·GPU·프레임·열을 측정한다. iPad 시뮬레이터 게임오버 15초 기준 실패(16.07초), OS 문 전환 제한 시간과 예전 검정 HUD 캡처도 각각 재현한다. [게임오버 기록](gameover-restoration-20261111.md) · [비교 계획](iphone-fixes-and-profiling-plan-20261114.md)
- [ ] **화면과 출력 대상 전수**. 폰·태블릿·데스크톱별 남은 허브·정비·상점·사령부, Help·인게임 메뉴·미션 페이지, 원본 배경 샘플링·클립과 음향을 실제 화면·터치·사운드에서 확인한다. 소스의 패키지 해시로 화면/기기 판정을 대신하지 않는다. [에셋 소비 경로](code-document-fixes-20261121.md) · [표시 규칙](design-and-asset-rules.md)

### 5. 전체 캠페인과 원작 대조

재현 가능한 검증용 진행과 기준 판본을 준비한 뒤 공통 코어·치트·정식 순서로 진행한다. 한 기능당 30~120분의 부분 검사와 달리 이 범위는 여러 세션을 사용한다. [공통 검증 순서](validation-build.md#작업-순서)

- [ ] **원본 iPhone과 추가·변경 모듈 전수**. 일반판과 HD를 각 환경에 두고 신규 13파일·수정 48파일 및 1,077/1,300 함수 목록의 남은 의미·소비를 상태·호출·카메라/입력·장비/SP·리소스 수명과 연결한다. 59개 차이 테이블, 무기 213행 중 126행 차이, 화면 밖 여유 60의 단위, 빔/미사일·충돌과 선택/명중 범위 안/경계/밖을 비교한다. [원본 분석](iphone-original-logic-audit-20261116.md) · [모듈 결과](iphone-module-audit-20261118.md) · [두 판본 차이](standard-hd-logic-differences-20261113.md)
- [ ] **원작 reader·조립·등록표**. MCB/TBLE/TBL의 오류 입력과 왕복을 대조한다. Packed PartsTile의 실제 활성 조건·개별 PNG 경로, Tile2D/forRetina·DummyParts·그림자와 일반판 ItemInfo 12021, 함수·버튼 7000과 `Screens::coverage()`의 오래된 주소를 실행 근거에 연결한다. [원본 자원 로드 조사](original-resource-loading-audit-20261114.md) · [원작 함수 기록](hd-original-function-audit-20261111.md)
- [ ] **전투·진행·GPU 동시 검사 확대**. 임무 60개·전 지역과 최종 캠페인, 하체 23종과 전체 BOT 행동, HUD·사거리·목표/이동 표시·그림자·레벨업·한국어 UI·맵 전환과 저장을 실제 조건에 넣는다. 개별 게임 상태가 독립적인지와 공통 시간·전역 RNG·예열 뒤 I/O·PCM·GPU를 판정한다. 무기/픽업/사망/보상과 `isSave`, loaded working 슬롯 삭제, `Blt_Missile06/07.png` 부재의 원본 조건을 비교한다. [해금·드롭 안내 PR #110](https://github.com/gelb2/HeavyMach2/pull/110)의 Boss 04 완료·표시 Lv28 의뢰, Lv35 Sn-IGn 드롭 조건도 정식 판정과 대조한다. [기능 매핑](feature-verification.md) · [전체 검사 인벤토리](full-validation-development-plan-20261110.md)
- [ ] **원본 비교 앱과 최종 iPad 일괄 검사**. 일반판의 지형 렌더링·슬롯 진행률, HD의 입력·저장·전투·음향과 기기 방향 조건에서 신뢰할 기준을 확정한다. 실제 같은 iPad의 HD 포팅판·정식판·치트판을 마지막에 대조한다. 독립 빌드와 정식/치트 전환·저장 분리를 플랫폼별로 확인한다. [비교 앱 기준](reference-port-validation-plan.md) · [원본 환경과 실패 이력](todo-audit-20261122.md#초기-구현과-포팅판)

<a id="세-작품-사운드-출처라이선스-조사--2026-11-12-사용자-요청"></a>

- [ ] **세 작품의 음원 출처·라이선스**. 실제 사용하는 파일과 로컬 해시·메타데이터·원작 크레딧, 구매/제작 근거를 맞추고 작품별 재배포 조건을 기록한다. 출처 추정은 확정과 구분한다. [사용자 요구 원문](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L538-L543) · [라이선스 범위](../LICENSE-SCOPE.md)

<a id="hud-선택메뉴-스크롤가로-배너--사용자-추가-지시-2026-11-14"></a>

## 보류

- [ ] **ExperimentalHUD**. 11조각·좌우 무기 패널·클래식/HUD 선택과 저장은 롤백 이후 미구현이다. 클래식 HUD와 실제 기기 회귀를 확인한 뒤 다시 검토한다. [롤백 기록](evidence/experimental-ui-rollback-20261115.json) · [요구](experimental-hud-plan-20261114.md)
- [ ] **추가 방향**. 태블릿 가로 PWA의 상점 복귀, iPad 가로·iPhone 세로 방향 및 세로 Stage03_04 판정은 현재 iPhone 가로·iPad 세로 검증 이후에 처리한다. [가로 재현](pwa-wasm-audit-20261120.md#후속-작업인-태블릿-가로)
- [ ] **미등장 능력**. 화염방사기·근접 공격은 사용자 지시에 따라 보류한다. EMP와 방어 BOT도 발동·피해·음향·이펙트와 획득 경로를 검토한 뒤 구현 여부를 결정한다. [후속 설계](campaign-and-ipad-followup-20261120.md#후속-스킬과-방어형-bot-계획)
- [ ] **온라인 랭킹 실제 연동**. 현재 정식판은 이용 불가 안내와 진행 보존을 사용한다. 외부 인증·서버 제출은 서비스 가용성과 배포 구성이 확정된 뒤 검사한다. [원본의 조건](gamecenter-reference-contract-20261110.md)

<a id="개발자의-변-마지막-시--사용자-원문"></a>

## 원문과 근거

[감사 전 TODO 원문 936줄](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md)에는 사용자 원문·실패·롤백·과거 체크가 그대로 남아 있다. [항목별 대조](todo-audit-20261122.md)에서 과거 기록이 현재 어느 작업으로 이어지는지 찾을 수 있다. 제거한 네이티브 훅은 당시 실행·실패 근거로 보존한다.
