# Sass 집필과 미디어 인계 목록

[프로젝트 소개](../../README.md) · [English](../../README.en.md) · [플레이 화면](../public/gameplay-gallery.md) · [Gameplay gallery](../en/public/gameplay-gallery.md)

## Apps 소개 문안

### 한국어

**HeavyMach 2**

메크를 조립하고 전장에서 이동 경로를 그어 싸운다. 기지의 사령부에서 임무를 받아 진행하고, 전리품과 보상으로 장비를 바꾼다. iPad용 HD 1.0.3을 기준으로 게임 동작과 데이터를 복원했고, iPhone의 화면과 조작은 일반판 2.0.1과 대조했다. 원작에 남아 있던 화이트마운틴 기지와 설원 그림 여섯 장에 포털과 적 배치, 보스 임무를 연결했다. iPhone·iPad, Android, 웹·PWA와 Apple Silicon Mac에서 플레이할 수 있다.

### English

**HeavyMach 2**

Assemble a mech, draw a route across the battlefield, and engage enemy units. Accept missions at base Command Centers and use loot and rewards to change your equipment. The restoration follows the game behavior and data of iPad HD 1.0.3 and compares the iPhone screens and controls with the regular 2.0.1 release. It connects the original White Mountain base artwork and six snowfield images to portals, enemy placements, and boss missions. Play on iPhone, iPad, Android, the web or an installed PWA, and Apple silicon Macs.

### 기술 본문에 연결할 근거

