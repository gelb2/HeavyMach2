# HeavyMach 2 보고 사이트

보고서는 기존 HeavyMach·MachDefense 개발 보고 사이트에 HTML 페이지로 게시한다. 로컬 HTML 파일이나 Markdown 링크만 전달한 상태를 게시 완료로 쓰지 않는다. 원본 IPA·ARM 실행파일·개인 저장·비밀정보는 올리지 않는다.

- Site: `appgprj_6addbf55aea081918d3d182d7e496e88`.
- 접근: 본인 한 명만 허용한 비공개 사이트. 본인 ChatGPT 계정 로그인이 필요하다. 공개 범위를 임의로 바꾸지 않는다.
- [보행·터치 검증과 무기 데이터 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/).
- [전체 기능 치트 앱 개발·검증 계획](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/plan.html).
- 현재 사이트 소스: `.local/report-site/.sites-checkout`. 독립된 Sites 소스 저장소다. 게임 저장소의 main/develop과 혼동하지 않는다.

2026-11-10 배포 시스템 기록: 사이트 소스 `8d65f7eec8c0a714c7600444b76531a5381f322a`, 버전266. 배포 `appgdep_6af24a7457808191ba8d3e4d43568d82`는 `succeeded`로 끝났다. 버전ID는 `appgprj_6addbf55aea081918d3d182d7e496e88~appgver_27bc5da3e6708191a41b9b1ccc809895`다. 동일 버전을 재배포한 `appgdep_6af24b40f4188191bb488b87933243a0`도 succeeded를 반환했다. **그러나 실제 새 URL은404이며 게시 완료 확인은 미완료다.** 서비스 인증을 붙인 HTTP 검사에서 보고서·계획·이미지4개가 모두404다. Safari에서 본인 계정 로그인을 완료한 뒤에도 기존 루트만 열리고 새 경로는404다. 루트에는 새 HeavyMach2 링크가 없다. worker 오류 로그는 비어 있다. 소스/패키지 저장 성공과 실제 서비스 반영을 구분한다.

게임 구현은 `176cfd5`, 전체 치트 계획은 `1a14e65`를 기준으로 한다. 이번 게시에서 게임 기능을 새로 구현하거나 전체 치트 검사를 실행한 것은 아니다.

## 게시와 화면 검사

기존 사이트의 종이색 배경·제목·표·문서/이미지 링크 형식에 맞췄다. 첫 화면의 전체 보고서에 HeavyMach 2 항목을 추가했다. 배포한 소스와 저장 버전에는 실제 시뮬레이터 캡처4장, 현재15개 C++/기기별5개 Apple 검사 결과, 한계와 계획 전문이 있다. 실제 서비스의 새 경로는 아직 열리지 않는다. 연결된 기술 문서는 HTML로 렌더링하며 Markdown은 선택적 다운로드로 제공한다.

로컬 Safari에서 본문·표·실행 화면과 이미지4장을 확인했다. 새 보고서 하위의196개 로컬 링크/자원 참조에 누락이 없고, 계획 다운로드와 저장소 원고가 일치한다. 모바일 기기 브라우저 검사는 이번에 수행하지 않았다. 온라인 HTTP·이미지 검사는 실패했다. 실제 플랫폼 관측과 배포 응답의 불일치를 로컬 증거에 남겼다.

## 용량 제한과 보존

초기 묶음은 Sites의 압축 해제 후256MiB 제한을 넘었다. 기존 보고 문서 전체에서 참조되지 않는 미디어69개를 배포 대상에서만 제외했다. 바이트가 같은 원본은 사이트 checkout의 무시된 `.local/preserved-unreferenced-media/`와 기존 Git 이력에 보존했다. 연결된 문서·이미지·영상은 유지했다.

새 PNG4장은 `oxipng`로 무손실 압축했다. 원본 캡처는 로컬 증거에서 유지한다. 16비트RGBA로 읽은 픽셀, 색상 프로필·방향 메타데이터가 모두 같음을 검사했다. 게시한 PNG의 해시와 원본 캡처 해시는 구분한다. 재생성·크롭·색 보정을 하지 않았다.

macOS tar의 AppleDouble `._` 파일1432개가 들어가 두 번째 묶음도 제한을 넘었다. 공식 Sites helper를 `COPYFILE_DISABLE=1`로 실행해 부가 파일을 제외했다. 최종 archive의 압축 해제 크기는267819520바이트로 제한268435456바이트보다 작고 AppleDouble 항목은0개다. 이 환경 변수는 사이트 파일 내용이나 접근 권한을 바꾸지 않는다.

## 다음 갱신

Sites 스킬로 현재 접근 범위와 원격 소스를 확인한 뒤 같은 project ID를 연다. 소스 helper의 stdin으로만 인증 정보를 전달한다. 원문·이미지와 계획/TODO를 갱신하고 링크·필요한 화면·자원 검사를 수행한다. 공식 helper로 정확한 소스를 push·패키징한 뒤 owner-private 배포를 사용한다. 배포 상태가 succeeded인지 확인하고 실제 URL을 제공한다. 업로드 파일 크기뿐 아니라 압축 해제한 tar 전체 크기를 검사한다.

근거: `.local/evidence/full-validation-plan-20261110/`의 `site-preparation.json`, `image-compression.json`, `site-link-check.json`, `packaging-failures.json`, `archive-check.json`, `deployment.json`, `published-http-check.json`. 인증 토큰은 이 기록에 저장하지 않는다.

## 서비스 반영 미확인 후속

저장된 버전266과 archive의 새 파일 존재를 확인했고, 같은 버전 재배포도 수행했다. 사이트의 project ID·공개 범위·URL을 바꾸지 않았다. 근거 없는 코드/권한 변경이나 같은 업로드 반복으로 해결됐다고 쓰지 않는다. 다음 재개 시 먼저 기존 저장 버전의 실제 경로와 루트 링크를 확인한다. 서비스가 계속 이전 내용을 제공하면 제공되는 Sites 운영 진단으로 원인을 좁힌다. 새 URL의 본문과 이미지가 실제로 열릴 때만 TODO를 완료 처리한다.
