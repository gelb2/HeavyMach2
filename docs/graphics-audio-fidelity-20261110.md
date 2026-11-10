# 그래픽·음향의 원본 절차 대조와 확인한 차이

사용자가 원본 그래픽·음향 로직을 따르지 않은 임의 구현이 있는지 질문해 현재 소비 코드를 다시 대조했다. 원본 소스는 확보하지 못했다. 복원판은 HD ARM 분석과 데이터에 근거한 새 C++ 구현이며, 원본 IPA를 실행하는 별도 포팅판과 구분한다.

**원본과 다른 구현과 실제 버그가 있다.** 확인한 차이, 수정한 코드, 실행 검사의 범위를 아래에 구분한다. 일부 검사 성공을 원작 전체의 복원 완료로 쓰지 않는다. 이 단계는 작업 브랜치의 수정·검증 중이며 배포나 실기기 교체 완료가 아니다.

## 1. 효과음 컨텍스트 연결·해제

원본 SoundManager 초기화0x412d0은 context 생성→현재 context 연결 후 반환한다. 이전 context로 복원하지 않는다. 재생0x407e4/0x40aa8은 준비된 source에 position→stop→play를 수행한다.

수정 전 OpenALEffects는 생성자 끝에서 이전 context를 복원하고, prepare/play마다 Scope가 현재 context를 연결한 뒤 이전 값을 복원했다. 새 프로세스에서 이전 값이 nil이면 매 재생마다 bind/unbind했다. 이것은 원본 절차와 다른 구현이다. context 생성·파괴 횟수가 늘지 않는다는 기존 검사만으로는 찾지 못했다.

같은 Core 코드의 Mac 정상 출력 장치 probe에서 다음을 실제 관측했다.

| 시점 | 현재 context | 누적 bind/unbind | context/source/buffer |
| --- | --- | --- | --- |
| 생성 직후 | nil | 1/1 | 1/0/0 |
| 원본 WAV 준비 후 | nil | 2/2 | 1/1/1 |
| 첫 재생 후 | nil | 3/3 | 1/1/1 |
| 둘째 재생 후 | nil | 4/4 | 1/1/1 |

이는 실제 반복 연결·해제의 증거다. iOS 실기기 프레임 지연을 측정한 결과는 아니다.

초기화 후 context를 유지하도록 고쳤다. 같은 owner의 반복 준비·재생·관측에서는 추가 연결·해제를 하지 않는다. 다른 Runtime이 current를 바꾼 경우에만 필요한 전환을 하고 종료 때 정리한다. 저장된 owner 포인터와 실제 alcGetCurrentContext를 구분하고 실제 bind/clear 호출을 센다.

같은 Mac 정상 출력 장치 probe의 수정 후 결과는 생성·준비·첫 재생·둘째 재생 모두 current=owner, 누적 bind1/clear0이다. 종료 때만 clear1이 됐다. 실제 두 재생 사이 추가 bind0/clear0을 확인했다. iPhone·iPad 시뮬레이터에서 각각 Audio3개·Concurrent2개 검사가 수정판으로 통과했다. 각1,200프레임의10,973회 효과음 재생 동안 초기 bind1·clear0·이름 변환0, 실제 current=owner를 확인했다. Core 음향 소스의 ASan/UBSan 독립 실행은 통과했다. vendor OpenAL 정적 라이브러리는 이 독립 실행의 계측 대상이 아니며 누수 검사는 끈 상태다. 실기기의 청취 품질·출력 지연·프레임 개선을 측정한 결과는 아니다.

## 2. 사격 요청의 문자열 할당

수정 전 GameRuntime의 sound bridge는 요청마다 NSString을 만들고 HM2Audio에서 다시 UTF8 이름으로 조회했다. context/source/buffer 생성0은 이 heap 할당0을 뜻하지 않는다. 준비한 이름을 동기 string_view 조회로 전달하도록 고쳤다. Source와 Concurrent의 실제 재생 경로는 NSString 변환을 거치지 않는다. 기존 ObjC API는 남겼다. 새 효과음 엔진이나 플레이어로 교체하지 않았다. 이름 변환 계수0은 SDK 내부까지 포함한 전체 heap 할당0을 뜻하지 않는다.

현재 효과음은 OpenAL Soft를 사용하며 AVAudioPlayer/AVAudioEngine 효과음 대체는 찾지 못했다. 이름 cache·PCM8/16bit/mono/stereo·기본 SFX ID·position→stop→play가 연결돼 있다는 사실과, 컨텍스트/문자열 절차가 잘못됐다는 사실을 구분한다.

## 3. 슬롯 미리보기 그림자 좌표

