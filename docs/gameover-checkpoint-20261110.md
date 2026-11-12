# 게임오버 연결 중간 기록 — 2026-11-10

사용자의 “적당한 선에서 끊고 커밋해 그리고 멈춰” 지시에 따라 현재 구현을 중간 저장한다. 이 기록은 게임오버 전체 검증이나 배포 완료가 아니다.

## 반영한 코드

- Player의 기존 HP/SP 처리 다음, 표시된 actor의 unpaused state2에서 원본 processCrash/onDestroy에 맞춰 state3과 World gameState3을 연결했다. 임의 사망 대기·pause·저장·즉시 회복은 추가하지 않았다.
- World 상태를 바꿀 때 원본 렌더 시간도 기록한다. 게임오버의 반복 상태 진입은 알림을 다시 요청하지 않는다.
- Session에 게임오버 확인 token을 추가했다. 다른 슬롯 선택·장면 교체 때 이전 요청을 무효화하고, 확인 때 lastBase→location과 기존 기지 문 전환을 요청한다. 회복·BOT HP 초기화·저장은 기존 enterBase를 사용한다.
- Apple 요청에 별도 gameOver 종류와 token을 추가하고 원문 제목 `Destroy Your Mach.`, Description 키 `DestroyMach`, OK 한 개를 연결했다. 정상 앱의 실제 OS 알림 실행은 아직 검사하지 않았다.
- 파괴 때 weaponReady를 임의로 false로 바꾸지 않았다. 원본 getter/부품 controller에서 새 HP/state 조건을 추가할 근거가 없음을 별도 원본 조사에서 확인했다.

## 실제 확인한 범위

수정 전 공통 코어에서 시작 HP1의 실제 Stage01_01 전투로 사망을 재현했다. 강제 피해·HP0 주입은 하지 않았다. 적4회 발사와 실제 피해 처리 뒤587번째 프레임에서 Player state2·World state0이었고, 다음 tick에도 그대로이며 알림0이었다. 원본의 state3/게임오버 소비 누락을 assert 실패로 확인했다.

수정 후 같은 probe에서 같은 사망 프레임의 state2/World0 다음 tick에 state3/World3과 DestroyMach 알림1회를 확인했다. 공통 코어 빌드와 이 probe 실행은 exit0이다. 원본 계약은 기존 함수와 추가 로컬 REA로 독립 확인했으며 새 분석 세션은 종료했다. 구현 diff의 독립 검토와 전체 회귀는 아직 하지 않았다.

로컬 근거는 `.local/evidence/gameover-restoration-20261110/`의 `baseline.cpp`, `baseline-red.log`, `baseline-green.log`, `core-initial-build.log`, `reference-contract/contract.md`와 manifest다. 원본 IPA·ARM·저장·서명 비밀은 커밋하지 않는다.

## 재개할 작업

1. pause/show 경계, gameState 변경 시각·멱등, 실제 알림 token·중복/오래된 응답·슬롯/Session 교체의 정식 회귀 검사 추가.
2. 확인→문 전환→다른 lastBase 복귀→body bonus 포함 회복·BOT savedHP3개 초기화·기존 저장1회와 재시작 검사. 저장 실패도 기존 오류 처리로 검증.
3. Apple 빌드와 iPhone/iPad 시뮬레이터의 실제 OS 알림·OK·기지 복귀 확인. 현재 Apple 변경은 빌드/실행 미검증이다.
4. 기존112조건과 함께 돌아가는 실제 사망·복귀 동시 치트 조건 추가. 전체 Core/메모리/Native 회귀와 구현 독립 검토.
5. 결과와 한계를 기존 비공개 HTML 보고 사이트에 게시한 뒤 후속 완료 임무 창/5007·랭킹/5009 작업 진행.

이번 중간 구현으로 develop/main 병합·실기기 갱신·새 HTML 게시를 하지 않았다. 기존 그래픽·음향 수정 보고276과 해당 단계의 성공 검사는 이 새 구현의 전체 검증을 대신하지 않는다. 기존 포팅판 미커밋 작업은 별도로 보존한다.
