# iPadOS 27 시작 오류 수정

2026-11-10. 사용자가 직접 플레이하기 위해 iPad 설치를 지시했다. 시뮬레이터에서 검증한 `c26fff0` 정상 Source를 별도 로컬 작업 사본에서 Release로 빌드했다. BOT·그림자·Help의 진행 중 변경은 이 설치 후보에 포함하지 않았다.

## 원인과 수정

실제 iPadOS 27.2에서 설치 후 시작 직후 SIGTRAP이 발생했다. 종료 기록의 첫 프레임은 `___UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption_block_invoke`다. [Apple의 UIKit 수명 관리 전환 문서](https://developer.apple.com/documentation/uikit/transitioning-to-the-uikit-scene-based-life-cycle)는 최신 SDK로 빌드한 iOS·iPadOS 27 앱에 UIScene을 요구한다. 앞선 iOS 26.5 시뮬레이터 검사는 이 새 요구를 검증하지 못했다.

`HM2SceneDelegate`에서 UIWindowScene 창을 만들도록 수정했다. Scene 활성·비활성·백그라운드·연결 해제를 기존 GPU 게임 뷰의 `setActive:`에 전달한다. Info.plist에 단일 Scene 설정을 추가했다. 정상 앱과 Validation은 기존 게임 코어·GPU·OpenAL 경로를 계속 쓴다. 여러 창에서 같은 저장 파일을 동시에 여는 기능은 켜지 않았다.

## 설치와 보존

CLI 서명은 `errSecInternalComponent`로 실패했으나 Xcode GUI에서 같은 Source의 Release 서명과 codesign 검증을 통과했다. 첫 설치는 무료 개발 프로필 앱 3개 제한에 걸렸다. 사용자가 맥디펜스 치트판의 백업 후 교체를 승인했다. 해당 앱의 전체 데이터 컨테이너 14파일·22,942,142바이트를 로컬에 복사하고 파일별 SHA-256을 기록한 뒤 교체했다. 실제 게임 저장 파일 42,496바이트도 포함한다. 헤비멕1과 맥디펜스 일반판은 유지했다. 백업·실기기 식별자·프로비저닝 자료는 Git과 보고 사이트에 올리지 않는다.

## 검사 범위

수정 후 시뮬레이터 및 실기기 검사 결과는 `docs/evidence/apple-scene-lifecycle-20261110.json`에 기록한다. 전체 게임 복원·BOT/Help 통합·최종 기기별 화면 비례·치트 앱 수동 조작 기능의 완료를 뜻하지 않는다. 앱 자체 UIAlertController를 띄울 때 게임을 정지하는지와 Scene 연결 해제 후 미저장 전투를 복원하는지는 별도 미검증이다.

로컬 증거: `.local/evidence/source-entry-20261110/`. 실제 종료 기록, 서명 실패·성공, 앱 목록, 백업 명세, 설치·실행 결과, 수정 시뮬레이터 xcresult와 독립 검토를 보존한다.
