# HeavyMach 1 초기 이력에서 가져올 복원 순서

2026-11-09. HeavyMach 2 계획을 보완하기 위해 HeavyMach 1 저장소의 보존 이력을 읽었다. HeavyMach 1·MachDefense 파일은 수정하지 않았다. 과거 실행 기록을 읽은 결과이며 당시 검사를 이번에 다시 실행한 것은 아니다.

## 조사 범위와 최초 커밋

`git log --all --reverse`와 root commit 목록을 확인했다. `40209cde7b44770a23c9d56c8f307da8d595a694`가 보존된 HeavyMach 복구 이력의 시작이다. 별도 저장소의 빈 bootstrap root `3374d897`와 혼동하지 않는다. 커밋 이전의 모든 시행착오가 별도 커밋으로 남았다고 보지 않는다. 최초 커밋은 이미 작성된 호환 실행기·패치·실행 기록을 보존한 스냅샷이다.

아래 초기 전환과 후속 실패를 중심으로 커밋 내용·코드 diff·문서를 확인했다. 전체 커밋의 모든 코드 줄을 감사한 결과는 아니다.

| 근거 | 확인한 사실 | HeavyMach 2에 반영할 조치 |
|---|---|---|
| [40209cde](https://github.com/gelb2/HeavyMach/commit/40209cde7b44770a23c9d56c8f307da8d595a694) | 첫 단계는 원본 ARMv6 IPA를 CPU 인터프리터로 실행하는 호환 앱. 원본 게임 소스 복원 완료와 명확히 구분했다. upstream·Rust·SDL·IPA 해시를 고정하고 게임 전용 CPU 우회를 제거했다. | 일반판·HD판의 원본 실행을 각각 보존하는 비교 앱을 먼저 확보한다. 호환 앱 실행 성공을 새 게임 코드 복원 완료로 보고하지 않는다. |
| 같은 커밋의 [초기 실행 기록](https://github.com/gelb2/HeavyMach/blob/40209cde7b44770a23c9d56c8f307da8d595a694/docs/heavymach-recovery-20261024.md) | UIScene·방향·서명 실패 수정. 에이전트의 메뉴/사격 검사와 사용자의 미션 전환·아이템·새 무기·저장 유지 확인을 구분했다. 레벨업 미확인은 남겼다. | 두 포팅 앱 모두 빌드→서명→설치→실행→플레이를 별도 항목으로 기록한다. 타이틀 성공으로 전투·저장까지 완료 처리하지 않는다. |
| [80c68f22](https://github.com/gelb2/HeavyMach/commit/80c68f2231b9ba1f8bdc332f84a0a170a167039c)와 그 시점의 `docs/current-status.md` | C++/Objective-C++ 복원 코드가 포함됐다. 렌더·음향·프레임 갱신·저장 시점·자원 수명 차이를 호환판과 대조한 기록이 있다. 양방향 회전의 틸트 반전도 수정했다. | 각 판본의 렌더/시간/입력/저장 계약을 먼저 기록한다. HeavyMach 1의 480×320·60Hz·저장 바이트 수를 HeavyMach 2 상수로 복사하지 않는다. |
| [6ec12e85](https://github.com/gelb2/HeavyMach/commit/6ec12e851393ef9c9c6ef83fd49e06215d135e1e) | 클리어 일러스트의 JPEG 자원 경로를 수정했다. | 시작·전투뿐 아니라 클리어·실패·메뉴 전체의 실제 소비 자원과 확장자를 대조한다. |
| [14d45563](https://github.com/gelb2/HeavyMach/commit/14d455637b69859cd3c32d277a04fd78a56138f2), [804fc2be](https://github.com/gelb2/HeavyMach/commit/804fc2be1087e70f1244fd87bd43ac5858a3aa80), [c437ed78](https://github.com/gelb2/HeavyMach/commit/c437ed7835f5beaf6480e771331546189bda89c9) | 전체 폭 확장 후 필살기 좌표·결과 화면·보스 이동/공격 활성 조건을 수정했다. | 원작 화면 재현과 현대 화면 확장을 별도 단계로 둔다. 보이는 범위가 넓어진 것을 적 생성·공격 활성 범위 확대와 섞지 않는다. |
| [b0ce141f](https://github.com/gelb2/HeavyMach/commit/b0ce141f2f8862dd3f4b7965f20df7c7bd579acf), [daf0c178](https://github.com/gelb2/HeavyMach/commit/daf0c178248b262148ee56008ef743cf7be80f73) | IPA/호환 실행기 접근 없이 복원판을 빌드·실행하는 검사와, 새 clone에 필요한 자원을 포함하는 조치가 별개였다. | 로컬 기존 캐시로만 빌드되는 상태를 완료로 보지 않는다. 최종 복원 앱에서 IPA·원본 ARM 실행파일·호환 실행기 의존성을 제거하고 독립 입력으로 검증한다. |
| `docs/reviews/new-unit-implementation-incident-20261109.md`, [3729fd35](https://github.com/gelb2/HeavyMach/commit/3729fd35) | 신규 유닛의 별도 C++ 정의·그리기·런타임 축소와 원작 공통 데이터 경로의 불일치를 감사하고 공통 데이터·에셋 경로로 수정했다. | 초기 복원부터 원본 파일→reader→파츠 조립→전투/표시→패키지→실화면을 연결한다. 화면이 비슷하다는 이유로 별도 구현을 허용하지 않는다. |

## 그대로 가져오지 않을 사항

HeavyMach 1의 실행기 패치와 앱 구성은 해당 게임에서 확인한 결과다. HeavyMach 2의 ARMv7·iPad API·이미지 배율·화면 방향 지원까지 증명하지 않는다. 게임별 주소 후킹, 원작1의 데이터 형식·상수·UI·세이브 구조를 이식하지 않는다.

공통화할 것은 원본 실행을 먼저 확보하고 같은 조건으로 비교하는 절차다. [두 포팅 앱 검증 계획](reference-port-validation-plan.md)에 실제 앱 구성·기기·수용 기준을 정리했다.
