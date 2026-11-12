# HeavyMach 2 Restoration

HeavyMach 2의 독립 비공개 복원 저장소다. REA/Ghidra로 HD 원본을 분석하고 C++ 공통 코어에 맵·보행·조준·대시·발사·피해·차량/포탑/항공기·음향·진행·장비·저장 모듈을 연결하고 있다. Source 앱은 타이틀·슬롯·이름 입력/저장·기지·전장과 실제 터치 이동/발사에 연결했다. 사용자는 실제 iPad에서 첫 미션 근처의 이동·전투·여러 경로 입력과 퀘스트 수락·성공을 확인했다. [iPad 설치와 사용자 관측](docs/ipad-release-installation-20261111.md).

Validation은 구급상자 6종·공습 3종·38개 전장·대시·BOT·도움말·게임오버·랭킹 안내·완료 임무를 포함한 115조건을 함께 실행한다. iPad 시뮬레이터의 GPU·OpenAL 검사와 별도 치트 Release 앱에서 1,200프레임 실행을 확인했다. 정상 게임 전체와 전체 치트 기능·최종 원작 일괄 대조는 미완료다. [현재 검사와 남은 범위](docs/evidence/app-icons-validation-20261111.json), [이전 전장·대시 구현](docs/enemy-dash-restoration-20261110.md)을 따른다.

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

Apple 앱 자원을 준비할 때는 원본 HD 자원과 앱 아이콘을 각각 생성한다. 원본 입력은 `docs/source-provenance.json`의 로컬 경로·해시를 사용한다.

```sh
python3 tools/prepare_hd_assets.py
python3 tools/prepare_app_icons.py
xcodegen generate --spec Apple/iOS/project.yml
```

정식판은 원본 HD 탭투스타트 일러스트 `Main_BG.png`를 비율을 유지해 정사각형으로 자른 아이콘을 쓴다. 치트판은 같은 그림에 `TEST` 표식을 넣는다. 생성된 그림은 `.local/prepared/AppIcons.xcassets`에 두며 Git에 원본 그림을 추가하지 않는다. [앱 아이콘 구성](docs/app-icons.md).

[작업 지침](AGENTS.md) · [복원 계획](docs/restoration-plan.md) · [원본 위치와 해시](docs/source-provenance.json) · [TODO](docs/todo.md)

[HeavyMach 1 초기 이력 검토](docs/heavymach1-history-review.md) · [일반판·HD판 포팅 앱 검증 계획](docs/reference-port-validation-plan.md)

[기술스택과 에셋 생성·저장·조립 규칙](docs/technology-and-assets.md)

[세 작품의 실제 기술스택 대조](docs/technology-stack-review-20261110.md) · [시간·저장·음향·인게임 표시 계약](docs/runtime-contract.md)

[iOS·iPadOS 첫 구현·검사](docs/initial-source-restoration-20261110.md) · [치트 앱·동시 검증](docs/validation-build.md) · [모델 사용 이력](docs/model-usage-history.md)

[맵·플레이어 장비 후속 구현](docs/player-map-restoration-20261110.md)

[보행·조준·터치 검사](docs/locomotion-restoration-20261110.md) · [전투 복원 계획](docs/combat-restoration-plan-20261110.md) · [무기 데이터·포구 구현](docs/weapon-data-restoration-20261110.md)

[전체 기능 치트 앱 개발·검증 계획](docs/full-validation-development-plan-20261110.md)

[보고 사이트](docs/report-site.md) · [온라인 보행 검증 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/) · [온라인 전체 치트 앱 계획](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/plan.html) — 본인 계정 로그인 필요. 보고서·계획·이미지4장의 실제 서비스 표시를 확인했다.

[온라인 59조건 동시 검증 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/concurrent.html) — C++46개와 iPhone/iPad 각각1,200프레임 결과. 본인 계정 로그인 필요. 게시 본문·링크·결과JSON·이미지의 서비스 응답을 확인했다.

[구급상자·공습·메시지와68조건 검증](docs/quick-items-restoration-20261110.md) — C++48개·양 시뮬레이터1,200프레임. 차량 외 Scene 연결과37개field 진입은 다음 과제다.

[온라인 38개 전장·대시106조건 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/fields-dash.html) — C++51개·양 시뮬레이터 각각1,200프레임, 최초695대 생성·대표 포탑/항공기 전투·대시·구급상자/공습 결과. 본인 계정 로그인이 필요하다. 실제HTTP 본문/링크·결과JSON·PNG 일치를 확인했다. 전체게임·Source정상시작·원작대조는 미완료다.

[정상 앱 진입·슬롯/저장/입력 검증](docs/source-entry-restoration-20261110.md) · [등록한 복원 Goal](docs/restoration-goal.md)

[온라인 정상 앱 메뉴·전투 연결 보고](https://heavymach-dev-report-20261025.hiphopxy.chatgpt.site/heavymach2-restoration-20261110/source-entry.html) — 본인 로그인 필요. 실제 Source 조작·슬롯/저장·발사와 현재 HD canvas의 한계, Core55·양 시뮬레이터 검사·원본 차이·진행 중인 다음 작업을 담는다.

[개발자의 변](docs/RESTORATION.md)
