# 개발자의 변과 GitHub·Sass 원고 자료

[개발자의 변 초안](RESTORATION.md) · [기능과 추가 작업](public/features.md) · [개발 과정](public/development-history.md)

## 사용자 원문과 기존 회고

- HeavyMach 2의 2026-11-20 사용자 원문과 네 차례 추가 글감은 이 작업 사본의 `.local/writing/heavymach2-developer-note-20261120.md`에 있다. 첫 메모와 Threads 글감, 즉자·대자, 창작과 경고에 관한 가정, 원작 기능에 관한 괄호 보충이 들어 있다.
- 2026-11-15에 GitHub에 넣도록 지정한 글은 [약식 하네스 도입 경위](harness-adoption-note-20261115.md)에 원문이 있다.
- 11월 20일의 개인 회고와 마지막 두 문단을 한글 초안에 반영했다. 개발 중의 지시와 화면 문제는 각 기록에서 찾을 수 있다. [개발 과정](public/development-history.md) · [타우리 화면 문제](public/troubleshooting.md#7-tauri-mac-화면이-검거나-깨져-보임) · [마지막 두 문단의 기록](todo.md#개발자의-변-마지막-시--사용자-원문)
- 대학생 때의 경험과 개발자로 전업한 계기는 앞선 두 작품의 회고에도 있다. [HeavyMach 1의 개발자의 변](https://github.com/gelb2/HeavyMach/blob/bd0aa73ad0bf765f3e54c1800c89b8d550a5e1e9/docs/RESTORATION.md) · [MachDefense의 개발자의 변](https://github.com/gelb2/MachDefense/blob/387e554a504acce44230fc007bd95253ea425862/docs/RESTORATION.md)

## 원고와 기술 근거

| 글감 | 사용자 기록 | 함께 볼 근거 |
| --- | --- | --- |
| 내가 세 작품을 다시 만든 이유 | iPod touch에서의 경험, 변해준 개발자와 박재철 일러스트레이터에게 받은 시간, 11월 20일의 신체 상태와 판단. | 이전 회고와 [한글 초안](RESTORATION.md#헤비멕을-다시-만든-이유) |
| 원작을 복원한 범위 | 원작 그림·폰트·조작·데이터에 맞추고 에셋 사용과 기기별 부작용을 확인하라는 요청. | [원작에서 복원한 것](public/features.md#원작에서-복원한-것) · [기능별 확인](feature-verification.md) |
| 내가 정한 확장 | 화이트마운틴 포털·스테이지·보스·드롭, 공습의 격파 기준, 실제 기기에서 발견한 상점과 이동 문제. | [내가 한 일](public/features.md#내가-한-일) · [화이트마운틴 원본 조사](original-unused-content-20261119.md) · [설계와 검사](white-mountain-implementation-20261119.md) |
| 모듈 경계와 AI 작업 | 사용자 지시로 Apple 파일을 보존하고 공통 모듈을 가져다 쓰는 Android 포팅. PWA와 Tauri, 모델이 크게 헤맸던 경험과 당시의 Tauri 화면 문제. | [Android로 이어진 경로](public/development-history.md#android웹mac과-저장-이동) · [타우리 화면 증상과 검증](public/troubleshooting.md#7-tauri-mac-화면이-검거나-깨져-보임) |
| 자산가치·창작·안전규약 | Threads에서 기억한 글의 취지, 정크본드·부실채권 비유, 즉자·대자, 카라바지오·젠틸레스키, 게임 효과를 만들 때 경고가 날 것이라는 가정. | [한글 초안의 해당 부분](RESTORATION.md#인간의-자산가치와-창작) · [근접공격·화염방사기 원본 자료](original-unused-content-20261119.md#무기와-입수-경로) |

Threads 글의 게시자와 주소는 기록에 없다. 작성자가 기억한 취지를 사용한다. 글에 적힌 약 80%와 창작을 원할 사람 수는 작성자의 비유와 생각이다. 근접공격·화염방사기 구현 때 경고가 날 것이라는 대목도 작성자가 제기한 가정이다. 원본의 아이콘과 주석 행을 확인했고, 아머드코어 영향은 작성자 추측이다.

GitHub에서는 [README](../README.md)가 게임·설치·기능 소개를 맡는다. [개발자의 변](RESTORATION.md)에 작성자의 동기와 생각을 담고, 구체적인 기능과 기술 과정은 [기능 문서](public/features.md)와 [개발 과정](public/development-history.md)에 연결한다. Sass에서 기술 설명을 쓸 때는 기능과 검사 기록을 참고하고, 인물 이야기를 쓸 때는 로컬에 보관한 사용자 원문부터 읽는다.
