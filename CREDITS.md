# HeavyMach 2 제작자와 외부 코드

HeavyMach 시리즈의 개발자는 변해준, 일러스트레이터는 박재철이다. 이 저장소는 HeavyMach 2가 현재 기기에서 다시 실행되도록 복원한 작업이다. 제작자와 시리즈 권리 이력은 [HeavyMach 1의 크레딧](https://github.com/gelb2/HeavyMach/blob/main/CREDITS.en.md), 복원을 시작한 이유는 [개발자의 변](docs/RESTORATION.md)에 남겼다.

복원을 위해 새로 작성한 코드의 이용 조건은 [MIT 라이선스](LICENSE)와 [적용 범위](LICENSE-SCOPE.md)에 있다. 원작 그림·음향·게임 데이터의 권리는 각 권리자에게 있다. 외부 코드는 해당 소스와 다음 고지를 따른다.

- MachDefense에서 가져온 코드와 stb 이미지 디코더는 출처와 허가문을 보존했다. [재사용 내역](docs/source-reuse.md) · [MachDefense MIT 고지](ThirdParty/MachDefense/LICENSE) · [stb 소스와 허가문](ThirdParty/stb/stb_image.h)
- 음향 처리에 사용한 OpenAL Soft의 저작권과 사용 조건은 번들 소스에 있다. [OpenAL Soft](ThirdParty/OpenALSoft/README.md) · [라이선스](ThirdParty/OpenALSoft/COPYING)
- 웹은 Emscripten으로 빌드하며, Mac 앱은 Tauri를 사용한다. [Emscripten](https://github.com/emscripten-core/emscripten/blob/main/LICENSE) · [Tauri](https://github.com/tauri-apps/tauri)
- 원작 실행을 비교하는 호환 앱의 외부 코드는 별도 고지를 따른다. [호환 실행기 고지](Compatibility/THIRD-PARTY-NOTICES.md)
