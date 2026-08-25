#ifndef CGS_SOUND_PLAYBACK_CONTENT_H
#define CGS_SOUND_PLAYBACK_CONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h" // Entity (ContentSpec base)
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"   // Object (canonical DWARF home -- fold done, see the note below)

#include <cstddef> // size_t

namespace CgsSound
{
namespace Playback
{

    // Forward declaration -- ContentSpec::GetContentType returns a resolved
    // ContentType (full home in CgsDataStructures.h).
    struct ContentType;
    enum EContentState
    {
        E_CONTENT_STATE_INVALID = 0,
        E_CONTENT_STATE_UNLOADED = 1,
        E_CONTENT_STATE_LOADING = 2,
        E_CONTENT_STATE_LOADED = 3,
        E_CONTENT_STATE_UNLOADING = 4,
        E_CONTENT_STATE_COUNT = 5,
        E_CONTENT_STATE_CHANGED = 128
    };

    enum EContentRemoveState
    {
        E_CONTENT_REMOVE_INVALID = 0,
        E_CONTENT_REMOVE_ALIVE = 1,
        E_CONTENT_REMOVE_REMOVING = 2,
        E_CONTENT_REMOVE_REMOVED = 3
    };

    struct Content;

    // (2026-08-25, audio-faithfulness wave 3): the former TU-local rival
    // `class Factory { GetContentDisposer(); }` + ContentDisposer +
    // ContentDisposeRequest trio is RETIRED -- it was an invented model of
    // Content::DoDispose's tail, which (like Factory::DoDispose @0x826AD450) is the
    // owning environment's rw ALLOCATOR taking the carve back. The real Factory
    // (CgsFactory.h) is forward-declared here (`Factory& mFactory` is held by
    // reference only); DoDispose is bodied out-of-line in CgsObject.cpp where the
    // full Factory/Environment/rw surface is includable without a header cycle.
    class Factory;

    // CgsDataStructures.h:425 (DWARF). ContentSpec : public Entity. The serialised
    // content specification: a resolved ContentType* (+8), the packed load
    // method/time bytes, and an inline full-path buffer beginning at +0x10 (the
    // X360 GetPath()/GetPathZone read `this + 16`). Members pinned BY NAME +
    // SEQUENCE (host-width FLAG: the pointer member widens on the 64-bit host).
    struct ContentSpec : public Entity
    {
        // The interned type-name the Registry keys a ContentSpec entity on. FLAG
        // (ADDITIVE home-grow, by-name): Registry::GetEntity<ContentSpec> compares a
        // slot's mTypeName against this per-type static Name (the X360 word
        // dword_830080B8). DECLARED here for the lookup; its interned DEFINITION lives
        // with the ContentSpec/EntityFixer<ContentSpec> registration TU (DEFERRED),
        // mirroring the other Entity subclasses in CgsDataStructures.h.
        static const Name SK_TYPE_NAME;

        // '|' path-zone separator (DWARF CgsDataStructures.h:1792 / :430).
        static const char SK_PATH_SEPERATOR = '|';

        // @ 0x826927C8. Return the resolved ContentType (assert present + resolved).
        // @ 0x826928C8. Copy the au32Zone'th '|'-separated path zone into apcPathOut.
        const ContentType& GetContentType() const;
        bool GetPathZone(u32 au32Zone, char* apcPathOut, size_t auMaxLen) const;

        // The inline full path (starts at +0x10; the X360 reads `this + 16`).
        const char* GetPath() const { return macFullPath; }

        const ContentType* mpContentType;   // (+0x8)  resolved content type
        u16                mu16PathLength;   // (+0xC)  full-path byte length
        u8                 mu8LoadMethod;    // (+0xE)
        u8                 mu8LoadTime;      // (+0xF)
        char               macFullPath[1];   // (+0x10) inline '|'-joined path buffer
    };

    // Object: the CONDUCTOR fold from CgsObject.h's note is DONE (2026-08-25,
    // audio-faithfulness wave 3) -- this header's private simplified copy is deleted
    // and the single canonical CgsSound::Playback::Object (CgsObject.h, DWARF home,
    // with Acquire/Release/GetRefCount and the out-of-line asserting ~Object
    // @0x826916F0 bodied in CgsObject.cpp) is included at the top of this file.

    class Voice;   // fwd -- Content attach/detach hooks take a Voice + Slot.
    class Slot;

    struct Content : public Object
    {
        Content(Factory& lFactory, const ContentSpec& lContentSpec, u32 lu32DataSize);
        virtual ~Content();

