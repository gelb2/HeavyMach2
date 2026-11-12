#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

namespace hm2 {
using Bytes=std::vector<std::uint8_t>;
struct ResourceAccess {
    virtual ~ResourceAccess()=default;
    virtual Bytes read(std::string_view name)=0;
    virtual bool exists(std::string_view name)const=0;
};
}
