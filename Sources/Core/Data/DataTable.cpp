#include "Core/Data/DataTable.hpp"
#include <cstdlib>
#include <cerrno>
#include <limits>
#include "Core/Data/DarwinRandom.hpp"
#include <algorithm>
#include <stdexcept>

namespace hm2 {
namespace {
// QTable::getDataFrom 0x3e778: TAB, LF, 0 and 1 terminate tokens;
// only trailing ASCII spaces are trimmed, not leading spaces.
struct Token {std::string text;std::size_t next;};
Token token(std::string_view line,std::size_t pos) {
    std::size_t end=pos;
    while(end<line.size()&&line[end]!='\t'&&line[end]!='\n'&&line[end]!=0&&line[end]!=1)++end;
    auto text=line.substr(pos,end-pos);
    while(!text.empty()&&text.back()==' ')text.remove_suffix(1);
    return {std::string(text),end<line.size()&&line[end]=='\t'?end+1:end};
}
}
DataTable DataTable::parse(std::span<const std::uint8_t> encoded) {
    const std::string_view marker="EncTbl";
    std::string text;
    if(encoded.size()>=marker.size()&&std::equal(marker.begin(),marker.end(),encoded.begin())) {
        DarwinRandom random(0xb27f7);text.reserve(encoded.size()-marker.size());
        for(auto b:encoded.subspan(marker.size()))text.push_back(char(b^(random.next()%227)));
    }else if(!encoded.empty())text.assign(reinterpret_cast<const char*>(encoded.data()),encoded.size());
    auto result=parseText(text);result.encoded_.assign(encoded.begin(),encoded.end());return result;
}
DataTable DataTable::parseText(std::string_view text) {
    DataTable result;result.decoded_=text;
    result.encoded_.assign(text.begin(),text.end());
    // Original strtok stops at the first NUL in the complete decoded buffer.
    text=text.substr(0,text.find('\0'));
    while(!text.empty()) {
        const auto end=text.find_first_of("\r\n");const auto line=text.substr(0,end);
        if(end==std::string_view::npos)text={};else text.remove_prefix(end+1);
        if(line.empty())continue;
        std::size_t pos=0;
        if(result.columns_.empty()) {
            for(;;){auto t=token(line,pos);if(t.next==pos)break;result.columns_.push_back(std::move(t.text));pos=t.next;}
        }else {
            if(line.starts_with("//"))continue;
            std::vector<std::string> values;
            do{auto t=token(line,pos);values.push_back(std::move(t.text));if(t.next==pos)break;pos=t.next;}while(values.size()<result.columns_.size());
            result.rows_.push_back(std::move(values));
        }
    }
    if(result.columns_.empty())throw std::runtime_error("QTable header is missing");
    return result;
}
std::optional<std::string_view> DataTable::value(std::size_t row,std::string_view column)const {
    if(row>=rows_.size())return std::nullopt;
    std::optional<std::string_view> result;
    // Original dictionary insertion: the last visited duplicate header wins;
    // an absent later field never overwrites an earlier value.
    for(std::size_t i=0;i<columns_.size()&&i<rows_[row].size();++i)
        if(columns_[i]==column)result=rows_[row][i];
    return result;
}
std::optional<std::size_t> DataTable::findRow(std::string_view key)const {
    for(std::size_t i=0;i<rows_.size();++i)if(!rows_[i].empty()&&rows_[i][0]==key)return i;
    return std::nullopt;
}
namespace {
std::string numeric(std::optional<std::string_view> value){
    if(!value)return {};
    if(value->size()>=24)throw std::runtime_error("Original numeric buffer would be unterminated");
    std::string text(*value);text.erase(std::remove(text.begin(),text.end(),','),text.end());return text;
}
}
std::int32_t DataTable::integer(std::size_t row,std::string_view key)const{
    const auto text=numeric(value(row,key));errno=0;const long result=std::strtol(text.c_str(),nullptr,10);
    if(errno==ERANGE||result<std::numeric_limits<std::int32_t>::min()||result>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("Original integer outside defined32-bit input range");
    return std::int32_t(result);
}
float DataTable::real(std::size_t row,std::string_view key)const{
    const auto text=numeric(value(row,key));return float(std::strtod(text.c_str(),nullptr));
}
}
