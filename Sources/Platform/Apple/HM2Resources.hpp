#pragma once
#include "Core/Presentation/AssemblyPreview.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace hm2::apple {
class Files final:public ResourceAccess {
    std::filesystem::path root_;
public:
    std::map<std::string,unsigned> reads;
    explicit Files(std::filesystem::path root):root_(std::move(root)){}
    bool exists(std::string_view name)const override {
        if(std::filesystem::path(name).filename()!=name)throw std::runtime_error("Invalid resource path");
        return std::filesystem::is_regular_file(root_/name);
    }
    Bytes read(std::string_view name)override {
        if(std::filesystem::path(name).filename()!=name)throw std::runtime_error("Invalid resource path");
        std::ifstream f(root_/name,std::ios::binary);
        if(!f)throw std::runtime_error("Missing prepared HD resource: "+std::string(name));
        ++reads[std::string(name)];return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
    }
};
}
