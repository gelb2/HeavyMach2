# HeavyMach 2 복원

[한국어](README.md) | [English](README.en.md)

## HeavyMach 2를 다시 만드는 이유

HeavyMach는 내가 개발자로 직업을 바꾸는 데 영향을 준 게임이다. 대학생 시절 아이팟터치에서 이 게임을 만났고, 나중에는 미국 앱스토어 계정을 만들어 구매했다. 한때 내게 방향을 보여준 게임을, 개발자가 된 내가 다시 작동하게 만들고 싶었다.

HeavyMach 1을 복원한 뒤, 개발자 변해준과 일러스트레이터 박재철의 연작인 HeavyMach 2의 복원을 이어가고 있다. 원작자의 게임과 일러스트를 보존하고, 현재 기기에서 다시 플레이할 수 있게 만드는 것이 이 프로젝트의 목적이다.

나는 이 복원판으로 돈을 벌 생각이 없다. 이 게임을 어떻게 복원했고 무엇을 고쳤는지도 기록으로 남기려 한다.

**[개발자의 변 전문](docs/RESTORATION.md)**

## 프로젝트 소개

HeavyMach 2 HD를 현재 iPad에서 다시 플레이할 수 있도록 복원하는 프로젝트다. 원본 실행파일과 게임 데이터를 분석하고, 이동·전투·장비·임무·메뉴·음향·저장을 C++ 공통 코어로 구현하고 있다. 게임은 한국어를 기본으로 사용한다.

**현재 iPad 정식판으로 플레이할 수 있다.** 사용자는 첫 미션의 이동·전투·경로 그리기와 퀘스트 수락·완수를 확인했고, Wasteland까지 진행한 시점에 크래시를 발견하지 않았다고 보고했다. 전체 기능 복원과 최종 원작 대조는 진행 중이다. [플레이 기록](docs/ipad-release-installation-20261111.md) · [확인한 범위와 남은 문제](docs/public/validation.md)

