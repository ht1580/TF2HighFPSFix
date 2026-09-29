// Minimal ABI-bound transplant of r2ion/IonLauncher cdfccc7. See upstream/LICENSE.
#include <windows.h>
#include <shellapi.h>
#include <MinHook.h>
#include "validation.h"
#include "timing_core.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace {
using namespace tf2hf;
HMODULE pluginModule{};
ValidatedModules modules;
std::wstring directory,iniPath;
std::atomic<bool> initialized{false},running{false};
bool debug=true,trace=true;
using CLMoveFn=void(*)(float,bool);
using ExtraFn=void(*)(void*,float);
using ButtonsFn=std::uint32_t(*)(void*,bool);
using MouseFn=void(*)(void*,int,void*,void*,float,float);
using InputMoveFn=void(*)(void*,int,float,bool);
using PlayerMoveFn=bool(*)(void*,float,void*,bool);
using InputStateFn=void(*)(void*);
using SetTimeFn=void(*)(void*,float);
using BeginFrameFn=void(*)(void*,float);
CLMoveFn originalMove{};
ExtraFn originalExtra{};
ButtonsFn originalButtons{};
MouseFn originalMouse{};
InputMoveFn originalInputMove{};
PlayerMoveFn originalPlayerMove{};
InputStateFn originalActivate{},originalDeactivate{},originalClear{};
SetTimeFn originalSetTime{};
BeginFrameFn originalBeginFrame{};
void *engineClient{},*prediction{},*clientDLL{},*input{};
void *hostScale{},*cmdRate{},*useDt{};
struct Key {int down[2];int state;};
static_assert(sizeof(Key)==12);
Key* keys[8]{};
Key* strafe{};
thread_local InputAccumulator accumulator;
thread_local Scheduler scheduler;
thread_local TimeRemainder precision;
thread_local bool wasActive=false;
thread_local void* previousClient=nullptr;
thread_local double lastMovement=0;
thread_local bool timingEligible=false;
thread_local bool insideMove=false;
std::atomic<DWORD> eventThread{0};
LARGE_INTEGER frequency{};
double epoch=0;
double now() {LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);return double(counter.QuadPart)/double(frequency.QuadPart);}

enum Kind:unsigned { Render, Move, Extra, Command, Packet, Reset, Error };
struct Event {double timestamp;Kind kind;int sequence;float time,aux;std::uint32_t buttons;};
constexpr std::size_t capacity=65536;
std::array<Event,capacity> events{};
std::atomic<std::uint64_t> writeIndex{0},readIndex{0},lostEvents{0};
void record(Kind kind,float time=0,int sequence=0,float aux=0,std::uint32_t buttons=0) {
    if(!debug) return;
    const auto thread=GetCurrentThreadId();
    DWORD expected=0;
    eventThread.compare_exchange_strong(expected,thread,std::memory_order_relaxed);
    if(eventThread.load(std::memory_order_relaxed)!=thread) {++lostEvents;return;}
    const auto write=writeIndex.load(std::memory_order_relaxed);
    if(write-readIndex.load(std::memory_order_acquire)>=capacity) {++lostEvents;return;}
    events[write%capacity]={now()-epoch,kind,sequence,time,aux,buttons};
    writeIndex.store(write+1,std::memory_order_release);
}
void log(const char* format,...) {
    auto path=directory+L"TF2HighFPSFix.log";
    FILE* file=_wfopen(path.c_str(),L"ab");
    if(!file) return;
    SYSTEMTIME time{};GetLocalTime(&time);
    std::fprintf(file,"[%02u:%02u:%02u] ",time.wHour,time.wMinute,time.wSecond);
    va_list args;va_start(args,format);std::vfprintf(file,format,args);va_end(args);
    std::fputc('\n',file);std::fclose(file);
}
template<class T> T& field(void* object,std::size_t offset) {return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(object)+offset);}
template<class T> T at(int module,std::size_t rva) {return reinterpret_cast<T>(modules.address(module,rva));}
template<class Fn> Fn fn(const char* name) {return reinterpret_cast<Fn>(modules.symbol(name));}
template<class Fn> Fn method(void* object,unsigned slot) {return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot]);}
bool virtualValid(void* object,unsigned slot,int module) {
    return readable(object,sizeof(void*)) && readable(*reinterpret_cast<void***>(object),(slot+1)*sizeof(void*)) &&
      executableInModule((*reinterpret_cast<void***>(object))[slot],modules.modules[module]);
}
void* cvParent(void* cv) {return readable(cv,0x60) ? field<void*>(cv,0x38):nullptr;}
float cvFloat(void* cv) {auto p=cvParent(cv);return readable(p,0x60)?field<float>(p,0x58):NAN;}
bool cvBool(void* cv) {auto p=cvParent(cv);return readable(p,0x60) && field<int>(p,0x5C)!=0;}
bool cvName(void* cv,const char* name) {
    auto p=cvParent(cv);
    if(!readable(p,0x60)) return false;
    const auto text=field<const char*>(p,0x18);
    return readable(text,std::strlen(name)+1) && std::strcmp(text,name)==0;
}
bool demo() {
    auto object=*at<void**>(0,0xFD15608);
    return !virtualValid(object,6,0) || method<bool(*)(void*)>(object,6)(object);
}
void reset(const char* reason) {
    scheduler.reset();accumulator.reset();lastMovement=0;timingEligible=false;
    record(Reset);(void)reason;
}
void syncView() {
    float angles[3]{};
    method<void(*)(void*,void*)>(engineClient,63)(engineClient,angles);
    method<void(*)(void*,void*)>(prediction,12)(prediction,angles);
}
void sampleTime(void* self,float time) {
    method<void(*)(void*,float)>(self,20)(self,time);
    method<void(*)(void*,float)>(self,21)(self,time);
}

