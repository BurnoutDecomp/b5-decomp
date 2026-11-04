// DirtySDK ProtoSSL entropy, ARTIST 82B356A0..82B35750.
#include "types.hpp"
#include "cryptarc4.h"

extern "C" u32 NetTick();

// ARTIST 8324EE34..8324EE43: process-lifetime 16-byte seed, initially zero.
static u32 gProtoSSLRandomSeed[4] = { 0, 0, 0, 0 };

// This leaf has no frame and leaves r1 at its caller's native frame. The XDK
// listing must show the GenerateRandom caller using the ARTIST 0x80-byte frame
// and savegprlr_28 prologue. It intentionally samples the same 32 stack words
// as ARTIST, including words in the caller's frame; C++ reads of uninitialized
// arrays do not express this machine behavior safely.
extern "C" __declspec(naked) __declspec(noinline) u32 ProtoSSLSampleStack(u32)
{
    __asm {
        mr r10, r3
        li r11, 0
        stw r11, 80(r1)
    sample_loop:
        slwi r9, r11, 2
        addi r8, r1, 80
        addi r11, r11, 1
        cmpwi cr6, r11, 32
        lwzx r9, r9, r8
        stw r11, 80(r1)
        add r10, r9, r10
        blt cr6, sample_loop
        mr r3, r10
        blr
    }
}

extern "C" void ProtoSSLGenerateRandom(u8* buffer, s32 length, CryptArc4T* arc4)
{
    if (!gProtoSSLRandomSeed[0])
        gProtoSSLRandomSeed[0] = NetTick();
    const u32 tick = NetTick();
    ++gProtoSSLRandomSeed[2];
    gProtoSSLRandomSeed[1] += tick;
    gProtoSSLRandomSeed[3] = ProtoSSLSampleStack(gProtoSSLRandomSeed[3]);
    if (buffer)
    {
        CryptArc4Init(arc4, reinterpret_cast<const u8*>(gProtoSSLRandomSeed), 16, 3);
        CryptArc4Apply(arc4, buffer, length);
    }
}
