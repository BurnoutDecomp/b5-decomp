#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"    // Playback::Object (base) / Factory / Content fwds
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"     // Handle<T>
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"   // RegistrySpec / Registry

// CgsSound::Playback::Environment - the sound-playback world: it owns the
// factory/voice/content handle tables, the registry and the DAC plug-in, and is
// carved (operator new/delete) out of a caller-supplied rw allocator. Class shape /
// member names / method set verbatim from the DecFIGS DWARF (CgsEnvironment.h);
// gated on the X360 ledger. This header TU bodies the one ledger function
// attributed to it -- the header-inline DoDispose @0x826BFDF8 -- and declares the
// rest of the DWARF surface (their own ledger functions).
//
// X360 layout note (DoDispose @0x826BFDF8): Object base {vptr, mu32RefCount} = 8
// bytes, mCpuMonitors 40 bytes -> mpAllocator at +48, exactly the word DoDispose
// snapshots before destroying itself.

namespace rw { struct IResourceAllocator; }
namespace rw { namespace audio { namespace core { class PlugIn; } } }

namespace CgsSound
{
namespace Playback
{
    class Factory;
    class Voice;

    // DWARF CgsEnvironment.h:69 -- the per-stage CPU perf-monitor ids the
    // environment update registers. Construct (h:77) is its own ledger function.
    struct CpuMonitors
    {
        s32 miModule;               // h:83
        s32 miProcessCommands;      // h:84
        s32 miEnvironmentUpdate;    // h:85
        s32 miRwacFactoryUpdate;    // h:86
        s32 miAemsFactoryUpdate;    // h:87
        s32 miAemsFactoryUpdate2;   // h:88
        s32 miSplicerFactoryUpdate; // h:89
        s32 miContentUpdate;        // h:90
        s32 miVoiceUpdate;          // h:91
        s32 miVoiceUpdateOutput;    // h:92

        void Construct();
    };

    // DWARF CgsEnvironment.h:104 -- the creation spec Create/operator new consume.
    struct EnvironmentSpec
    {
        rw::IResourceAllocator* mpAllocator;      // h:108
        u32                     mu32FactoryCount; // h:109
        u32                     mu32VoiceCount;   // h:110
        u32                     mu32ContentCount; // h:111
        RegistrySpec            mRegistrySpec;    // h:112
    };

    struct Environment : public Object
    {
        // DWARF CgsEnvironment.h:334.
        enum eAudioMode
        {
            E_AUDIO_MODE_STEREO   = 0,
            E_AUDIO_MODE_SURROUND = 1,
        };

        static const u32 KU32_INVALID_INDEX = 0xFFFFFFFFu;   // DWARF h:365

        // ---- DWARF surface (their own ledger functions; declaration-only) ----
        virtual ~Environment();
        static size_t GetAllocationSize(const EnvironmentSpec& lrSpec);
        size_t GetAllocatedSize();
        void* operator new(size_t luSize, const EnvironmentSpec& lrSpec);
        void operator delete(void* lpMemory, rw::IResourceAllocator* lpAllocator);   // h:495
        void operator delete(void* lpMemory, const EnvironmentSpec& lrSpec);         // h:163
        void operator delete(void* lpMemory);                                        // h:167
        void* Allocate(u32 lu32Size, u32 lu32Alignment, const char* lpcName);
        void Free(void* lpMemory);
        static Handle<Environment> Create(const EnvironmentSpec& lrSpec);
        bool AddFactory(Factory& lrFactory);
        u32  AddVoice(Voice& lrVoice);
        u32  AddContent(Content& lrContent);
        bool RemoveFactory(Factory& lrFactory);

        // @0x826BFDF8 (this TU, DWARF h:535) -- the ref-count-zero disposer:
        // snapshot the owning allocator, run the (non-deleting) destructor, then
        // release the carve through the allocator-keyed operator delete. The X360
        // dispatches the dtor virtually (vtbl slot 0, deleting-flag 0).
        virtual void DoDispose()
        {
            rw::IResourceAllocator* lpAllocator = mpAllocator;
            this->~Environment();
            operator delete(this, lpAllocator);
        }

    private:
        CpuMonitors             mCpuMonitors;             // h:404 (X360 +0x08)
        rw::IResourceAllocator* mpAllocator;              // h:405 (X360 +0x30 -- the DoDispose snapshot)
        u32                     mu32FactoryCount;         // h:406
        u32                     mu32VoiceCount;           // h:407
        u32                     mu32ContentCount;         // h:408
        Handle<Factory>*        mphFactory;               // h:409
        Handle<Voice>*          mphVoice;                 // h:410
        Handle<Content>*        mphContent;               // h:411
        Registry*               mpRegistry;               // h:412
        rw::audio::core::PlugIn* mpDacPlugin;             // h:413
        f32                     mafVoiceTypeTickTotals[5];// h:437
        u32                     muActiveVoices;           // h:439
        u32                     muActiveContent;          // h:440
    };
}
}