void extraHook(void* self,float time) {
    if(!timingEligible || demo()) {originalExtra(self,time);return;}
    if(!(time>0) || !std::isfinite(time)) return;
    if(accumulator.extraTime>0) syncView();
    const int count=(strafe->state&1)?8:6;
    int* states[8]{};
    for(int i=0;i<count;++i) states[i]=&keys[i]->state;
    PreservedKeyStates preserve(states,count);
    const bool previous=accumulator.extra;
    accumulator.extra=true;
    sampleTime(self,time);
    originalExtra(self,time);
    accumulator.extra=previous;
    accumulator.extraTime+=time;
    record(Extra,time);
}
std::uint32_t buttonsHook(void* self,bool consume) {return originalButtons(self,consumeButtons(consume,accumulator.extra));}
void mouseHook(void* self,int slot,void* angles,void* command,float x,float y) {
    if(!accumulator.extra || !command) {originalMouse(self,slot,angles,command,x,y);return;}
    const float forward=field<float>(command,0x34),side=field<float>(command,0x38);
    originalMouse(self,slot,angles,command,x,y);
    accumulator.addMouseMovement(forward,side,field<float>(command,0x34),field<float>(command,0x38));
}
void inputMoveHook(void* self,int sequence,float time,bool active) {
    if(!timingEligible || demo()) {accumulator.reset();originalInputMove(self,sequence,time,active);return;}
    if(accumulator.extraTime>0) syncView();
    const float viewTime=std::max(time-accumulator.extraTime,0.f);
    accumulator.extraTime=0;
    sampleTime(self,viewTime);
    if(!active) accumulator.resetMovement();
    const float oldTime=accumulator.fullCommandTime;
    const bool oldCreating=accumulator.creating,oldRestore=accumulator.restoreTime;
    accumulator.fullCommandTime=time;accumulator.creating=true;accumulator.restoreTime=viewTime!=time;
    originalInputMove(self,sequence,viewTime,active);
    accumulator.restoreTime=oldRestore;accumulator.creating=oldCreating;accumulator.fullCommandTime=oldTime;
    accumulator.resetMovement();
}
bool playerMoveHook(void* self,float time,void* command,bool extra) {
    if(accumulator.creating&&!extra&&command) accumulator.merge(field<float>(command,0x34),field<float>(command,0x38));
    const bool result=originalPlayerMove(self,time,command,extra);
    if(accumulator.restoreTime&&!extra&&command&&cvBool(useDt)) {
        field<float>(command,0xAC)=accumulator.fullCommandTime;
        field<float>(self,0x29AC)=accumulator.fullCommandTime;
    }
    if(accumulator.creating&&!extra&&command)
        record(Command,field<float>(command,0xAC),field<int>(command,0),time,field<std::uint32_t>(command,0x40));
    return result;
}
void activateHook(void* self) {
    const bool active=field<bool>(self,9);originalActivate(self);
    if(!active&&field<bool>(self,9)) accumulator.resetMovement();
}
void deactivateHook(void* self) {originalDeactivate(self);accumulator.resetMovement();}
void clearHook(void* self) {originalClear(self);accumulator.resetMovement();}

