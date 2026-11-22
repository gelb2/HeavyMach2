# 헤비멕2 GitHub 문서 정리 계획

Quick Play와 빌드 안내를 웹·PWA, iPhone·iPad, Android, Mac 순으로 정리했다. 상점과 iPhone 이동·Skyway 문제는 트러블슈팅에 뒀다. 다음 작업은 두 선행작의 문서 구성에서 필요한 부분을 가져와, 독자가 설치·조작·추가 기능·검증 결과를 한 경로에서 찾도록 하는 것이다. [Quick Play](../README.md#quick-play) · [트러블슈팅](public/troubleshooting.md)

## 두 저장소에서 참고할 구성

| 참고 자료 | 헤비멕2에서 적용할 내용 |
| --- | --- |
| [헤비멕1 문서 목차](https://github.com/gelb2/HeavyMach/blob/main/docs/public/index.md)와 [구현 상태](https://github.com/gelb2/HeavyMach/blob/main/docs/public/implementation-status.md) | 처음 플레이하는 사람이 실행·조작·빌드로 바로 이동하게 한다. 구현·배포·검사 범위는 별도로 찾아볼 수 있게 한다. |
| [맥디펜스 문서 목차](https://github.com/gelb2/MachDefense/blob/main/docs/public/README.md)와 [영문 목차](https://github.com/gelb2/MachDefense/blob/main/docs/en/public/README.md) | 한·영에서 설치, 조작, 기능, 코드 구조, 트러블슈팅, 검증을 같은 순서로 찾게 한다. 개발 정본과 과거 증거는 목차의 별도 구역에 둔다. |
| [헤비멕1 기능 소개](https://github.com/gelb2/HeavyMach/blob/main/docs/public/features.md)와 [맥디펜스 플레이 갤러리](https://github.com/gelb2/MachDefense/blob/main/docs/public/gameplay-gallery.md) | 사용자가 추가하도록 지시한 조건을 항목별로 설명하고, 실제 동작을 보여 주는 캡처와 캡션을 옆에 둔다. 결함 제보와 회귀 수정은 트러블슈팅에서 찾게 한다. |

## 작업 순서

1. **한·영 문서 목차를 맞춘다.** [국문 목차](README.md)와 [영문 목차](en/public/index.md)를 플레이, 설치·빌드, 조작·저장, 추가 기능·갤러리, 검증·문제 해결, 개발·출처 순으로 정리한다. 기존 URL과 문단 링크를 유지한다. README는 첫 실행 절차로 연결하고, 공통 준비 명령과 플랫폼별 빌드 절차는 설치 문서에서 관리한다.
2. **화이트마운틴과 미등장 콘텐츠의 공개 화면을 보강한다.** [기능 소개](public/features.md) 두 절에는 현재 설명과 실행 기록 링크가 있다. [공개 갤러리](public/gameplay-gallery.md#화이트마운틴-보스전)는 Boss 05 전투를 보여 준다. [비공개 보고 기록](report-site.md#화이트마운틴-구현과-치트-검증--2026-11-20)에는 이미지 83장의 게시 근거가 있다. 공개에 쓸 게임 화면을 골라 설원 진입·포털, 적·보스 출현, 무기 입수·장착·발사, 레이저 공습, 수집 BOT을 항목별로 연결한다. 부족한 장면은 캡처한다. [캡처·캡션 계획](white-mountain-public-documentation-plan.md)
3. **플랫폼별 현재 상태와 버전을 대조한다.** [정본](canonical.md#플랫폼과-검증-현황), [기능별 근거](feature-verification.md), [배포 기준](public/release-policy.md)에서 현재 값을 확인한다. README 검증 표의 갤럭시 v0.2 배경음 기록과 정본의 v0.3 전체 화면 기록은 각각 날짜와 확인 동작을 적는다. 빌드 성공, 실기기 설치, 직접 플레이, 시뮬레이터 검사를 구분한다. 지난 검사 수치는 날짜별 기록에서 찾을 수 있게 한다.
4. **공개 기능 설명과 영문판을 맞춘다.** 현재 영문은 Quick Play, 빌드·설치, 검증, 목차가 있다. 기능 소개와 화이트마운틴 캡션부터 작업하고, 코드 구조·트러블슈팅·개발 과정·테스트 방법을 차례로 번역한다. 새 영문 문서를 올릴 때 해당 국문도 함께 확인하고 양쪽 목차에 연결한다. 개발자의 변과 사용자 원문은 편집 범위를 명시한 별도 작업에서 다룬다.
5. **공개 결과를 확인한다.** 항목마다 작업 브랜치에서 기존 링크, 한·영 문단과 캡션, 실제 이미지 파일, 배포 버전을 확인한다. 각 항목을 따로 병합하고 공개 `main`의 문서·이미지·다운로드 링크를 연 뒤 다음 항목을 진행한다. TODO와 날짜별 검사 기록의 증거 링크를 사용한다.

현재 해야 할 일은 [TODO](todo.md#github-문서와-개발자의-변--2026-11-20)에 기록한다.
