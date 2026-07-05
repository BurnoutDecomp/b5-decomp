#ifndef CGS_SOUND_PLAYBACK_CONTENT_H
#define CGS_SOUND_PLAYBACK_CONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h" // Entity (ContentSpec base)

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

    struct ContentDisposeRequest
    {
        Content* mpContent;
        u32 mauReserved[4];
    };

    class ContentDisposer
    {
    public:
        virtual int DisposeContent(ContentDisposeRequest* lpRequest);
    };

    class Factory
    {
    public:
        ContentDisposer* GetContentDisposer();
    };

    // CgsDataStructures.h:425 (DWARF). ContentSpec : public Entity. The serialised
    // content specification: a resolved ContentType* (+8), the packed load
    // method/time bytes, and an inline full-path buffer beginning at +0x10 (the
    // X360 GetPath()/GetPathZone read `this + 16`). Members pinned BY NAME +
    // SEQUENCE (host-width FLAG: the pointer member widens on the 64-bit host).
    struct ContentSpec : public Entity
    {
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

    class Object
    {
    public:
        Object() : mu32RefCount(0) {}
        virtual ~Object() {}
        virtual void DoDispose() {}

    protected:
        u32 mu32RefCount;
    };

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

        // Attach/unload hooks the detach path uses. FLAG (DEFER): declared-only --
        // bodied in their own Content TUs. OnAttach takes a load reference; DoUnload
        // performs the type-specific unload and returns success.
        void OnAttach(Voice& arVoice, Slot& arSlot);
        bool DoUnload();

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
        virtual bool DoOnPostLoad();

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

    inline void Content::DoDispose()
    {
        Factory& lFactory = mFactory;
        this->~Content();

        ContentDisposeRequest lRequest = {};
        lRequest.mpContent = this;
        // DWARF: Content::DoDispose() is void; the disposer call's result is not
        // propagated out of the override (return type corrected above).
        lFactory.GetContentDisposer()->DisposeContent(&lRequest);
    }

    inline bool Content::DoOnPostLoad()
    {
        return true;
    }
}
}

#endif