GameSession.preview가 만든 본체와 그림자를 Screens의 PreviewNode가 슬롯 위치에 옮긴다. 기존 구현은 Sprite.center와 strip만 옮기고 그림자의 AtlasBatch 정점은 옮기지 않는다. Renderer의 atlas 경로는 center 대신 batch의 실제 정점을 사용한다. 따라서 본체만 슬롯으로 이동하고 그림자는 원점 부근에 남는 버그다.

부모 위치를 본체·strip·atlas 정점에 함께 적용하도록 고쳤다. 원본 preview Frame은 변형하지 않고 node 준비 시 복사·변환한 atlas payload를 재사용한다. 슬롯3개·원점 아닌 좌표·UV/color/index/clip 보존을 회귀 검사에 추가했다. append 때 preview atlas payload를 다시 만드는지 별도로 검사한다. Screens::append 전체의 heap 할당0을 주장하지 않는다.

## 4. 플레이어 출력 방식·슬롯 레이어·항공기 그림자

원본 setHvMach0x1c780은 machType0인 Player 본체를 direct 출력하고 다른 type의 본체는 atlas로 등록한다. 슬롯도 Player를 생성한다. 앞선 감사에서 슬롯 본체의 direct 출력 자체를 문제로 읽은 부분을 정정한다. 실제 차이는 전장 Player까지 atlas로 강제한 코드였다. 공통 helper에서 owner kind에 따라 원본 방식을 사용하도록 고쳤다.

원본 direct 출력은 이번 tick의 표시 범위를, atlas 출력은 이전 표시 범위를 등록에 사용한다. setPartsInfo가 저장하는 boundRect는 회전하지 않은 그림 크기에서 raw socket0을 뺀 값이다. 조립 중심에도 socket 보정이 있다는 이유로 이 bounds 보정을 임의 제거하지 않았다. 경계의 바로 안/밖과 숨김·제거·현재/이전 tick을 검사한다.

원본 슬롯은 GUI·본체가 layer3, 그림자는 명시적 layer0이다. SelSlot_BG.png의 모든 픽셀은 불투명하므로 layer0 그림자가 배경 뒤에 가려진다. 복원판은 슬롯 전체를 layer7에 올렸다. 실제 원본 레이어로 나눠 제출하도록 고쳤다. 앞서 새 Native 검사에 넣은 “그림자가 보여야 한다”는 기준도 원본과 달랐다. 새 검사는 원본 그림자0→불투명 배경3→본체3 순서, 배경에 의한 그림자 가림과 세 본체의 실제 표시를 확인한다. 이전 잘못된 기준과 실패/성공 로그는 보존한다.

독립 검토에서 항공기 그림자 위치 오류도 찾았다. 원본은 machType2의 shadowLen을10으로 정한 뒤 shadow child를 생성하므로 offset은 (10,-10)이다. 복원 helper는 모든 owner에 (4,-4)를 썼다. 공통 helper에 kind별 offset을 연결했다. 실제 Stage02_07의 적 항공기9개 파츠와 Stage01_02의 BOT7개 파츠에서 본체 기준 (10,-10)을 확인했다. 각도·반전에서 offset 자체를 회전하지 않는 원본 규칙도 검사했다. 독립 검토에서 이 P2의 소스·CPU 회귀 해결을 확인했다.

슬롯에서 같은 texture를 쓰는 여러 preview의 atlas batch는 현재 각각 보존한다. 원본의 전역 batch 병합과 같은 drawcall 수까지 맞췄다고 주장하지 않는다. 위치·레이어·픽셀 검사와 batch 병합 여부를 구분한다.

## 5. packed PartsTile의 정상 활성 여부

원본 PartsManager/ImageAttacher에 여러 PNG를 굽는 packed bake/tileNo 구현이 있고 현행 생산 코드에는 그 packer가 없다. 그러나 추가 정적 분석에서 정상 슬롯·전장 생성은 빈 dictionary를 조회하고 miss면 개별 PNG와 alpha tile을 사용하는 경로로 확인됐다. packed 선택은 tileCnt>0뿐 아니라 getPartsTile의 non-nil 반환이 필요하다.

addTile·bake·addMach·load의 정상 외부 caller와 실제 selector 참조는 찾지 못했다. getter의 selector 참조는 확인했다. addMach는 filename 목록을 만들 뿐 bake를 호출하지 않고, load도 packed dictionary를 채우지 않는다. 따라서 packer 미구현만으로 실제 원본의 정상 제작·소비 경로를 생략했다고 단정한 표현을 정정한다. 이 판단은 정적 분석이며 실행 중 호출0이나 getter miss를 직접 관측한 것은 아니다. 별도 격리된 포팅판에서 안전한 호출/반환 관측을 준비하고 있다. baker 강제 호출이나 원본 IPA 변경으로 결과를 만들지 않는다.

