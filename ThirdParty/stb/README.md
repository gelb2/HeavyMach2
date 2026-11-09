# stb_image 2.28

Copied unchanged from MachDefense commit `0aa03aa7c4adf53d6aa8b33c6d4e193105298d6e`, `Modules/ThirdParty/stb/stb_image.h`.
The same bytes are used by HeavyMach commit `bd0aa73ad0bf765f3e54c1800c89b8d550a5e1e9`, `vendor/stb/stb_image.h`.

Git blob: `d4fd2fc7e6668273f3c9a986111b59631404f770`.
SHA-256: `8b64b9e299f7a0305ac15737647c09603d2758bc01814809d7ed57856bc66a89`.

The upstream MIT / public-domain license is preserved in the header.
`Sources/Assets/Image.cpp` compiles PNG/JPEG memory decoding once. Game frames use prepared GPU textures, with no image decoder or file read in the draw loop.
