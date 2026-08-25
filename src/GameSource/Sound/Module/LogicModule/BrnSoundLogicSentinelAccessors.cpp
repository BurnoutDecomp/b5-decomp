// BrnSoundLogicSentinelAccessors.cpp
//
// BrnSound::Logic namespace-scope accessors (BURNOUT_X360_ARTIST.XEX). Each is a
// 2-instruction leaf (`lis/addi &unk_XXXX; blr`).
//
// IDENTIFIED 2026-08-25 (audio-faithfulness wave 5): the returned unk_82F2xxxx
// records were dumped from the decrypted XEX and are the per-class STATIC RTTI
// DESCRIPTORS -- CgsSound::Logic::ClassTypeInfo records {ObjectID, typeName*,
// baseTypeInfo*, createObject*}. The truncated IDA symbols ("E","F","G","H","M",
// "P","S","Streamin","T","Traffic_","W") are the class-name initials of
// GetStaticTypeInfo-style accessors. Recovered record contents (BE words + the
// followed typeName string):
//
//   H        @0x82686838 -> 82F2F88C {0x00000010, "HUDEffect",            base 82F2E7FC, create 0x826E71C0}
//   G        @0x826867D8 -> 82F2F85C {0x00000000, "GlobalStateManager",   base 82F2FAA0, create 0x826FE380}
//   F        @0x82687948 -> 82F2F8BC {0x00000030, "FxEffect",             base 82F2E7FC, create 0x826D2778}
//   E        @0x82689178 -> 82F2F99C {0x00080000, "ExplosionEffect",      base 82F2E7FC, create 0x826D5518}
//   M        @0x826871D8 -> 82F2F8AC {0x00000020, "MusicEffect",          base 82F2E7FC, create 0x826D2718}
//   P        @0x82687968 -> 82F2F8CC {0x00000000, "PresentationEffect",   base 82F2E7FC, create 0x826F7770}
//   S        @0x82683A78 -> 82F2E84C {0x00000006, "StreamingStateManager",base 82F2FAA0, create 0x82700B68}
//   Streamin @0x82683A58 -> 82F2E83C {0x00060000, "StreamingState",       base 82F2E7DC, create 0x826C9AA8}
//   T        @0x826840F8 -> 82F2E8BC {0x00030000, "TrafficEngine",        base 82F2E7FC, create 0x826CADE0}
//   Traffic_ @0x826841A8 -> 82F2E8F0 {0x00030000, "TrafficState",         base 82F2E7DC, create 0x826CB080}
//   W        @0x82686668 -> 82F2F81C {0x00070000, "EmitterEffect",        base 82F2E7FC, create 0x826E6AE0}
//
// The shared base records: 82F2E7FC = the EffectBase-family base descriptor,
// 82F2FAA0 = the StateManager base descriptor, 82F2E7DC = the State base
// descriptor. The ObjectIDs corroborate the PS3 static-init pins
// (GlobalStateManager=0, StreamingStateManager=6) and RECOVER the effect/state
// family ids (HUDEffect=0x10, MusicEffect=0x20, FxEffect=0x30; states in the
// 0x10000-stepped space: TrafficEngine/TrafficState=0x30000, StreamingState=
// 0x60000, EmitterEffect=0x70000, ExplosionEffect=0x80000) -- seed data for the
// per-class GetStaticTypeInfo TUs as they land.
//
// The two classes whose HOST descriptor already exists (the mounted state
// managers) FORWARD to it -- the true decompiled behaviour (return &sTypeInfo).
// The rest keep the honest sentinel return until their classes grow descriptors;
// each carries its recovered identification above.

#include "GameSource/Sound/Global/BrnGlobalStateManager.h"       // GlobalStateManager::GetStaticTypeInfo
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h" // StreamingStateManager::GetStaticTypeInfo

namespace BrnSound
{
namespace Logic
{
    // @ 0x82686838  -> &unk_82F2F88C  == HUDEffect::sTypeInfo (descriptor un-homed on host)
    void* H()
    {
        return (void*)"";
    }

    // @ 0x826867D8  -> &unk_82F2F85C  == GlobalStateManager::sTypeInfo.
    // FORWARDS to the committed host descriptor (BrnGlobalStateManager.cpp seeds
    // ObjectID 0 / "GlobalStateManager" / the StateManager base -- exactly the
    // dumped record).
    void* G()
    {
        return GlobalStateManager::GetStaticTypeInfo();
    }

    // @ 0x82687948  -> &unk_82F2F8BC  == FxEffect::sTypeInfo (descriptor un-homed on host)
    void* F()
    {
        return (void*)"";
    }

    // @ 0x82689178  -> &unk_82F2F99C  == ExplosionEffect::sTypeInfo (descriptor un-homed on host)
    void* E()
    {
        return (void*)"";
    }

    // @ 0x826871D8  -> &unk_82F2F8AC  == MusicEffect::sTypeInfo (descriptor un-homed on host)
    void* M()
    {
        return (void*)"";
    }

    // @ 0x82687968  -> &unk_82F2F8CC  == PresentationEffect::sTypeInfo (descriptor un-homed on host)
    void* P()
    {
        return (void*)"";
    }

    // @ 0x82687988  -> return this + 0x224  (truncated "PresentationEffect_" fragment)
    //
    // FLAG (confidence low): 2-instruction leaf `addi r3,r3,0x224 ; blr` -- a member accessor
    // returning a pointer to a sub-object at byte offset 0x224 (548) of its owning object
    // (candidate: a PresentationEffect member inside/after the maVoices span; un-attested).
    // Per HARD RULE 6 only the observable pointer arithmetic is reproduced. char* keeps this
    // TU's include surface minimal.
    void* PresentationEffect_( void* lpThis )
    {
        return reinterpret_cast<char*>( lpThis ) + 0x224;
    }

    // @ 0x82683A78  -> &unk_82F2E84C  == StreamingStateManager::sTypeInfo.
    // FORWARDS to the committed host descriptor (ObjectID 6 corroborated by the dump).
    void* S()
    {
        return Streaming::StreamingStateManager::GetStaticTypeInfo();
    }

    // @ 0x82683A58  -> &unk_82F2E83C  == StreamingState::sTypeInfo (descriptor un-homed on host)
    void* Streamin()
    {
        return (void*)"";
    }

    // @ 0x826840F8  -> &unk_82F2E8BC  == TrafficEngine::sTypeInfo (descriptor un-homed on host)
    void* T()
    {
        return (void*)"";
    }

    // @ 0x826841A8  -> &unk_82F2E8F0  == TrafficState::sTypeInfo (descriptor un-homed on host)
    void* Traffic_()
    {
        return (void*)"";
    }

    // @ 0x82686668  -> &unk_82F2F81C  == EmitterEffect::sTypeInfo (descriptor un-homed on host)
    void* W()
    {
        return (void*)"";
    }
}
}
