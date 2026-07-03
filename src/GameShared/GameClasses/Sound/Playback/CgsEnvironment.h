#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"     // Playback::Object (base, with Acquire/Release)
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
//
// NOTE (Object base): Environment derives from the CgsObject.h `Object` (vptr +
// mu32RefCount, WITH Acquire()/Release()), NOT the CgsContent.h copy -- the wave-11
// Add*/GetR/GetV bodies open-code the handle-slot ref-count ops as Object::Acquire()
// / Object::Release() (CgsObject.h:45/49). Content is only referenced as a pointer
// (static AddContent / GetR's Handle<Content>), so it is forward-declared here to
// avoid pulling CgsContent.h's competing Object definition into this TU.

namespace rw { struct IResourceAllocator; }
namespace rw { namespace audio { namespace core { class PlugIn; } } }

// The RWAC engine entries StartDac/StopDac drive (declared BY NAME; resolved in the
// RWAC System / PlugIn TUs). RwacSystemLock + GetDefaultRwacSystem already live in
// RWAC/CgsGenericRwacFactory.h; the .cpp reaches them through that header and adds
// the Unlock + 3-arg PlugIn event forms below.
namespace rw { namespace audio { namespace core {
    class System;
    void RwacSystemUnlock(System* apSystem);                              // X360 System::Unlock
    void RwacPlugInEvent(PlugIn* apPlugIn, int aiEvent, int aiArg);       // 3-arg engine event
} } }

namespace CgsSound
{
namespace Playback
{
    class Factory;
    struct Content;   // pointer-only surface here (static AddContent / Handle<Content>)

    // FLAG (DEFER): minimal Playback::Voice slice for GetR. GetR walks the voice
    // table and calls Voice::FindNamedSlot on each candidate; only that one static
    // entry (Voice*, u32*) is load-bearing here. The full Voice home is CgsVoice.h;
    // this minimal decl-only slice avoids pulling CgsVoice.h (which drags in the
    // CgsContent.h Object and its competing definition) into this TU, mirroring the
    // module-header minimal-slice pattern. No single TU sees both this and CgsVoice.h.
    class Voice
    {
    public:
        static int FindNamedSlot(Voice* apVoice, u32* apSpec);  // X360 Voice::FindNamedSlot
    };

    // GetR rodata globals (X360 dword_83008650 / dword_830080A8). Bodied in their
    // owning data TUs.
    extern const u32 gu32VoiceTypeTag;        // X360 dword_83008650
    extern const u32 gu32NamedSlotSentinel;   // X360 dword_830080A8

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

        // @0x826AD130 / 1E0 / 290. Insert into the factory/voice/content handle tables.
        // STATIC 2-arg form (X360 __fastcall a1=Environment&, a2=object*): the asm
        // NULL-checks the object arg, so it is a pointer. This is the form the committed
        // CgsFactory.cpp caller uses (Environment::AddFactory(mEnvironment, this) /
        // AddContent(mEnvironment, arHandleOut.GetObject())) and CgsFactory.h declares.
        static bool AddFactory(Environment& arEnvironment, Factory* apFactory);   // @0x826AD130
        static u32  AddVoice  (Environment& arEnvironment, Voice*   apVoice);     // @0x826AD1E0
        static u32  AddContent(Environment& arEnvironment, Content* apContent);   // @0x826AD290

        bool RemoveFactory(Factory& lrFactory);

        // @0x82680EF8 / EA0. Accessors (assert the member is present).
        rw::IResourceAllocator* GetAllocator();   // @0x82680EF8
        Registry*               GetRegistry();    // @0x82680EA0

        // @0x826BFAF0 / FE50. Look a voice up by ident / a content up by plugin key.
        Handle<Voice>   GetV(u32 au32Id);         // @0x826BFAF0
        Handle<Content> GetR(u32 au32Plugin);     // @0x826BFE50

        // @0x82680F50 / FE8. Start/stop the DAC plug-in through the RWAC engine.
        void StartDac();                          // @0x82680F50
        void StopDac();                           // @0x82680FE8

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
