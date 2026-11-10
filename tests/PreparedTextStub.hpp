#pragma once
#include "Core/UI/Screens.hpp"

namespace hm2::test {
// CPU game-state tests provide the declared A8 layout without an OS font/GPU.
// Native tests use the real Apple rasterizer and must not reuse this stub.
inline ui::PreparedScreenText preparedTextStub(const ui::TextRasterRequest& request){
    auto bitmap=std::make_shared<ui::TextBitmap>();bitmap->request=request;
    bitmap->layout=ui::textRasterLayout(request);
    bitmap->alpha.resize(std::size_t(bitmap->layout.width)*bitmap->layout.height);
    return {"cpu-text:"+request.sourceId,bitmap};
}
}