[플레이 방법](docs/public/quick-play.md) · [빌드와 설치](docs/public/build-and-install.md) · [치트 앱 사용](docs/public/build-and-install.md#치트-앱-실행과-결과) · [TODO](docs/todo.md)

## 플레이와 설치

설치된 **HM2 Source**를 열고 iPad를 세로로 놓는다. 타이틀을 터치한 뒤 저장 슬롯을 선택한다. 새 슬롯에서는 이름을 입력하고, 기존 슬롯에서는 이어서 시작한다. 기지에서 임무를 받고 전장으로 나가 지면 터치로 이동하거나 손가락으로 경로를 그린다. 적을 터치하면 표적으로 선택하고 장착한 무기의 조건에 따라 공격한다.

현재 App Store·TestFlight·공개 설치 파일·브라우저 플레이는 제공하지 않는다. 설치하려면 접근 권한이 있는 이 저장소의 소스와 로컬 원본 자원을 준비하고 Xcode에서 본인의 팀으로 서명한다. **저장소를 clone하는 것만으로는 게임 자원이 준비되지 않는다.** [필요한 원본과 준비 순서](docs/public/build-and-install.md#원본-자원-준비)

| 대상 | 현재 상태 |
| --- | --- |
| iPad 정식판 | 실제 기기에 Release 설치·시작을 확인했다. 사용자 플레이는 Wasteland까지 보고됐다. 현재 세로 화면이다. |
| iPad 치트판 | 별도 앱에서 115조건을 같은 프레임에 실행한다. 시뮬레이터에서 1,200프레임 실행과 결과 저장을 확인했다. |
| iPhone | 유니버설 타깃과 가로 화면 설정이 있다. 현재 HD 기준 검증은 iPad에서 진행하며, iPhone 최종 표시·플레이 검증은 남아 있다. |
| Android·웹/PWA·Mac·Tauri | 후속 대상이다. 현재 이 저장소에는 해당 게임 앱의 빌드·설치 경로가 없다. |

### 정식판·치트판·포팅판

| 앱 | 용도 | Xcode scheme |
| --- | --- | --- |
| HM2 Source | 정상 진행 규칙과 사용자 저장을 쓰는 일반 플레이 앱. 정식판은 Release 구성으로 빌드한다. | `HeavyMach2Source` |
| HM2 Validation | 경험치·레벨·장비·아이템·진척도 조건을 준비해 여러 게임 상태를 동시에 실행하는 치트 검증 앱. | `HeavyMach2Validation` |
| HD 포팅판 | 원본 HD 실행파일을 호환 실행기로 구동하는 비교 앱. 복원판과 별도 앱·저장을 사용한다. | [호환 실행 안내](Compatibility/README.md) |

정식판과 치트판은 같은 게임 코어를 사용하고 앱 ID와 저장 공간을 분리한다. 치트판은 자동 검사 후 결과를 저장하는 앱이며, 현재 자유 조작용 치트 메뉴는 제공하지 않는다. 아이콘은 원본 탭투스타트 일러스트를 사용하고 치트판에만 `TEST`를 표시한다. [아이콘 구성](docs/app-icons.md)

## 소스에서 준비하기

Apple Silicon Mac, Xcode, Python 3.11 이상, CMake 3.20 이상, XcodeGen을 사용한다. 저장소 접근 권한과 로컬 HD 1.0.3 원본이 필요하다.

```sh
git clone git@github.com:gelb2/HeavyMach2.git
cd HeavyMach2
git config core.hooksPath .githooks
```

[원본 명세](docs/source-provenance.json)의 경로와 SHA-256을 확인한 뒤 자원·아이콘·선택한 SDK의 OpenAL을 준비한다.

```sh
python3 tools/prepare_hd_assets.py
python3 tools/prepare_app_icons.py
python3 tools/build_openal.py --sdk iphonesimulator
xcodegen generate --spec Apple/iOS/project.yml
open Apple/iOS/HeavyMach2.xcodeproj
```

Xcode에서 `HeavyMach2Source`와 iPad 시뮬레이터를 선택해 실행한다. 실제 iPad에 설치할 때는 `iphoneos`용 OpenAL을 준비하고 개발 팀·서명·기기 연결을 설정한다. 갱신 전에는 현재 앱의 데이터 컨테이너를 백업한다. [시뮬레이터 명령·실기기 설치·오류 해결](docs/public/build-and-install.md)

## 확인한 동작과 남은 과제

타이틀·슬롯·기지·상점·정비·인벤토리·임무·도움말·전장 이동·공격·아이템·BOT·게임오버·저장을 공통 코드에 연결했다. 완료한 개별 검사와 전체 게임의 검증 범위는 구분한다.

2026년 11월 12일 아이콘 적용 후 iPad 시뮬레이터의 치트 앱에서 115조건·1,200프레임, 무기 110종 발사, 효과음 동시 진행 최대 26개, 효과음 컨텍스트 1개를 확인했다. 예열 뒤 자원 재읽기와 이미지 재업로드는 0이었다. 이 수치는 해당 실행의 관측이며 전체 게임의 합격률이 아니다. [검사 명세](docs/evidence/title-illustration-icons-20261112.json)

E-10 격파 과제의 달성 효과음·토스트 누락, 수집 퀘스트의 아이템 지급 의심은 후속 점검 대상이다. 전체 임무·캠페인, 하체 장비 23종의 동작, 전체 BOT 결과, OS 음향 중단·경로 전환과 HD 포팅판 최종 일괄 대조도 남아 있다. [검증 범위](docs/public/validation.md) · [상세 TODO](docs/todo.md)

## 구현과 복원 기준

C++20 코어에서 원작 데이터 reader, 파츠 조립, 게임 규칙과 UI를 공유한다. Apple 호스트는 Objective-C++이며, 게임 그림은 OpenGL ES, 효과음은 OpenAL Soft, BGM은 AudioQueue 경로로 출력한다. 원작 소스코드는 확보하지 못했으며, 바이너리·데이터·실행 관측을 근거로 새 소스를 작성한다.

사격마다 파일·DB를 쓰거나 오디오 컨텍스트를 만들지 않는다. 원본 파츠와 신규 파츠는 공통 데이터·조립·렌더 경로를 따른다. 광고를 표시하거나 요청하지 않는다.

[기술스택·에셋 절차](docs/technology-and-assets.md) · [실행·저장·음향 계약](docs/runtime-contract.md) · [기존 복원 코드의 출처](docs/source-reuse.md)

## 개발 기록

- [복원 계획](docs/restoration-plan.md) · [원본 일반판·HD판의 차이](docs/original-phone-hd-separation.md) · [여섯 판본 조사](docs/ipa-version-audit.md)
- [치트 앱과 작업 순서](docs/validation-build.md) · [전체 기능 검증 계획](docs/full-validation-development-plan-20261110.md) · [HD 함수와 복원 범위](docs/hd-original-function-audit-20261111.md)
- [정식판 설치·사용자 플레이](docs/ipad-release-installation-20261111.md) · [최신 아이콘·동시 검사 기록](docs/evidence/title-illustration-icons-20261112.json)
- [보고 사이트와 게시 목록](docs/report-site.md) — 기존 비공개 보고 사이트를 사용하며 본인 계정 로그인이 필요하다.
- [HeavyMach 1 초기 이력 검토](docs/heavymach1-history-review.md) · [포팅 앱 검증 계획](docs/reference-port-validation-plan.md) · [모델 사용 이력](docs/model-usage-history.md)

## 저장소와 자원

이 저장소는 비공개로 유지한다. HeavyMach 1·MachDefense는 절차와 근거를 읽는 참고 대상이며, 이 프로젝트의 소스·출력·저장을 두 저장소에 쓰지 않는다. 작업 브랜치는 `develop`을 거쳐 `main`에 병합한다.

원본 IPA·실행파일·사용자 저장·서명 자료는 커밋하거나 외부로 보내지 않는다. 준비된 원작 자원과 빌드는 Git에서 제외한 로컬 공간에 둔다. 코드의 이용 조건이 원작 그림·음원·게임 데이터의 이용 허락을 포함하지는 않는다. 외부 코드의 허가문은 [재사용 고지](docs/source-reuse.md)와 각 `ThirdParty` 소스에서 확인한다. 사운드 출처·권리 조사는 [TODO](docs/todo.md)에 남아 있다.

[작업 지침](AGENTS.md)
