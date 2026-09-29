"""Generate signatures for the explicitly supported retail build only."""
import argparse, hashlib, json
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

EXPECTED = {
    'engine.dll':'58eb1a1b44b30275bdd21368de264d856bc310d37d93a0f76f111f0026913487',
    'client.dll':'002b36487fec7c98882929fbccdb506d0146bbf3270cbb964a4d21c5edf7eebc',
    'materialsystem_dx11.dll':'1afffb8dfe2a97a00ca3c5a68ca5277249176c2fd51ee7c5ad70f6eb699a6050'}
SYMBOLS = {
    'engine.dll': {'CLMove':0x734C0,'GetClient':0x78200,'HostShouldRun':0x157B40,
      'IsPaused':0x8F520,'GetFrameTime':0x8E400,'SendMove':0x74F10,
      'CanPacket':0x20F620,'SendNetMsg':0x213270,'SendDatagram':0x212CD0,
      'SetChoked':0x213760,'SetClientTime':0x91AD0},
    'client.dll': {'ExtraSample':0x2554C0,'GetButtonBits':0x251620,
      'ApplyMouse':0x2567B0,'InputCreateMove':0x254D10,'PlayerCreateMove':0x14A880,
      'ActivateMouse':0x256700,'DeactivateMouse':0x256940,'ClearStates':0x2568F0},
    'materialsystem_dx11.dll': {'BeginFrame':0x5A200}}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('bin_dir',type=Path)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--report',type=Path,required=True)
    a=p.parse_args()
    lines=['// Generated from SHA-256-pinned retail modules. Do not regenerate for unknown builds.',
      '#pragma once', '#include <cstdint>',
      'namespace tf2hf {',
      'struct ModuleSpec { const char* name; const char* sha256; uint32_t stamp, imageSize; };',
      'struct SymbolSpec { const char* name; int module; uint32_t rva; const char* signature; };',
      'inline constexpr ModuleSpec moduleSpecs[] = {']
    profiles=[]
    for index,(name,sha) in enumerate(EXPECTED.items()):
        data=(a.bin_dir/name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=sha:
            raise ValueError('Unsupported module '+name)
        pe=pefile.PE(data=data)
        text=next(s for s in pe.sections if s.Name.rstrip(b'\0')==b'.text')
        code=text.get_data()
        profile={'name':name,'sha256':sha,'timestamp':pe.FILE_HEADER.TimeDateStamp,
          'image_size':pe.OPTIONAL_HEADER.SizeOfImage,'symbols':[]}
        lines.append(f'  {{"{name}", "{sha}", 0x{profile["timestamp"]:X}, 0x{profile["image_size"]:X}}},')
        for symbol,rva in SYMBOLS[name].items():
            if not text.VirtualAddress<=rva<text.VirtualAddress+text.Misc_VirtualSize:
                raise ValueError('Non-code symbol '+symbol)
            # Exact bytes are ASLR-invariant unless a relocation falls inside them.
            for n in [24,32,48,64,96,128]:
                signature=pe.get_data(rva,n)
                if code.count(signature)==1:
                    break
            else:
                raise ValueError('Non-unique signature '+symbol)
            for block in getattr(pe,'DIRECTORY_ENTRY_BASERELOC',[]):
                for relocation in block.entries:
                    if relocation.type and rva<=relocation.rva<rva+n:
                        raise ValueError('ASLR-dependent signature '+symbol)
            instructions=list(Cs(CS_ARCH_X86,CS_MODE_64).disasm(signature,rva))
            if not instructions or instructions[0].address!=rva:
                raise ValueError('Invalid function entry '+symbol)
            profile['symbols'].append({'name':symbol,'rva':rva,'signature':signature.hex(),
                'first_instruction':instructions[0].mnemonic+' '+instructions[0].op_str})
        profiles.append(profile)
    lines += ['};','inline constexpr SymbolSpec symbolSpecs[] = {']
    for index,profile in enumerate(profiles):
        for s in profile['symbols']:
            lines.append(f'  {{"{s["name"]}", {index}, 0x{s["rva"]:X}, "{s["signature"]}"}},')
    lines += ['};','}']
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    a.report.write_text(json.dumps(profiles,indent=2),encoding='utf-8')
    print(f'Validated {sum(len(m["symbols"]) for m in profiles)} unique signatures across 3 pinned modules')

if __name__=='__main__': main()
