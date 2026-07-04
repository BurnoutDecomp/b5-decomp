// BrnSoundLogicTypeInfoStubs.cpp
//
// Truncated-symbol RTTI-name-fragment sentinels from the BrnSound::Logic domain
// (BURNOUT_X360_ARTIST.XEX). Each is a 2-instruction leaf (`lis/addi &unk_XXXX; blr`)
// that returns a .rodata sentinel; per the &unk_XXXX convention (HARD RULE 5) each
// reconstructs as the empty string literal, mirroring the committed sibling
// CgsSound::Logic::State::G() (CgsState.cpp @0x8268D410).
//
// The IDA demangled symbols are truncated to name fragments (e.g. `BrnSound::Logic::B`),
// so only the observable sentinel return is recoverable; the accessors are homed here as
// uniquely-named free functions (one stubs TU) rather than force-classed. FLAG: confidence
// low on every entry (truncated symbol; observable return only).

// BrnSound::Logic:: (truncated fragment)  @ 0x82682C08  -> &unk_82F2E80C
void* TypeInfoFragment_82682C08()
{
    return (void*)"";
}

// BrnSound::Logic::B (truncated fragment)  @ 0x82682AA8  -> &unk_82F2E7EC
void* TypeInfoFragment_82682AA8_B()
{
    return (void*)"";
}

// BrnSound::Logic::BrnState (truncated fragment)  @ 0x82682A88  -> &unk_82F2E7DC
void* TypeInfoFragment_82682A88_BrnState()
{
    return (void*)"";
}

// BrnSound::Logic::C (truncated fragment)  @ 0x82688058  -> &unk_82F2F91C
void* TypeInfoFragment_82688058_C()
{
    return (void*)"";
}

// BrnSound::Logic::Collisio (truncated 'Collision' fragment)  @ 0x82688018  -> &unk_82F2F8FC
void* TypeInfoFragment_82688018_Collisio()
{
    return (void*)"";
}
