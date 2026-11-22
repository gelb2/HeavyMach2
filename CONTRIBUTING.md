# HeavyMach 2에 기여하기

[빌드와 설치](docs/public/build-and-install.md) · [코드 구조](docs/public/architecture.md) · [테스트 방법](docs/public/testing-method.md)

오류를 제보할 때는 앱 종류와 버전, 기기·OS, 지역 또는 임무 이름, 재현 순서, 예상과 실제 동작을 함께 적는다. 저장 전체나 개인 정보가 들어간 화면 대신 문제를 확인할 수 있는 로그와 캡처를 첨부한다.

코드는 `develop`에서 분기하고 같은 브랜치를 대상으로 PR을 연다. 빌드와 설치 문서의 명령으로 변경한 플랫폼을 확인하고, 코드·화면·데이터 중 무엇이 바뀌었는지 PR에 적는다. 공통 게임 규칙·화면 변경은 해당 회귀 검사와 실제로 확인한 기기를 적는다. [브랜치와 릴리스 기준](docs/public/release-policy.md)

원본 IPA, 사용자 저장과 서명 자료는 커밋 대상에서 제외한다. 새 복원 코드의 MIT와 원작 자료·외부 코드의 고지는 [라이선스 범위](LICENSE-SCOPE.md)와 [제작자와 외부 코드](CREDITS.md)를 따른다.

## 문서 변경 확인

`python3 tools/check_docs.py`는 Git에 등록된 프로젝트 Markdown의 상대 링크와 제목·앵커를 확인한다. 한국어와 영어 문서의 대응 파일과 언어 전환 링크도 확인한다. 새 번역 문서는 두 파일을 Git에 추가하고 [번역 문서 목록](docs/translations/pairs.json)에 등록한다. 이전 시리즈 회고의 영문 발췌는 발췌본으로 등록했다.

`python3 tools/check_docs.py --base origin/develop`로 PR에 넣을 변경도 확인한다. GitHub의 `documentation` 작업은 PR의 기준 브랜치 및 기존 브랜치에 올리기 전 커밋과 대조한다. 등록한 본문을 수정할 때는 두 언어를 함께 검토하고 같은 PR에서 갱신한다. 사실·수치와 번역의 의미는 원문을 대조해 확인한다.
