# iPad 홈 화면 재설치에서 이전 하단 띠가 남는 문제

공개 v0.5는 반투명 상태 막대 지정을 제거했다. 이 HTML을 읽은 다음 새로 설치한 iPad 9의 아이콘에는 기본 상태 막대가 저장됐고 전장과 HUD가 화면 맨 아래까지 그려졌다. 배포 직후 iPad Air 13에서 다시 설치한 아이콘에는 이전 반투명 설정이 저장됐다. 첫 배포 보고에서 이 실패 사례를 빠뜨렸다. [v0.5 배포 파일과 버전](https://gelb2.github.io/HeavyMach2/build.json)

## 레퍼런스 소스와 실제 공개 파일

| 항목 | HeavyMach 1 | MachDefense | HeavyMach 2 v0.5 |
| --- | --- | --- | --- |
| Safari가 설치할 때 읽는 문서 | 루트에 설치 안내와 정적 매니페스트가 있다. 시작한 뒤 버전이 있는 경로의 게임을 iframe으로 연다. 반투명 설정은 그 게임 문서에 들어 있다. | 루트에 캔버스와 정적 매니페스트가 있다. 기본 상태 막대를 쓴다. | 루트에서 설치와 게임 실행을 처리한다. 기기 분류 후 JavaScript로 휴대폰 또는 일반 매니페스트를 선택한다. v0.3 루트에는 반투명 상태 막대 지정이 있었다. |
| 화면 크기 | 설치 페이지가 정한 iframe 안에서 게임 캔버스의 실제 크기를 읽고, 안전 영역과 방향을 WebAssembly에 전달한다. | 캔버스를 브라우저 안쪽 크기에 맞추고, 안전 영역과 픽셀 비율을 WebAssembly에 전달한다. | 캔버스 사각형과 안전 영역을 공통 표시 모듈에 전달한다. iPad 홈 화면에서는 HUD용 위·아래 안전 영역을 중복 적용하지 않는다. |
| 새 페이지와 오프라인 갱신 | 기본 주소와 index.html은 설치 캐시에서 읽는다. 슬롯·게임 자원은 해시로 버전을 구분한다. 갱신이 준비되면 모든 게임 창을 닫으라는 안내를 준다. | 시작에서 build.json과 워커 상태를 읽고 워커 갱신을 요청한다. 대기 워커는 다른 게임 창이 없으면 자원 검증 후 활성화한다. 시작 페이지는 controllerchange 뒤 다시 열린다. 게임 데이터는 해시에 따라 재사용하며 이전 완성 캐시 하나를 남긴다. | 모든 탐색 주소를 현재 워커의 index.html에 연결한다. 새 워커를 등록하지만 이전 워커가 브라우저 탭을 제어하는 경우 대기 버전의 활성화나 시작 페이지 재로드를 처리하지 않는다. |

HeavyMach 1은 현재 공개 시작 페이지의 `host.js`, `index.html`, `host.css`와 소스의 SHA-256을 대조했다. WASM PR의 게임 문서는 현재도 iframe 안에서 실행되며, 설치는 루트 문서에서 한다. MachDefense에서도 `app.js`, `index.html`, `style.css`의 해시가 소스와 공개 브랜치에서 일치했다. MachDefense의 build.json 요청은 활성 워커에서도 응답할 수 있다. 새 버전 확인에는 워커의 `registration.update()` 호출이 함께 필요하다. [HeavyMach 1 WASM PR](https://github.com/gelb2/HeavyMach/pull/24) · [HeavyMach 1 시작 코드](https://github.com/gelb2/HeavyMach/blob/e3b54774a76823300e786e85fd0a1dc8e6097e44/Distribution/Web/pwa/host.js#L34) · [HeavyMach 1 워커](https://github.com/gelb2/HeavyMach/blob/e3b54774a76823300e786e85fd0a1dc8e6097e44/Distribution/Web/pwa/sw.js) · [MachDefense 시작 코드](https://github.com/gelb2/MachDefense/blob/3f38bd161d159f4a369fa1555a7a38e3ca4e8456/Distribution/Web/app.js#L59) · [MachDefense 워커](https://github.com/gelb2/MachDefense/blob/3f38bd161d159f4a369fa1555a7a38e3ca4e8456/Distribution/Web/sw.js#L62)

## 재현과 변경

iPadOS의 홈 화면 추가는 당시 Safari 문서의 상태 막대 설정을 아이콘에 저장한다. 아이콘을 지우면 Safari 탭이 관리하는 캐시까지 삭제되지는 않는다. 실제 iPad 시뮬레이터에는 이전 방식으로 등록된 v0.5 아이콘과 새 방식으로 등록된 v0.5 아이콘이 둘 다 있었다. 사용자의 기기에서 어떤 단계에 이전 문서가 남았는지는 기기 기록 없이 특정할 수 없다.

실제 Chrome에서 초기 공개판을 열고 두 탭을 유지한 채 v0.5로 서버를 교체했다. 새 워커는 대기 상태였고, 브라우저에서 새로고침한 뒤에도 이전 설치 메타데이터가 나왔다. 따라서 서버가 v0.5를 제공하는 상태에서도 이전 설정으로 다시 설치할 수 있다.

후보에는 배포 패키지의 런타임 파일 목록, 크기와 SHA-256을 이전 패키지와 비교하는 검사를 추가했다. 일치하는 이전 패키지의 검증된 자원을 재사용하고 새 HTML 검증이 끝난 뒤 워커를 활성화한다. 기존 문서를 강제로 다시 열지 않는다. 브라우저가 다음에 탐색할 때 새 HTML을 제공한다. 게임, 음향, 화면 배치, 이벤트 처리, 저장 형식, Apple·Android 소스는 그대로다. [패키지 검사](../Web/package.py) · [워커 설치 경로](../Web/Shell/sw.js) · [브라우저 회귀 검사](../Web/tests/installation-update.mjs)

첫 회귀 검사에서는 비동기 조건이 참이 되기 전에 새로고침했다. 조건의 실제 결과를 반복 확인하도록 검사 코드를 수정했다. 다시 실행하니 열린 탭을 유지하면서 새 HTML을 받았고, 수정이 없는 게임 파일을 다시 받지 않았다. 브라우저를 오프라인으로 바꿔 다시 열고 게임을 시작했다. 변경은 로컬 작업 브랜치에 있다.

## Safari 설치 앱 실측

iPad Air 13의 iPadOS 26.5 Safari에서 v0.3 페이지를 열어 오프라인 캐시를 만들었다. Safari 데이터와 열린 탭을 보존한 상태에서 같은 시험 주소에 수정 후보를 제공했다. Safari에서 주소를 다시 연 뒤 홈 화면에 추가했고, 새 아이콘은 기본 상태 막대로 등록됐다. 새 게임에서 슬롯을 만들고 기지에서 Ruin Field로 나갔다. 이동 입력과 인벤토리·임무·시스템 메뉴를 확인했다. 2,048×2,732 전장 캡처에서 HUD가 마지막 행까지 그려졌다. 기존 64행의 배경 띠는 보이지 않았다. 이전 캐시 Safari (`ipad-v06-old-cache-frames/3A6B7906-6B9D-4962-8BC2-3EC957C0EF85.png`) · 새 설치 등록값 (`v06-ipad-upgraded-icon-metadata.json`) · 전장 캡처 (`ipad-v06-cache-install-frames/3C229AF3-EB0F-4040-AF48-F94E0CD6E829.png`) · 버튼 캡처 (`ipad-v06-install-panels.png`) · XCTest 실행 기록 (`ipad-v06-cache-install.log`)

공개 v0.3과 v0.5의 패키지 명세를 호환 대상으로 검증했다. WASM 해시가 다른 명세는 패키저에서 거부됐다. 브라우저 회귀 검사에서 이전 워커를 둔 채 쿼리가 붙은 설치 주소를 열어도 이전 HTML이 나오던 것을 확인했다. 후보 활성화 후 같은 주소를 새로고침해 새 메타데이터를 확인했다. 브라우저 결과 (`v06-update-browser-corrected.log`) · 다른 해시 거부 (`v06-incompatible-package.log`)

현재 공개 주소는 v0.5다. 후속 변경은 로컬에 있다. 물리 iPad에서 후속 후보를 실행한 기록은 없다.
