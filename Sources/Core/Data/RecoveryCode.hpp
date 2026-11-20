#pragma once

#include "Core/Data/SaveData.hpp"

namespace hm2::recovery {

// The code preserves all decoded save bytes except the encoding seed and
// checksum. Each host writes the result through the existing SaveData codec.
std::string encode(const SaveData& save);
SaveData decode(std::string_view code);

}
