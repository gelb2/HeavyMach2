# 기존 복원 코드 재사용

2026-11-10. 소스는 HM2 저장소 안에서 빌드한다. 다른 게임 checkout을 빌드 입력이나 출력 경로로 연결하지 않는다.

| 새 소비 파일 | 고정 출처 | 변경점 |
|---|---|---|
| `Sources/Core/Data/Chunk.hpp`, `Chunk.cpp` | MachDefense `0aa03aa7c4adf53d6aa8b33c6d4e193105298d6e`, `Sources/Core/Data/Chunk.hpp`, `Chunk.cpp` | hm2 namespace. HM2 원본 loadChunkWithFp를 대조. payload 뒤 childCount 경계 검사·원본 헤더/알 수 없는 바이트를 보존한 encode·합성 입력 검사를 추가. stale size를 실제 재귀 길이로 덮지 않음 |
| `Sources/Core/Data/DarwinRandom.hpp` | 같은 MD 커밋, `Sources/Core/Contracts/DarwinRandom.hpp` | hm2 namespace. EncTbl 준비용 지역 난수 상태. macOS libc의5개 seed×5000회와 대조. 게임/저장 난수 소유권 복원 완료가 아님 |
| `ThirdParty/stb/stb_image.h`·`Sources/Assets/Image.cpp` | 같은 MD 커밋의 `Modules/ThirdParty/stb/stb_image.h`. HM1 main `bd0aa73a`의 `vendor/stb/stb_image.h`와 동일 Git blob `d4fd2fc7e6668273f3c9a986111b59631404f770` | stb2.28 원문·내장 MIT/public-domain 허가 보존. PNG/JPEG 메모리 decode, CgBI→RGBA, unpremultiply·float premultiply 준비. 같은 decoder를 공통 C++에서 사용하며 ImageIO는 독립 픽셀 검사의 oracle에만 사용 |

위 코드의 저작권·MIT 허가문은 [원문 고지](../ThirdParty/MachDefense/LICENSE)에 보존한다. 이 고지는 원작 게임 데이터·그림·음원·바이너리의 라이선스를 바꾸지 않는다.

`Models`·`DataTable`은 HM2 HD 실행파일과 원본 데이터에서 확인한 계약으로 작성했다. MD의 소켓 정수화·TileList 매핑·조립 보정·엄격한 TSV 열 수 검사는 복사하지 않았다. 근거 주소와 검사 연결은 [첫 구현 기록](initial-source-restoration-20261110.md)에 있다.

Apple 호스트·일반/검증 타깃·GPU 출력은 기존 두 작품의 구성을 참고했다. 새 렌더러는 기존 OpenGL ES1 스택과 원작 Texture2D의 POT padding·content UV·linear 필터·RGBA blend를 사용한다. 현재는 개별 Tile2D 이미지 fallback의 첫 조립 출력이다. 원작 atlas 생성·그림자·다른 pixel format·실제 게임 표시와의 전체 동등성은 아직 검증하지 않았다.
