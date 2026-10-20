#pragma once
#include <new>

// Typed fixture boundaries for four extracted production bodies. The real Apt VM,
// async parser, TLS and shared-pointer primitives are outside this ownership test.
// The native resource header and PC BundleLoader/ownership code stay production code.
namespace AptBoundaryFixture
{
    inline CgsResource::Pool* mpPool = nullptr;
    inline CgsGui::AptDataHeader* mpHeader = nullptr;
    inline std::vector<char> mCalls;
    inline bool mbWatch = false;
    inline bool mbPinnedAtCompletion = false;
    inline bool mbActiveAtUnresolve = false;
    inline bool mbLoadedAtFree = false;
    inline bool mbLoadedAtRetirement = false;
    inline bool mbHeaderFieldsPreserved = false;
    inline bool mbCompletionArguments = false;
    inline int mnCompletions = 0;
    inline int mnDeletes = 0;
    inline intptr_t mnResolveContext = 0;
    inline void* mpRoot = nullptr;
    inline uintptr_t muSpanBaseAtCompletion = 0;
    inline uint32_t muSpanSizeAtCompletion = 0;
    inline int References(u64 luId)
    {
        CgsResource::ID lId;
        lId.SetHash(luId);
        const s32 liSlot = mpPool->FindResourceIndex(lId, true, 3);
        return liSlot < 0 ? 0 : mpPool->GetEntryRefCount(liSlot);
    }
    inline void Retiring(void* lpResource)
    {
        if (!mbWatch || lpResource != mpHeader) return;
        mCalls.push_back('R');
        mbLoadedAtRetirement = mpHeader->meCurrentState == CgsGui::AptDataHeader::E_APTDATASTATE_LOADED;
        mbHeaderFieldsPreserved = mpHeader->mpAptData == 64 && mpHeader->mpConstData == 128
            && mpHeader->muSizeOfHeader == 256;
    }
}

struct AptCharacterAnimation
{
    void* Unresolve(intptr_t lnContext)
    {
        using namespace AptBoundaryFixture;
        mCalls.push_back('U');
        mnResolveContext = lnContext;
        mbActiveAtUnresolve = this == static_cast<void*>(static_cast<char*>(mpRoot) + 0x20)
            && References(781) == 1
            && mpHeader->meCurrentState == CgsGui::AptDataHeader::E_APTDATASTATE_ACTIVE;
        return this;
    }
};
inline constexpr unsigned int KU_AptEmbeddedMovieOff = 0x20;
struct FixtureAptFileName { const char* GetBuffer() const { return "callback"; } };
struct AptFile
{
    int32_t mnRefCount = 2; // one external owner and the incoming callback handle
    FixtureAptFileName mFileName;
    int32_t mnState = 2;
    int32_t mnField12 = 1;
    void* mpResolveContext = nullptr;
    void* mpData = nullptr;
    void* mpDataBlock = nullptr;
    ~AptFile(); // exact production destructor is extracted by the runner
};
inline int AptSharedPtrIncRef(AptFile* lpFile) { return ++lpFile->mnRefCount; }
inline int AptSharedPtrDecRef(AptFile* lpFile) { return --lpFile->mnRefCount; }
inline void AptSharedPtrDelete(AptFile* lpFile)
{
    ++AptBoundaryFixture::mnDeletes;
    delete lpFile;
}
template<class T> struct AptSharedPtr
{
    T* pData;
    static void Dispose(T* lpFile)
    {
        if (lpFile && AptSharedPtrDecRef(lpFile) == 0) AptSharedPtrDelete(lpFile);
    }
};
using AptFilePtr = AptSharedPtr<AptFile>;
struct AptConstFile {};
struct AptLoader
{
    void Invalidate(AptFile*) { AptBoundaryFixture::mCalls.push_back('I'); }
};
struct AptTarget
{
    AptLoader* GetLoader() { static AptLoader loader; return &loader; }
};
inline AptTarget* GetTarget() { static AptTarget target; return &target; }
inline void AptLifeLog(const char*, const char*, const AptFile*) {}
inline void (*gpAptFreeAnimationHook)(void*) = nullptr;

namespace CgsGui
{
    struct AptDataHandler
    {
        AptDataHeader* FindAptData(const char*) { return AptBoundaryFixture::mpHeader; }
    };
    struct AptAux { AptDataHandler mAptDataHandler; };
    struct AptAuxPointer { static AptAux* mpAptAuxInst; };
    inline AptAux gFixtureAptAux;
    inline AptAux* AptAuxPointer::mpAptAuxInst = &gFixtureAptAux;
    struct AptCallbackFile
    {
        static void LoadAnimation(const char*, AptFilePtr*);
        static void FreeAnimation(void*);
    };
    inline uintptr_t gAptResourceSpanBase = 0;
    inline uint32_t gAptResourceSpanSize = 0;
}
inline void AptCompleteAnimationAsyncLoad(AptFilePtr* lpHandle, void* lpBase,
                                         AptConstFile* lpConst, void* lpHeader)
{
    using namespace AptBoundaryFixture;
    ++mnCompletions;
    muSpanBaseAtCompletion = CgsGui::gAptResourceSpanBase;
    muSpanSizeAtCompletion = CgsGui::gAptResourceSpanSize;
    mbPinnedAtCompletion = References(781) == 2 && References(782) == 2;
    mbCompletionArguments = lpHeader == mpHeader
        && lpBase == static_cast<void*>(reinterpret_cast<char*>(mpHeader) + 64)
        && lpConst == reinterpret_cast<AptConstFile*>(reinterpret_cast<char*>(mpHeader) + 128);
    mpRoot = reinterpret_cast<char*>(mpHeader) + 160;
    ::new (static_cast<void*>(static_cast<char*>(mpRoot) + 0x20)) AptCharacterAnimation;
    lpHandle->pData->mpResolveContext = lpBase;
    lpHandle->pData->mpDataBlock = lpHeader;
    lpHandle->pData->mpData = mpRoot;
    lpHandle->pData->mnState = 3;
    AptSharedPtr<AptFile>::Dispose(lpHandle->pData);
    lpHandle->pData = nullptr;
}
inline void AptFixtureFreeHook(void* lpHeader)
{
    using namespace AptBoundaryFixture;
    mCalls.push_back('F');
    CgsGui::AptCallbackFile::FreeAnimation(lpHeader); // extracted production body
    mbLoadedAtFree = static_cast<CgsGui::AptDataHeader*>(lpHeader)->meCurrentState
        == CgsGui::AptDataHeader::E_APTDATASTATE_LOADED;
}
