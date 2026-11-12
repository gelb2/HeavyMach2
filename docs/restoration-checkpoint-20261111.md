# 복원 작업 중간 저장 — 2026-11-11

사용자 지시로 전체 복원 골을 일시정지했다. 작업 브랜치는 `feat/reference-ports`다. 게임 개발을 재개하지 않고 현재까지 완료한 변경과 남은 일을 기록한다.

## 완료한 변경

- `15d44e0`: 완료 임무 자동 보상·결과창·5007, 수락 직후 목록 입력 수정. Core70개·ASan/UBSan68개 및 이후 관련 검사, 양 Simulator Debug Source 실제 입력·앱 재시작, 동시115조건 각1,200프레임·메뉴360프레임을 검증했다.
- `7cae2b2`: 검증 결과·TODO·기존 비공개 사이트 게시 기록. [완료 임무 HTML 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/completed-missions.html)의 실제7경로 HTTP·본문/링크·JSON/원문/PNG 일치를 확인했다. Safari native pipe 오류로 해당 페이지의 브라우저 시각 검수는 미완료다.
- 완료 임무 단계의 세부 결과와 한계는 [복원 보고](completed-missions-restoration-20261111.md), [검사 근거](evidence/completed-missions-results-20261111.json), [사이트 기록](report-site.md)에 보존했다.

새 사이트 archive의 게시·접속 확인 후 이전 생성 archive 256,497,519바이트를 사용 중인 프로세스가 없음을 확인하고 정리했다. 재생성 근거와 해시는 `.local/evidence/completed-missions-restoration-20261111/archive-cleanup.json`에 남겼다. 원본 IPA·저장·소스·검사 로그·캡처·xcresult는 보존했다.

## 중단한 후속 조사

사령부에서 임무를 선택할 때 상세창이 펼쳐지는 원본 동작을 조사했다. 현재 구현은 즉시 전체 크기로 표시한다. 원본 `refreshMissionInfo 0x26af8`·`tick 0x26558`의 보간과 같은 행 재선택 시 초기화, ACCEPT 버튼과 텍스트의 서로 다른 배율 처리를 확인 중이었다. 이 후속 기능의 코드 수정·회귀 검사·원본 실행 대조는 하지 않았다.

확보한 로컬 REA 근거는 `.local/evidence/completed-missions-restoration-20261111/reference-followup/command-animation/`에 보존한다. 조사 중간 결과를 완성된 구현 계약이나 원본 앱 실행 검증으로 취급하지 않는다.

## Codex 오류 점검

사용자가 제공한 모델 오류를 별도로 점검했다. 2026-11-11 07:07:49 KST, 이 대화의 `gpt-6.1-sol` 요청에서 HTTP400과 최신 Codex 요구 메시지가 기록됐다. 오류를 기록한 서버 PID는33435이며, 10월25일 시작한 `0.164.0-alpha.11.1`이었다. 설치된 CLI와 앱 내 CLI는 `0.167.0-alpha.3.2`였다. `app-server daemon version`도 설치 버전과 실행 서버 버전의 차이를 반환했다.

같은 세션에서 오류 전 해당 모델 사용 기록도 확인했다. 서버 측 요구가 언제 바뀌었는지는 확인하지 않았다. 공유 데몬의 갱신·재시작은 실행하지 않았다. 게임 코드·빌드 오류로 분류하지 않는다.

## 재개 시 남은 범위

1. 임무 상세창 펼침의 원본 계약을 마무리하고 누락을 구현·검증한다.
2. 원본 포팅판에서 완료창·게임오버·중단/복귀를 같은 조건으로 대조한다.
3. 전체60임무·캠페인과 하체23종, 표시·입력·음향·저장 경로의 미검증 범위를 처리한다.
4. iPhone 가로·iPad 세로의 원본 비례·HUD·입력을 확정한다. 현재 iPhone 검사 통과를 최종 가로 구성 완료로 취급하지 않는다.
5. iPad Simulator의 기지 복귀15초 성능 기준 실패를 조사한다. 기능 검사 통과와 구분한다.

실기기 설치 전달·다른 플랫폼 확장·develop/main 릴리스는 보류한다. 전체 골은 완료 상태가 아니다. 기존 미커밋 Compatibility·참고 포팅 계획·보존 문서·패키지 검사는 이번 완료 임무 변경에 섞지 않고 보존했다.
