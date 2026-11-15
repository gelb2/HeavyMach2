// Offline harness adapter. Reuse the product's original readers/decoder.
#include "Assets/Image.hpp"
#include "Core/Data/DataTable.hpp"
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
int main(int argc,char** argv){
    if(argc!=2)return 64;
    std::vector<std::uint8_t> input{std::istreambuf_iterator<char>(std::cin),{}};
    if(input.empty()||input.size()>128*1024*1024)return 65;
    try{
        if(std::string(argv[1])=="image"){
            const auto image=hm2::decodeImage(input);
            for(const unsigned value:{image.width,image.height})for(unsigned i=0;i<4;++i)std::cout.put(char((value>>(i*8))&255));
            std::cout.write(reinterpret_cast<const char*>(image.rgba.data()),image.rgba.size());return 0;
        }
        if(std::string(argv[1])=="table"){
            const auto table=hm2::DataTable::parse(input);std::cout<<'[';bool first=true;
            for(std::size_t row=0;row<table.rowCount();++row){
                const auto resource=table.value(row,"ImageFile");if(!resource||resource->empty())continue;
                if(!first)std::cout<<',';first=false;
                std::cout<<"{\"type\":"<<std::quoted(std::string(table.value(row,"Type").value_or("")))<<",\"id\":"<<table.integer(row,"ID")<<",\"image\":"<<std::quoted(std::string(*resource))<<'}';
            }
            std::cout<<"]\n";return 0;
        }
    }catch(const std::exception&){std::cerr<<"Shared original decoder rejected input\n";return 65;}
    return 64;
}
