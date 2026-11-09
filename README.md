# HeavyMach 2 Restoration

HeavyMach 2의 독립 비공개 복원 저장소다. 현재 일반판·HD 원본 포팅 앱을 시뮬레이터에서 검증하며 데이터·표시·전투와 기술스택을 조사했다. 독립 C++ 복원 코어·복원 앱 구현은 아직 미완료다.

## 작업 경계

- HeavyMach 2 소스·도구·검사·파생 자원은 이 저장소 안에서만 만든다.
- HeavyMach 1과 MachDefense의 폴더에 HeavyMach 2 코드를 넣지 않는다. 두 프로젝트의 파일·설정·에셋·저장을 수정하지 않는다.
- 두 프로젝트는 절차와 기존 조사 근거를 읽는 참고 대상으로만 사용한다. 소스 링크·쓰기 경로·빌드 출력으로 연결하지 않는다.
- 저장소는 private을 유지한다. 별도 사용자 지시 없이 공개·외부 게시하지 않는다.
- 원본 IPA는 저장소 밖에 보관한다. Git·LFS·릴리스·CI artifact·보고 사이트·외부 분석 서비스에 올리지 않는다. 이름 변경·재압축·분할도 금지한다.

## 시작

```sh
git config core.hooksPath .githooks
python3 -m unittest discover -s tests -p 'test_*.py'
python3 tools/check_no_ipa.py --all
```

`.gitignore`, 커밋 전 검사, 푸시 전 전체 이력 검사로 IPA를 차단한다. 확장자를 바꾼 ZIP형 IPA도 Payload/*.app 패키지 구조로 검사한다. GitHub 검사는 업로드 뒤 검증이므로 로컬 훅을 대신하지 않는다. 훅을 우회하지 않는다.

빈 초기 커밋에서 main/develop을 만든 뒤 작업 브랜치→develop→main 순서로 준비 내용을 반영한다. 이후 작업도 같은 순서를 따른다.

[작업 지침](AGENTS.md) · [복원 계획](docs/restoration-plan.md) · [원본 위치와 해시](docs/source-provenance.json) · [TODO](docs/todo.md)

[HeavyMach 1 초기 이력 검토](docs/heavymach1-history-review.md) · [일반판·HD판 포팅 앱 검증 계획](docs/reference-port-validation-plan.md)

[기술스택과 에셋 생성·저장·조립 규칙](docs/technology-and-assets.md)

[세 작품의 실제 기술스택 대조](docs/technology-stack-review-20261110.md) · [시간·저장·음향·인게임 표시 계약](docs/runtime-contract.md)
