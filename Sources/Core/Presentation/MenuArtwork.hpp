#pragma once
#include "Core/Presentation/DisplayLayout.hpp"
#include <string>
#include <string_view>
namespace hm2::menu {
// Presentation profile, chosen from the common surface configuration; never OS.
inline bool phone(const DisplayConfiguration& c){return c.enabled&&(c.device==DisplayDevice::phone||(c.device==DisplayDevice::automatic&&c.compact));}
inline std::string phoneAsset(std::string_view name){return "PhoneMenu/"+std::string(name);}
inline std::string background(const DisplayConfiguration& c,std::string_view hd){
    if(!phone(c))return std::string(hd);
    if(hd=="SelSlot_BG.png")return phoneAsset("SelSlot_BG.jpg");
    if(hd=="CmdCenterBG.png")return phoneAsset("CmdCenterBG.jpg");
    if(hd=="ShopBG.png")return phoneAsset("ShopBG.jpg");
    if(hd.size()==11&&hd.starts_with("Base00")&&hd.ends_with(".png"))return phoneAsset(std::string(hd.substr(0,7))+".jpg");
    return phoneAsset(hd);
}
inline bool retina(std::string_view name){
    return name=="PhoneMenu/ItemDesc.png"||name=="PhoneMenu/ShopCategory.png"||name=="PhoneMenu/SelSlot_BG.jpg"||name=="PhoneMenu/CmdCenterBG.jpg"||name=="PhoneMenu/ShopBG.jpg"||name=="PhoneMenu/MaintenanceBG.jpg"||name=="PhoneMenu/Main_Title.png"||name=="PhoneMenu/Main_Episode.png";
}
struct Title {float bigStart,bigEnd,bigStep,titleIn,episodeIn,prompt;Vec2 root,main,episode,tap,copyright,bgStart;};
inline Title title(const DisplayConfiguration& c){
    // Phone201 GuiTitle0x454f0/0x45e8c; HD GuiTitle existing restoration.
    if(phone(c))return {850,-800,4,7.2f,8.7f,11,{240,160},{0,-80},{104,-18},{-150,40},{0,0},{0,0}};
    return {1024,-1024,6,6.2f,7.7f,10,{384,512},{0,-380},{114,-310},{0,-230},{0,-500},{100,-10}};
}
}
