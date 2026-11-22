# 외부 코드와 라이선스 고지

[English](THIRD_PARTY_NOTICES.en.md)

## 게임과 빌드에 쓰는 코드

| 구성요소 | 용도와 버전 | 라이선스와 원문 |
| --- | --- | --- |
| OpenAL Soft | 1.22.2. Apple·Android 효과음에 정적으로 연결한다. | 주요 소스 LGPL-2.0-or-later, 일부 BSD-3-Clause. 파일별 고지를 따른다. [번들 원문](ThirdParty/OpenALSoft/COPYING) · [LGPL](LICENSES/OpenAL-Soft-LGPL-2.0.txt) · [BSD](LICENSES/OpenAL-Soft-BSD-3-Clause.txt) · [고정 출처](LICENSES/OpenAL-Soft-source.json) |
| stb_image | 2.28. 공통 C++ 코드에서 PNG·JPEG를 읽는다. | 원문에서 허용하는 MIT 조건을 사용한다. Copyright © 2017 Sean Barrett. [소스 안의 허가문](ThirdParty/stb/stb_image.h) · [공식 원문](https://github.com/nothings/stb/blob/master/LICENSE) |
| MachDefense에서 가져온 코드 | 데이터 청크, 난수 처리, PCM, 효과음, Apple 음악 처리, OpenAL 빌드 스크립트 일부를 가져와 수정했다. | MIT. [보존한 허가문](ThirdParty/MachDefense/LICENSE) · [재사용 코드와 원본 커밋](docs/source-reuse.md) · [음향 코드 목록](LICENSES/OpenAL-Soft-source.json) |
| Gradle과 Wrapper | Android 빌드 도구. 배포판은 8.13이다. Wrapper JAR에 `META-INF/LICENSE`가 들어 있다. | Apache-2.0. [고정 버전과 SHA-256](Android/gradle/wrapper/gradle-wrapper.properties) · [Gradle 라이선스](https://github.com/gradle/gradle/blob/v8.13.0/LICENSE) · [배포판 NOTICE](https://github.com/gradle/gradle/blob/v8.13.0/subprojects/distributions-full/src/toplevel/NOTICE) |
| Emscripten과 musl | 웹과 Tauri 웹 화면의 Wasm·JS를 빌드한다. 확인에 사용한 SDK는 6.0.13이다. 웹 효과음은 SDK의 OpenAL 구현을 사용한다. | Emscripten은 MIT 또는 University of Illinois/NCSA. musl은 MIT 및 파일별 고지를 따른다. [Emscripten 원문](https://github.com/emscripten-core/emscripten/blob/6.0.13/LICENSE) · [musl COPYRIGHT](https://github.com/emscripten-core/emscripten/blob/6.0.13/system/lib/libc/musl/COPYRIGHT) · [웹 연결 설정](Web/CMakeLists.txt) |
| Tauri | 2.12.5. Mac 데스크톱 호스트다. | MIT 또는 Apache-2.0. 각 Rust·npm 패키지의 개별 고지를 따른다. [MIT](https://github.com/tauri-apps/tauri/blob/tauri-v2.12.5/LICENSE-MIT) · [Apache-2.0](https://github.com/tauri-apps/tauri/blob/tauri-v2.12.5/LICENSE-APACHE-2.0) · [Cargo.lock](Desktop/Tauri/src-tauri/Cargo.lock) · [package-lock.json](Desktop/Tauri/package-lock.json) |

정적으로 연결한 OpenAL Soft의 라이브러리 소스와 앱 소스는 이 저장소에 있다. Apple은 [OpenAL 빌드 스크립트](tools/build_openal.py), Android는 [CMake 설정](Android/cmake/CMakeLists.txt)으로 빌드한다. Android에서 다시 링크할 때는 같은 빌드가 적용하는 [OpenSL 재개 수정](Android/cmake/opensl-resume.cmake)을 함께 사용한다. 배포본의 재링크 조건은 [LGPL 제6절](LICENSES/OpenAL-Soft-LGPL-2.0.txt)을 따른다. [플랫폼별 빌드](docs/public/build-and-install.md)

## 원작 비교용 실행기

touchHLE 계열 실행기와 수정 패치는 MPL-2.0 고지를 유지한다. SDL2와 `rust-sdl2`에는 각각 원본 라이선스와 저작권 고지가 적용된다. 실행기·SDL 연결 코드·기준 패치의 커밋을 고정했다. [호환 실행기 고지](Compatibility/THIRD-PARTY-NOTICES.md) · [고정 커밋](Compatibility/runtime.json) · [SDL2 원문](https://github.com/libsdl-org/SDL/blob/SDL2/LICENSE.txt) · [rust-sdl2 원문](https://github.com/touchHLE/rust-sdl2/tree/b67f98fe6a147773b6f05d50ae775d9fe16561e7)

[코드 라이선스 범위](LICENSE-SCOPE.md) · [원작 자원 이용 조건](ASSET-LICENSES.md) · [제작자](CREDITS.md)
