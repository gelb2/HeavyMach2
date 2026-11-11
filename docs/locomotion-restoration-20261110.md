# 보행·조준·터치·지형 충돌

2026-11-10. 작업 브랜치 `feat/reference-ports`에서 정적 맵·장비 표시 다음 범위를 구현했다. 현재 Source는 지형을 누르면 기본 장비 Player가 보행하는 앱이다. Validation은 보행할 Player와 다른 장비의 조준 사례를 함께 표시한다. 전투·진행·전체 치트 앱은 아직 미완료다. 실기기 설치·develop/main 병합은 하지 않았다.

## 원본에 맞춰 연결한 경로

- `setDestPos → processMove_Bipod → footChange → addPosition → mountedPose → GPU`를 같은 C++ 코어에 연결했다. 고정 속도×시간 계산으로 바꾸지 않았다. 보폭·발 교대·최단각·도착 뒤 잔여 이동을 보존한다. 원본의 scale 상태는 실제 그림 크기에 소비되는 근거가 없어 상태로만 보존한다.
- 화면 입력은 UIKit UITouch에서 renderer의 역변환을 거쳐 공통 waypoint 경로로 전달한다. 일반 빈 지형 누르기·드래그 간격·최대12개 경로점·기존 경로 뒷부분 자르기를 연결했다. 아래100 논리 좌표의 원본 HUD 구역은 이동 입력에서 제외한다. 적 선택·대시·아이템 입력은 아직 미구현이다.
- 지형은 원본43개 맵의 충돌 다각형에서1354개 edge를 만든다. 원본8×8 후보 구획·반지름16·역순 처리·보정 방향·벽 방향 조절을 보존한다. 구획 표본의 중복 좌표도 원본 값대로 사용한다. 일반 물리 엔진으로 대체하지 않았다.
- world 시간은 원본 float 상수0.016599999740719795를double로 승격해 매 tick 더한다. 경로 갱신→카메라→메크 이동→충돌 순서를 유지한다. 카메라의 중간점·보간·HD 제한·선택적 배경의.94 이동도 연결한다. 앱 비활성/활성에서 진행을 멈추고 지난 시간을 한꺼번에 따라잡지 않는다.
- Player 부모의 원본 visual은0이고 초기isInScreen은1이다. 기반 tick은visual 객체만 이 값을 갱신한다. 부모에 임의의 현대 화면 cull을 넣지 않았다. 명시적인inScreen=false 상태에서는 원작 wrapper처럼 이동·충돌을 중단한다. 적·탄환의 시야 조건을 이 Player 규칙으로 일반화하지 않는다.

## 검사에서 고친 차이와 오류

1. 발 교대 후 같은 tick에 다음 phase까지 진행하던 처리를 고쳤다. 원본 첫tick의 이동량은0이고 둘째tick부터 전진한다.
2. 발 각도의.01 snap을 보간 후 검사하도록 고쳤다. 원본에 없는 진입phase2 동작과 저장 필드도 제거했다.
3. 다른touch ID가 끝나도 원작처럼 이동 포인터를 해제하게 고쳤다. 실제 UIView에도 다중터치 전달을 켰다. OS 취소·비활성 전환에서는 오래된 포인터만 해제한다.
4. float `from_chars`가 iOS26부터 제공되어 최소iOS15 빌드가 실패했다. 최종 구현은 원본 atof와 같은 double→float 변환을 공통 reader에서 수행한다. 임시 정수 변환은 원본 ItemInfo의 float getter와 달라 제거했다. 실제 원본 장비 값과 참조를 보존했다.
5. 새 UI 검사 target의 UIKit 링크 누락을 고쳤다. 앱 영역 캡처의 회전/잘림 문제는 전체 화면 캡처로 고쳐 두 기기에서 다시 검사했다. 기존 최소OS·화면 방향·테스트 허용오차를 바꾸지 않았다.

## 실행 결과

| 검사 | 결과 |
| --- | --- |
| C++ | 후속 무기 데이터 검사까지 포함한 최종15개 통과. 기존 reader·이미지·정적 조립 회귀 포함 |
| 보행·조준 | 원본ASM 수식에서 독립적으로 도출한 첫40tick, 발 교대·절대각·가까운 표적의 좌우 무장 각·정지 잔여 이동·목적지 변경·idle 상태 유지 검사 |
| 경로·카메라 | 드래그60/끝30 경계, 최대12점, 다중ID 종료, 경로 자르기, 카메라 보간/제한, pause 검사 |
| 실제 원본 맵 | 43맵1354edge. Stage01_01의(-45,130)을(-52.8894424438,130)으로 edge62가 한 번 보정. 두 표시 구성에서 같은 월드 목적지로600tick 진행한 위치 일치 |
| iPhone·iPad unit | 최종 각4개 통과. 무기 데이터 검사와 실제 보행/조준180tick·192회 출력, 텍스처9개와 컨텍스트 재사용, core 자원 재읽기0. 기존 고정 조립 픽셀 차이0 |
| 실제 터치 | 기기별UI 검사1개 통과. iPhone x0→48.365730, iPad x0→34.266243 관측. 서로 다른 실행 시각/카메라와 목적지이므로 이 두 값의 차이를 기기별 보행 차이로 해석하지 않음 |
| 병렬 검사 | 최종 두 시뮬레이터의 실제 unit 검사 시간이 약6.989초 겹침. 각기 unit4개·UI1개가 실패0으로 종료 |

원본 화면을 실행해 이40tick을 직접 계측한 것은 아니다. 수식 기반 검사와 원작 실행 대조를 구분한다. 실제 UI 검사는 한 번의 누르기다. 실제 드래그·다중 손가락·영역 밖 종료는 아직 별도UI 검사가 없다. C++ 경계 검사가 이를 전부 대신하지 않는다. 전체 프레임 주기·실기기 성능·소리·전투·저장·성장 완료도 아니다.

## 다음 단계

사용자 지시에 따라 검증에서 드러난 차이를 고친 뒤 [전투 복원 계획](combat-restoration-plan-20261110.md)을 기록했다. 계획 기록 후 [공통 WeaponInfo213개와 포구 계산](weapon-data-restoration-20261110.md)을 구현·검사했다. 다음은 controller의 재장전·발사 시각·탄 수·난수·반동을 연결한다. 기본 기관총은 직접 피해, 미사일은 GobBullet 경로라는 차이를 유지한다. 포구·controller·음향·실제 적·피해·보상은 차례로 연결하고 각각 검증한다.

## 근거

`.local/evidence/locomotion-restoration-20261110/`의 player/implementation-rules.md, player/locomotion-evidence.json, player/derived-no-target-trace.json, world/rea-evidence-updated.json, core-tests-final.log, source-build-final.log, validation-build-final.log, parallel-final/summary.json과 기기별 tests.log/xcresult를 보존했다. 최종 회귀 결과와 회전/잘림을 고친 UI 캡처는 `.local/evidence/combat-restoration-20261110/parallel-final/`과 `screenshots/`에 있다. 이전 잘린 캡처는 실패 증거로 보존한다. [추적용 해시 명세](evidence/locomotion-restoration-20261110.json)에 코드·근거·번들 정보를 기록한다. 원본 IPA나 ARM 실행파일을 Git에 넣지 않았다.

현행 Apple DerivedData와 C++ 빌드를 재사용했다. 새 빌드를 별도 복제하지 않았다. 이전 검사의 로그·캡처·원본·사용자 저장은 유지한다. [모델 사용 이력](model-usage-history.md)은 사용자 제공 기록으로 보존한다.
