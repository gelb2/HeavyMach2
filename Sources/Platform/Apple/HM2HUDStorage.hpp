#pragma once
#import <Foundation/Foundation.h>
#include "Core/Types.hpp"
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

namespace hm2::apple {
// Display preference storage, independent of the original progress-save format.
// Screens calls write only after an explicit Apply; drawing/dragging never saves.
inline NSURL* hudLayoutURL(NSURL* directory,std::string_view profile){
    if(profile!="compact"&&profile!="regular")throw std::invalid_argument("Unknown HUD display profile");
    NSString* name=profile=="compact"?@"HUDLayoutV1-compact.json":@"HUDLayoutV1-regular.json";
    return [directory URLByAppendingPathComponent:name];
}
inline std::optional<Bytes> readHUDLayout(NSURL* directory,std::string_view profile){
    NSURL* file=hudLayoutURL(directory,profile);
    if(![NSFileManager.defaultManager fileExistsAtPath:file.path])return {};
    NSError* error=nil;NSData* contents=[NSData dataWithContentsOfURL:file options:0 error:&error];
    if(!contents)throw std::runtime_error(error.localizedDescription.UTF8String?:"Cannot read HUD layout");
    if(contents.length>65536)throw std::runtime_error("HUD layout exceeds its size limit");
    if(!contents.length)return Bytes{};
    const auto* begin=static_cast<const std::uint8_t*>(contents.bytes);
    return Bytes(begin,begin+contents.length);
}
inline bool writeHUDLayout(NSURL* directory,std::string_view profile,std::span<const std::uint8_t> bytes){
    NSURL* file=hudLayoutURL(directory,profile);
    if(bytes.empty()||bytes.size()>65536)return false;
    NSError* error=nil;
    if(![NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error])return false;
    NSData* contents=[NSData dataWithBytes:bytes.data() length:bytes.size()];
    return [contents writeToURL:file options:NSDataWritingAtomic error:&error];
}
}
