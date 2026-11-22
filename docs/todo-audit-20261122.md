# TODO 원문과 항목별 대조

[현재 TODO](todo.md) · [감사 전 원문 전체](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md)

`main` `e5e41b0`의 936줄을 기준으로 봤다. 이전 체크 473개에는 중복, 사용자 관측, 후보별 실행, 폐기된 하네스 상태가 함께 있다. 아래 원문 링크는 각 절의 전체 범위다. 수정 커밋과 발표·배포의 PR, 빌드의 버전과 기기 실행을 맞췄다. 원문 일부가 완료여도 별도 근거가 부족한 다른 플랫폼·실제 기기는 현재 TODO의 열린 검사에 연결했다. 추정·실패·보류·사용자 원문은 앞선 커밋에 그대로 남아 있다.

## 최신 배포와 기기 근거

| 대상 | 근거 |
| --- | --- |
| Apple·Android 저장 이동 | [PR #70](https://github.com/gelb2/HeavyMach2/pull/70), [PR #71](https://github.com/gelb2/HeavyMach2/pull/71), [호스트별 검사](save-transfer.md#호스트별-확인-범위). 11/22 Apple Release·갤럭시 v0.4 설치 기록, iPhone 시뮬레이터 런타임 통과, Android 에뮬레이터 백업·세 슬롯 보존을 대조했다. iPad 쪽 새 XCTest의 기기 실행 실패와 iPhone XCTest가 시작한 뒤 끝까지 통과하지 못한 기록을 확인했다. |
| PWA | [Pages v0.3 파일](https://github.com/gelb2/HeavyMach2/commit/e40314f), [실제 공개 `build.json`](https://gelb2.github.io/HeavyMach2/build.json), [갤럭시 v0.2·v0.3](pwa-lifecycle-audit-20261121.md), [Mac·모바일 후보](pwa-device-validation-20261120.md). Pages `8d11a10`은 공유 이미지 변경이다. `build.json`의 `v0.3`은 그대로다. |
| Mac과 공개 클론 | [PR #35](https://github.com/gelb2/HeavyMach2/pull/35) · [PR #37](https://github.com/gelb2/HeavyMach2/pull/37) · [공개 클론](public/validation.md#공개-클론에서-빌드와-실행) · [공개 v0.1 실행](public/release-policy.md#v01-공개-배포-확인). 공개 ZIP과 후속 정식·치트 빌드의 직접 조작 범위가 다르다. |
| 설원과 iPad | [캠페인 PR #21](https://github.com/gelb2/HeavyMach2/pull/21), [후속 PR #22](https://github.com/gelb2/HeavyMach2/pull/22), [두 실기기 설치 명세](evidence/white-mountain-dual-release-20261120.json), [시뮬레이터 캡처 PR #109](https://github.com/gelb2/HeavyMach2/pull/109), [해금·드롭 안내 PR #110](https://github.com/gelb2/HeavyMach2/pull/110). 실제 정식판의 포털·사령부·일반 획득 조건 확인을 공개한 시뮬레이터 캡처와 분리한다. |
| iPhone·iPad 표시와 상점 | [실행 명세](evidence/iphone-runtime-regressions-20261118.json), [상점 후보 명세](evidence/iphone-shop-layout-verification-20261119.json), [정식 후보의 실제 조작](dual-release-installation-20261119.md) 및 [iPad GPU·회귀](evidence/ipad-field-regression-20261115.json). 원본 동일 조건을 모두 검사하지 않은 상태를 유지한다. |

## 원문 절별 대조

| 원문 | 현재 위치 | 확인한 범위와 남은 일 |
| --- | --- | --- |
| [HeavyMach 2 TODO](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L1-L6) | 대조 기준 | 원문 936줄을 고정하고, 당시 체크박스와 각 후보·실행·실패 이력을 보존했다. |
| [현재 체크 — 2026-11-21](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L7-L27) | 현재 검증·1~5차 | PR #27·#63·#71, Pages·Releases·기기 기록을 대조했다. Mac v0.1·PWA v0.3·Android v0.4를 분리하고 표의 남은 항목을 각 단계로 옮겼다. |
| [네이티브 정식판 복구코드와 설치 — 2026-11-22](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L28-L33) | 현재 검증·1/2차 저장 | PR #70/71, 저장 이동 문서와 로컬 Release 로그. Apple은 시뮬레이터 런타임, 실기기 설치, OS 복구창을 분리했다. Android 실제 교체는 미완료다. |
| [GitHub 문서와 개발자의 변 — 2026-11-20](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L34-L58) | 현재 검증 문서·캡처 | PR #73~109와 공개 README, 정리·번역·갤러리 및 20개 문서 점검을 맞췄다. 문서의 증빙 게재와 기기 검증을 분리했다. 대조 29/31의 나머지는 3차다. |
| [3부작 복원 소회 — 사용자 원문](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L59-L62) | 원문 | 사용자 소회는 이 링크의 문장을 원문으로 보존한다. 공개 반영은 `b9efa0b`와 한·영 README에서 확인했다. |
| [공유 복구코드와 웹·Mac 후속 작업 — 2026-11-20](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L63-L82) | 현재 검증 PWA·1~3차 | Wasm 조사와 설치형 개발 근거는 PR #25/27, 가로/음향·전체 화면은 PR #63/67과 Pages 기록을 대조했다. 기기·실제 OS의 갱신/회전/복원은 공개판별로 남겼다. |
| [관련 실행 결과와 남은 검증](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L83-L109) | 1~4차 PWA·Mac·저장 | 공개 코드 PR #65와 저장 문서, 검은 화면 `46e9a08`·`14b4b56`, OpenAL `f03ad93`을 확인했다. 미사용 분기·제목·동시 쓰기·자동 검사 로그·기기 갱신을 따로 추적한다. |
| [브라우저 탭과 후속 방향 지원](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L110-L114) | 1/2차 재현·보류 | Wasm 보고의 탭 874×338은 재현, 설치 PWA의 실제 화면은 별도다. 태블릿 가로는 사용자 범위 보류다. |
| [선행 게임의 복구 모듈](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L115-L119) | 현재 검증 저장 이동 | 원문에 링크된 HeavyMach 1·MachDefense 구현을 설계 근거로 유지한다. HeavyMach 2의 실행 결과는 코드와 호스트별 기록에 근거한다. |
| [Android 구현과 실기기 확인 — 2026-11-20](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L120-L127) | 현재 검증 Android·2/3차 | PR #23, Android 실기기 노트와 공개 APK 기록을 대조했다. 장시간·태블릿 잘림·첫 Help가 남았다. |
| [작업 브랜치의 현재 상태 — 2026-11-19](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L128-L138) | 2/3차 상점 및 설원 | 설원 변경 이력과 사용자 저장 사본을 대조했다. iPhone LS-102와 iPad 특수공격 02006은 별도 정식판 검사다. |
| [화이트마운틴과 미등장 콘텐츠 구현 — 사용자 지시 2026-11-19](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L139-L157) | 현재 검증 화이트마운틴·2차·보류 | PR #21, `75743aa`, `58b0938`, `90b0e6c`와 설치/치트 명세를 대조했다. 포털 14·보스 4·장비 29, 안내 글꼴·적 밀도·피해, 실제 iPhone 진입 및 iPad 미확인을 구분했다. |
| [화이트마운틴·미등장 콘텐츠 기술조사 — 사용자 지시 2026-11-19](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L158-L167) | 현재 검증 원본 조사·보류 | 6판본과 MachDefense 설원 비교, 미등록 6장과 배치 미정 콘텐츠, 레이저 재생 요청의 코드·PCM 근거를 유지했다. 추가 스킬과 마지막 대조는 남겼다. |
| [포팅 모듈 분석 결과 — 2026-11-18](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L168-L182) | 현재 검증 이동·2/5차 | `20ccf0d`, `e57a623`, `5b07e44`, `ab41cda`, `f8e82d8`의 변경·GPU·설치를 대조했다. size class와 화면 논리 프로필의 차이를 2차 회귀 근거로 둔다. |
| [원본 분석 목적·복원 모듈의 부작용 — 2026-11-16 추가 지시](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L183-L189) | 5차 원본·모듈 | 신규 13/변경 48파일과 3함수 분석, 배율 단일 갱신의 CPU 재현을 구분했다. 원본 의미와 실제 Skyway·기기 결과는 진행 중이다. |
| [상점 오류 수정 후 원본 분석 — 2026-11-16](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L190-L196) | 현재 검증 텍스처·2/5차 | `1420cc1` 첫 텍스처 준비와 `f8e82d8` 설치를 확인했다. 원본 독립 조사와 동일 제보 순서는 미완료다. |
| [iPhone 점프 실패·원본 포팅판 전체 로직 대조 — 사용자 지시 2026-11-16](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L197-L210) | 2/5차 이동·원본 | 점프 제보와 SP·path 추정을 원문에 유지했다. 수치 입력과 카메라 수정의 통과, 물리 기기 반복과 전체 원본 조사가 따로 있다. |
| [iPhone 전수진단 결과 — 2026-11-16](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L211-L223) | 2~5차·훅 이력 | 텍스처 오류와 Skyway GPU 캡처 근거가 있다. 거래/차가운 캐시 전수, HD 로딩과 타이틀 시간은 남았다. 자동 훅의 stale 실패는 당시 기록에 보존했다. |
| [상점 크래시·Skyway 레이어 떨림 — 사용자 제보 2026-11-16](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L224-L238) | 2/3/5차·제보 원문 | 상점 시스템 얼럿 사진과 레이어 떨림, 사용자 iPad 관측을 유지했다. 이후 수정은 텍스처와 카메라 기록에서, 동일 기기·저장 재시험과 코드 감사를 현재 항목에서 추적한다. |
| [현재 남은 일 — 2026-11-16 검토](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L239-L242) | 남은 작업 순서 | 당시 후보 `7a69b90`과 PR #20을 이후 설치·구현과 대조했다. 구체적인 작업과 조건은 1~5차 및 보류로 정렬했다. |
| [완료한 수정과 남은 확인](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L243-L251) | 현재 검증·2차 | iPad 배율·Phone 페이지·배너의 시뮬레이터 통과, 실제 설치, 물리 기기 전수 잔여와 훅 이력을 분리했다. |
| [남은 수정·조사·검사](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L252-L274) | 2~5차·보류 | 상점·타이틀·임무·화이트마운틴·복귀·음향·계측·HUD·방향·음원·전달의 14개 순서를 후속 근거에 맞춰 합쳤다. |
| [iPad 인게임 출력 회귀 — 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L275-L281) | 현재 검증 iPad·2차 | 공통 수정과 Native·GPU 후보의 통과, 11/16 실기기 설치를 확인했다. 해당 시험의 실제 메뉴·전장 재검증을 남겼다. |
| [화이트마운틴베이스 원본 조사 — 사용자 지시 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L282-L295) | 현재 검증 원본 조사·2차 | 원본의 에어리프트 방문·미등록 설원 자료 조사와 신규 Base006·설원 구현을 분리했다. 실제 기기 첫 진입·사령부는 2차다. |
| [iPad 메뉴 배율·검은 여백 회귀 — 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L296-L303) | 현재 검증 iPad·2차·훅 이력 | 원인 `7622bd1`과 수정·36장 검사, 실제 설치를 근거로 삼았다. 자동 발동과 실제 플레이 항목을 별도 유지했다. |
| [iPhone 탭 투 플레이 구도 조정 — 사용자 지시 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L304-L310) | 3차 타이틀 | 기존 HD 그림 적용과 후속 구도 수정(메크·하늘·석상)을 분리하고 iPhone 캡처를 완료 조건으로 뒀다. |
| [상점 원본 Phone 표시 일치 — 사용자 추가 지시 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L311-L331) | 현재 검증 상점·2/3차 | 원본 Phone의 몇 상품·40단위 크기와 20·5셀·거래창 검사, `f8e82d8`의 실제 겹침을 비교했다. 전체 원본 5분류와 최종 실기기 거래는 미완료다. |
| [메뉴 하네스·원본 Phone 배치·탭 투 플레이 — 사용자 지시 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L332-L350) | 3/4차·훅 이력 | Phone 배경·페이지·16사례, 앵커/배너 근거를 살렸다. 가변 보상과 배치 전수, 타이틀·로딩·전장 회귀는 묶되 실제 검사로 남겼다. |
| [독립 배경 검사와 Phone 에셋 — 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L351-L363) | 4/5차·훅 이력 | 독립 배경 36/36과 Phone 자원 판별을 확인했다. 메뉴 허브 전체와 사용자의 거래 조건, 제거된 훅의 자동 발동은 서로 다른 범위다. |
| [약식 하네스 도입 경위 — 사용자 기록 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L364-L368) | 문서와 원문 | 약식 하네스 도입 경위는 작성자 기록으로 보존하고 개발 과정과 연결했다. 신규 게임 과제는 없다. |
| [디자인·에셋 정본과 기존 로컬 하네스 기록 — 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L369-L384) | 4/5차·훅 이력 | 디자인·에셋 정본 적용과 기존 하네스 조사/철거를 분리했다. 판정 경계와 원본 정상 검은색의 구분은 화면 회귀 조건으로 유지한다. |
| [타이틀·스테이지 간 로딩 일러스트 — 사용자 추가 지시 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L385-L392) | 3차 타이틀·로딩 | 원작 HD 로딩 이미지, 화면 위쪽 정렬, Phone 허브 배경과 시간/음향 보존은 실출력으로 검사해야 한다. |
| [HUD 선택·메뉴 스크롤·가로 배너 — 사용자 추가 지시 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L393-L406) | 4차 화면·보류 | 검정 HUD 캡처와 세로 스크롤 제보를 열린 상태로 둔다. 11조각·두 무기 패널 등 롤백 요구는 별도 보류다. |
| [iPhone 비전투 일러스트·버튼·스크롤 재점검 — 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L407-L422) | 2/3/4차 메뉴·거래 | Phone 메뉴별 그림과 배치, 네이티브 실기기 구매 불가·취소·예외창, iPad 상태의 구분을 유지했다. 16건 성공으로 전수를 닫지 않았다. |
| [최신 정식판 설치 후 화면·포털 조사 요청 — 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L423-L440) | 2/4/5차·보류 방향 | 원본 배경 샘플링, 카메라 경계, HUD 표시·터치, 허브·상세의 스크롤 없이 배치를 검증한다. 기존 포털 명세와 최종 물리 대조를 분리했다. |
| [사용자 화면 수정 지시 — 2026-11-14 카메라 설치 후](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L441-L449) | 2/4/5차 화면 | 카메라 설치 뒤 사용자 화면 요구와 실제 관측을 살렸다. 배경·버튼·HUD 정책의 후속 시험과 원본 동적 비교를 연결했다. |
| [최신 iPhone 설치 후 사용자 관측 — 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L450-L460) | 4차 이전 화면·생명주기 | `bc3323a`의 포그라운드 게임 전체 정지와 슬롯 전장 직행은 제보 상태다. 과거 실행을 재현·수정 완료로 승격하지 않았다. |
| [iPhone 수정·측정 순서 — 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L461-L468) | 2/4/5차 | 수정→동시 치트/iPad 회귀→실기기 계측 순서를 현재 후보에 적용한다. 계측·성능·동등성은 아직 별도다. |
| [iPhone 사용자 피드백·배경 일러스트 분기 조사 — 2026-11-14](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L469-L484) | 3/4차 이전 화면·생명주기 | 중단·전화·복귀·컨텍스트·발열·가변 줌과 일러스트 그림 선택을 분리했다. 후속 카메라 수정의 실제 장시간·OS 관측은 남는다. |
| [남은 원본 조사·계획·금지사항 보존 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L485-L499) | 5차 원본·모듈 | 일반판/HD 샘플, 59개 테이블, 초기화·상태·포털·필수 버튼/Enemy 예외 및 `Screens::coverage()` 갱신을 살아 있는 비교 기준으로 유지한다. |
| [iPhone 개발 브랜치와 동시 검증 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L500-L509) | 현재 검증·5차 | 후속 공통 화면 적용과 126조건 실행이 이전 개발 개시의 미완료 표시를 대체한다. 전체 원작 대응·전수는 남겼다. |
| [유니버설 설계 보강·원본 차이 조사 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L510-L525) | 5차 원본·모듈 | 4+2 패키지와 PNG/JPEG, 함수 본문 1,077/1,300의 전수 정적 수집을 확인했다. 표시 비례와 함수의 의미 실행은 미완료다. |
| [README·플레이·설치 안내 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L526-L530) | 현재 검증 문서 | 한·영 README·Quick Play·설치의 후속 배포와 원래 iPad 관측을 대조했다. 최신 안내는 원격 PR #93/105에 반영됐다. |
| [탭투스타트 일러스트 앱 아이콘 — 2026-11-12 최종 지정](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L531-L537) | 현재 검증 아이콘·2차 갱신 | 11/12 iPad 실기기 아이콘, 28,204바이트 저장·치트 115/1,200을 확인했다. `iTunesArtwork` 옛 선택과 설치형 아이콘 갱신은 다르게 기록한다. |
| [세 작품 사운드 출처·라이선스 조사 — 2026-11-12 사용자 요청](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L538-L544) | 5차 음원 출처 | 소비·PCM/MP3 대조와 라이선스 또는 판매처 확인은 별개다. 출처의 추정은 확정으로 처리하지 않는다. |
| [Wasteland 진행 — 2026-11-12 사용자 확인](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L545-L550) | 현재 검증·4/5차 | Wasteland까지의 사용자 실제 플레이는 확인한 범위로만 유지한다. 전체 캠페인의 무충돌·발열 판정은 미완료다. |
| [개발자의 변 마지막 시 — 사용자 원문](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L551-L558) | 원문 글감 | 마지막 시와 앞 문단은 이 링크의 원문을 유지한다. 개별 후속 원고·개발 과정과 새 할 일 표에서 본문을 중복하지 않는다. |
| [스카이베이스 주변 플레이 — 2026-11-11 사용자 확인](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L559-L568) | 현재 검증·5차 | 기지 임무·이미 가진 물품의 수락 사용자 관측을 공개 검증과 맞췄다. 전체 임무 지급·완료로 확대하지 않았다. |
| [퀘스트 아이템 지급 확인 — 2026-11-11 사용자 피드백](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L569-L573) | 3/5차 임무 | 크롬 10개 예와 다른 임무의 실제 복원/지급 여부를 수락·현장·완료 보상·알림·저장으로 나눈다. |
| [임무 달성 알림 누락 — 2026-11-11 사용자 피드백](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L574-L579) | 3/5차 임무 | 실제 iPad에서 보고된 E-10 효과음·토스트, 다른 격파/수집 조건은 미완료다. 완료 임무 UI 시뮬레이터 통과와 분리했다. |
| [iPad 정식판 사용자 확인·앱 아이콘 — 2026-11-11](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L580-L587) | 현재 검증·2/5차 | 실제 iPad 정식 앱 시작·설치와 아이콘을 당시 후보로 보존했다. 치트 자동 결과는 시뮬레이터 범위에 둔다. |
| [현재 범위 — 2026-11-11 재개](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L588-L599) | 현재 검증·4/5차 | 첫 플레이 구간, 공통 결과와 남은 핸드오프를 분리했다. 넓은 검증에 설치·정식·치트 판정을 따로 적용한다. |
| [게임오버·랭킹 후속 — 2026-11-11](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L600-L610) | 4차 성능·보류 랭킹 | 게임오버 흐름과 랭킹 이용 불가 안내는 연결했다. iPad 16.07초 실패와 외부 서비스 검증은 별개다. |
| [완료 임무 후속 — 2026-11-11](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L611-L626) | 3/5차 임무 | 완료 화면·수량·저장의 부분 실행은 확인했다. 재저장 뒤 신규 행 순서와 60임무 전체는 남긴다. |
| [사용자 중지 요청의 중간 저장 — 2026-11-10](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L627-L631) | 초기 기록·5차 | 당시 중지 시점과 그때 보존한 검사 근거를 유지한다. 후속 목표의 범위를 현재 캠페인/원본 대조로 연결했다. |
| [iPad 사용자 피드백 — 2026-11-10](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L632-L672) | 2~5차 iPad 화면·전투 | 당시 SELL·아이템 선택·이동 마크·적 HP·그림자·레벨업·BOT·거리·출입 표시·한국어 제보를 원문에 보존했다. 후속 부분 동작과 전수 일치 조건을 분리한다. |
| [iPad 직접 확인용 정상 앱 — 2026-11-10](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L673-L679) | 현재 검증·4/5차 | 정상 앱 설치·시작은 해당 후보에서 관측했다. 실제 기기 중단·전체 기능의 최신 자동 실행은 각각 열린다. |
| [전체 기능 치트 앱 계획 — 2026-11-10 후속](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L680-L694) | 5차 전체 치트 | 단계0~6의 공통 소비자·시간·저장과 동시 시나리오 목표를 유지했다. 115/126조건 실행을 전체 데이터·버튼 완료로 확대하지 않았다. |
| [실제 동시 검증 후속](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L695-L703) | 5차 전체 치트 | 독립 상태의 동시 실행 근거와 미연결 분기를 유지한다. 실제 원작 소비자에 연결된 조건을 추가해야 한다. |
| [전투·진행·저장 모듈 후속](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L704-L717) | 현재 검증 기본 구현·5차 | 진행·무기·인벤토리·저장 연결이 후속으로 반영됐다. 23하체·BOT·전 지역·최종 캠페인·전역 RNG·수명 검사는 별도다. |
| [보행 검증·전투 단계 시작 — 2026-11-10](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L718-L729) | 5차 전투 | 보행과 대시·드롭·UI 기본 연결의 후속을 확인했다. 원본 미사일 누락 그림·이동/조준 고정 조건 및 경계 입력은 남는다. |
| [맵·기본 장비 후속 구현 — 2026-11-10](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L730-L753) | 현재 검증 기본 구현·5차 | 기본 맵·장비·무기·전투·메뉴 연결 및 별도 치트 빌드가 이후 있었다. 포팅판 전수 비교와 네 방향 검사는 분리한다. |
| [HM2-DUAL-REFERENCE — 일반판·HD판 비교 앱](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L754-L779) | 5차 포팅판·보류 추가 방향 | R1~R6, hit/가시성/공격 분기·59테이블과 두 원작 환경을 유지한다. 기준 판본과 초기 실행은 확보했고 실제 의미·전체 대조는 진행 중이다. |
| [HM2-STACK-ASSETS — 기술스택과 원작 에셋 절차](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L780-L799) | 현재 검증 에셋·4/5차 | reader·PNG/JPEG·MCB 왕복과 공통 스택, 웹 에셋 소비의 후속을 확인했다. atlas 활성·Tile2D·오류·실제 저장 I/O/OpenAL/GPU 수명은 미완료다. |
| [일반판 판본 비교 — 2026-11-09](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L800-L816) | 5차 원본 환경 | 일반판 1.1.0/2.0.1과 HD의 초기 설치·관측을 유지했다. 큰 진행률·지형 깜박임·이름 오류·전투·저장·음향은 별도로 남았다. |
| [HD의 아이폰 실행·방향 확인 — 2026-11-09](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L817-L836) | 5차 원본 환경·보류 방향 | 판본별 설정·함수와 HD의 iPhone 시험 기록이다. 현재 HD 대조의 기준 기기는 iPad다. |
| [HD 1.0.0 포팅판 실행 — 2026-11-09](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L837-L850) | 5차 원본 환경 | HD1.0.0 타이틀·회전·이름 입력 일부와 CUA 실패를 보존했다. 미완료 플레이·저장 검증을 그대로 OPEN으로 둔다. |
| [HD 가로 지원·전 버전 콘텐츠 조사 — 2026-11-09](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L851-L861) | 5차·보류 방향 | 여섯 IPA 목록·날짜를 확인했다. ItemInfo 12021의 실제 reader·화면 및 HD 가로 시험은 남았다. |
| [퀵 아이템 후속](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L862-L874) | 현재 검증 기본 구현·5차 | 구급상자 6종·공습·대시와 당시 68/106조건, iOS 동시 실행을 확인했다. 전체 BOT·하체·포팅판은 별도다. |
| [등록한 Goal의 후속](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L875-L886) | 5차 초기 구현·보류 랭킹 | 기본 슬롯과 BOT·Help·GameOver·완료 UI는 후속 근거를 확인했다. 로드 슬롯 삭제·실제 랭킹 인증과 전체 게임 전달 검사는 각 범위에 남겼다. |
| [iPhone 공통 화면 구현 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L887-L901) | 현재 검증 iPhone·2/5차·보류 방향 | 공통 표시와 20/20 수정 후보의 통과를 살렸다. 이후 정식 설치와 왼쪽 실제 방향·기존 세로 실패·원작 비례 비교를 구분했다. |
| [iPhone 정식 Release 설치 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L902-L909) | 현재 검증 설치·2차 | 11/13 정식 설치·앱·저장·프로파일의 당시 결과다. 현재 실제 크로스 호스트 코드 교체와 세트 반복 검사를 별도로 추적한다. |
| [iPhone 실기기 직접 조작 — 2026-11-13](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L910-L931) | 현재 검증 iPhone·2~5차 | 11/14의 실제 허브·사령부·전투·일시정지 일부 관측을 유지했다. 전체 게임·장시간·가변 팝업과 드롭은 열린 채로 둔다. |
| [제거한 네이티브 훅의 연결 기록 — 2026-11-15](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L932-L936) | 훅 이력·5차 | 제거된 네이티브 훅의 실행과 실패는 역사다. 현재 검증에는 공통 코드·네이티브·GPU의 결과를 남긴다. |

## 이전 iPhone 화면과 생명주기

[11월 14일 실제 기기 관측](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L450-L483)을 기준으로, 슬롯 재개 위치와 가변 줌, 홈·전화 뒤 게임 전체 상태 및 음향 문제를 분리했다. [11월 18일 수정 기록](iphone-module-audit-20261118.md)과 [후속 설치](dual-release-installation-20261119.md)의 성공 범위는 점프·출구·대표 경로다. 휴대폰 유저 관측이나 호스트 수명 검사로 두 Apple 실기기의 전화·장시간·전체 화면 검사를 닫지 않았다.

## 초기 구현과 포팅판

[원본 판본·호환 시험](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L754-L861)과 [최초 치트·게임오버·저장 이력](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L588-L753)에는 이후 소스가 바뀌기 전의 실패가 포함돼 있다. [기능별 검증 대응표](feature-verification.md), [비교판 검증 방법](reference-port-validation-plan.md), [기술스택과 실제 소비 경로](technology-and-assets.md)에서 후속 구현을 찾을 수 있다. 초기 기록에서 열려 있던 기능 중 일부는 공통 코드·제한 실행까지 확인됐고, 전수와 최종 실제 iPad HD 대조는 열린 상태다.

## 제거한 네이티브 훅

자동 호출 미확인과 stale 오류는 [원문](https://github.com/gelb2/HeavyMach2/blob/e5e41b0f133d77882ae6664e7e4d38ea8733a57e/docs/todo.md#L211-L220) 및 [연결 복구 기록](harness-runtime-repair-20261115.md)에 남아 있다. 기존 네이티브 훅은 현행 AGENTS의 제거 기록을 따른다. 지금은 공통 테스트·GPU 캡처·실제 기기 결과를 해당 작업의 근거로 남긴다.
