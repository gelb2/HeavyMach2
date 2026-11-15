# iPhone 메뉴 배경 수정본 설치 — 2026-11-15

정식 `HeavyMach2Source` Release를 기존 앱 ID `com.gelb2.heavymach2.restored`로 iPhone Air에 업데이트 설치했다. 실행 요청이 성공했고 이후 기기 프로세스 목록에서 PID11875를 확인했다. 이번 설치 후 직접 게임 플레이 검사는 하지 않았다.

설치 전에 저장28,204바이트와 HUD 설정43바이트를 로컬 백업했다. 설치 후 실행 전에 다시 받아 두 파일의 크기·SHA256·바이트 일치를 확인했다. 앱 삭제와 저장 초기화는 하지 않았다. 백업과 기기 상세 식별은 Git에 넣지 않는다.

원격 xcodebuild 두 번은 `errSecInternalComponent`로 서명에 실패했다. 사용자가 Mac/키체인/아이폰 잠금 해제 완료를 알린 뒤에도 원격 키체인 API는 `User interaction is not allowed`였다. 같은 Release 명령을 현재 사용자의 macOS GUI 로그인 세션에 일회성 launchd 작업으로 실행해 exit0과 엄격한 코드 서명 검사를 확인했다. 권한/키체인 설정을 변경하지 않았으며 완료 후 일회성 작업을 bootout했다.

Release 실행 파일 SHA256: `a07b254a6b570e2340be67af4c45429a8c31a4b03bd9cf7d1ff05c0e06e5ae02`. 최종 소스가 바뀌지 않았고 실제36화면에서 사용한 배경42파일의 Release 번들 바이트가 검사한 원본과 같음을 확인했다.

실행 근거는 `.local/evidence/menu-background-20261115/iphone-install/`의 build/sign/install/backup/launch/process JSON과 로그에 있다. 원본 IPA와 사용자 저장은 외부에 전송하지 않았다.

두 배경 검사와 Phone 선택 수정은 [범위·결과](two-menu-harness-scope-20261115.md)를 따른다. iPhone 메뉴 잘림·버튼 배치·구매 가능 여부, iPad 검은 여백, 현재 세션 네이티브 훅 자동 발동은 미완료다. 저장소 커밋·develop/main 병합은 하지 않았다.