그리기 명령을 묶는 VisualLayer atlas와 여러 PNG를 texture 하나로 굽는 PartsTile bake는 다른 단계다. 전장에서는 MCB socket 조립→공통 pose→owner별 direct/atlas→GPU를 사용한다. Player/Enemy/BOT의 A8 그림자 alpha>>1·~shd·layer0도 공통 helper를 쓴다.

## 6. 플랫폼 연결과 남은 대조

- Apple 출력은 공통 C++ GPU 명령을 소비한다. UIKit 게임 버튼·이미지 뷰는 검색한 생산 경로에서 찾지 못했다. 글자는 준비 단계에서 비트맵으로 만든 뒤 GPU 텍스처로 소비한다. 전체 원본 글꼴/좌표의 동일성은 별도 대조다.
- BGM은 AudioQueue를 사용한다. 큐/버퍼/반복의 원본 대응과, 현재 중단·복귀·Prime 정책의 차이를 나눠 기록한다. 원본 AppDelegate의 조사한 비활성/활성 callback은 no-op이므로 현재 UIScene 정책과 같다고 주장하지 않는다.
- 원본 lazy 음원 준비와 현재57음원 사전 준비, Apple OpenAL과 OpenAL Soft, 지원 이미지 형식·Retina·캐시/수명 차이는 별도 항목이다. 현재 프레임 중 IO0·할당0 검사를 원본 절차 전체의 동일성으로 확대하지 않는다.

앞선 Core68개·ASan/UBSan66개와112조건 검사는 구현한 기능·입력/출력의 회귀 근거다. 이 차이를 놓친 상태의 성공을 원본 그래픽·음향 전체 복원으로 사용하지 않는다. 실제 원본과의 기능·화면·음향·자원 수명 대조를 계속한다.

로컬 근거는 `.local/evidence/ipad-ui-feedback-20261110/runtime-fidelity/`의 graphics-audit·audio-audit·context probe와 각 manifest다. IPA·ARM·사용자 저장·비밀은 Git/보고 사이트에 올리지 않는다.

## 7. GPU 픽셀 검사의 실패와 예열 후 결과

원본 레이어와 부모 좌표를 적용한 최초 Native 슬롯 검사도 실패했다. iPhone은786,432바이트 중 예상 화면과12바이트가 달랐고, iPad는 그림자 가림 비교에서5바이트가 달랐다. 이 결과를 성공으로 바꾸거나 허용 오차를 늘리지 않았다.

진단에서는 게임 tick·입력·Frame 변경 없이 같은 명령만 반복해도12개 색상 채널이1씩 바뀐 뒤 유지되는 현상을 확인했다. GL_DITHER를 꺼도 발생했다. 별도 RGBA8 버퍼에서도 같은 현상이 나와 drawable 전용 원인으로 좁히지 못했다. 네 화면을 한 버퍼의 패널로 옮기는 비교도118바이트 차이를 만들었다. 초기 셰이더 준비나 좌표 이동의 정밀도가 원인이라는 설명은 아직 가설이다.

진단을 위해 추가한 offscreen 출력 API는 제품에서 제거했다. 해당 소스·실패 로그·검토 기록은 로컬에 보존했다. 기존 제품의 출력 API와 GL 상태를 유지한다.

최종 슬롯 검사는 새 참조 Renderer를120draw 준비한 뒤 같은 viewport에서 비교한다. iPhone/iPad 각각 동일Frame16회 반복의 전체 바이트 차이0, 실제/예상Frame786,432바이트 차이0, 그림자 제거판과 차이0, 세 슬롯 본체의 실제 표시365/364/365픽셀을 확인했다. preview atlas payload120draw 재사용과 자원 읽기·새 업로드·글자 준비 증가0도 통과했다.

이 결과는 예열 후의 공통 GPU 명령·좌표·레이어 가림 검사다. 초기1-LSB 변동의 원인 해결, 단일 초기Frame의 완전 결정성, 원본 앱 전체의 픽셀 동등성을 뜻하지 않는다. 항공기 그림자 수정까지 포함한 Native 관련7개 검사는 iPhone/iPad에서 각각 모두 통과했다. 공통 코드68개·ASan/UBSan66개와 독립 검토도 마쳤다. 게임의 develop/main 배포와 물리 iPad 교체는 하지 않았다. 전체 게임 복원 Goal은 계속 active다.
