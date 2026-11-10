# 정상 앱의 메뉴·전투 연결

Source 앱의 보행 preview 시작점을 실제 GameRuntime으로 바꿨다. 타이틀→슬롯→이름 입력·저장→기지→전장, 실제 터치 이동·적 선택·발사·메뉴 일시정지/재개를 연결했다. 별도 Validation 앱은 기존106조건 동시 실행을 유지한다. 이번 단계는 HD canvas와 영어 데이터 기준이다. 기기별 최종 화면 비례·전체 언어·모든 게임 행동을 완료했다는 뜻이 아니다.

## 수정한 원본 차이

HD 실행파일 SHA256 `5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090`의 REA/Ghidra 결과와 기존 근거를 재사용했다. 확보한 원본 소스가 아닌 실행파일·데이터에 근거한 새 구현이다.

| 동작 | 기존 문제 | 원본 근거와 수정 |
| --- | --- | --- |
| 이름 생성 | initSlot 뒤 selectSlot/save 누락 | initSlot0x17e10 끝의 selectSlot을 연결. 저장하되 slots 화면 유지. 사용자의 별도 START에서 다음 화면 진입 |
| 슬롯 삭제 | 전체 슬롯을0으로 지우고 저장 요청 없음 | 선택된 raw 슬롯의 used byte8만0으로 변경→기존 saveDataFile 순서. 다른 바이트와 다른 슬롯 보존 |
| 슬롯 시작 | 저장된 전장도 enterBase로 이동 | GuiSelectSlot.selectSlot의 Base 접두어 분기. Base면 screen3, 전장이면 저장된 map/gate의 screen7로 재개 |
| 시작 저장 필드 | working+0x1b5c 증가 누락 | 원본 ARM0x2e5c2–0x2e5cc의32bit+1. GlobalInfo.selectSlot 성공/저장 뒤, 화면 분기 전에 적용. 이름 생성의 내부 select에는 적용하지 않음 |
| 설명문 | raw Value 직접 조회 | openDescriptionList0x16f04처럼 로드 시 ASCII pipe를 개행으로 변경하고 dictionary 생성. 중복 key는 마지막 값. getDescription0x14b84처럼 대체 문구 없음 |
| 오류 알림 | 종류별 title 정보 전달 없음 | 원본 BODY parts/FOOT parts/BOT, Alert!, Incorrect Name/More Games와 설명 key를 전달. 버튼의 대소문자/확인·취소 차이 보존 |

0x1b5c는 최초 도움말 표시 조건에도 쓰인다. 이를 임의로 플레이 횟수라고 명명하지 않는다. Base START는 후속 Base 저장에 증가분이 들어가고, field 재개는 앞선 select 저장 뒤 증가하므로 즉시 디스크에 반영되지 않는 차이도 검사했다.

삭제 순서의 한계: 기존 loaded working 사본이 UI에서 삭제한 슬롯과 같으면, 원본 saveDataFile의 memcpy가 raw used0을 덮을 수 있다는 정적 조건부 분석이 있다. 포팅판 실행에서 이 조건을 아직 재현하지 않았다. 임의로 working=nil이나 별도 삭제 절차를 만들지 않았다. 이번 실제 삭제 검사는 재실행 후 working을 불러오기 전의 슬롯을 대상으로 한다. 현재 loaded 슬롯의 삭제가 전부 검증됐다고 보고하지 않는다.

## Apple 연결

HM2SourceView는 창·터치·이름 입력/확인창·앱 활성 상태를 GameRuntime에 연결한다. 메크·배경·HUD·게임 버튼·글자는 기존 C++/GPU 화면을 사용한다. UIKit 게임 버튼이나 별도 게임 로직을 만들지 않는다. Input 좌표는 실제 renderer의 aspect-fit 역변환을 사용한다.

일반 저장은 앱의 Application Support/HeavyMach2/HeavyMach2.sav다. UI 검사는 별도 Documents/UITests/검증UUID 아래에서 실행한다. UUID를 검증하므로 임의 경로를 주입하지 않는다. 같은 UUID의 실제 앱 재실행으로 저장을 확인했다. 기존 사용자 저장을 검사 fixture로 덮지 않는다.

