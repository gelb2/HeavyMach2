# 무기 데이터와 포구 계산

2026-11-10. [보행 검증 뒤 전투 계획](combat-restoration-plan-20261110.md)을 기록한 다음 공통 WeaponInfo 소비를 구현했다. 실제 발사·피해·탄환·음향·재장전 controller는 아직 연결하지 않았다.

## 구현한 내용

원본 `GlobalInfo::openWeaponInfo0x14bc4`를 따라 WeaponInfo.tble213행을 문자열 Parts ID로 읽는다. 뒤의 같은 ID가 앞의 값을 대체한다. 원본 무기9종·피해·범위·발사 수·간격·재장전·속도·명중률·타일·효과·음원 참조를 보존한다. 쓰지 않는 원본 열과 encoded 바이트도 원본 DataTable에 보존한다.

원본의 ShtRng는 정수→float→2배→정수→signed16 변환을 거친다. 기본 MG-16의 표200은 실행값400, ML-1의 표300은 실행값600이다. 발사 수와UI타일은unsigned8, 피해·범위·탄피는signed16이다. 공격점수는 원본의 reciprocal→곱셈→.5 추가 순서를 따른다. Rail 효과는4열이며 원본 파일명 생성 규칙을 유지한다.

숫자 처리는 공통 `DataTable::integer/real`로 옮겼다. 정수와float 모두 comma를 제거하고 숫자 접두어를 읽는다. float은 원본 atof처럼double로 해석한 뒤float로 변환한다. 없는 값0은 원본 ARM nil 분기와0literal로 확인했다. 원본24바이트 버퍼의 미종단 입력·정수 범위 밖은 오류로 거부한다. 그 범위에서 원본의 미정의 메모리 동작을 재현하지 않는다. 장비 Param3도 같은 reader를 사용한다.

포구는 공통 `Models::muzzlePosition`에 연결했다. 원본은 파츠 중심에 회전·Y반전한 `(현재 소켓−소켓0)`를 더한다. 일반적인 중심+소켓 위치로 바꾸지 않았다. 영각·기본 장비의 기체 위치를 더하기 전 값은 MG(38,24.5), ML(27,-26.5)다. 소켓 수/index가 잘못되면 원본은 미정의 좌표를 반환하거나 잘못된 메모리를 읽으므로 새 코드는 명확히 거부한다.

## 검사와 남은 원본 자원 참조

C++전체15개와 최종 숫자 경계 관련2개 재검사를 통과했다. iPhone/iPad의unit4개·실제터치UI1개도 각각 통과했다. 양쪽에서213개 레코드와 기본 무기의 사거리·포구를 확인했다. 같은 실행에서 보행·조준 GPU 회귀와 터치 이동도 검사했다. 최종 unit 실행의 겹친 시간은 약6.989초다.

원본 IPA에도 Blt_Missile06.png와 Blt_Missile07.png는 없다. 참조자는 각각 E_Turret17_Wpn_01, Boss08_Wpn_01이다. 목록을 보존하고 임의 대체 그림을 넣지 않았다. 기본 MG/ML의 참조와 음원 파일은 확인했다. 이 두 누락 참조의 원작 nil그림·효과·공격 실행은 후속 대조 범위다. 숫자 검사 통과와213개 무기의 시각·발사 완료를 구분한다.

독립 검토에서 정수·float의comma 처리 누락과32비트long 포화 위험을 지적받아 고쳤다. 부호·접미어·float comma·hex float·없는 키·int32 양끝/범위초과·24바이트 미종단을 검사했다. 검토자는 원본 근거와 코드만 읽었고 실행 검사는 부모 작업에서 수행했다.

## 다음 controller 연결에 필요한 원본 차이

- MG는 GobBullet 이동탄을 생성하지 않는다. 선택한 target에 직접 피해를 주고, 포구·도착점 효과와 약.2초 궤적을 표시한다.
- ML은 원본 GobBullet의 속도·가속·고정 목적점·폭발 경로를 사용한다. 기본 경로의 point-collision은꺼져 있다. 임의 점충돌이나범용 직선탄을 추가하지 않는다.
- 장착 직후 탄창은 비어 있고 원본 재장전/발사 조건을 기다린다. 사거리 경계·weaponReady·발사 난수·VFX/탄피pool 성공에 따른 RNG 호출 순서를 보존해야 한다.
- 무기 준비에서 음원을 캐시하며 발사는 기존 sound ID를 재사용한다. ML의 원본 중복 재생 요청도 별도 조사 기록에 남겼다. 새context/source를 발사마다 만들거나 사격별DB/파일 쓰기를 추가하지 않는다.

로컬 근거는 `.local/evidence/combat-restoration-20261110/`의 table/rea-evidence.json·numeric-evidence.json, weapon/implementation-rules.md·weapon-evidence.json, core-tests-final.log·weapon-tests-boundaries.log, parallel-final/summary.json·기기별tests.log/xcresult, screenshots/에 있다. 원본 코드의 정적 분석과 새 소스 검사를 원작 실행 관측으로 표현하지 않는다.
