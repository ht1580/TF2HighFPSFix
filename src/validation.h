#pragma once
#include <windows.h>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <array>
#include "build_profile.h"
namespace tf2hf {
struct ValidatedModules {
    std::array<HMODULE,3> modules{};
    std::unordered_map<std::string,void*> symbols;
    void* symbol(const char* name) const { return symbols.at(name); }
    void* address(int module,std::size_t rva) const;
};
bool readable(const void* address,std::size_t bytes);
bool executableInModule(const void* address,HMODULE module);
bool validateModules(ValidatedModules& result,std::string& error);
}
