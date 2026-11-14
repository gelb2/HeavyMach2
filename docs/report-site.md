# HeavyMach 2 보고 사이트

## 화면 늘림·Phone 배경·HUD 끝단 수정 — 버전292

[화면 수정 결과](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/surface-art.html)를 기존 첫 화면과 [HeavyMach2 누적 목록](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/#report-log)에 연결했다. 게임 구현2f1e5ef와 결과 원고0fed66e를 기준으로20개 연결 문서를 갱신했다. C++77/77·양 시뮬레이터 최종5/5·126조건 동시 실행·원본 자원52파일·정식/치트 Release 패키지 결과와 미확인을 기록했다.

사이트 소스는 `dca1202d76e23038d44c56a12db0711616f4ded4`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_02839c7a8ae481919ed6998a99392c36`, 배포는 `appgdep_6af81e359ab48191bd2d343d104e1a8b`다. 공식 succeeded와 현재 버전292를 확인했다. 본인1명·그룹0·외부0 접근을 유지했다. HTML·gzip 본문·페이지20개·첫 화면/누적 목록/복귀 링크·JavaScript 구문을 로컬에서 확인했다. 새 온라인 브라우저 시각 검수는 수행하지 않았다. 이번 수정본의 실기기 설치와 develop/main 병합도 하지 않았다.

서버 archive는1,389파일·268,339,200바이트다. 기존 JSON을 의미 변경 없이 compact 저장해 공간을 확보했고 사이트 경로·과거 보고/이미지를 보존했다. 최신 게시 성공 후 이전291 archive257,196,998바이트와 사용 완료한 Xcode 중간 캐시/중복 분석 실행파일246,759,950바이트를 정리했다. 최신 앱·dSYM·원본·사용자 저장·실패 로그·GPU 캡처·trace는 보존했다. 토큰은 기록하지 않았다.

로컬 근거: `.local/evidence/iphone-surface-art-20261114/`의 `site-publication-final.json`, `site-archive-final.json`, `report-render-final.json`, `site-json-compaction.json`, `build-cleanup-final.json`. 세 항목의 코드/실행 근거는 [구조화 결과](evidence/iphone-surface-art-20261114.json)에 있다.

## 공통 카메라 구현·아이폰 설치 결과 — 버전291, 2026-11-14

[공통 카메라 결과](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/camera-plan.html)와 기존 누적 보고 목록을 갱신했다. 정식14e9ddb의 설치/실행·저장28,204바이트 보존, 공통77/77·수정 Native양쪽8/8·iPad전후 지문 대조, 실제 Instruments 측정 범위와 남은 실패를 구분했다. 설치 후 사용자 추가3항목(늘림 금지·원본 iPhone 비전투 배경·HUD 화면 끝 배치)은 미반영으로 명시했다.

보고 커밋0769b77, 사이트 소스dabefe57364f1e91c6d0bd7b002f231ffd72cc58, 배포appgdep_6af7fb00c80c8191b824a3d62c5ebd39, 저장버전appgprj_6addbf55aea081918d3d182d7e496e88~appgver_382b9fe73fc08191a6e5c5ea1e76c3f7의 공식 succeeded를 확인했다. 기존 본인 비공개 접근을 유지했다. 첫 전달은 macOS 확장 속성의 PAX 헤더 때문에256MiB 한도를 넘었다. 같은 정적 파일을 ustar로 포장해 metadata만 제외한268,425,728바이트의 archive로 게시했다. 원본 IPA·사용자 저장·trace는 포함하지 않았다. 로컬 근거는 camera-implementation-20261114/site-publication.json이다.

## iPad 보존·SOLID 경계 보강 — 버전290, 2026-11-14

같은 [공통 줌 계획](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/camera-plan.html)에 regular 고정/compact 거리 정책, Scene별 상태 격리, SOLID 책임 경계와 iPad 전후 대조 기준을 추가했다. 배율1뿐 아니라 표시·입력·전투/진행·저장/시간/RNG·동시 세션·화면 재구성을 보존 조건으로 명시했다. 새 iPad 회귀는 완료·병합·설치 전 해결한다. 기준 소스는 `bfde129`, 계획 커밋은 `a31473f`다. 게임 코드와 새 검사 결과를 추가한 작업은 아니다.

사이트 소스는 `9b73740b2adb51408f3734397c41469d670c817f`, 배포는 `appgdep_6af7eaf5c0d88191bb2e28827cd363d7`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_51ed9fd419f0819180fdcac67d7483b0`이며 공식 succeeded를 확인했다. 기존 본인 비공개 접근을 유지했고 Safari 온라인에서 정책 격리·SOLID·실패 처리 본문을 확인했다. 최초 게시 호출의 결과 보관 오류 뒤 Site289가 최신이고 신규 저장버전이 없음을 조회했으며, 다시 게시해 이번 버전을 확인했다. Site289의 이전 전달 압축본은 게시 성공 후 제거해 최신1개만 남겼다.

## 공통 줌 수정 계획과 빌드 정리 — 버전289, 2026-11-14

[공통 C++ 줌 수정 계획](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/camera-plan.html)을 고정 보고 목록과 기존 원인 조사 보고서에 연결했다. 새 `CameraPresentation`의 책임, 기존 Scene/Session/Layout의 수정 위치, iPad 배율1 보존·휴대폰 대기1.0 후보, 앱별 줌 분기 금지와 동시 검사 기준을 기록했다. 게임 구현·새 앱 설치는 하지 않았다.

계획 전에 오래된 빌드/캐시/전달 압축본 약3.30GB를 정리했다. 게시 성공 뒤 이전 Site288 전달 압축본도 삭제해 이전 생성물 삭제 합계는3,552,915,809바이트다. 전달 압축본은 이번 최신1개만 남겼다. 원본·사용자 저장·실패 검사 증거와 최신 앱/심볼은 보존했다. [정리 요약](evidence/build-cleanup-summary-20261114.json).

계획 커밋은 `21b8359`, 문면 보완은 `ab2b700`, 사이트 소스는 `fb77277af7fefdf475944ec88f028e213e7775c3`다. 배포 `appgdep_6af7e65f3c448191a63385946d0d0a2d`, 저장버전 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_cdee92b03a1c819182e488467207f65a`의 공식 succeeded를 확인했다. 기존 본인 비공개 접근을 유지했고 Safari 온라인에서 결정·담당 파일 표·배율 후보·정리 결과를 확인했다. 로컬 기록은 `common-plan-package-verification.json`, `common-plan-publication.json`과 `build-cleanup-20261114/post-publication-archive-cleanup.json`이다.

## 기본 줌 보고서의 결론 보강 — 버전288, 2026-11-14

사용자가 보고서의 핵심과 결론이 불명확하다고 지적했다. 같은 [기본 줌 보고서](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/camera-baseline.html)의 첫 화면을 결론·원인·고칠 내용·현재 미완료 범위 순서로 고쳤다. 관련 문서 목록은 접고 수치표는 근거로 뒤에 배치했다. 기본 줌 구현 오류와 수정 필요, 게임 배율 수정·새 설치 미완료를 명시했다. 문서 수정 커밋은 `f0cf6eb`이며 게임 코드는 바꾸지 않았다.

사이트 소스는 `93542f069b151ca3b093f3895a408e51712de7cf`, 배포는 `appgdep_6af7e2876cf081918f14829d5504ba29`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_a41e3aabade88191a173be0c3b52bff7`다. 공식 succeeded와 Safari 온라인의 결론/수정 항목 표시를 확인했다. 기존 본인 비공개 접근을 유지했다.

## iPhone 기본 줌 조사 — 버전287, 2026-11-14

[기본 줌 과확대 원인·측정·수정 기준](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/camera-baseline.html)을 기존 누적 보고 목록에 추가했다. 공통 코어49표본과 원본 REA10파일을 대조했고, 설치본과 같은 카메라 코드에서 화면 채움 배율과 대기 계수1.6의 곱셈을 확인했다. 기본 배율 수정·새 앱 설치는 하지 않았다. 카메라 초기화 직후 표본과 실제 사용자가 보는 첫 전장 프레임의 시점을 같다고 단정하지 않는다.

근거 커밋은 `f3ecb3c`, 사이트 소스는 `a8f3d05c795226adbafd959be79305c0eb1acc32`, 버전287이다. 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_cf481ea93be88191a9786fb2c6fd6499`, 배포는 `appgdep_6af7e11831a08191b3a023bde234cc6f`이며 공식 succeeded를 확인했다. 기존 본인1명·그룹0·외부0 접근 범위를 유지했다. Safari에서 온라인 본문·수치표·원본 분석과 수정 기준을 확인했다.

새 페이지를 포함한18페이지와 목록을 연결했다. 기존 보고/이미지는 보존했다. 배포 한도를 맞추기 위해 기존 HM2 조사 JSON5파일의 공백만 줄였고 파싱한 값의 동일성과 전후 해시를 기록했다. 최종 tar 확장 크기는268,413,440바이트다. 로컬 근거는 `.local/evidence/camera-baseline-20261114/`의 `report-local-verification.json`, `archive-verification.json`, `site-publication.json`이다.

## iPhone 정식판 설치·직접 검사 — 버전286, 2026-11-14

[기존 구현 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/implementation.html#2026-11-14-iphone-정식판-설치와-직접-검사)와 [누적 보고 목록](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/#report-log)을 갱신했다. 게임 소스64ebe70과 동일한 Release를 다시 빌드·서명·설치·실행했고 저장28,204바이트 보존을 확인했다. 실제 기지 설명/EXIT 분리·사령부 첫3행·이동/BOT 추종·E_Tank_06 공격 피해·파괴 후 기지 복귀를 관측했다. 관측 중 출력 영역/HUD 크기는 유지됐고 검은 레터박스나 크래시는 없었다. 맵 가장자리 외형·가로 왼쪽·원작 표시 비례·전 캠페인은 남는다. 기록 커밋은1a07444다.

사이트 소스는 `51fa8e3fab60d5015f44aa13b83791697fbec843`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_078384afeaa481919c2a41db6d8f3e27`, 배포는 `appgdep_6af7a1594b5481919f50cc94c695985b`다. 공식 succeeded와 본인1명·그룹0·외부0의 비공개 접근을 확인했다. 이번 새 문단의 브라우저 시각 검수는 수행하지 않았다. 기존 보고·그림·주소를 유지했고 원본 IPA·사용자 저장·기기 식별자는 게시하지 않았다.

공식 workflow archive는 확장268,434,432바이트로256MiB 이하이며 콘텐츠1,378개가 현재 소스와 바이트 단위로 일치했다. 생성 hosting manifest는 별도로 구분했다. 게시 후24경로의 HTTP200·HTML 본문/링크 일치·payload/JSON/CSS/JS 바이트 일치를 확인했다. 근거는 `.local/evidence/iphone-release-check-20261114/`다. 실제 iPad·다른 두 게임·develop/main은 변경하지 않았다.

## iPhone 전체 화면·HUD 후속 — 버전285, 2026-11-13

[전체 화면·HUD 수정과 검사](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/implementation.html)를 갱신했다. 기존 고정 보고 목록 첫 절에도 연결했다. 계획12bfcdd·구현64ebe70·검사/설치 기록bd5dfae를 구분한다. C++73개, 양쪽 Native115조건과 실제 Source 터치/게임오버 복귀를 통과했다. iPhone 정식 Release 업데이트와 저장28,204바이트 보존을 확인했다. 수정본 실기기 화면 재검사는 잠금 해제 대기이며 맵 끝 픽셀 연장 외형은 TODO로 남겼다. develop/main 병합은 하지 않았다.

사이트 소스는 `b47c67d17f3fcb38258aa5faddb3e188dab23627`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_abfba75b4c988191af1ac33e832d63e2`, 배포는 `appgdep_6af70bac91b88191b07e56722c5d31e7`다. 공식 succeeded와 본인1명·그룹0·외부0 접근을 확인했다. Safari에서 로컬 보고 본문·링크·배치를 확인했다. 게시 후 브라우저 전환은 `cgWindowNotFound`로 실패했으므로 게시본 브라우저 검수 완료로 쓰지 않는다.

처음 묶음은 macOS 확장 메타데이터 때문에269,972,992바이트로 한도를 넘었다. 공식 workflow에 로컬 tar 형식/메타데이터 설정을 적용한 최종 스트림은268,431,872바이트다. 콘텐츠1,378개와 생성 hosting manifest1개를 대조했고 모든 콘텐츠 바이트 일치를 확인했다. 기존 이미지·본문·링크를 삭제하거나 축소하지 않았다. 원본 IPA·개인 저장·서명 비밀은 포함하지 않았다.

근거는 `.local/evidence/iphone-fullscreen-fix-20261113/`에 보존했다. 이전 사용하지 않는 Release 출력2개271,201,685바이트도 lsof 미사용 확인 후 정리했다. 현재 서명 Release·Source/Validation 검사 제품·소스·사용자 백업·실패 로그/캡처는 유지했다.

## HeavyMach2 고정 진입점

[HeavyMach2 누적 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/#report-log)를 계속 사용한다. 새 조사·계획·구현·검사 보고는 이 목록에 추가하고, 모든 새 보고서에서 같은 목록으로 돌아갈 수 있게 한다. 전체 사이트 첫 화면에서도 이 주소를 연결한다. 기존 보고서 주소와 과거 근거는 보존한다.


## iPhone 카메라·HUD 수정과 재검증 — 버전284, 2026-11-13

[HeavyMach2 누적 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/#report-log)와 [구현·재검증 기록](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/implementation.html)에 최신 결과를 반영했다. 실제 Player 화면 이탈과 중앙 HUD 밖 게이지 노출을 수정했다. 카메라 수정 뒤 Native 전체는 iPhone/iPad각20/20, 이후 HUD/atlas clip 수정 뒤 관련 Native는 각각4/4이며 전체C++73개도 통과했다. 원작 전체·전캠페인 동등성이나 모든 방향의 Native 검증 완료는 아니다. 실기기 설치·develop/main 병합은 하지 않았다.

구현 커밋은 `57b5158f1e9ed581aef87f88957625303d679e62`, 사이트 소스는 `ba2f35989ecd7e52a16011deea67a64ae7f1ab54`다. 버전284의 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_e52dfc37d3488191a78f9f946a0a5dab`, 배포는 `appgdep_6af6eb4012ac81919e6b5c6e8c65803f`이며 succeeded다. 본인1명·그룹0·외부0을 유지했고 실제24경로 HTTP200·HTML본문/링크·payload/CSS/JS/JSON 일치를 확인했다. 로컬 Chrome에서 공통 메뉴·수정 본문·하단 문구를 확인했다. 온라인 Chrome은 로그인 화면까지 열었으므로 로그인 후 시각 검수 완료로 보고하지 않는다.

기존 보고/그림/링크를 보존했다. 반복된 메뉴·하단 문구를 공통JS로 모으고 메타 JSON만 압축 형식으로 써서 archive 확장 크기268,426,240바이트를 유지했다.1,379개 파일을 소스와 대조했다. 원본 IPA·ARM·사용자 저장·분석 원문은 새로 넣지 않았다. 재생성 가능한 이전283 전달용 archive는 lsof 미사용과 소스/저장버전 보존을 확인하고 정리했다.

로컬 근거는 `.local/evidence/iphone-implementation-20261113/revalidation-01/`, `revalidation-02/`, `revalidation-03/`다. 실패한 Native/CLI/카메라/HUD 증거를 유지했다. 최종 `test-counts.json`, `gameplay-comparison.json`, `site-archive-check.json`, `site-published-http.json`, `generated-cache-cleanup.json`과 양쪽 실제 전장 캡처를 보존했다. 토큰은 저장하지 않았다.


## iPhone 공통 구현·검사와 누적 목록 — 버전283, 2026-11-13

[HeavyMach2 누적 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/#report-log)에 기존 보고·측정·일반판/HD 조사·추가 조사·계획·이번 구현을 연결했다. [이번 구현과 기기별 최종 검사](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/implementation.html)와 근거 JSON도 같은 목록으로 돌아간다. 사이트 전체 첫 화면에도 고정 진입점을 추가했다. 기존 본문·그림·주소를 보존했고 초기 개발 상태는 당시 기록으로 표시했다.

구현은 `7622bd1c2ac4476700553867f5b7f581de2749c9`로 작업 브랜치에 커밋·푸시했다. HD 자원 한 벌·같은 공통 게임 코어의 iPhone 창/HUD/입력과115조건 치트 표시를 연결했다. C++73개와 Apple 컴파일을 통과했다. 마지막 Native 한 묶음은 iPhone16/20·iPad18/20으로 실패 상태이며, 검사 준비·좌표·창 구성 수정 후 Native 재실행은 하지 않았다. 실기기 설치·develop/main 병합도 하지 않았다.

사이트 소스는 `e9d5880dcb9bf542bcd34513f2f95bada325a261`, 버전283, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_5c2144c22d608191a1b0bda2e96f020d`, 배포는 `appgdep_6af6d442e5fc8191ab74c11177d2d4ee`다. 공식 succeeded와 본인1명·그룹0·외부0을 확인했다. 실제24경로 HTTP200, HTML 표시 본문·링크 일치와 payload·CSS·JS·JSON의 소스 바이트 일치를 확인했다. 누적 목록과 구현 본문의 레이아웃·이동 링크는 로컬 Chrome에서 확인했다. 결과 표 추가 후 온라인 시각 검사는 수행하지 않았으며, 게시 본문은 인증 HTTP로 검증했다.

공식 archive의 확장 크기는268,429,824바이트이며1,379개 파일을 소스와 대조했다. 후속 보고 추가 전256MiB 제한을 다시 확인해야 한다. 원본 IPA·ARM·사용자 저장·전체 분석 출력은 포함하지 않았다. 성공한 이전282의 전달용 tar257,125,363바이트는 lsof 미사용과 재생성 가능한 사이트 소스·저장버전 보존을 확인하고 정리했다. 현재283 archive·실패 xcresult/첨부·사용자 저장·원본은 보존했다.

로컬 근거는 `.local/evidence/iphone-implementation-20261113/`의 `final-simulator-batch/summary.json`, 기기별 `*-results-summary.json`, `core-final-tests.log`, `apple-final-harness-build.log`, `site-local-check.json`, `site-archive-check.json`, `site-published-http.json`, `generated-cache-cleanup.json`이다. 토큰은 저장하지 않았다.


## iPhone 추가 조사·개발 계획·금지사항 — 버전282, 2026-11-13

[추가 조사](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/followup.html), [개발 계획·금지사항](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/development.html), [인계](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/handover.html), 카메라·전투/UI/시간·저장·음향의 분야별 후속을 기존 네 페이지와 연결해 총10페이지로 게시했다. 근거 커밋은 iPhone 작업 브랜치의 `94985910f97ad7ce8ab1d135473e366c9ac66635`다. 추가 정적 조사·계획·금지사항 보존이며 게임 구현·새 앱 실행·실기기 설치·develop/main 릴리스는 하지 않았다. 실제 표시 크기·가독성·오디오 중단/복귀와 남은 간접 분기는 미확정 또는 다음 실행 조건으로 남겼다.

사이트 소스는 `c8da8badc69dfc7275076c25d9c552d390f6868d`, 버전282, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_d6f0a81459748191b7d2dbfee5bf844c`, 배포는 `appgdep_6af6c2916df88191bd8813a1d9dd2250`이다. 공식 succeeded와 본인1명·그룹0·외부0을 확인했다. 실제18경로 HTTP200, HTML 표시 문구·링크 일치와 본문 payload·스크립트·CSS·명세 JSON 바이트 일치를 확인했다. 로컬 Chrome에서 새 여섯 페이지의 본문·표와 목록을 확인했다. 온라인 Chrome에서는 로그인 화면까지 확인했으므로 로그인 후의 시각 검수 완료로 보고하지 않는다.

256MiB 한도를 지키기 위해 목록 `index.html`의 기존 본문을 `report-index-content.bin`에 gzip으로 보존하고 기존 head/style과 새 loader로 표시한다. 본문은 새 보고 링크 하나를 제외하면 이전 소스와 같다. `report-index-source.json`에 이전/현재 본문·압축 결과 해시를 구분했다. 이후 목록을 고칠 때는 압축 payload를 풀어 기존 본문을 보존한 채 링크를 수정하고 해시를 갱신한다. 최초 변환은 `tools/compact_report_index.py`로 재현할 수 있으며 재압축을 이유로 기존 보고서·이미지를 지우지 않는다.

기존 파일 중1,357개는 바이트가 같고 변경은 목록과 기존 조사 폴더 안7개 파일에 한정됐다. 원본 이미지·게임 자원은 바꾸지 않았다. 최종 tar 확장 크기는268,413,440바이트이며 콘텐츠1,373개와 생성 hosting manifest1개의 바이트 일치를 확인했다. 원본 IPA·실행파일·전체 함수 본문·전체 CFString·사용자 저장은 새 사이트 소스에 넣지 않았다.

로컬 근거: `.local/evidence/iphone-universal-integration-20261113/`의 `evidence-verification.json`, `document-validation.json`, `site-local-check.json`, `site-archive-check.json`, `site-deployment.json`, `site-published-http.json`. 토큰은 파일에 저장하지 않았다.

## 일반판·HD판 차이 조사 — 버전281, 2026-11-13

[조사 결과와 유니버설 설계 보강](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-universal-audit-20261113/)에 결과·조사 기준·패키지 전수 비교·네이티브 로직 비교 네 페이지를 게시했다. 감사 브랜치의 근거 커밋은 `75d0bfa5ef58d4d90385fd249669538e48cce561`이다. 여섯 IPA의 파일9,123개와 이미지7,740항목을 조사했다. 함수 본문 보존과 주요 경로의 정적 비교를 전체 함수 의미 동등성·실행 검증 완료로 표현하지 않는다. 유니버설 설계에 영향을 주는 미확정과 다음 확인을 본문·TODO에 남겼다. 게임 코드·자원·앱 빌드·설치는 변경하지 않았다.

사이트 소스는 `c1de2a6f56b32cec7da0d44c1c90c9dc70354845`, 버전281, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_b2bd32a7ca5c819186283ba8cbd23c57`, 배포는 `appgdep_6af6b3b2ec748191aee4a59b0462e410`이다. 공식 상태 succeeded와 본인1명·그룹0·외부0 접근을 확인했다. 실제9경로의 HTTP200을 확인했다. HTML은 호스팅 서비스가 주입한 스크립트를 구분해 표시 문구·자원 링크를 대조했고, 본문 payload·CSS·JS·명세 JSON은 소스와 바이트가 같았다. 기존 콘텐츠1,355파일은 바이트가 같고 첫 화면에는 새 보고 링크만 추가했다. 로컬 Safari에서 네 페이지의 본문·표를 확인했다. 게시 후 Safari에서는 사용자가 다른 탭을 조작하고 있어 온라인 시각 검수 완료로 보고하지 않는다.

이전 사이트가256MiB 한도에 근접해 새 보고서 본문만 gzip JSON으로 묶고 공통 표시 스크립트에서 해제한다. HTML 본문 전체와 원고 해시를 대조했으며 기존 이미지·파일·URL을 지우거나 바꾸지 않았다. 최종 tar 확장 크기는268,413,440바이트로 제한268,435,456바이트 이하이며 콘텐츠1,364개와 생성 hosting manifest1개의 바이트 일치를 확인했다. 원본 IPA·ARM 실행파일·전체 디컴파일·전체 복호 테이블·저장은 포함하지 않았다.

로컬 근거는 `.local/evidence/universal-report-20261113/`의 `site-local-check.json`, `site-archive-check.json`, `site-published-http.json`이다. 인증 토큰은 파일에 저장하지 않는다.

## 사령부 보상창 배경과 완료 표시 — 2026-11-11

[사령부 보상창 배경과 완료 표시](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/command-panel.html)를 기존 본인 전용 비공개 사이트에 게시했다. 구현과 검사 기록은 `f20a8b8`이다. iPad 시뮬레이터의 복원판 GPU 캡처384×512를 연결했다. 원본 포팅판 캡처와 영상·저장은 로컬에 보존한다.

사이트 소스는 `480c1ec0d7bc72382eb7b851429a436a1f772443`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_77e3b413cf6481918895bd7289570950`, 배포는 `appgdep_6af3a38d8b908191910423e192b87329`다. 실제6경로 HTTP200, 표시 본문·링크와 Markdown·JSON·PNG 바이트를 확인했다. 브라우저 시각 검사는 이번 단계에서 실행하지 않았다. 로컬 근거는 `.local/evidence/command-panel-restoration-20261111/site-published-http.json`에 있다.

## 사령부 임무 상세창과 iPad 원본 대조 — 2026-11-11

[사령부 임무 상세창 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/command-detail.html)를 기존 본인 전용 비공개 사이트에 게시했다. 구현은 `f210d72`, 원본 대조 기록은 `5895234`다. 같은 iPad에서 슬롯 OK만 눌러 두 판의 보상 순서를 확인했다. 게시 자료는 보고 본문·원문 다운로드·검사 수치다. 원본 실행 캡처와 영상 및 저장은 로컬에 보존한다.

사이트 소스는 `39605c8fdfe759312e216211b3abea3a985b7e63`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_a512ade898c8819181a526426277dc4d`, 배포는 `appgdep_6af3a0c9b69c8191afbeaa7a48740be5`다. 실제 5경로 HTTP200, 표시 본문·링크, Markdown·JSON의 바이트를 확인했다. 브라우저 시각 검사는 이번 단계에서 실행하지 않았다. 로컬 근거는 `.local/evidence/command-detail-restoration-20261111/site-published-http.json`에 있다.

## 완료 임무 보상·결과창 — 2026-11-11

[완료 임무 복원 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/completed-missions.html)를 기존 본인 전용 비공개 사이트에 게시했다. 원문·결과 JSON·실제 동시 치트 PNG384×512 한 장을 연결했다. 다른 보고서·앵커·파일은 보존했다. 원본 IPA·ARM·저장 파일은 포함하지 않았다.

게임 구현은 작업 브랜치 `15d44e0`에 커밋·푸시했다. Core70/ASan68과 이후 관련 검사, Debug 정상 Source·Native115조건의 양 Simulator 결과를 구분했다. 각1,200프레임·메뉴360프레임, 보상·저장 재실행과 완료창 가시성을 확인했다. 원본 실행 대조·임무 상세창 펼침·전체 게임·최종 기기별 비례·실기기 전달·develop/main은 완료하지 않았다.

사이트 소스는 `866187dcaa112500f877d6978ba18cc27ed937af`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_a34e3e4651ec8191b21a3a40e704c475`, 배포는 `appgdep_6af3990d65bc8191a1233fa13714e90e`다. 공식 succeeded, 본인1명/그룹0/외부0을 확인했다. 실제7경로 HTTP200, HTML 표시본문·링크와 JSON·원문·이미지 바이트 일치를 확인했다. Safari 연결은 `Sky Computer Use native pipe startup failed`로 실패했다. 이번 페이지의 브라우저 시각 검수는 미완료다.

공식 helper로 만든 archive의 확장 크기는268,065,280바이트로256MiB 이하이며 콘텐츠1350개를 소스와 대조했다. 이전 보고 자산을 삭제하거나 게임 캡처를 수정하지 않았다. 로컬 근거는 `.local/evidence/completed-missions-restoration-20261111/`의 `site-deployment.json`, `site-archive-check.json`, `site-local-check.json`, `site-published-http.json`, `final-source-manifest.json`이다.


## 게임오버·랭킹 안내와114조건 — 2026-11-11

[게임오버와 랭킹 안내 복원](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/gameover.html)을 기존 비공개 사이트에 게시했다. 첫 화면의 전체 보고서에 연결했고 기존 계획 두 페이지에는 현재 iOS·iPadOS 범위와 후속 보고 링크만 추가했다. 기존 본문·앵커·자료 URL을 보존했다. 새 보고는 Core69/ASan67·양 시뮬 Native114조건과 실제 OS 알림·기지 복귀·저장 재시작을 다룬다. iPad15초 성능 실패와 원본 포팅판 GameOver/lifecycle 대조 미완료도 명시한다. 게임 구현은 작업 브랜치 `feat/reference-ports`의 `6f17a24842f4101d427aca5cc25b29d27697a266`에 커밋·푸시했다. 설치 전달·다른 플랫폼·develop/main 배포는 수행하지 않았다.

사이트 소스는 `f1a3f62a096014c64c49a45c9ae9c202a60dc318`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_7e70a19c0ab88191ad96ba8ba13efc8a`, 배포는 `appgdep_6af38c3e95e08191972262455c97ff9f`다. 공식 succeeded와 본인1명/그룹0/외부0 접근을 확인했다. 실제8경로HTTP200, HTML 본문/링크 일치와 JSON·원문 바이트 일치를 확인했다. Safari에서 로컬 본문·표의 시각 배치와 게시된 본문·표 접근성 정보를 확인했다. 모바일 브라우저 시각 검수는 수행하지 않았다.

공식 archive의 압축 해제 스트림은267,759,616바이트다. 기존 ustar 설정으로 컨테이너 확장 메타데이터만 제외했고 콘텐츠1345개와 생성 hosting manifest1개의 바이트 일치를 확인했다. IPA·원본 실행파일 검사는 위반0이다. 새 게시의 서비스 바이트 확인 뒤 이전276 묶음과 쓰지 않는 분석 실행파일을 lsof 미사용 확인 후 정리했다. 합계261,575,498바이트이며 소스·저장·원본·실패 로그·xcresult·현재 묶음/빌드는 보존했다.

근거: `.local/evidence/gameover-restoration-20261110/`의 `site-local-check.json`, `site-archive-check.json`, `site-deployment.json`, `site-published-http.json`, `unused-output-cleanup.json`. 인증정보는 기록하지 않았다.

보고서는 기존 HeavyMach·MachDefense 개발 보고 사이트에 HTML 페이지로 게시한다. 로컬 HTML 파일이나 Markdown 링크만 전달한 상태를 게시 완료로 쓰지 않는다. 원본 IPA·ARM 실행파일·개인 저장·비밀정보는 올리지 않는다.

- Site: `appgprj_6addbf55aea081918d3d182d7e496e88`.
- 접근: 본인 한 명만 허용한 비공개 사이트. 본인 ChatGPT 계정 로그인이 필요하다. 공개 범위를 임의로 바꾸지 않는다.
- [보행·터치 검증과 무기 데이터 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/).
- [전체 기능 치트 앱 개발·검증 계획](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/plan.html).
- 현재 사이트 소스: `.local/report-site/.sites-checkout`. 독립된 Sites 소스 저장소다. 게임 저장소의 main/develop과 혼동하지 않는다.

보행 보고 확인: 2026-11-10 보고서·계획과 이미지4개가 실제 서비스에서 열렸다. HTTP200, 보고서·계획의 표시 본문/링크 일치, PNG4개 바이트 일치를 확인했다. 본인 로그인 Safari에서도 본문 이미지4장이 보인다. 당시 소스는 `473d78fc1b67e1491f1a68cf7411727ffb10dcbc`, 버전267이며 접근 범위는 본인 한 명이다.

초기 실패 기록: 사이트 소스 `8d65f7eec8c0a714c7600444b76531a5381f322a`, 버전266. 배포 `appgdep_6af24a7457808191ba8d3e4d43568d82`와 재배포 `appgdep_6af24b40f4188191bb488b87933243a0`는 succeeded였지만 당시 실제 경로는404였다. 본인 로그인 Safari와 인증 HTTP에서 재현했다. 이후 같은266의 실제 경로가200으로 바뀐 것을 확인했다. 전환 원인은 확인하지 못했다. 당시 실패 증거는 보존한다.

게임 구현은 `176cfd5`, 전체 치트 계획은 `1a14e65`를 기준으로 한다. 이번 게시에서 게임 기능을 새로 구현하거나 전체 치트 검사를 실행한 것은 아니다.

## 게시와 화면 검사

기존 사이트의 종이색 배경·제목·표·문서/이미지 링크 형식에 맞췄다. 첫 화면의 전체 보고서에 HeavyMach 2 항목을 추가했다. 이 보고서는 당시 보행 단계의 시뮬레이터 캡처4장, C++15개/기기별5개 Apple 검사 결과, 한계와 계획 전문을 담는다. 이후 전투 모듈 구현 결과를 이 과거 수치에 섞지 않는다. 연결된 기술 문서는 HTML로 렌더링하며 Markdown은 선택적 다운로드로 제공한다.

로컬 Safari에서 본문·표·실행 화면과 이미지4장을 확인했다. 새 보고서 하위의196개 로컬 링크/자원 참조에 누락이 없고, 계획 다운로드와 저장소 원고가 일치한다. 후속 온라인 HTTP·이미지 검사와 로그인 Safari 검사를 통과했다. 모바일 기기 브라우저 검사는 수행하지 않았다.

## 용량 제한과 보존

초기 묶음은 Sites의 압축 해제 후256MiB 제한을 넘었다. 기존 보고 문서 전체에서 참조되지 않는 미디어69개를 배포 대상에서만 제외했다. 바이트가 같은 원본은 사이트 checkout의 무시된 `.local/preserved-unreferenced-media/`와 기존 Git 이력에 보존했다. 연결된 문서·이미지·영상은 유지했다.

새 PNG4장은 `oxipng`로 무손실 압축했다. 원본 캡처는 로컬 증거에서 유지한다. 16비트RGBA로 읽은 픽셀, 색상 프로필·방향 메타데이터가 모두 같음을 검사했다. 게시한 PNG의 해시와 원본 캡처 해시는 구분한다. 재생성·크롭·색 보정을 하지 않았다.

macOS tar의 AppleDouble `._` 파일1432개가 들어가 두 번째 묶음도 제한을 넘었다. 공식 Sites helper를 `COPYFILE_DISABLE=1`로 실행해 부가 파일을 제외했다. 최종 archive의 압축 해제 크기는267819520바이트로 제한268435456바이트보다 작고 AppleDouble 항목은0개다. 이 환경 변수는 사이트 파일 내용이나 접근 권한을 바꾸지 않는다.

## 다음 갱신

Sites 스킬로 현재 접근 범위와 원격 소스를 확인한 뒤 같은 project ID를 연다. 소스 helper의 stdin으로만 인증 정보를 전달한다. 원문·이미지와 계획/TODO를 갱신하고 링크·필요한 화면·자원 검사를 수행한다. 공식 helper로 정확한 소스를 push·패키징한 뒤 owner-private 배포를 사용한다. 배포 상태가 succeeded인지 확인하고 실제 URL을 제공한다. 업로드 파일 크기뿐 아니라 압축 해제한 tar 전체 크기를 검사한다.

근거: `.local/evidence/full-validation-plan-20261110/`의 `site-preparation.json`, `image-compression.json`, `site-link-check.json`, `packaging-failures.json`, `archive-check.json`, `deployment.json`, `published-http-check.json`. 인증 토큰은 이 기록에 저장하지 않는다.

## 서비스 반영과 이미지 표시 후속

266의 경로가200으로 바뀐 뒤에도 Safari 본문에서 lazy 이미지 높이가 잡히지 않아 캡션만 표시되는 문제를 확인했다. 원본 PNG를 유지하고 HTML에 이미지의 실제 표시 폭·높이와 비동기 디코드를 명시했다. 첫 캡처는 EXIF 방향8이므로 저장 픽셀1260×2736과 화면 표시2736×1260을 구분했다. 수정한 소스473d78f를 버전267로 저장·배포했고 Safari에서4장이 본문에 표시되는 것을 확인했다. 배포ID는 `appgdep_6af25be02a84819199b06f79fbf450a8`, 버전ID는 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_c3efc9fba1a481918943bfc18fa8bd47`다. project ID·URL·접근 범위는 유지했다.

후속 근거: `.local/evidence/combat-runtime-20261110/site-current-http-check.json`, `site-html-content-check.json`, `report-image-dimensions.json`. 이미지 원본과 실패 기록은 보존한다.


## 59조건 동시 검증 보고 — 버전271

[동시 검증 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/concurrent.html)를 추가했다. 게임 구현 기준은 `6f58835741e6c614efaf041041b70ae3cd846ff5`이며, C++46개와 Apple iPhone/iPad 각각59조건·1,200프레임의 결과를 담는다. 전체 기능 미완료와 다음 퀵 아이템 연결을 명시한다. 이전 보행 보고와 계획·기존 모든 자료는 유지했다.

사이트 소스는 `c0671615dde61667626eac6ee938214ce2dc0f06`, 버전271, 배포는 `appgdep_6af27c1f3adc8191a2f494fa3b9e04bf`다. 공식 결과는 succeeded다. 본인 한 명의 비공개 접근을 유지했다. 게시 HTML의 실제 HTTP200·표시 본문/링크 일치, 결과JSON·PNG의 전체 바이트 일치를 확인했다. IAB는 unavailable이었고 Safari 화면 읽기는 시간 초과여서 이번 새 페이지의 브라우저 시각 검수를 완료했다고 쓰지 않는다.

초기 묶음은 압축 해제 후268,571,648바이트로 제한을 넘었다. 모든 파일에 붙은 macOS provenance 확장 속성을 포함한 값이다. 공식 Sites helper의 tar 호출에 시스템 tar의 `--no-xattrs --no-acls --no-fflags --no-mac-metadata` 생성 옵션을 적용했다. 목록 검사는 일반 시스템 tar로 유지했다. 사이트 파일은 모두 보존했고 최종 묶음과 소스의 모든 파일 바이트가 같음을 검사했다. 최종 압축 해제 크기는267,089,920바이트다. 이미지의 내용·해상도·색은 바꾸지 않았다.

근거: `.local/evidence/combat-runtime-20261110/site-concurrent-deployment.json`, `site-concurrent-published-http.json`, `site-concurrent-metadata-free-check.json`. 토큰은 저장하지 않았다. 빌드 검증 뒤 쓰지 않는 임시 산출물116,789,617바이트를 정리했고 소스·실패 로그·캡처·원본·저장은 유지했다.

## 38개 전장·대시106조건 보고 — 버전274

[전장·대시 검증 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/fields-dash.html)를 추가했다. 게임 구현은 `342cf15e72ad7c3dd81ae7d7e51d9e386919208f`다. C++51개, 전체Core ASan/UBSan106조건, iPhone/iPad 시뮬레이터 각각106조건·1,200프레임을 통과했다. 두 시뮬레이터의 별도Validation 앱도106조건으로 갱신했고 자동 종료와 결과 저장을 확인했다. 정상Source 시작은 보행이며 전체게임·전체치트·원작동등성·실기기·develop/main은 미완료다.

현재 보고서는38개field 최초생성695대와 대표 포탑/항공기 전투, 실제두탭/SP100 대시·효과·SFX14, 구급상자6종·공습3종·동시음향·기존메뉴11/11 겹침을 다룬다. 이전59조건 보고와 모든 기존파일은 유지했다. 캡처PNG의 opaqueRGB export를 고쳤고 양 기기의 개별sourceFrame8장을 원시GPU RGB와 대조해 차이0을 확인했다. 보고에 사용한 대시/공습PNG2장은 검증된 원본바이트 그대로 올렸다.

사이트 소스는 `397d771e0ed8f1ca5fe51831d06ddabe872fa09f`, 버전274, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_1908fdd59ee88191bd2e24425884a310`, 배포는 `appgdep_6af294c320688191bd7ca04ccbd70b3f`다. 공식 상태succeeded를 확인했다. 본인1명·그룹0의 비공개 접근을 유지했다. 새HTML/결과JSON/PNG2장과 수정한이전페이지·전체목록 총8경로의 실제HTTP200, 표시본문/링크 일치, JSON/PNG 전체바이트 일치를 확인했다. 브라우저 시각검수는 기존CUA 연결실패 때문에 완료하지 못했다.

공식 helper로 만든 archive는 확장속성·AppleDouble없이268,212,224바이트이며256MiB 제한 이하다. 현재사이트 소스1326파일과 hosting metadata1개의 바이트 일치를 확인했다. 새버전의실제서비스 확인후 재생성가능한 이전271archive를 정리했다. 현재274archive·원본·캡처·실패로그·배포증거는 보존한다. 사용하지 않는 공습검사 scratch빌드83,020,518바이트도 현재51개검사/Apple106성공과lsof미사용 확인후 정리했다.

근거: `.local/evidence/combat-runtime-20261110/site-fields-deployment.json`, `site-fields-published-http.json`, `site-fields-local-check.json`, `site-fields-archive-check.json`, `capture106-rgb-verification.json`, `standalone106-verification.json`, `build-cleanup-after106.json`, `archive-cleanup-after274.json`. 토큰은 파일에 저장하지 않았다.

## 정상 앱 연결 보고 — 버전275

[정상 앱 메뉴·전투 연결](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/source-entry.html)을 게시했다. 구현은 `206b063`이며 Source의 실제 타이틀/슬롯/이름·저장/기지/전장/터치 이동·적 선택·발사/일시정지·재개/재실행·삭제를 양 시뮬레이터에서 확인했다. C++55개·전체Core ASan/UBSan 관련6개(106조건1,200프레임 포함)·기기별UIKit/Native이름경계 검사 결과와 실제 iPhone 전체화면을 제공한다. HD canvas·영어 locale 범위이며 iPhone 최종 가로 배치와 전체 게임·치트는 미완료다. Goal은 active이고 다음은 기본BOT의 Scene 통합이다.

사이트 소스는 `d15661ab6f64112b4c0713950d87111e1d871185`, 버전275, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_4b7a09876da881919b0f9deb9c25da54`, 배포는 `appgdep_6af2a27b5654819194ef87d899b48b9c`다. 공식succeeded와 본인1명/그룹0 접근을 확인했다. 실제12경로의HTTP200·표시본문/링크 일치·JSON/PNG전체바이트 일치를 통과했다. 브라우저 시각검수는미완료다.

기존HD 포팅판 측정PNG2장을 oxipng로 무손실 압축해3,126,197바이트를 확보했다. 원본캡처·Git이력·URL을 보존했고 입력/출력을 RGBA16으로 decode해 모든값이같음, 모든부가chunk(색상/방향 포함)의바이트같음을 확인했다. 새Source캡처도같은검사를통과했다. 원시PNG와 인코딩PNG의 SHA를 구분하고 이전측정보고에 후속설명과 [인코딩명세](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/png-encoding-20261110.json)를 연결했다. 새이미지는자르거나리사이즈하지않았다. 래스터1260×2736,EXIF8이므로 HTML표시는2736×1260이다.

공식archive의 압축해제크기는267,224,064바이트다. 사이트1330파일과 hosting metadata1개의바이트일치, 기존1326개파일보존을 확인했다. 게시후 이전274archive와 쓰지않는 재현실행파일3개를 lsof미사용확인후 정리했다. 현재275archive·빌드후보·원본·저장·소스·실패로그·캡처는유지한다.

근거: `.local/evidence/source-entry-20261110/site-deployment.json`, `site-published-http.json`, `site-local-check.json`, `site-archive-check.json`, `png-optimization/final-verification.json`, `superseded-output-cleanup.json`. 인증토큰은파일에저장하지않았다.

## UI·BOT·맵 전환과 그래픽·음향 대조 — 2026-11-10

[UI·BOT·맵 전환 복원](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/ui-feedback.html), [그래픽·음향 원본 절차 대조](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/graphics-audio-fidelity.html), [게임오버·임무·점수 계획](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/gameover-missions-score-plan.html), [GameCenter 원본 계약](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/gamecenter-reference-contract.html)을 버전276으로 비공개 게시했다. 첫 화면의 전체 보고서에도 연결했다. Markdown은 선택적 다운로드다.

게임 수정은 작업 브랜치 `feat/reference-ports`의 `9f50fe9050c35fa1d69bd315e7f519892d7e260c`에 커밋·푸시했다. Core68개·ASan/UBSan66개, iPhone/iPad Native 관련각7개와 이전 정상 입력·음향 검사의 범위를 구분했다. 초기 동일Frame의1-LSB 변동과 실패한 진단을 남겼으며, 예열 후 슬롯 동일Frame16회·예상Frame·그림자 가림 비교는 오차0이다. 전체 게임·develop/main 배포·물리 iPad 갱신 완료가 아니다.

사이트 소스는 `426e69fa6b5a6741adf83bbc9c4d980e2838f025`, 저장버전은 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_0d366d21bf6481919456c0c408891fc6`, 배포는 `appgdep_6af2ea2a064881919dd66952cf941f87`다. 공식 succeeded와 본인1명/그룹0/외부0 접근을 확인했다. 실제15경로의HTTP200, HTML 표시본문/링크와 원문·JSON·이미지 바이트 일치를 확인했다. 로컬 Safari에서 본문·표·상점 이미지2장을 확인했다. 게시 후 Safari 재연결은 cgWindowNotFound로 실패해 온라인 브라우저 시각 검수는 미완료다.

보고서의 기존 PNG1장과 새 상점 PNG2장은 RGBA16 픽셀과 모든 부가chunk를 보존한 인코딩이다. 원본 파일·Git 이력과 원본/게시 해시를 유지했다. 기존 측정 페이지에 추가 인코딩 명세를 연결했다. 새 상점 캡처의 해상도는 원본1640×2360·복원 GPU384×512이며 전체 화면 픽셀 동등성의 근거로 쓰지 않는다.

최초 게시 archive는 확장 스트림269,223,936바이트로 서버256MiB 한도를 초과했다. 파일 내용 합계만 확인한 검사 오류를 고쳤다. 공식 Sites workflow가 호출하는 시스템 tar에 `--format=ustar --no-xattrs --no-acls --no-fflags --no-mac-metadata`와 `COPYFILE_DISABLE=1`을 적용했다. tar 컨테이너의 확장헤더를 제외했으며 PNG 안의 색상·방향 메타데이터와 모든 파일 내용은 유지한다. 최종 확장 스트림은267,721,728바이트다. 콘텐츠1342개와 생성 hosting manifest1개의 바이트 일치를 확인했다. 원본 IPA·ARM·저장·비밀은 포함하지 않았다.

최신 후보 검증 후 쓰지 않는 이전 시뮬레이터 DerivedData `build/apple-device`를 lsof 미사용 확인 후 정리했다. 논리118,236,875바이트이며 실제 기기 빌드가 아니다. 재현용 명세·현재 build/apple·소스·저장·검증 로그·화면은 보존했다.

로컬 근거: `.local/evidence/ipad-ui-feedback-20261110/`의 `final-results.json`, `site-deployment.json`, `site-published-http.json`, `site-archive-check.json`, `report-png/final-images-verification.json`, `old-simulator-build-cleanup/receipt.json`, `review/final-graphics-*`. 게시용 tar 설정은 `site-tar-bin/tar`에 기록했다. 인증토큰은 파일에 저장하지 않았다.
