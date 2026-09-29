// Generated from SHA-256-pinned retail modules. Do not regenerate for unknown builds.
#pragma once
#include <cstdint>
namespace tf2hf {
struct ModuleSpec { const char* name; const char* sha256; uint32_t stamp, imageSize; };
struct SymbolSpec { const char* name; int module; uint32_t rva; const char* signature; };
inline constexpr ModuleSpec moduleSpecs[] = {
  {"engine.dll", "58eb1a1b44b30275bdd21368de264d856bc310d37d93a0f76f111f0026913487", 0x5A271230, 0x14DB3000},
  {"client.dll", "002b36487fec7c98882929fbccdb506d0146bbf3270cbb964a4d21c5edf7eebc", 0x5A271254, 0x3D4C000},
  {"materialsystem_dx11.dll", "1afffb8dfe2a97a00ca3c5a68ca5277249176c2fd51ee7c5ad70f6eb699a6050", 0x5A271217, 0x1E1B000},
};
inline constexpr SymbolSpec symbolSpecs[] = {
  {"CLMove", 0, 0x734C0, "48895c2418574881ece0000000440f296424700fb6fa440f"},
  {"GetClient", 0, 0x78200, "488d05b9e27200c3cccccccccccccccc4883ec2883f9ff75"},
  {"HostShouldRun", 0, 0x157B40, "4883ec28488b059d19001383785c007507b0014883c428c3"},
  {"IsPaused", 0, 0x8F520, "4883ec2880b960010000007534803d66940c1300742b488b"},
  {"GetFrameTime", 0, 0x8E400, "40534883ec20488bd9e8a215090084c07529388350010100"},
  {"SendMove", 0, 0x74F10, "48895c240848896c2410488974241848897c24204156b860"},
  {"CanPacket", 0, 0x20F620, "40534883ec20488b05eb04d913488bd983785c0075184881"},
  {"SendNetMsg", 0, 0x213270, "48896c241857415641574883ec20488bf94881c18c360000"},
  {"SendDatagram", 0, 0x212CD0, "48897424185741544155415641574883ec704532e44532ff"},
  {"SetChoked", 0, 0x213760, "ff4104ff4110c3cccccccccccccccccc889164010000c3cc"},
  {"SetClientTime", 0, 0x91AD0, "f30f1189b0000000c3cccccccccccccc48895108c3cccccc"},
  {"ExtraSample", 1, 0x2554C0, "48895c24084889742410574883ec40488bf983ceff0f2974"},
  {"GetButtonBits", 1, 0x251620, "40554155488bec4883ec38f605f6caf6000348895c245048"},
  {"ApplyMouse", 1, 0x2567B0, "48895c24084889742410574883ec40f20f1015d9197b00f3"},
  {"InputCreateMove", 1, 0x254D10, "48895c241855565741544155415641574881ec800000004c"},
  {"PlayerCreateMove", 1, 0x14A880, "488bc45553574156488da8e8feffff4881ecf80100004889"},
  {"ActivateMouse", 1, 0x256700, "40534883ec2080790900488bd90f858e0000008079080048"},
  {"DeactivateMouse", 1, 0x256940, "40574883ec2080790900488bf9746480790800745ec64109"},
  {"ClearStates", 1, 0x2568F0, "40534883ec2080790900488bd9743a83caffe859b4ffff83"},
  {"BeginFrame", 2, 0x5A200, "48895c24084889742410574883ec300f29742420488bf10f"},
};
}
