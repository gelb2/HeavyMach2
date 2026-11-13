# HeavyMach2 iPhone 작업 인계

이 문서는 맥락이 없는 다음 세션의 작업 진입점이다. 실제 조사 결과와 개발 기준은 아래 연결 문서에서 읽는다. 목록만 출력한 것을 읽기로 간주하지 않는다.

## 현재 사용자 지시와 작업 위치

최신 실행 지시: iPhone 개발을 진행한다. 사용자는 실제 iPhone을 계속 사용 중이므로 실기기 연결·빌드·설치·실행을 하지 않는다. 공통 표시·UI·입력·정식/치트 연결을 모두 구현한 뒤 마지막 시뮬레이터 검사 한 묶음을 실행한다. 구현 중 기능별 시뮬레이터 실행·포팅판 왕복은 하지 않는다. 그전의 정적 검사·공통 C++ 검사와 컴파일로 결함을 찾는다. 최종 묶음이 실패하면 실패를 보존하고 통과로 보고하지 않는다.

보고서는 기존 첫 화면과 HeavyMach2의 고정 보고 목록에 계속 누적 연결한다. 새 결과를 개별 링크로만 전달하지 않는다.

- 저장소: `/Users/sokol/Git/ProjectGameDev-Hub/HeavyMach2`, 비공개 `gelb2/HeavyMach2`.
- 브랜치: `feat/iphone-universal-20261113`. `origin/develop`에서 분기했고 기존 조사 브랜치를 병합했다. 준비 커밋은 `7247972`다. 이 문서의 존재를 게임 구현 완료로 해석하지 않는다.
- 최신 요청: 남은 원본 차이 조사를 진행하고, 조사 기록·개발 계획·발견한 금지사항을 모두 문서로 보존한다.
- HD 에셋 한 벌과 공통 C++20 코어를 유지한다. iPhone의 표시 크기는 일반판, iPad의 표시 크기는 HD 포팅판 기준과 연결한다. 게임 규칙·콘텐츠·저장은 HD 기준이다.
- 최초 목표는 iPhone 가로·iPad 세로다. 사용자가 확인한 현재 iPad 플레이와 저장을 보존한다. iPhone으로 HD 정상동작을 판정하지 않는다.
- HeavyMach1·MachDefense·분리 전 ProjectGameDev·허브의 기존 파일은 이 작업에서 수정하지 않는다.

## 반드시 읽을 문서

1. `AGENTS.md`, `.agents/policy.md`, `.agents/project.md`.
2. [공통 개발 기준·작업 순서·금지사항](docs/iphone-development-contract.md).
3. [추가 조사 결과와 미확정](docs/iphone-followup-investigation-20261113.md), 그 안의 네이티브·UI·시간/저장/음향 상세 근거.
4. [정식판·치트판·포팅판의 역할과 동시 검사](docs/validation-build.md), [TODO](docs/todo.md).
5. 실제 구현 전에는 [실행 계약](docs/runtime-contract.md)과 [기술스택·에셋 절차](docs/technology-and-assets.md)를 함께 읽는다.

## 이전에 끝낸 범위

- 여섯 IPA의 파일9,123개 전수 비교와 원본 해시 전후 일치를 확인했다.
- 이미지7,740항목을 기존 공통 decoder로 읽었고 오류0이다. 원본 GPU 출력이나 조립된 메크의 전체 화면 동등성 검사가 아니다.
- 일반판1,077·HD1,300 함수 몸체의 목록·ASM·의사 코드를 보존했다. 모든 함수 의미 대조가 끝났다는 뜻은 아니다.
- 사용자 직접 관측: 실제 iPad에서 Wasteland까지 진행했고 당시 크래시를 발견하지 않았다. 일부 이동·전투·스킬·임무도 확인했다.
- 기존 iPad 치트 실행은115조건·1,200프레임이다. 전체 게임 분기·전체 임무·iPhone 화면 검증 완료가 아니다.
- 상세 기록은 [초기 조사 결과](docs/universal-design-audit-results-20261113.md)와 [현재 검증 범위](docs/public/validation.md)에 있다. 이번 추가 조사 결과가 같은 항목을 보강하면 그 근거를 우선한다.

## 다음 세션이 재사용할 위치

- 원본 명세: `docs/source-provenance.json`. 원본 IPA는 저장소 밖에서 읽는다.
- 분석 원문: `/Users/sokol/Documents/Codex/HeavyMach2/Analysis/`. 원본·전체 함수 본문을 Git이나 보고 사이트에 올리지 않는다.
- 기존 함수 인덱스: `.local/evidence/universal-native-audit-20261113/`.
- 추가 조사 근거: `.local/evidence/iphone-universal-*-followup-20261113/` 및 `.local/evidence/iphone-universal-integration-20261113/`.
- 비공개 보고 사이트: [접근 범위·Site ID·게시 기록](docs/report-site.md). 토큰을 문서·파일·명령 인수에 넣지 않는다.

기존 근거를 재검사할 때는 `python3 tools/verify_iphone_investigation.py`를 사용한다. 함수·원본 참조 파일의 현재 바이트와 저장한 해시를 비교하며 게임은 실행하지 않는다. 실제 소비 코드가 조사 뒤 바뀌었다면 해당 근거를 갱신하거나 조사 당시 소스와 대조해야 한다. 새 게임 코드의 기능 검사를 대신하는 명령이 아니다.

## 재개할 때 확인할 것

현재 HEAD·브랜치·dirty 상태를 먼저 확인한다. 기존 조사와 SHA가 맞는 근거를 재사용한다. 조사·문서 커밋을 임의로 되돌리거나 새 세션이라는 이유로 처음부터 빌드를 만들지 않는다. 게임 코드 수정, 검사 통과, 앱 설치, develop/main 병합, 사이트 게시를 각각 구분한다. 현재 남은 일은 추가 조사 표와 TODO의 최신 체크 상태에서 읽는다.

## 2026-11-13 구현 후 진입점

현재 구현·검사·발견 수정은 [iPhone 공통 화면 구현](docs/iphone-implementation-20261113.md)에 누적한다. DisplayLayout은 원작 조립 뒤 출력/역입력을 담당한다. 정식판·115개 독립 치트 상태는 같은 HD 자원과 C++ 코어를 쓴다. 전체73개 C++ 검사와 관련 재검사, Apple test build/별도Validation 컴파일을 확인했다. 시뮬레이터 최종 검사 한 묶음의 결과는 위 문서와 로컬 `iphone-implementation-20261113/final-simulator-batch/summary.json`을 확인한다. 실행 중이면 완료로 가정하지 않는다. 실기기 연결·설치는0회다. 사용자가 정한 한 묶음을 이미 실행했다면 자동 재실행하지 않는다.

최종 한 묶음은 iPhone16/20·iPad18/20으로 실패 상태다. 공통 첫 기지 Help 누락과 iPhone 검사 창 세로 크기/HD-point 혼용을 검사 코드에서 수정하고 CPU73·Apple 컴파일을 확인했다. 수정 뒤 Native 재실행은0회이며 통과로 간주하지 않는다. 상세한 기기별 결과와 원인은 위 구현 문서·evidence JSON을 읽는다.
