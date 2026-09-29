// Map game images without resolving imports or calling DllMain; never run game code.
#include "validation.h"
#include <filesystem>
#include <iostream>
#include <MinHook.h>
void neverCalledHook() {}
int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"Usage: ModuleValidationTests BIN_DIR\n";return 2;}
    HMODULE mapped[3]{};
    for(int i=0;i<3;++i) {
        auto file=std::filesystem::path(argv[1])/tf2hf::moduleSpecs[i].name;
        mapped[i]=LoadLibraryExW(file.wstring().c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
        if(!mapped[i]) {std::cerr<<"Image mapping failed: "<<GetLastError()<<'\n';return 1;}
    }
    tf2hf::ValidatedModules clean;
    std::string error;
    if(!tf2hf::validateModules(clean,error)) {std::cerr<<error<<'\n';return 1;}
    std::cout<<"PASS: exact loaded PE/hash/20 unique signatures; no game code executed\n";
    auto entry=static_cast<unsigned char*>(clean.symbol("CLMove"));
    DWORD protection=0;
    if(!VirtualProtect(entry,1,PAGE_EXECUTE_READWRITE,&protection)) return 1;
    const auto saved=*entry;
    *entry=0xE9; // only this test process's copy-on-write mapping, never the file or game
    tf2hf::ValidatedModules altered;
    const bool refused=!tf2hf::validateModules(altered,error);
    *entry=saved;
    DWORD ignored=0;VirtualProtect(entry,1,protection,&ignored);
    if(!refused || error.find("CLMove")==std::string::npos) {std::cerr<<"Modified prologue accepted\n";return 1;}
    std::cout<<"PASS: modified CL_Move prologue refused before any hooks: "<<error<<'\n';
    const char* hookNames[]={"CLMove","ExtraSample","GetButtonBits","ApplyMouse","InputCreateMove",
      "PlayerCreateMove","ActivateMouse","DeactivateMouse","ClearStates","SetClientTime","BeginFrame"};
    if(MH_Initialize()!=MH_OK) return 1;
    for(auto name:hookNames) {
        void* original=nullptr;
        auto status=MH_CreateHook(clean.symbol(name),reinterpret_cast<void*>(neverCalledHook),&original);
        if(status!=MH_OK || !original || MH_QueueEnableHook(clean.symbol(name))!=MH_OK) {
            std::cerr<<"Trampoline failure "<<name<<' '<<MH_StatusToString(status)<<'\n';return 1;
        }
    }
    if(MH_ApplyQueued()!=MH_OK) return 1;
    for(auto name:hookNames) if(MH_DisableHook(clean.symbol(name))!=MH_OK) return 1;
    for(auto name:hookNames) if(MH_RemoveHook(clean.symbol(name))!=MH_OK) return 1;
    MH_Uninitialize();
    tf2hf::ValidatedModules restored;
    if(!tf2hf::validateModules(restored,error)) {std::cerr<<"Restore failed: "<<error<<'\n';return 1;}
    std::cout<<"PASS: 11 real-prologue trampolines enabled/disabled/restored in isolated image mappings; no game functions called\n";
    for(auto image:mapped) FreeLibrary(image);
    return 0;
}
