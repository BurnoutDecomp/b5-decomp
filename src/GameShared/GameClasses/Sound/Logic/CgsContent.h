#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"   // CgsSound::Playback::Object (Acquire/Release)
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"   // CgsSound::Playback::Handle<T>

// ============================================================================
// CgsContent.h -- CgsSound::Logic::Content (COHERENT HOME, revised to DWARF).
//
// The logic-side handle onto one refcounted playback-content object. DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Sound/Logic/CgsContent.h)
// is authoritative for the member SHAPE and NAMES:
//   struct CgsSound::Logic::Content {           // CgsContent.h:74
//       vptr                                              @ +0x00
//       typedef Handle<Playback::Content> ContentHandle;  // CgsContent.h:60
//       Handle<Playback::Content> mContentHandle          @ +0x04  (CgsContent.h:153)
//       CgsSound::Logic::Module*  mpModule                @ +0x08  (CgsContent.h:154)
//   };
//
// Total size stays 12 bytes {vptr, Handle<T> (single pointer), Module*} == the
// same 12-byte element stride the AI-engine spec arrays use
// (BrnAIVehicleStateManager::maLoopContentSpecs et al.), so this revision is
// stride/ODR-compatible with the prior {mpContent, muTrailing} model.
//
// CONSOLIDATOR NOTE: CgsLogicContentDtor.cpp is updated in this same change to drop
// the ref via mContentHandle (identical single observable side effect).
//
// DISTINCT from CgsSound::Playback::Content (the refcounted spec in
// Playback/CgsContent.h).
// ============================================================================

namespace CgsSound
{
namespace Playback
{
    // FLAG (DEFER): forward decl of the refcounted playback content the handle owns.
    // Full home is Playback/CgsContent.h (struct Content : public Object). Only its
    // Acquire/Release (from Object) and BeginRemove() are load-bearing for this TU.
    struct Content;

    // FLAG (DEFER): begin removing the content from the playback graph. Called on the
    // held content in Content::Destruct (X360 bl CgsSound::Playback::Content::BeginRemove).
    // Declared BY NAME to match the `bl`; real body is a separate Playback::Content slice.
    // Free shim keeps Content incomplete here (the member fn lives on Playback::Content).
    void ContentBeginRemove(Content* lpContent);
}

namespace Logic
{
    // Command::QueueElement -- a 4-byte queue selector (Command namespace, X360 4-byte
    // value). The two Construct selectors are passed opaque straight through to the
    // content-create helper. FLAG: modelled as a 4-byte value; the full home is the
    // command-queue TU.
    namespace Command { typedef u32 QueueElement; }

    // FLAG (DEFER): the sound-logic module. Full home is CgsSoundLogicModule.h
    // (declaration-only keystone). Content::Construct needs (a) the content ident
    // from the module's vtable slot +0x48 and (b) the embedded Playback::Module at
    // X360 +0x238. Both are deferred surface; because the committed Module home is an
    // incomplete keystone (no such accessors), they are modelled here as BY-NAME free
    // shims (mirroring the deferred style in CgsVoice.cpp) so this TU bodies + compiles.
    // Fold onto the Module keystone when it exposes these directly.
    class Module;

    // vtable-slot-0x48 virtual: the module's content ident. X360:
    //   r11 = *lpModule; r11 = *(r11 + 0x48); ident = (*r11)(lpModule);
    Command::QueueElement ModuleGetContentIdent(Module* lpModule);

    // The content-create helper. X360 `bl CgsSound::Playback::Module::Module_` with
    //   r3 = &out-handle, r4 = &lpModule->mPlaybackModule (+0x238),
    //   r5 = ident, r6 = aQueueElementA, r7 = aQueueElementB.
    // Returns the out-handle (r3). Deferred; the real Module_ home is the
    // Playback::Module TU. Declared BY NAME here to match the single `bl`.
    Playback::Handle<CgsSound::Playback::Content>* ModuleCreateContent(
        Playback::Handle<CgsSound::Playback::Content>* lphOut,
        Module* lpModule,
        Command::QueueElement lIdent,
        Command::QueueElement aQueueElementA,
        Command::QueueElement aQueueElementB);

    // CgsContent.h:74 (DWARF). Logic-side handle onto one refcounted playback content.
    struct Content
    {
    public:
        // CgsContent.h:60. The held-content handle type (DWARF typedef).
        typedef Playback::Handle<CgsSound::Playback::Content> ContentHandle;

        // CgsContent.cpp:54. Null-initialise the handle + owning module.
        Content() : mContentHandle(), mpModule(0) {}

        // CgsContent.cpp:72. Virtual destructor -- drops the held reference. Bodied
        // out-of-line (CgsLogicContentDtor.cpp) so this class emits the single ~Content
        // symbol the X360 vector-deleting destructor @0x826DC378 is synthesised from.
        virtual ~Content();

        // CgsContent.cpp:92 @ 0x826C4CA0. Create/replace the held content from the module.
        void Construct(Module* lpModule,
                       Command::QueueElement aQueueElementA,
                       Command::QueueElement aQueueElementB);

        // CgsContent.cpp:116 @ 0x826C4D98. Begin-remove + release the held content.
        void Destruct();

        // CgsContent.h:195. The held content handle (const accessor).
        const ContentHandle& GetContent() const { return mContentHandle; }

        // CgsContent.h:213. Created == the handle owns a content object.
        bool IsCreated() const { return mContentHandle.GetObject() != 0; }

        // CgsContent.h:231. Deferred body -- returns true (the specs are asserted loaded
        // on the boot path that reaches the accessor). Kept from the prior commit.
        bool IsLoaded() const { return true; }

    private:
        // Non-copyable (CgsContent.h:142/147 -- private copy ctor / assign, declared-only).
        Content(const Content&);
        Content& operator=(const Content&);

    protected:
        ContentHandle            mContentHandle; // +0x04 DWARF:153 (held content, or null)
        CgsSound::Logic::Module* mpModule;       // +0x08 DWARF:154 (owning logic module)
    };
}
}
