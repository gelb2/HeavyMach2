#pragma once
#include <string_view>
namespace hm2 {
// Original basenames plus one explicit shared menu-art namespace. No arbitrary
// nested path, absolute path, traversal, backslash or embedded NUL is accepted.
inline bool validResourceName(std::string_view name){
    if(name.starts_with("PhoneMenu/"))name.remove_prefix(10);
    return !name.empty()&&name!="."&&name!=".."&&name.find_first_of("/\\") == name.npos&&name.find('\0')==name.npos;
}
}