void timeHook(void* self,float time) {
#ifdef _MSC_VER
    void* caller=_ReturnAddress();
#else
    void* caller=__builtin_return_address(0);
#endif
    const bool accumulation=caller==modules.address(0,0x15952B);
    time=precision.update(self,field<float>(self,0xB0),field<float>(self,0x10160),field<float>(self,0x10184),time,accumulation);
    originalSetTime(self,time);
}
void beginFrameHook(void* self,float time) {record(Render,time);originalBeginFrame(self,time);}

struct TickMessage {
    void* vtable;
    std::int32_t group;
    bool reliable;
    unsigned char pad[3];
    void* channel;
    void* handler;
    std::int32_t deltaTick,stringTick;
    float frameTime,stddev;
    unsigned char serverCPU;
    unsigned char endPad[7];
};
static_assert(sizeof(TickMessage)==0x38);
static_assert(offsetof(TickMessage,frameTime)==0x28);
static_assert(offsetof(TickMessage,serverCPU)==0x30);
void sendTick(void* client,void* channel) {
    TickMessage message{};
    // Use the game's CLC_ClientTick vtable and serialization, not a GCC C++ vtable.
    message.vtable=modules.address(0,0x5D8C88);
    message.deltaTick=field<int>(client,0x14C);message.stringTick=field<int>(client,0x150);
    message.frameTime=*at<float*>(0,0x13158BA4);message.stddev=*at<float*>(0,0x13158BAC);
    message.serverCPU=static_cast<unsigned char>(*at<float*>(0,0x130024C0)*100.f);
    fn<bool(*)(void*,void*,bool,bool)>("SendNetMsg")(channel,&message,false,false);
}
void moveHook(float originalArgument,bool finalTick) {
    auto client=fn<void*(*)()>("GetClient")();
    if(!readable(client,0x10190)) {originalMove(originalArgument,finalTick);return;}
    const bool active=field<int>(client,0x98)==8;
    if(client!=previousClient || active!=wasActive) {
        reset("connection transition");previousClient=client;wasActive=active;lastMovement=active?now():0;
    }
    if(field<int>(client,0x98)<2 || !fn<bool(*)()>("HostShouldRun")() || demo()) {
        reset("inactive/demo");originalMove(originalArgument,finalTick);return;
    }
    // Re-resolve through the stock engine's registered-ConVar references; no CVar writes.
    hostScale=*at<void**>(0,0x1315A2A8);cmdRate=*at<void**>(0,0xFDA5AC8);useDt=*at<void**>(0,0xFDA5738);
    const float scale=cvFloat(hostScale),rate=cvFloat(cmdRate);
    auto channel=field<void*>(client,0x60);
    const bool paused=fn<bool(*)(void*)>("IsPaused")(client);
    if(!std::isfinite(scale) || scale<=0 || !std::isfinite(rate) || rate<=0 ||
       !cvBool(useDt) || !readable(channel,sizeof(void*)) || (active&&paused) || insideMove) {
        reset("guard/fallback");record(Error,0,1);originalMove(originalArgument,finalTick);return;
    }
    const float frame=fn<float(*)(void*)>("GetFrameTime")(client);
    if(!std::isfinite(frame) || frame<0) {reset("invalid frame");originalMove(originalArgument,finalTick);return;}
    timingEligible=active;
    record(Move,frame,0,scheduler.pending);
    const auto snapshot=field<void*>(client,0x10140);
    const int commandTick=readable(snapshot,0x40)?field<int>(snapshot,0x3C):-1;
    const int pending=field<int>(client,0x10164)-commandTick+1;
    const float netTime=float(*at<double*>(0,0x13FA2DE0));
    bool send=true;
    if(field<double>(client,0xA0)<=netTime && (finalTick || pending>=15)) send=fn<bool(*)(void*)>("CanPacket")(channel);
    else if(pending<15 || scale==1.f) send=false;
    if(active) {
        const double movementTime=now();
        auto split=*at<void**>(0,0x7A6490);
        if(!virtualValid(split,13,0)) {reset("invalid split interface");originalMove(originalArgument,finalTick);return;}
        if(!method<bool(*)(void*,int)>(split,13)(split,0)) {
            const auto decision=scheduler.advance(frame,scale,false,float(movementTime-lastMovement));
            if(decision.extra) {
                if(decision.sampleTime>0) method<void(*)(void*,float)>(clientDLL,27)(clientDLL,decision.sampleTime);
                return;
            }
            const int sequence=field<int>(client,0x10164)+1;
            insideMove=true;
            method<void(*)(void*,int,float,bool)>(clientDLL,25)(clientDLL,sequence,decision.commandTime,true);
            insideMove=false;
            field<int>(client,0x10164)=sequence;
            auto recorder=*at<void**>(0,0xFD14FB8);
            if(virtualValid(recorder,10,0)&&method<bool(*)(void*)>(recorder,5)(recorder))
                method<void(*)(void*,int)>(recorder,10)(recorder,sequence);
        }
        if(send) fn<void(*)()>("SendMove")();else fn<void(*)(void*)>("SetChoked")(channel);
        lastMovement=movementTime;
    }
    if(send) {
        if(active) sendTick(client,channel);
        const int datagram=fn<int(*)(void*,void*)>("SendDatagram")(channel,nullptr);
        record(Packet,0,datagram);
        const float interval=1.f/rate;
        const float maxAdjustment=std::max(*at<float*>(0,0x7CB418),interval);
        const float delta=netTime-float(field<double>(client,0xA0));
        field<double>(client,0xA0)=double(interval+netTime-std::clamp(delta,0.f,maxAdjustment));
    }
}

