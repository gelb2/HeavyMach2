---
name: write-project-text
description: HeavyMach 2의 GitHub 문서, 보고서, 게임 UI 문구, 번역, 캡션, 대화 답변을 작성하거나 고칠 때 적용한다. 사용자 원문을 보존하고 공개 글에서 메타카피·컴플라이언스 문체·자기합리화를 걷어 낸다.
---

# HeavyMach 2 글쓰기

사용자 질문에 먼저 답한다. 독자가 알아야 할 동작, 결과, 다음 행동을 앞에 쓴다. 보고에서는 확인한 사실, 사용자 관측, 계획을 구분한다. 확인한 결과에는 기기·판본·조건을 붙인다. 문장은 짧게 쓰고 익숙한 말과 능동형 동사를 쓴다.

## 공개 문서와 게임 문구

- 공개 글에 작성자용 편집 지침, 내부 준수 선언, 자기평가, 자기합리화, 선제적 면책을 넣지 않는다. 기능에는 실제 동작과 플레이어가 얻는 결과를, 설치에는 필요한 입력과 단계를 쓴다.
- 내부 구현 제약은 [실행·저장·음향 기준](../../../docs/runtime-contract.md)이나 해당 [정본](../../../docs/canonical.md)에 둔다. 공개 소개에는 “효과음은 준비한 OpenAL 소스와 버퍼를 재사용한다”처럼 코드가 하는 일을 쓴다.
- 검사는 대상·조건·걸린 시간·결과를 쓴다. “실기기 전체 성능으로 확대하지 않는다” 대신 “iPad 시뮬레이터에서 전환에 16.07초가 걸렸다. 기준은 15초였다”처럼 확인한 범위를 보여 준다.
- 플레이어에게 필요한 오류와 제약은 해결 행동과 함께 쓴다. “복구코드를 적용하면 받는 앱의 슬롯 세 개를 교체한다”처럼 실제 영향을 알린다.
- 수정 요청을 받으면 같은 유형이 있는 README, 연결된 문서, 번역, 캡션을 함께 찾는다. 검색 결과는 후보로 삼고 문단 전체를 읽어 고친다.

## 사용자 원문과 번역

- [개발자의 변](../../../docs/RESTORATION.md), [개발 동기 자료](../../../docs/developer-background.md), README의 작성자 문단과 직접 인용은 사용자 원문을 우선한다. 사용자가 요청한 범위에서만 원문을 고친다.
- 회고의 ‘나’는 복원 방향을 결정하고 직접 플레이한 작성자다. Codex의 분석·구현·시험은 실제 수행 주체와 함께 쓴다. 영문은 같은 화자, 사실, 수치, 시간과 완료 상태를 전달한다.
- Apps에는 기능과 구현을, Mags에는 작성자가 제공한 경험과 말을 쓴다. [Sass 집필과 미디어 인계](../../../docs/editorial/sass-publication-guide.md)의 원문과 캡처 조건을 대조한다.

## 게시 전 확인

수정한 글을 읽어 반복, 과장과 자기설명을 걷어 낸다. 수치·인용·버튼 이름을 원문과 근거에 대조한다. 문서 링크와 번역 쌍은 [문서 검사](../../../CONTRIBUTING.md#문서-변경-확인)로 확인한다.

이 스킬은 [헤비멕1 글쓰기 지침](https://github.com/gelb2/HeavyMach/blob/e3b54774a76823300e786e85fd0a1dc8e6097e44/.agents/skills/write-project-text/SKILL.md)과 [맥디펜스 글쓰기 지침](https://github.com/gelb2/MachDefense/blob/3f38bd161d159f4a369fa1555a7a38e3ca4e8456/.agents/skills/write-project-text/SKILL.md)을 헤비멕2에 적용한 것이다. [문장 규칙 사본](references/editorial-rules.md)은 [맥디펜스에 공개된 참고 원문](https://github.com/gelb2/MachDefense/blob/3f38bd161d159f4a369fa1555a7a38e3ca4e8456/.agents/skills/write-project-text/references/upstream/candidate-editorial-rules.md)에서 가져왔다.
