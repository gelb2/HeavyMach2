# 맵과 플레이어 장비 구현

2026-11-10. 작업 브랜치 `feat/reference-ports`에서 첫 reader·정적 조립 구현 다음 범위를 연결했다. 현재 앱은 맵과 장비를 표시하는 정적 미리보기다. 정상 플레이·전체 치트 앱·원작 화면의 정밀 일치는 미완료다. 실기기 설치와 develop/main 병합은 하지 않았다.

## 구현한 내용

원본 HD1.0.3의 `GlobalInfo::openMapInfo`, `GobWorld::openMap/readColBox/readSpawner/readWarpGate`를 대조해 공통 C++ 맵 reader를 추가했다. 맵43개, 출입구90개, 충돌 다각형152개, 적 생성 지점695개를 읽는다. 마지막 맵의 오래된 크기 헤더 때문에 마지막 생성 지점이 잘리지 않는지도 검사했다. 원본 바이트·미해석 영역·오래된 헤더를 보존한다. 실제 충돌 계산·적 생성·출입구 전환을 구현했다는 뜻은 아니다.

맵 표시는 원본 순서대로 선택적인 `이름BG.jpg`를2배로 그리고, `이름.jpg`가 없으면 `이름.png`를 사용한다. 지형과 유닛은 같은 카메라 이동을 적용하고 지형 중심만 원본 HD 규칙으로 정수 반올림한다. 카메라 추적·흔들림·시야와 전투 판정의 연결은 아직 구현하지 않았다.

플레이어는 MCB의 Player 모델을 그대로 쓰지 않는다. 원본 `initSlot → ItemInfo → addMyMach → setParts`를 따라 연결 파츠·두발·몸체·좌무기·우무기 순서로 조립한다. 기본 장비는11001·12001·10011·10111이다. 각 장비의 실제 파츠 이름은 ItemInfo의 `strParam`에서 읽는다. 문자열 Code의 앞자리0을 보존한다.

원본 소켓의 소수점을 유지하며, 우무기는 그림과 소켓Y를 함께 반전한다. 부품각은 절대각으로 입력받는다. 장비의 여섯 장착점과 MCB 모델은 같은 `Models::mountedPose → PartSprites → Frame → Apple GPU` 경로를 사용한다. 확인한 원본 연결 파츠 DummyParts만 원본과 같은 무그림 상태로 처리한다. 다른 누락 그림은 오류로 거부한다. 기본 여섯 장착점 중 실제 그림은 다섯 개다. 그림자·보행·조준 controller는 아직 연결하지 않았다.

Source 앱은 Stage01_01 지형과 기본 장비 Player 하나를 표시한다. Validation 앱은 같은 지형에 장비와 절대각이 다른 Player 두 사례를 함께 표시한다. 시뮬레이터의 iPhone 가로·iPad 세로에서 두 앱을 각각 설치·실행·캡처했다. 현재 고정 비교 viewport와 여백은 진단용이며, 최종 기기별 크기·HUD·화면 전체 사용 기준의 검증 완료가 아니다.

## 확인한 결과

| 검사 | 실제 결과 |
| --- | --- |
| C++ 전체 | 10개 통과. raw 왕복·테이블·난수·이미지·기존 조립 회귀 포함 |
| 맵 | 43맵의 레코드 수, 부호 있는 좌표,31자 설명, JPEG/PNG 선택, 선택적 배경2배 표시, 맵2400회 재합성 중 읽기 증가0 |
| 장비 | 원본 장비157개의 파츠 참조 확인. 기본 장착점·우무기 반전·서로 다른 절대각·잘못된 슬롯/순환 부모 거부. 장비2조건을 함께1200회 재합성해 읽기 증가0 |
| Apple 빌드 | 같은 코어의 Source와 Validation 시뮬레이터 빌드 성공 |
| iPhone GPU | 검사2개, 실패0. 맵/장비122출력·텍스처9개 유지. 두 Player 영역에서22707/20547픽셀 변화. 기존 조립1:1 픽셀 차이0 |
| iPad GPU | 검사2개, 실패0. 맵/장비122출력·텍스처9개 유지. 두 Player 영역에서17151/15387픽셀 변화. 기존 조립1:1 픽셀 차이0 |
| 병렬 실행 | 두 시뮬레이터의 실제 검사 시간이 약3.482초 겹침. 결과·로그·프로세스 분리 |
| 앱 실행 | 기기2종×앱2종, 네 조합의 지형·장비 표시 캡처. 검사 후 해당 앱 프로세스 종료 |

장비157개의 참조 검사는 모든 장비의 시각·공격 검증이 아니다. 새 GPU 검사의 영역별 픽셀 변화는 두 Player가 실제 표시됐는지를 확인한다. 원작 화면과 파츠별 픽셀·겹침 순서가 완전히 같은지 증명하지 않는다. 실제 프레임 주기·성능·입력·전투·음향 검증도 아니다. 기존 조립 픽셀 검사는 별도 고정 사례에서만 유효하다.

독립 검토에서 설명 필드 길이24 제한을 발견해32로 고쳤고,31자 설명 검사를 추가했다. 초기 검사에서 마지막 맵 생성 지점을17개로 잘못 예상해 실패했다. raw 레코드를 직접 세어16개임을 확인했고, 마지막 E_Turret_20의 좌표187,-95도 검사했다. 원본 데이터와 허용 오차는 바꾸지 않았다.

## 다음 구현

1. 원본 `setDir/processMove_Bipod`와 controller의 각·발 오프셋·갱신 순서를 복원한다. 터치→월드 좌표→이동·카메라와 맵 충돌을 연결한다.
2. 실제 무기 controller·포구·탄환·명중·피해·격파·보상을 연결한다. 터치 선택 반지름과 탄환 사각형, 카메라에 따른 전투 후보 조건을 따로 검사한다.
3. 공통 GPU HUD·메뉴·인벤토리·아이템·성장·임무·저장·OpenAL 음향을 연결한다. 실제 전투에서 자원 수명·파일 쓰기·음향 시점을 계측한다.
4. 진행 조건을 해제한 한 전투의 복합 기능 검사와 독립 인스턴스의 병렬 검사를 확장한다. [동시 검증](validation-build.md)의 전체 기능·버튼 조건을 적용한다.

## 근거 위치

로컬 `.local/evidence/player-map-restoration-20261110/`의 `player/implementation-rules.md`, `player/player-evidence.json`, `map/implementation-rules.md`, `map/complete-rea-evidence.json`, `core-tests.log`, `source-build-final.log`, `validation-build.log`, `parallel/*/tests.log`, `parallel/summary.json`, `preview-execution.json`, 네 화면 캡처를 보존했다. [추적용 해시 명세](evidence/player-map-restoration-20261110.json)는 원본 IPA·원본 실행파일·자원 내용을 포함하지 않는다.

원본 실행파일 SHA-256: `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`. 근거 주소: map open0x14120/dictionary0x154a4/gate0x100dc/polygon0xff68/spawner0xff18/camera0x37870·0x37958/fixedPos0x36d30, Player initSlot0x17e10/addMyMach0x11770/setParts0xb7b4/socket0x1c7e4/rotation0x1ca2c. 디컴파일 자료는 확보한 원본 개발 소스가 아니다.

이전 DerivedData를 재사용했으며 빌드를 별도 복제하지 않았다. 현재 Apple 빌드 약182MB·C++ 빌드 약20MB를 유지한다. 기존 원본·저장·실패 로그·캡처는 보존했다. [모델 사용 이력](model-usage-history.md)은 사용자 제공 기록으로 별도 보존한다.
