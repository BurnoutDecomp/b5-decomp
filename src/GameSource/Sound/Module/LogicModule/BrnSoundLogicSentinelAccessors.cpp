// BrnSoundLogicSentinelAccessors.cpp
//
// BrnSound::Logic namespace-scope sentinel accessors (BURNOUT_X360_ARTIST.XEX). Each is a
// 2-instruction leaf (`lis/addi &unk_XXXX; blr`) returning a .rodata sentinel; per HARD RULE
// 5 each reconstructs as the empty string literal, mirroring the committed sibling
// CgsSound::Logic::State::G() (CgsState.cpp @0x8268D410).
//
// FLAG (confidence low): the IDA symbols are truncated ("S", "Streamin", "T", "Traffic_",
// "W"); the accessors' meaning is not recoverable from the single instruction pair, so only
// the observable sentinel return is modelled.

namespace BrnSound
{
namespace Logic
{
    // @ 0x82683A78  -> &unk_82F2E84C
    void* S()
    {
        return (void*)"";
    }

    // @ 0x82683A58  -> &unk_82F2E83C  (truncated "Streamin", likely a Streaming-* fragment)
    void* Streamin()
    {
        return (void*)"";
    }

    // @ 0x826840F8  -> &unk_82F2E8BC
    void* T()
    {
        return (void*)"";
    }

    // @ 0x826841A8  -> &unk_82F2E8F0  (truncated "Traffic_", a Traffic-* fragment)
    void* Traffic_()
    {
        return (void*)"";
    }

    // @ 0x82686668  -> &unk_82F2F81C
    void* W()
    {
        return (void*)"";
    }
}
}
