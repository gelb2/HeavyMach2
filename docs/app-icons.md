# 정식판·치트판 앱 아이콘

정식판 `HeavyMach2Source`는 `AppIcon`, 치트판 `HeavyMach2Validation`은 `AppIconValidation` 앱 아이콘 세트를 사용한다. 두 타깃은 같은 로컬 asset catalog를 읽으며 `ASSETCATALOG_COMPILER_APPICON_NAME`으로 세트를 선택한다. 게임 그래픽·렌더러·저장 경로는 바꾸지 않는다.

헤비멕1과 맥디펜스의 기존 Apple 앱처럼 1024×1024 universal iOS 앱 아이콘을 패키징한다. 원본 HD 1.0.3의 `iTunesArtwork`는 512×512 JPEG다. 정식 아이콘은 이 그림을 같은 가로·세로 비율로 1024픽셀로 변환한 opaque sRGB PNG다. 치트 아이콘은 같은 그림의 아래 HD 표시 영역을 어두운 배경과 흰색 `TEST` 글자로 바꾼다. 메크 그림을 새로 그리거나 게임 내 에셋을 수정하지 않는다.

## 생성

```sh
python3 tools/prepare_app_icons.py
xcodegen generate --spec Apple/iOS/project.yml
```

`prepare_app_icons.py`는 기록된 원본 IPA 해시와 `iTunesArtwork` 해시를 확인한 뒤 해당 그림만 로컬로 읽는다. 기존 Apple 기술스택의 Objective-C++·CoreGraphics·CoreText 오프라인 도구 `render_app_icon.mm`로 그림 크기와 치트 표식을 처리한다. 이 도구는 게임 앱에 링크되지 않는다. 새 게임 엔진·런타임 의존성을 추가하지 않는다.

원본 IPA SHA-256: `7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2`.

원본 그림 SHA-256: `a181cb689cf741788acf7a903bf843ab533f03405ccd692148c70ef1870152f4`.

결과는 `.local/prepared/AppIcons.xcassets`에 생성한다. 그림 원본·도구 실행파일·출력 해시 명세는 `.local/app-icon-tools/`에 둔다. 원본 IPA·그림·생성 PNG·앱 빌드는 Git에 넣지 않는다. 커밋에는 생성 도구·Xcode 설정·절차만 포함한다.

검사는 실제 빌드의 `CFBundleIconName`·아이콘 PNG·asset catalog와 iPad 시뮬레이터의 홈 화면을 확인한다. 치트 실행은 기존 공통 게임 코드의 동시 시나리오 결과와 GPU·OpenAL 검사를 사용한다. 해당 조건 검사의 통과를 전체 게임 기능 검증으로 확대하지 않는다.

## 2026-11-11 확인 결과

- 두 아이콘은 1024×1024, 알파 채널 없는 PNG다. 실제 두 앱의 `CFBundleIcons`와 iPad용 설정에서 각각 `AppIcon`·`AppIconValidation` 선택을 확인했다. 시뮬레이터 홈 화면에서 `HD`·`TEST` 아이콘을 함께 확인했다.
- 실제 iPad 정식 앱을 아이콘 포함 Release로 업데이트했다. 업데이트 전 사용자 저장을 백업했으며, 업데이트 후 같은 파일을 다시 읽어 바이트 일치를 확인했다. 정식 앱의 실행 유지와 실제 iPad 홈 화면의 아이콘을 확인했다. 실기기 치트 앱 설치는 수행하지 않았다.
- 공통 코어 70개, iPad GPU·음향 XCTest 2개가 통과했다. XCTest는 115개 독립 상태의 1,200프레임 동시 검사와 360프레임 메뉴 예열 검사다. 같은 캐시 프레임을 복원 렌더러로 다시 그린 두 GPU 출력의 RGB 차이는 0이었다. 이 비교는 원본 포팅판과의 화면 비교가 아니다.
- 첫 Release XCTest 빌드는 호스트가 테스트에 필요한 심볼을 공개하지 않아 링크에 실패했다. 검사 빌드 명령에만 `ENABLE_TESTABILITY=YES`를 적용해 동일 검사를 통과했다. 실제 iPad에 설치한 정식 Release에는 시험 환경변수 이름과 XCTest 플러그인이 없었다. 별도 치트 Release도 원래 설정으로 빌드·실행했다.
- 별도 치트 앱도 115조건·1,200프레임을 완료하고 `report.json`을 저장했다. 모든 인스턴스의 첫·마지막 프레임과 틱 수, 무기 110개 발사, 효과음 동시 진행 최대 27개, 컨텍스트 1개, 버튼 실패 0, 예열 뒤 자원 재읽기와 이미지 재업로드 0을 확인했다.

정리한 수치와 남은 범위는 [검사 기록](evidence/app-icons-validation-20261111.json)에 있다. 전체 임무·캠페인·하체 장비별 이동·전체 BOT 결과·OS 음향 중단/경로 전환과 마지막 HD 포팅판 일괄 대조는 계속 미완료로 유지한다. 전체 결과·로그·화면·실패 기록·앱·사용자 저장은 로컬 `.local/evidence/icons-validation-20261111/`에 보존한다.

## 2026-11-12 정식판 재설치

사용자 지시에 따라 기존 아이콘 파일의 해시와 정식판·치트판 선택 설정을 재확인했다. 아이콘 구현은 `e140275`에 이미 포함돼 있었으며 이번에는 게임 코드나 아이콘 그림을 새로 변경하지 않았다. Xcode에서 정식 Release 빌드와 서명을 완료하고 실제 iPad의 같은 앱에 덮어 설치했다. 준비 자원 1,479개의 해시 일치, 시험 저장 환경변수·XCTest 플러그인 미포함, 현재 사용자 저장의 설치 전후 바이트 일치, 앱 시작·타이틀 표시·프로세스 유지를 확인했다.

검사 결과는 [설치 기록](evidence/ipad-icons-release-20261112.json), 사용자 Wasteland 진행 관측은 [플레이 기록](ipad-release-installation-20261111.md#2026-11-12-wasteland-진행-관측)에 남겼다. 게임 코드가 같으므로 이전의 유효한 공통 코어·동시 치트 검사 결과를 재사용했다. 이번 설치를 새 전체 게임 검사나 Wasteland 자동 플레이 검사로 보고하지 않는다.