원본 이름 입력 title은 New User, placeholder는 Name: 1~10 Character, 취소는 소문자 cancel, 확인은 OK다. NSString UTF16 길이1~10을 검사하며 trim하지 않는다. 공백1개와 emoji5개(UTF16 길이10)는 허용하고 emoji6개(12)·빈 이름은 거부했다. 잘못된 UTF8은 현대 플랫폼 API 경계에서 거부한다. 국가 코드는 현재 locale의 countryCode 소문자이며 nil일 때 us다. 해당 국가 PNG가 없으면 공통 tile reader의 nil 경로를 사용하며 국가값을 us로 바꾸지 않는다.

알림은 weak view/weak alert를 사용해 수명 순환을 피한다. 비활성 시 실제 UI 입력 캡처와 경로 입력을 정리하고 기존 코어/오디오 중단 경로를 호출한다. C++ 실행 오류는 OS 오류창으로 연결하며 저장 초기화/덮어쓰기 복구를 추가하지 않았다. SourceView의 fatal 오류 표시 전체는 정적 검토이며, 손상 저장 거부/파일 보존은 별도 Native Runtime 검사에서 실제 확인했다.

## 확인한 결과

- C++55개 전부 통과. 생성·삭제 바이트 보존·저장된 전장 재개·0x1b5c wrap/저장 순서·설명문 dictionary를 포함한다.
- 전체 Core 계측 ASan/UBSan에서 변경 관련6개 검사 통과. 기존106조건1,200프레임 검사도 포함한다. 누수 검사는 포함하지 않았다.
- iPhone/iPad 각각 실제 Source UI 검사1개와 이름 경계 Native 검사1개를 최종 빌드에서 통과했다.
- UIKit 실제 입력: 취소/잘못된 이름/정상 이름→slots 저장1회→별도 START/기지까지 총3회 저장→원본 출구/전장→보행→선택한 적ID와 추가 발사→GPU 메뉴 pause/resume→재실행→삭제 취소/확인→다시 실행한 삭제 상태를 확인했다.
- 최종 iPad는 적ID2 선택과 발사0→5, iPhone은 적ID2 선택과0→13을 확인했다. 읽기 전용 selectionTouches가0→1로 바뀌었다. 해당 검사 중 unsupported action은0이었다. 모든 게임 버튼의 미구현이0이라는 뜻이 아니다.
- 별도 GPU Runtime 검사는 양 기기에서180프레임의 추가 resource read/texture upload/audio 할당0, 실제 원자 저장·재실행, 손상 저장 거부/기존 파일 보존을 통과했다.
- 원본 차이의 이전 코드를 별도 로컬 binary로 재현했고 생성 저장 누락·전체 슬롯 삭제·기지 강제 재개의 실패를 보존했다. 작업 소스는 되돌리지 않았다.

## 다음 소비자

원본 시작 저장에는 기본 Picker04201이 이미 등록돼 있다. 기존106조건은 BOT actor 연결 전의 결과다. 이제 기본 Picker의 실제 생성·추종·획득부터 공통 Scene에 연결하고 Attacker/수리/HP/사망/저장/GPU 조건을 추가한다. 기존 조건 변경이나 난수·픽업 결과 변경을 숨기지 않는다.

최초 전장 도움말·시스템 Help, Player GameOver, 완료 임무 창의 자동 보상/다음 창5007, 점수·Game Center5009의 원본 분석도 확보했다. BOT 이후 이 소비자를 연결한다. 하체23종·모든60임무·전체 맵 이동/캠페인·KR locale·iPhone 가로/iPad 세로의 원작 비례/입력·추가 방향/플랫폼·실기기는 남는다. Goal은 계속 active다.

근거: [검증 수치](evidence/source-entry-validation-20261110.json), `.local/evidence/source-entry-20261110/`의 최종 Source xcresult/log·Core55·ASan6·독립 검토. 원본 절차는 `.local/evidence/combat-runtime-20261110/source-entry-reference/spec.md`, 후속은 `help-gameover-reference/spec.md`, `bot-reference/implementation-rules.md`다. 원본 IPA/ARM·개인 저장·비밀정보는 Git/보고 사이트에 올리지 않는다.
