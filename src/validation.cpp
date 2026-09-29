#include "validation.h"
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <vector>
#include <cstdio>

namespace tf2hf {
bool readable(const void* pointer,std::size_t bytes) {
    if(!pointer || !bytes) return false;
    auto cursor=reinterpret_cast<std::uintptr_t>(pointer);
    const auto end=cursor+bytes;
    if(end<cursor) return false;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION page{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&page,sizeof(page)) || page.State!=MEM_COMMIT ||
            (page.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || !page.Protect) return false;
        const auto next=reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize;
        if(next<=cursor) return false;
        cursor=next;
    }
    return true;
}
bool executableInModule(const void* address,HMODULE module) {
    MEMORY_BASIC_INFORMATION page{};
    if(!address || !VirtualQuery(address,&page,sizeof(page)) || page.AllocationBase!=module || page.State!=MEM_COMMIT) return false;
    return !(page.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
      (page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
void* ValidatedModules::address(int index,std::size_t rva) const {
    if(index<0 || index>=3 || !modules[index] || rva>=moduleSpecs[index].imageSize) return nullptr;
    return reinterpret_cast<unsigned char*>(modules[index])+rva;
}
static bool fileHash(HMODULE module,std::string& hash) {
    wchar_t path[32768]{};
    if(!GetModuleFileNameW(module,path,32768)) return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE state=nullptr;
    bool good=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    DWORD length=0,used=0;
    if(good) good=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&used,0)>=0;
    std::vector<unsigned char> object(length);
    if(good) good=BCryptCreateHash(algorithm,&state,object.data(),length,nullptr,0,0)>=0;
    std::array<unsigned char,65536> buffer{};
    DWORD bytes=0;
    while(good) {
        if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&bytes,nullptr)) { good=false; break; }
        if(!bytes) break;
        good=BCryptHashData(state,buffer.data(),bytes,0)>=0;
    }
    unsigned char digest[32]{};
    if(good) good=BCryptFinishHash(state,digest,sizeof(digest),0)>=0;
    if(state) BCryptDestroyHash(state);
    if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);
    if(!good) return false;
    char text[65]{};
    for(unsigned i=0;i<32;++i) std::snprintf(text+2*i,3,"%02x",digest[i]);
    hash=text;
    return true;
}
bool validateModules(ValidatedModules& result,std::string& error) {
    struct Code { unsigned char* pointer; std::size_t length; } text[3]{};
    for(int i=0;i<3;++i) {
        result.modules[i]=GetModuleHandleA(moduleSpecs[i].name);
        if(!result.modules[i]) {error=std::string("Module missing: ")+moduleSpecs[i].name;return false;}
        std::string hash;
        if(!fileHash(result.modules[i],hash) || hash!=moduleSpecs[i].sha256) {
            error=std::string("Unsupported SHA-256: ")+moduleSpecs[i].name;return false;
        }
        auto base=reinterpret_cast<unsigned char*>(result.modules[i]);
        if(!readable(base,sizeof(IMAGE_DOS_HEADER))) {error="Unreadable PE header";return false;}
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000) {error="Invalid PE header";return false;}
        auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if(!readable(nt,sizeof(*nt)) || nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp!=moduleSpecs[i].stamp || nt->OptionalHeader.SizeOfImage!=moduleSpecs[i].imageSize) {
            error="Loaded PE identity mismatch";return false;
        }
        auto section=IMAGE_FIRST_SECTION(nt);
        if(!readable(section,nt->FileHeader.NumberOfSections*sizeof(*section))) {error="Invalid section table";return false;}
        for(unsigned j=0;j<nt->FileHeader.NumberOfSections;++j) {
            if(std::memcmp(section[j].Name,".text\0",6)==0) {
                if(std::size_t(section[j].VirtualAddress)+section[j].Misc.VirtualSize>nt->OptionalHeader.SizeOfImage) {error="Invalid text section";return false;}
                text[i]={base+section[j].VirtualAddress,section[j].Misc.VirtualSize};
            }
        }
        if(!text[i].pointer || !readable(text[i].pointer,text[i].length)) {error="Unreadable text section";return false;}
    }
    for(const auto& symbol:symbolSpecs) {
        if(symbol.module<0 || symbol.module>=3) {error="Invalid signature module index";return false;}
        std::vector<unsigned char> pattern;
        const std::string encoded=symbol.signature;
        for(std::size_t i=0;i<encoded.size();i+=2) pattern.push_back(static_cast<unsigned char>(std::stoul(encoded.substr(i,2),nullptr,16)));
        if(pattern.empty() || pattern.size()>text[symbol.module].length) {error="Invalid signature";return false;}
        std::size_t matches=0;
        unsigned char* found=nullptr;
        const auto code=text[symbol.module];
        for(std::size_t i=0;i+pattern.size()<=code.length;++i) {
            if(code.pointer[i]==pattern[0] && std::memcmp(code.pointer+i,pattern.data(),pattern.size())==0) {
                ++matches;found=code.pointer+i;
                if(matches>1) break;
            }
        }
        if(matches!=1 || found!=result.address(symbol.module,symbol.rva) ||
            !executableInModule(found,result.modules.at(static_cast<std::size_t>(symbol.module)))) {
            error=std::string("Unique signature / prologue validation failed: ")+symbol.name;return false;
        }
        result.symbols.emplace(symbol.name,found);
    }
    return true;
}
}
