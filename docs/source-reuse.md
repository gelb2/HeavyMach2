# 기존 복원 코드 재사용

2026-11-10에 가져온 소스와 HeavyMach 2에서 바꾼 부분이다.

| 새 소비 파일 | 고정 출처 | 변경점 |
|---|---|---|
| `Sources/Core/Data/Chunk.hpp`, `Chunk.cpp` | MachDefense `0aa03aa7c4adf53d6aa8b33c6d4e193105298d6e`, `Sources/Core/Data/Chunk.hpp`, `Chunk.cpp` | hm2 namespace. HM2 원본 loadChunkWithFp를 대조. payload 뒤 childCount 경계 검사·원본 헤더/알 수 없는 바이트를 보존한 encode·합성 입력 검사를 추가. stale size를 실제 재귀 길이로 덮지 않음 |
| `Sources/Core/Data/DarwinRandom.hpp` | 같은 MD 커밋, `Sources/Core/Contracts/DarwinRandom.hpp` | hm2 namespace. EncTbl 준비용 지역 난수 상태. macOS libc의5개 seed×5000회와 대조. 게임/저장 난수 소유권 복원 완료가 아님 |
| `ThirdParty/stb/stb_image.h`·`Sources/Assets/Image.cpp` | 같은 MD 커밋의 `Modules/ThirdParty/stb/stb_image.h`. HM1 main `bd0aa73a`의 `vendor/stb/stb_image.h`와 동일 Git blob `d4fd2fc7e6668273f3c9a986111b59631404f770` | stb2.28 원문·내장 MIT/public-domain 허가 보존. PNG/JPEG 메모리 decode, CgBI→RGBA, unpremultiply·float premultiply 준비. 같은 decoder를 공통 C++에서 사용하며 ImageIO는 독립 픽셀 검사의 oracle에만 사용 |

가져온 코드에는 [MachDefense의 MIT 고지](../ThirdParty/MachDefense/LICENSE)를 적용한다. [원작 자원 이용 조건](../ASSET-LICENSES.md)

`Models`·`DataTable`은 HM2 HD 실행파일과 원본 데이터에서 확인한 계약으로 작성했다. 근거 주소와 검사 연결은 [첫 구현 기록](initial-source-restoration-20261110.md)에 있다.

Apple 호스트·일반/검증 타깃·GPU 출력은 기존 두 작품의 구성을 참고했다. [초기 정적 조립](initial-source-restoration-20261110.md)은 개별 Tile2D를 출력했다. 현재 공통 전장 프레임은 [atlas 배치](../Sources/Core/Presentation/VisualLayer.hpp)와 [그림자 준비](../Sources/Core/Presentation/PreparedTiles.cpp)를 쓴다. Apple·Android의 OpenGL ES와 웹·Tauri의 WebGL2가 같은 프레임을 소비한다. [후속 그래픽·음향 대조](graphics-audio-fidelity-20261110.md)에 직접 출력과 배치, 그림자와 원작 동등성의 확인 범위를 구분했다.
