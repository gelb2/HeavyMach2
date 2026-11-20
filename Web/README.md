# HeavyMach 2 웹 빌드

브라우저 호스트는 Emscripten으로 공통 C++ 세션, 화면, 복구코드 구현을 빌드한다. WebGL 2는 공통 프레임에 포함된 스프라이트, 아틀라스와 궤적을 그린다. DOM은 실행과 설치, 이름·복구코드 입력을 제공한다. 에셋은 로컬에서 준비한 캠페인 디렉터리만 읽는다.

## 빌드와 실행

Emscripten SDK를 활성화한 셸에서 저장소 루트에서 실행한다. 검증에는 Emscripten 6.0.13을 사용했다.

```sh
export EM_CACHE="$PWD/.local/emscripten-cache-hm2"
emcmake cmake -S Web -B build/web-normal -DCMAKE_BUILD_TYPE=Release
cmake --build build/web-normal --parallel 6
emcmake cmake -S Web -B build/web-validation -DCMAKE_BUILD_TYPE=Release -DHM2_WEB_VALIDATION=ON
cmake --build build/web-validation --parallel 6
python3 -m http.server 8773 --bind 127.0.0.1 --directory build
```

- 정식판은 [로컬 정식 주소](http://127.0.0.1:8773/web-normal/dist/)에서 연다.
- 검증판은 [로컬 검증 주소](http://127.0.0.1:8773/web-validation/dist/)에서 연다.

기본 에셋 입력은 `.local/prepared/Campaign/HD`다. 다른 준비본은 `-DHM2_WEB_ASSETS=경로`로 지정한다. `dist/asset-manifest.json`에 현재 산출물의 크기와 SHA-256을 기록한다. 앱 설치 아이콘은 웹 검사에 쓰는 임시 아이콘이다.

## 저장과 화면

브라우저는 정식·검증판에 다른 IndexedDB 저장소를 사용한다. 서비스 워커 캐시도 에디션, 설치 경로, 빌드 버전으로 구분한다. 저장과 복구는 IndexedDB 트랜잭션이 끝난 뒤 공통 세션에 성공을 반환한다. Tauri에서는 같은 웹 빌드를 실행하고, 주입한 `window.HM2Desktop`의 `read(key)`와 `write(key, bytes)`를 기다린다. 키는 `save`, `backup`, `hud/compact`, `hud/regular`다.

폰, 태블릿, 데스크톱 프로필은 앱 시작 때 고른다. 뷰포트와 안전 영역은 창 크기가 바뀔 때 다시 계산한다. 데스크톱에서 상점과 사령부는 원본 HD 패널에 조작부를 맞추고, 빈 주변에는 같은 그림의 조작부 없는 영역을 같은 가로세로 비율로 확대한다. 터치 기기에서 창 크기나 방향을 바꿔도 기기 분류는 유지된다.

## 자체 검사

```sh
npm --prefix Web ci
npm --prefix Web test
npm --prefix Web run smoke
```

브라우저 검사는 새 Chrome 프로필과 로컬 HTTP 서버에서 시작한다. 복구와 두 빌드의 저장 분리, 기지의 상점·사령부·전장 이동, 폰·태블릿 터치, 오프라인 재실행을 확인한다. 결과 이미지와 JSON은 `.local/evidence/web-smoke-20261120`에 기록한다. `HM2_WEB_TEST_RECOVERY_FIXTURE`에 테스트용 복구코드 파일을 지정하면 가져온 코드를 다시 생성해 파일과 비교한다. 실기기의 설치와 저장 동작은 별도 확인이 필요하다.
