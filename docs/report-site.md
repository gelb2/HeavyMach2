# HeavyMach 2 보고 사이트

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