void* factory(HMODULE module,const char* version) {
    auto create=reinterpret_cast<void*(*)(const char*,int*)>(GetProcAddress(module,"CreateInterface"));
    return create?create(version,nullptr):nullptr;
}
bool bindings() {
    engineClient=factory(modules.modules[0],"VEngineClient013");
    prediction=factory(modules.modules[1],"VClientPrediction001");
    clientDLL=factory(modules.modules[1],"VClient018");input=modules.address(1,0xB1D380);
    if(!virtualValid(engineClient,63,0)||!virtualValid(prediction,12,1)||!virtualValid(clientDLL,27,1)||
       !virtualValid(clientDLL,25,1)||!virtualValid(input,20,1)||!virtualValid(input,21,1)) return false;
    if((*reinterpret_cast<void***>(input))[4]!=modules.symbol("InputCreateMove") ||
       (*reinterpret_cast<void***>(input))[5]!=modules.symbol("ExtraSample") ||
       (*reinterpret_cast<void***>(input))[3]!=modules.symbol("GetButtonBits")) return false;
    const std::size_t addresses[]={0x11BE160,0x11BE170,0x11BE140,0x11BE150,0x11C14B8,0x11C14C8,0x11C1468,0x11C1478};
    for(unsigned i=0;i<8;++i) {keys[i]=at<Key*>(1,addresses[i]);if(!readable(keys[i],sizeof(Key))) return false;}
    strafe=at<Key*>(1,0x11BE110);
    if(!readable(strafe,sizeof(Key))) return false;
    hostScale=*at<void**>(0,0x1315A2A8);cmdRate=*at<void**>(0,0xFDA5AC8);useDt=*at<void**>(0,0xFDA5738);
    return cvName(hostScale,"host_timescale")&&cvName(cmdRate,"cl_cmdrate")&&cvName(useDt,"cl_move_use_dt");
}
struct Hook {const char* name;void* replacement;void** original;};
bool install() {
    const Hook hooks[]={
      {"CLMove",reinterpret_cast<void*>(moveHook),reinterpret_cast<void**>(&originalMove)},
      {"ExtraSample",reinterpret_cast<void*>(extraHook),reinterpret_cast<void**>(&originalExtra)},
      {"GetButtonBits",reinterpret_cast<void*>(buttonsHook),reinterpret_cast<void**>(&originalButtons)},
      {"ApplyMouse",reinterpret_cast<void*>(mouseHook),reinterpret_cast<void**>(&originalMouse)},
      {"InputCreateMove",reinterpret_cast<void*>(inputMoveHook),reinterpret_cast<void**>(&originalInputMove)},
      {"PlayerCreateMove",reinterpret_cast<void*>(playerMoveHook),reinterpret_cast<void**>(&originalPlayerMove)},
      {"ActivateMouse",reinterpret_cast<void*>(activateHook),reinterpret_cast<void**>(&originalActivate)},
      {"DeactivateMouse",reinterpret_cast<void*>(deactivateHook),reinterpret_cast<void**>(&originalDeactivate)},
      {"ClearStates",reinterpret_cast<void*>(clearHook),reinterpret_cast<void**>(&originalClear)},
      {"SetClientTime",reinterpret_cast<void*>(timeHook),reinterpret_cast<void**>(&originalSetTime)},
      {"BeginFrame",reinterpret_cast<void*>(beginFrameHook),reinterpret_cast<void**>(&originalBeginFrame)}};
    if(MH_Initialize()!=MH_OK) {log("DISABLED: private MinHook initialization failed");return false;}
    std::vector<void*> created;
    for(const auto& hook:hooks) {
        auto target=modules.symbol(hook.name);
        auto status=MH_CreateHook(target,hook.replacement,hook.original);
        if(status!=MH_OK) {
            log("DISABLED: CreateHook %s: %s",hook.name,MH_StatusToString(status));
            for(auto address:created) MH_RemoveHook(address);
            MH_Uninitialize();return false;
        }
        created.push_back(target);
        if(MH_QueueEnableHook(target)!=MH_OK) {
            for(auto address:created) MH_RemoveHook(address);
            MH_Uninitialize();log("DISABLED: cannot queue hook %s",hook.name);return false;
        }
    }
    const auto result=MH_ApplyQueued();
    if(result!=MH_OK) {
        // MinHook's apply operation can fail after a partial application: roll back our own targets.
        for(auto address:created) MH_DisableHook(address);
        for(auto address:created) MH_RemoveHook(address);
        MH_Uninitialize();log("DISABLED: hook transaction failed: %s",MH_StatusToString(result));return false;
    }
    log("ACTIVE: 11 hooks installed; Vanilla floor=5ms; original net serialization/datagrams; no CFG or FPS writes.");
    return true;
}
bool vanillaProfile() {
    int count=0;auto arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!arguments) return false;
    bool found=false;
    for(int i=1;i<count;++i) if(_wcsicmp(arguments[i],L"-profile=R2Vanilla")==0) found=true;
    LocalFree(arguments);return found;
}
void logger() {
    FILE* csv=trace?_wfopen((directory+L"TF2HighFPSFix.events.csv").c_str(),L"wb"):nullptr;
    if(csv) std::fprintf(csv,"qpc_seconds,event,sequence,frame_time,aux_time,buttons\n");
    const char* names[]={"render_begin","cl_move","extra_sample","usercmd","datagram","reset","fallback"};
    std::uint64_t counts[7]{};
    double totalCommand=0,maxCommand=0,start=0,lastReport=0;
    float latestRenderTime=0;
    for(;;) {
        auto read=readIndex.load(std::memory_order_relaxed);
        const auto write=writeIndex.load(std::memory_order_acquire);
        while(read<write) {
            const auto event=events[read%capacity];++read;
            ++counts[event.kind];
            if(event.kind==Command) {totalCommand+=event.time;maxCommand=std::max(maxCommand,double(event.time));}
            if(event.kind==Render) latestRenderTime=event.time;
            if(csv) std::fprintf(csv,"%.9f,%s,%d,%.9g,%.9g,%u\n",event.timestamp,names[event.kind],event.sequence,event.time,event.aux,event.buttons);
            if(!start) start=event.timestamp;
            lastReport=event.timestamp;
        }
        readIndex.store(read,std::memory_order_release);
        if(lastReport-start>=2.0) {
            const double elapsed=lastReport-start;
            log("render_submit=%.2f/s last_render_dt=%.3fms cl_move=%.2f/s cmd=%.2f/s extra=%.2f/s cmd_avg=%.3fms cmd_max=%.3fms datagrams=%.2f/s fallback=%llu trace_lost=%llu",
              counts[Render]/elapsed,latestRenderTime*1000,counts[Move]/elapsed,counts[Command]/elapsed,counts[Extra]/elapsed,
              counts[Command]?totalCommand/counts[Command]*1000:0,maxCommand*1000,counts[Packet]/elapsed,
              static_cast<unsigned long long>(counts[Error]),static_cast<unsigned long long>(lostEvents.load()));
            std::fill(std::begin(counts),std::end(counts),0);totalCommand=maxCommand=0;start=lastReport;
            if(csv) std::fflush(csv);
        }
        Sleep(100);
    }
}
DWORD WINAPI worker(void*) {
    try {
        if(!vanillaProfile()) {log("DISABLED: this build requires explicit -profile=R2Vanilla.");return 0;}
        if(!GetPrivateProfileIntW(L"TF2HighFPSFix",L"Enabled",1,iniPath.c_str())) {log("DISABLED by INI.");return 0;}
        debug=GetPrivateProfileIntW(L"TF2HighFPSFix",L"Debug",1,iniPath.c_str())!=0;
        trace=debug&&GetPrivateProfileIntW(L"TF2HighFPSFix",L"Trace",1,iniPath.c_str())!=0;
        for(unsigned attempt=0;attempt<1200;++attempt) {
            if(GetModuleHandleA("engine.dll")&&GetModuleHandleA("client.dll")&&GetModuleHandleA("materialsystem_dx11.dll")) break;
            Sleep(100);
        }
        std::string error;
        if(!validateModules(modules,error)) {log("DISABLED: %s. No hooks installed.",error.c_str());return 0;}
        log("SHA-256 / loaded PE identity / 20 unique function signatures validated.");
        // Validate the special SetClientTime call site and stock message vtable references.
        const unsigned char call[]={0xE8,0xA5,0x85,0xF3,0xFF};
        if(std::memcmp(modules.address(0,0x159526),call,sizeof(call))!=0 ||
           !readable(modules.address(0,0x5D8C88),8*sizeof(void*))) {log("DISABLED: time call-site or tick vtable mismatch.");return 0;}
        bool ready=false;
        for(unsigned attempt=0;attempt<1200&&!ready;++attempt) {ready=bindings();if(!ready) Sleep(100);}
        if(!ready) {log("DISABLED: interfaces/ConVars did not become ready. No hooks installed.");return 0;}
        QueryPerformanceFrequency(&frequency);epoch=now();
        if(!install()) return 0;
        running.store(true,std::memory_order_release);
        log("cl_move_use_dt=%d; zero uses stock fallback. Render rate is BeginFrame submissions, not display scanout.",cvBool(useDt)?1:0);
        if(debug) logger();
    } catch(const std::exception& e) {
        if(running.load(std::memory_order_acquire))
            log("DIAGNOSTIC ERROR: %s; hooks remain active until process exit.",e.what());
        else log("DISABLED: initialization error: %s",e.what());
    }
    return 0;
}
}
extern "C" __declspec(dllexport) void initializePlugin(void*(*)(int)) {
    if(initialized.exchange(true)) return;
    wchar_t path[32768]{};GetModuleFileNameW(pluginModule,path,32768);
    directory=path;const auto slash=directory.find_last_of(L"\\/");
    if(slash==std::wstring::npos) return;
    // Process-lifetime hooks: a normal module pin prevents accidental hot-unload.
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCWSTR>(initializePlugin),&pinned)) return;
    directory.resize(slash+1);iniPath=directory+L"TF2HighFPSFix.ini";
    log("Plugin initialized; validating a build-specific Ion timing transplant.");
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
    if(thread) CloseHandle(thread);else log("DISABLED: cannot start initializer.");
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {pluginModule=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
