#ifndef CGS_SOUND_PLAYBACK_CONTENT_H
#define CGS_SOUND_PLAYBACK_CONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{
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

    struct ContentSpec
    {
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

    struct Content : public Object
    {
        Content(Factory& lFactory, const ContentSpec& lContentSpec, u32 lu32DataSize);
        virtual ~Content();

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

    inline Content::~Content()
    {
        CGS_ASSERT(mu16LoadCount == 1, "Destroying content while it's still loaded. This is a Bad thing.");

        CGS_ASSERT(mu32RefCount == 0, "0 == mu32RefCount");
    }

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
