from pathlib import Path
root=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion')
base=root/'src/SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime'
def replace(path,old,new):
    text=path.read_bytes().decode('utf-8')
    assert old in text,path
    path.write_bytes(text.replace(old,new).encode('utf-8'))
replace(base/'include/ParticleRender/ParticleRender.h','#include <EABase/eabase.h>       // one bool8_t ABI regardless of include order\r\n','')
replace(base/'include/ParticleRender/ParticleRender.h',"// Use EABase's bool8_t. A separate u8 alias has the same size but a different\r\n// native C++ signature, making virtual declarations depend on include order.\r\n",'// Microsoft C++ EABase uses bool for bool8_t (GNU/PS3 uses int8_t). Keep\r\n// that native signature without importing modern EABase platform detection.\r\n#ifndef BOOL8_T_DEFINED\r\n#define BOOL8_T_DEFINED\r\ntypedef bool bool8_t;\r\n#endif\r\n')
replace(base/'ext-include/GameStructs/cTime.h','    // default one is `= default` (trivial) rather than a body: cParticleEmitter objects are\r\n    // carved out of a pool and never constructed, and cParticleEmitter::Init @0x82913228 is\r\n    // what zeroes their stamps. The U32 one is what cParticleEmitter::IsGenerating\r\n','    // default constructor leaves the tick untouched: cParticleEmitter objects are\r\n    // carved out of a pool, and cParticleEmitter::Init @0x82913228 is what zeroes\r\n    // mLastTime and mUpdateLastTime. Do not move those stores into construction.\r\n    // The U32 one is what cParticleEmitter::IsGenerating\r\n')
replace(base/'ext-include/GameStructs/cTime.h','    cTime() = default;','    cTime() {}')
replace(base/'include/LionSerialisedPtr.h','    explicit operator bool() const { return muSlot != 0u; }','    // C++03 safe-bool keeps contextual tests without an integer conversion.\r\n    typedef u32 tLionSerialisedPtr::* tBool;\r\n    operator tBool() const { return muSlot != 0u ? &tLionSerialisedPtr::muSlot : 0; }')
