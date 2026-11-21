# HeavyMach 1과 비교한 HeavyMach 2 GitHub 문서

HeavyMach 1의 main `bd0aa73`에서 README, 문서 목록, 기능, 개발 과정과 플레이 갤러리를 읽었다. 후속 교정에서는 개발자의 변과 트러블슈팅도 대조했다. HeavyMach 2는 직전 상태 `4d634c5`와 원고 커밋 `4686fcf`를 기준으로 대조했다. 처음 확인한 가장 큰 차이는 README에 플레이 스크린샷·영상·GIF가 없고, 원작에서 재현한 동작과 새로 정한 콘텐츠가 한곳에 섞여 있었다는 점이다. [HeavyMach 1 README](https://github.com/gelb2/HeavyMach/blob/bd0aa73ad0bf765f3e54c1800c89b8d550a5e1e9/README.md) · [HeavyMach 2 README](../README.md)

| HeavyMach 1에서 읽을 수 있는 내용 | HeavyMach 2에서 보완한 내용 | 남은 차이 |
| --- | --- | --- |
| 플레이 화면, GIF, 음향이 담긴 영상과 기기별 갤러리 | 현재 갤러리는 플레이 스크린샷 5장과 iPhone·iPad MP4 2편을 제공한다. iPhone 이동 GIF는 플레이가 오작동처럼 보인다는 지적을 받아 공개본에서 뺐다. 캡처의 기기·앱 종류와 영상 발췌 범위를 각각 적었다. [갤러리](public/gameplay-gallery.md) | 음향이 담긴 플레이 영상, 갤럭시 전투 영상, 화이트마운틴의 이동·격파·보상 전체를 한 번에 담은 연속 영상 |
| 원작의 이동·무기·성장과 추가 항공기·지상 유닛·조준 | 원작 조작·포털·사령부·에셋에서 복원한 부분과 새 전장·입수 경로·보스 행동·공습 보정에 적용한 결정을 기능별로 적었다. [원작 복원과 추가 작업](public/features.md) | 원작 HD와 전체 진행을 같은 조건에서 비교한 최종 기록 |
| Quick Play, 설치 주소, Mac 다운로드와 복구코드 예제 | 네 플랫폼의 빌드 명령, PWA 홈 화면 설치와 로컬 실행, 타우리 앱 가져오기와 첫 실행, 슬롯 이동 코드를 연결했다. [설치와 빌드](public/build-and-install.md) · [저장 이동](save-transfer.md) | 실기기용 PWA 설치 주소, 최신 Mac 배포본과 실기기 복구 왕복 |
| HUD 편집과 플레이 시 자주 쓰는 조작 | 현재 메뉴에서 HUD 배치로 들어가 이동·적용·기본 복원하는 순서를 적었다. [HUD 위치 바꾸기](public/quick-play.md#hud-위치-바꾸기) | 실제 기기 HUD 편집 전후 녹화와 ExperimentalHUD 구현 |
| [문제별 증상·원인·수정·검증](https://github.com/gelb2/HeavyMach/blob/bd0aa73ad0bf765f3e54c1800c89b8d550a5e1e9/docs/public/troubleshooting.md) | 점프, 배율과 Skyway, 상점, 설원 보스, Safari와 타우리에서 확인한 문제 7건을 [트러블슈팅](public/troubleshooting.md)에 정리했다. | 실기기의 반복 점프와 Skyway 프레임, 각 기기·브라우저 장시간 확인 |
| 최초 웹 PR의 기여와 코드 구조 | HeavyMach 1의 Wasm PR #24를 참고한 경로를 적었다. HeavyMach 2의 Apple·Android·웹·Tauri가 공통 코어에 연결하는 부분을 정리했다. [플랫폼 연결](public/development-history.md#코드-구조) · [Wasm PR과 비교](pwa-wasm-audit-20261120.md) | HeavyMach 2 복구코드·PWA·Tauri의 develop 통합 |
| 개발자의 변, 프로젝트의 동기, 개발을 마치고 남긴 글 | 두 제작자에게 답하기 위해 복원한 이유와 AI·창작에 관한 회고를 한글 초안에 담았다. 하네스와 모듈에 관한 결정은 [개발 과정](public/development-history.md)에 넣었다. [개발자의 변 초안](RESTORATION.md) · [글감과 근거](developer-background.md) | HeavyMach 2 한글 초안의 영문 전체 번역과 최종본 |
| 릴리스·변경 이력·제작자 및 라이선스 | 기존 제작자 표기와 외부 코드 고지를 유지하고, 개발 과정에서 실제 브랜치와 기기 검증을 찾을 수 있게 했다. [개발 과정](public/development-history.md) · [코드 출처](source-reuse.md) | 이 작업 브랜치를 반영한 정식 릴리스 이력, 음원의 제작·구매 출처와 라이선스 조사 |

README의 기기 표에서 화이트마운틴 상태도 정정했다. 두 실기기에 설치와 저장 적용을 확인했고, 실제 진입은 iPhone에서 사용자 확인이 있다. 포털 14방향과 사령부의 개별 확인은 시뮬레이터·호스트에서 실행했다. [실기기 설치와 포털 검사](white-mountain-dual-release-20261120.md)
