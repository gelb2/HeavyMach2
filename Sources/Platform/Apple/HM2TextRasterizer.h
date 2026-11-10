#pragma once
#include "Core/UI/Text.hpp"
#include <memory>

namespace hm2::apple {
// Preparation adapter only. The returned A8 bytes go through the common GPU
// resource uploader; this adapter creates no UIView/UILabel/UIButton.
std::unique_ptr<ui::TextRasterizer> makeTextRasterizer();
}