| 이야기 | 쓸 내용과 근거 |
| --- | --- |
| 원작 복원 | Internet Archive에서 받은 원본 빌드, HD 1.0.3의 동작·데이터, 일반판 2.0.1의 iPhone 좌표와 조작을 설명한다. [시작 경위](../../README.md#heavymach-2를-다시-만드는-이유) · [두 판본의 차이](../original-phone-hd-separation.md) |
| 화이트마운틴과 남아 있던 장비 | 확인한 원작 판본에는 화이트마운틴 그림, 설원 그림, 저격총과 LS 무기의 자료가 남아 있었다. 복원 작업에서 포털·전장 배치·보스 의뢰·장비 입수 조건을 만들어 플레이 경로에 넣었다. [원본 조사](../original-unused-content-20261119.md) · [내가 한 일](../public/features.md#내가-한-일) · [데이터와 검사](../white-mountain-implementation-20261119.md) |
| 공통 코드와 플랫폼 | C++ 게임 코어를 네 호스트가 쓴다. 웹은 Wasm·WebGL 2를 사용하고 Mac 앱은 Tauri로 웹 빌드를 묶는다. 헤비멕1의 `fastutlego` 웹 이식은 이 구성과 입력·음향·저장 연결의 참고였다. [구조](../public/architecture.md) · [헤비멕1 PR #24](https://github.com/gelb2/HeavyMach/pull/24) · [크레딧](../../CREDITS.md) |
| 저장 이동과 검증 | 복구코드는 슬롯 세 개를 함께 옮긴다. 받는 앱은 교체하기 전에 기존 저장을 백업한다. Android 정식판·PWA·Tauri Mac의 실제 검사 조건은 서로 다르다. [복구코드](../save-transfer.md) · [기기별 확인 범위](../public/validation.md) |
| 제보와 수정 | iPhone 상점·점프·Skyway는 재현과 수정 기록을 인용한다. 사용자 플레이에서 나온 제보와 Codex의 재현·구현을 각각 표시한다. [트러블슈팅](../public/troubleshooting.md) · [사용자의 검사 방법](../public/testing-method.md) |

2026년 11월 22일 기준, [공개 PWA](https://gelb2.github.io/HeavyMach2/)는 [v0.3](https://github.com/gelb2/HeavyMach2/releases/tag/v0.3)이다. Android 정식 APK는 [v0.4](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4), Android 치트 APK와 Apple Silicon Mac ZIP은 [v0.1](https://github.com/gelb2/HeavyMach2/releases/tag/v0.1)에서 제공한다. iPhone·iPad는 v0.4 소스를 받아 Xcode에서 본인 개발 팀으로 서명한다. [설치·빌드](../public/build-and-install.md) · [릴리스 기록](../public/release-policy.md)

## Mags 집필에 쓸 원문

| 주제 | 한국어 원문과 영문 |
| --- | --- |
| 헤비멕2를 다시 만든 이유 | [README 도입](../../README.md#heavymach-2를-다시-만드는-이유) · [Why I am restoring HeavyMach 2](../../README.en.md#why-i-am-restoring-heavymach-2) |
| 3부작을 마친 소회와 두 제작자 | [3부작 복원 후의 소회](../../README.md#3부작-복원-후의-소회-나는-두-제작자에게-의무를-다-했다) · [Reflections after restoring the trilogy](../../README.en.md#reflections-after-restoring-the-trilogy-i-have-fulfilled-my-duty-to-the-two-creators) |
| 헤비멕이 개발자 전업과 게임 제작에 미친 영향 | [개발자의 변](../RESTORATION.md) · [Developer’s statement](../en/heavymach2-restoration.md) · [헤비멕1 개발자의 변](https://github.com/gelb2/HeavyMach/blob/main/docs/RESTORATION.md) · [맥디펜스 개발자의 변](https://github.com/gelb2/MachDefense/blob/main/docs/RESTORATION.md) · [HeavyMach 1 developer’s statement](https://github.com/gelb2/HeavyMach/blob/main/docs/en/RESTORATION.md) · [MachDefense developer’s statement](https://github.com/gelb2/MachDefense/blob/main/docs/en/RESTORATION.md) |
| 사용자가 정한 기획과 직접 확인한 플레이 | [개발 과정](../public/development-history.md) · [Development history](../en/public/development-history.md) · [검사 방법](../public/testing-method.md) · [Testing method](../en/public/testing-method.md) |

3부작 회고를 인용할 때는 위 README의 독립 소제목과 본문을 한 벌로 사용한다. 사용자에게 선택·지시·플레이를, Codex에게 조사·구현·자동 검사에서 실제로 맡은 일을 표시한다. 기술 범위나 릴리스 확인이 필요하면 개발자의 변 옆에 해당 증거를 붙인다.

## 미디어 인계 목록

대표 이미지로 iPad 타이틀을 쓴다. 기기별 동작과 화이트마운틴 설명에는 아래 장면을 선택한다. 갤러리의 시뮬레이터 캡처를 인용할 때도 기기와 판본을 표시한다.

| 파일과 장면 | 한국어 캡션 | English caption | 촬영·검사 근거 |
| --- | --- | --- | --- |
| [iPad 타이틀 JPG](../../Web/Shell/social-preview.jpg) | iPad의 헤비멕2 타이틀에 TAP TO START GAME이 표시된다. | HeavyMach 2’s iPad title screen shows TAP TO START GAME. | [타이틀과 아이콘](../app-icons.md) · [README의 소개 이미지](../../README.md#플레이-화면과-영상) |
| [iPad 연못 교전 GIF](../public/media/player-captures-20261121/ipad-field-combat.gif) · [해안 교전 GIF](../public/media/player-captures-20261121/ipad-combat-route.gif) | iPad 세로 화면에서 메크가 연못 사이를 이동하고, 해안 전장에서는 이동 경로를 그으며 싸운다. | In the iPad portrait view, the mech moves between ponds and fights along a route drawn across the coastal battlefield. | 11월 12일 사용자 녹화. 연못 69.6–77.6초, 해안 11.3–19.3초. 각 8초, 무음. [구간·해시](../public/media/player-captures-20261121/sources.json) |
| [iPhone 사령부](../public/media/player-captures-20261121/iphone-command.png) · [Skyway Base](../public/media/player-captures-20261121/iphone-skyway-base.png) | iPhone 가로 화면에 진행 중인 의뢰와 Skyway Base 포털 주변 전장이 보인다. | The iPhone landscape screens show active missions and the battlefield near the Skyway Base portal. | 11월 14일 사용자 캡처. [출처와 해시](../public/media/player-captures-20261121/sources.json) |
| [갤럭시 Ruin Field](../public/media/gameplay-20261121/galaxy-field.png) | 갤럭시 정식판에서 메크 앞에 이동 목표가 표시된다. | The regular Galaxy build shows a waypoint in front of the mech. | 갤럭시 정식판 새 슬롯. [검사](../../Android/검증-20261120.md) · [캡처 근거](../public/media/gameplay-20261121/sources.json) |
| [iPad 전장 MP4](../public/gameplay-gallery.md#ipad-세로-전장) | Ruin Field의 세로 전장과 HUD를 보여 준다. | The iPad clip shows the Ruin Field battlefield and HUD in portrait. | iPad Pro 11형 시뮬레이터. 12초, 무음. [녹화 구간·해시](../public/media/gameplay-20261121/sources.json) |
| [Desert 01 포털](../public/media/white-mountain-gameplay/desert-01-portal.webp) · [기지 출구](../public/media/white-mountain-gameplay/white-mountain-base.webp) · [설원 여섯 곳](../public/gameplay-gallery.md#화이트마운틴-기지와-설원) | Desert 01에서 White Mt. Base에 들어가 Snow Pass 출구를 선택한다. | Enter White Mt. Base from Desert 01, then select the Snow Pass exit. | Apple 시뮬레이터 치트판. Snow Pass·Snow Fort·Snow Field·Snow Ridge·Snow Valley는 갤러리의 설원 절, Snow Basin은 보스 절에서 확인한다. [포털·배치 검사](../white-mountain-dual-release-20261120.md) |
| [Boss 05 의뢰](../public/media/white-mountain-gameplay/boss-05-mission.webp) · [전투](../public/media/white-mountain-gameplay/boss-05-combat.webp) · [보상](../public/media/white-mountain-gameplay/boss-05-reward.webp) | 화이트마운틴 사령부에서 Boss 05 의뢰를 받아 Snow Basin에서 격파하고 보고한다. | Accept the Boss 05 mission at White Mt. Base, defeat it in Snow Basin, and report back. | iPad 시뮬레이터 치트판. [등장·격파·보상 근거](../white-mountain-implementation-20261119.md#2단계-보스와-무장) |
| [Sn-IGn 상점](../public/media/white-mountain-gameplay/sniper-shop.webp) · [발사](../public/media/white-mountain-gameplay/sniper-combat.webp) · [LS-D 드롭](../public/media/white-mountain-gameplay/ls-10311-drop.webp) · [획득](../public/media/white-mountain-gameplay/ls-10311-inventory.webp) | 상점의 저격총 Sn-IGn와 전장에서 전리품으로 떨어진 LS-D | The Sn-IGn sniper rifle in the shop and an LS-D dropped as battlefield loot | iPad 시뮬레이터 치트판. LS-D는 난수 시드를 고정해 희귀 드롭을 재현했다. [일반 플레이 입수 조건](../public/gameplay-gallery.md#저격총과-ls-무기) |
| [레이저 공습 구매](../public/media/white-mountain-gameplay/laser-airstrike-buy.webp) · [퀵 슬롯](../public/media/white-mountain-gameplay/laser-airstrike-quick-slot.webp) · [발동](../public/media/white-mountain-gameplay/laser-airstrike-active.webp) | 상점에서 Laser Air-Strike를 사서 퀵 슬롯에 넣고 전장에 사용한다. | Buy a Laser Air-Strike, assign it to a quick slot, and call it in on the battlefield. | 레벨 22의 iPad 시뮬레이터 치트판. [가격과 검사 조건](../campaign-and-ipad-followup-20261120.md#공습-레벨-보정) |
| [Picker D 장착](../public/media/white-mountain-gameplay/picker-d-equipped.webp) · [접근](../public/media/white-mountain-gameplay/picker-d-approach.webp) · [전달](../public/media/white-mountain-gameplay/picker-d-collection.webp) · [인벤토리](../public/media/white-mountain-gameplay/picker-d-inventory-after.webp) | Item Picker D를 첫 BOT 칸에 장착한 뒤 전장에서 전리품을 건네받는다. | With Item Picker D equipped in the first BOT slot, the player receives loot collected on the battlefield. | iPad Pro 13형(M4) 시뮬레이터 치트판. [공습과 BOT 검사](../public/gameplay-gallery.md#레이저-공습과-수집-bot) |

팀에 가져갈 때는 공개 파생 파일, 한·영 캡션, 해당 갤러리 절과 출처 명세를 함께 사용한다. 화면 비율과 영상 속도는 그대로 사용한다. 게시 전에 실제 공개 화면에서 전 구간을 재생하고 GIF의 반복 경계도 확인한다. 화면이 깜박이거나 검은 영역이 나타나는 캡처는 게시하지 않는다. 자원의 이용 조건과 코드를 인용할 때 적용할 조건은 각 문서에 있다. [원작 자원 이용 조건](../../ASSET-LICENSES.md) · [외부 코드 고지](../../THIRD_PARTY_NOTICES.md)
