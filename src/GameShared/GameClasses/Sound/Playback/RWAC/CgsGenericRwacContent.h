#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_CONTENT_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_CONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"      // CgsSound::Playback::Content
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"        // Slot implementation/factory surface
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h" // CgsResource::ResourcePtr<T>

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacContent.h
//
// Home for the two generic-RWAC Content instances that the X360 ARTIST build
// emitted with compiler-synthesized scalar-deleting destructors:
//
//   CgsSound::Playback::GenericRwacReverbIRContent  (dtor @ 0x826D7EE0)
//   CgsSound::Playback::GenericRwacWaveContent      (dtor @ 0x826C7A48)
//
// Both are `: public Content` (CgsContent.h) carrying a single
// `ContentLoader<ResType> mLoader` member (DWARF:
//   CgsGenericRwacReverbIRContent.h:64  ContentLoader<AlignedBinaryFileResource>
//   CgsGenericRwacWaveContent.h:70      ContentLoader<BinaryFileResource>).
//
// LAYOUT (X360-authoritative, proven by the two dtors):
//   Content base = 0x20 (32) bytes  [vptr, mu32RefCount, mFactory, mContentSpec,
//                                    mIdent, mpLoadService, mu32DataSize,
//                                    mu16LoadCount, mu8ContentState, mu8RemoveState]
//   mLoader      @ +0x20  -> ContentLoader<>::mpResource is its first member, so
//                            the embedded ResourcePtr's BaseResourcePtr sits at
//                            object +0x20. Its intrusive alias-list links land at
//                            +0x0C/+0x10 of the BaseResourcePtr == object
//                            +0x2C(44)/+0x30(48). The dtor asm reads exactly
//                            *(this+44)/*(this+48) and re-self-links them -- that
//                            is ~BaseResourcePtr() (@0x821F1E18), which the
//                            compiler runs when it destroys mLoader.mpResource.
//
// The scalar-deleting destructor each TU lists is the compiler synthesis:
//   ~GenericRwacXxxContent()  ->  ~ContentLoader (destroys mpResource ==
//   ~BaseResourcePtr unlink)  ->  ~Content()  ->  (a2&1) ? operator delete : noop.
// We define the class destructor out-of-line in the matching .cpp so the symbol
// is emitted; its body is empty because the member/base destructors do all the
// work the asm performs.
//
// ContentLoader<T> is HOMED MINIMALLY here (no committed home exists yet). Its
// member layout matches the DWARF (CgsContentLoader.h: mpResource, mpLoadData,
// meUnloadState); its load/unload/update method family is its own TU and is
// DECLARED-ONLY here. The element resource types (BinaryFileResource /
// AlignedBinaryFileResource -- the resource *payload* classes, NOT the committed
// *ResourceType classes) are not yet homed; ResourcePtr<T> adds no data members
// and these dtors touch only the BaseResourcePtr portion, so the payload types
// are forward-declared (incomplete is sufficient -- no T-sized member exists).
// ============================================================================

namespace CgsResource
{
    // Resource payload records (their complete homes are included by the loader TU).
    struct BinaryFileResource;
    struct AlignedBinaryFileResource;
}

namespace CgsSound
{
namespace Playback
{
    // CgsContentLoader.h:52 (DWARF). Drives load/unload of a resource module for a
    // Content. MINIMAL home: layout-faithful members only; the method family lives
    // in its own (not-yet-done) ContentLoader TU and is declared-only here.
    //
    // Layout (DWARF CgsContentLoader.h):
    //   mpResource     ResourcePtr<ResType>   (intrusive alias-list smart ptr)
    //   mpLoadData     ResourceModuleLoadData*
    //   meUnloadState  EUnloadState
    template <class ResType>
    struct ContentLoader
    {
        // CgsContentLoader.h:55.
        enum EUnloadState
        {
            E_US_IDLE = 0,
            E_US_PRE_UNLOAD = 1,
            E_US_UNLOADING = 2,
        };

        // CgsContentLoader.h:100.
        enum EResourceModuleLoadState
        {
            E_RMLS_REQUEST = 0,
            E_RMLS_LOADING = 1,
            E_RMLS_POST_LOAD = 2,
            E_RMLS_FINISHED = 3,
        };

        // CgsContentLoader.h:109. Per-load bookkeeping (opaque to these dtor TUs).
        struct ResourceModuleLoadData
        {
            EResourceModuleLoadState meState;      // :110
            s16 mi16CurrentRequest;                // :111
            u8  mu8IsCancelled;                    // :112
            u8  mu8SpareByte;                      // :113
        };

