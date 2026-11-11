#pragma once
#include "Core/Data/DataTable.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace hm2 {
// GlobalInfo.openDescriptionList0x16f04 builds a dictionary once and changes
// UTF-8 ASCII pipes into newlines. getDescription0x14b84 has no fallback.
class Descriptions {
    std::map<std::string,std::string,std::less<>> values_;
public:
    static Descriptions parse(const DataTable& table){
        Descriptions result;
        for(std::size_t row=0;row<table.rowCount();++row){
            const auto key=table.value(row,"Key"),value=table.value(row,"Value");
            if(!key||!value||value->size()>=1024)throw std::runtime_error("Invalid original description record");
            std::string text(*value);std::replace(text.begin(),text.end(),'|','\n');
            result.values_.insert_or_assign(std::string(*key),std::move(text));
        }return result;
    }
    [[nodiscard]] std::optional<std::string_view> lookup(std::string_view key)const{
        const auto found=values_.find(key);if(found==values_.end())return std::nullopt;return found->second;
    }
};
}