        // The spec this content was created from (read by Slot::Attach to match the
        // slot's authored ContentClass). ADDITIVE grow (by-name accessor).
        const ContentSpec& GetContentSpec() const { return mContentSpec; }

        // @ 0x826A2458. Drop a load reference when detached from a voice slot;
        // commits the unload (via DoUnload) on the last reference, then hands off to
        // Slot::HandleDetach. Bodied in CgsObject.cpp.
        void OnDetach(Voice& arVoice, Slot& arSlot);

        // Attach hook the detach path uses. FLAG (DEFER): declared-only -- bodied in
        // its own Content TU. OnAttach takes a load reference.
        void OnAttach(Voice& arVoice, Slot& arSlot);

        // DWARF CgsContent.h:492. The owning factory, by name (the X360 reads
        // Content+0x08; ContentLoader's allocator walk goes through here).
        Factory& GetFactory() { return mFactory; }

        // Publish a resolved content state (E_CONTENT_STATE_*), setting the CHANGED
        // flag as required. FLAG (DEFER): declared-only -- bodied in its own Content TU
        // (X360 CgsSound::Playback::Content::SetContentState). Reached from
        // ContentLoader<>::UpdateResourceModuleLoading on the FINISHED transition.
        void SetContentState(int liState);

        // FLAG (committed-home DEFECT corrected to match DWARF ground truth):
        // CgsContent.h DWARF (line 141/301) declares Content::DoDispose() returning
        // *void*, matching Object::DoDispose() (CgsObject.h:27, also void). The prior
        // commit declared it `int`, which is an ILLEGAL non-covariant override of the
        // void base method -- the class did not compile once instantiated as a full
        // hierarchy (no .cpp had included this header before the CgsSound-RWAC-content
        // group). This is a return-type correction proven by DWARF; it changes NO
        // layout/offset/sizeof and adds no member, so it is safe under the
        // grow-additively rule. Not a semantic change -- the X360 method is void.
        virtual void DoDispose();

        // The full DWARF virtual set in DECLARATION ORDER == vtable order
        // ([0] ~Content, [1] DoDispose :141, [2] DoLoad :259, [3] DoUnload :267,
        //  [4] DoOnPostLoad :271, [5] DoOnPreUnload :274, [6] DoUpdate :278,
        //  [7] DoGetData :281) -- completed 2026-08-25 (audio-faithfulness wave 3;
        // DoUnload was previously declared NON-virtual here, breaking the
        // GenericRwac/Splicer/AEMS subclass overrides, and the missing slots forced
        // ContentLoader into raw vtable-index dispatch). DoOnPostLoad keeps its
        // inline base default (return true); the others are FLAG (DEFER)
        // declared-only base slices bodied in their own TUs.
        virtual bool  DoLoad();
        virtual bool  DoUnload();
        virtual bool  DoOnPostLoad();
        virtual bool  DoOnPreUnload();
        virtual void  DoUpdate(f32 af32DeltaTime);
        virtual void* DoGetData();

        Factory& mFactory;
        const ContentSpec& mContentSpec;
        u32 mIdent;
        void* mpLoadService;
        u32 mu32DataSize;
        u16 mu16LoadCount;
        u8 mu8ContentState;
        u8 mu8RemoveState;
    };

    inline Content::Content(Factory& lFactory, const ContentSpec& lContentSpec, u32 lu32DataSize)
        : Object(),
          mFactory(lFactory),
          mContentSpec(lContentSpec),
          mIdent(0),
          mpLoadService(0),
          mu32DataSize(lu32DataSize),
          mu16LoadCount(1),
          mu8ContentState(E_CONTENT_STATE_UNLOADED),
          mu8RemoveState(E_CONTENT_REMOVE_ALIVE)
    {
    }

    // ~Content() is defined OUT-OF-LINE in CgsContentDtor.cpp so the compiler emits
    // the single Content::~Content symbol the X360 vector-deleting destructor
    // (@0x82692A70) and the subclass dtors call. (Was inline; moved for Wave-6 item
    // 76 -- the load-count / ref-count asserts move with it. No layout change.)

    // Content::DoDispose is bodied OUT-OF-LINE in CgsObject.cpp (2026-08-25 wave 3;
    // was header-inline against the invented disposer trio above): snapshot the
    // owning factory, run the non-deleting virtual destructor, then hand the carve
    // back through the factory's environment allocator.

    inline bool Content::DoOnPostLoad()
    {
        return true;
    }
}
}

#endif