        ContentLoader() : mpLoadData(0), meUnloadState(E_US_IDLE) {}

        // Method family -- own TU (declared only; not exercised by the dtor TUs).
        void* GetData();
        bool  Load(Content& aContent, const ContentSpec& aSpec);    // :71
        bool  Unload(Content& aContent, const ContentSpec& aSpec);  // :76
        void  Update(Content& aContent, const ContentSpec& aSpec);  // :81

    private:
        // CgsContentLoader.h:88/93/98/118/122/126/130 (DWARF). The load/unload state machine
        // (bodied out-of-line as template members in CgsContentLoader.cpp). No storage change.
        void UpdateUnload(Content& arContent, const ContentSpec& arSpec);                 // :88
        void PurgeLoadData(Content& arContent, const ContentSpec& arSpec);                // :93
        void HandleResourceModuleLoading(Content& arContent, const ContentSpec& arSpec);  // :98
        bool StartResourceModuleLoading(Content& arContent, const ContentSpec& arSpec);   // :118
        void RestartResourceModuleLoading(Content& arContent, const ContentSpec& arSpec); // :122
        void UpdateResourceModuleLoading(Content& arContent, const ContentSpec& arSpec);  // :126
        void CancelResourceModuleLoading(Content& arContent, const ContentSpec& arSpec);  // :130

        CgsResource::ResourcePtr<ResType> mpResource;  // :132  (first member -> object +0x20)
        ResourceModuleLoadData*           mpLoadData;  // :133
        EUnloadState                      meUnloadState; // :134
    };

    // CgsGenericRwacReverbIRContent.h:40 (DWARF):
    //   GenericRwacReverbIRContent : public Content, member
    //   ContentLoader<CgsResource::AlignedBinaryFileResource> mLoader (:64).
    struct GenericRwacReverbIRContent : public Content
    {
        GenericRwacReverbIRContent(Factory& aFactory, const ContentSpec& aSpec, u32 au32Ident);
        virtual ~GenericRwacReverbIRContent();

    protected:
        virtual bool  DoLoad();          // (own TU)
        virtual bool  DoUnload();        // (own TU)
        virtual void  DoUpdate(f32 af32Dt); // (own TU)
        virtual void* DoGetData();       // CgsGenericRwacReverbIRContent.h:61 (own TU)

    private:
        ContentLoader<CgsResource::AlignedBinaryFileResource> mLoader; // :64  (object +0x20)
    };

    // CgsGenericRwacWaveContent.h:41 (DWARF):
    //   GenericRwacWaveContent : public Content, member
    //   ContentLoader<CgsResource::BinaryFileResource> mLoader (:70).
    struct GenericRwacWaveContent : public Content
    {
        GenericRwacWaveContent(Factory& aFactory, const ContentSpec& aSpec, u32 au32Ident);
        virtual ~GenericRwacWaveContent();

        bool GetStreamPath(char* apBuffer, size_t aBufferSize) const; // :94 (own TU)

    protected:
        virtual bool  DoLoad();          // (own TU)
        virtual bool  DoUnload();        // (own TU)
        virtual void  DoUpdate(f32 af32Dt); // (own TU)
        virtual void* DoGetData();       // CgsGenericRwacWaveContent.h:85 (own TU)

    private:
        ContentLoader<CgsResource::BinaryFileResource> mLoader; // :70  (object +0x20)
    };

    // CgsGenericRwacWaveContent.h:103 (DecFIGS). The ordinary wave-content
    // implementation mounted into authored GenericRwac player slots. ARTIST
    // @0x826C1EA8/1FE8/20A8 routes play/stop/completion through the RWAC plug-in
    // selected by Slot::mu16PluginOffset.
    struct GenericRwacContentSlot : public ISlotImplementation
    {
        static const Name SK_SLOT_CLASSNAME;

        virtual bool DoPlay(const Slot& arSlot, PlayerVoice& arVoice,
                            Content& arContent, u32 au32Param);
        virtual bool DoStop(const Slot& arSlot, PlayerVoice& arVoice,
                            Content& arContent);
        virtual bool DoUpdatePlaying(System* apSystem, const Slot& arSlot,
                                     PlayerVoice& arVoice, Content& arContent,
                                     f32 af32Dt);
    };

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_CONTENT_H
