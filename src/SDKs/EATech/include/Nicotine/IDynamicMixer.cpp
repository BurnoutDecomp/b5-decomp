#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"    // the mix master this drives
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"   // off_83250004 mixer allocator
#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp"  // the snapshot mixer this drives
#include <new>                                              // placement new
#include <cstring>

// ===========================================================================
//  Nicotine::IDynamicMixer -- the CgsSound -> NFS-mix seam. Reconstructed store-for-store
//  from BURNOUT_X360_ARTIST.XEX:
//    IDynamicMixer    @0x82B44A78    ~IDynamicMixer   @0x82B44DD0
//    GetInstance      @0x82B44AB8    CreateInstance   @0x82B44AD0
//    InitMap          @0x82B44C40    DestroyMap       @0x82B44D20
//    InitSnapshots    @0x82B44D68    ProcessMixMap    @0x82B44CA8
//    SetCameraState   @0x825487C0    SetSnapshot      @0x82B44DB8
//
//  FLAG (deferred, NOT homed): NFSLiveLink is a >6KB ARTIST-divergent object that is
//  not yet reconstructed. The X360 CreateInstance allocates 0x18BC (6332) bytes for it
//  and constructs NFSLiveLink(block, this); the dtor runs CgsSceneManager::CgsCollision::
//  BaseCollisionGenerator::Destruct on it (NFSLiveLink derives from that generator);
//  ProcessMixMap forwards to NFSLiveLink::ProcessLiveLink. Until NFSLiveLink is homed,
//  mLiveLink stays null, so every NFSLiveLink branch is a faithful no-op (the binary's
//  null-guard makes the same choice when the alloc fails). Each is FLAGGED at its site.
// ===========================================================================

namespace Nicotine
{

// dword_8324FFFC -- the process-wide IDynamicMixer instance pointer. The ctor installs
// `this` here (the only writer in the ARTIST asm); GetInstance reads it back.
IDynamicMixer* g_pDynamicMixer = 0;

// ---------------------------------------------------------------------------
// IDynamicMixer::IDynamicMixer @0x82B44A78 -- zero-init + install the global instance.
//   vptr = off_82147814;                          (stw r10, 0(r3) -- the C++ vtable)
//   miCamState     = -1;   (+0x04, li r9,-1)
//   mMixMaster     = 0;    (+0x08)   mLiveLink      = 0; (+0x0C)
//   mCreateParams  = {0,0,0}; (+0x10/+0x14/+0x18)  mpSnapshot = 0; (+0x1C)
//   dword_8324FFFC = this;
// ---------------------------------------------------------------------------
IDynamicMixer::IDynamicMixer()
    : miCamState(-1)        // +0x04
    , mMixMaster(0)         // +0x08
    , mLiveLink(0)          // +0x0C
    , mpSnapshot(0)         // +0x1C
{
    mCreateParams.mPrintSelect   = 0; // +0x10
    mCreateParams.NumMixStates   = 0; // +0x14
    mCreateParams.MixerAllocator = 0; // +0x18

    g_pDynamicMixer = this;           // dword_8324FFFC
}

// ---------------------------------------------------------------------------
// IDynamicMixer::~IDynamicMixer @0x82B44DD0 -- vtable slot 0. Tear down the sub-objects,
// freeing each through the mixer allocator (off_83250004 vtable+0xC == Free(ptr, 0)).
//   if (mMixMaster) { ~NFSMixMaster(mMixMaster); allocator->Free(mMixMaster, 0); }
//   if (mpSnapshot) { allocator->Free(mpSnapshot, 0); }       (no SnapshotMixer dtor)
//   if (mLiveLink)  { BaseCollisionGenerator::Destruct(mLiveLink); Free(mLiveLink, 0); }
// (the X360 also rewrites the vptr to off_82147814 before teardown -- the C++ compiler
//  emits that as part of the dtor epilogue, so it is implicit here.)
// ---------------------------------------------------------------------------
IDynamicMixer::~IDynamicMixer()
{
    if (mMixMaster)                       // +0x08
    {
        mMixMaster->~NFSMixMaster();
        g_pMixerAllocator->Free(mMixMaster, 0);
    }

    if (mpSnapshot)                       // +0x1C -- freed raw (no dtor in the asm)
        g_pMixerAllocator->Free(mpSnapshot, 0);

    // FLAG (deferred): the X360 here runs
    //   if (mLiveLink) { CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct(mLiveLink);
    //                    g_pMixerAllocator->Free(mLiveLink, 0); }
    // NFSLiveLink is not yet homed, so mLiveLink is always null and this branch is a
    // faithful no-op. Wired in once NFSLiveLink lands.
}

// ---------------------------------------------------------------------------
// IDynamicMixer::GetInstance @0x82B44AB8 -- return the global instance pointer.
// ---------------------------------------------------------------------------
IDynamicMixer* IDynamicMixer::GetInstance()
{
    return g_pDynamicMixer;               // dword_8324FFFC
}

// ---------------------------------------------------------------------------
// IDynamicMixer::CreateInstance @0x82B44AD0 -- install the allocator + build the
// sub-objects through it.
//   off_83250004 = lpParams->MixerAllocator;                  (a2+8 -> the mixer global)
//   mMixMaster   = allocator->Allocate(128, "NFSMixMaster")
//                  ? new(block) NFSMixMaster : 0;             (+0x08)
//   mMixMaster->mNumStates = lpParams->NumMixStates;          (*(v4+8) = *(a2+4))
//   block        = allocator->Allocate(32, "SnapshotMixer");
//   if (block) zero its 8 words;                              (the do-while, 8 x stw 0)
//   mpSnapshot   = block;                                     (+0x1C)
//   mpSnapshot->Construct(this);
//   mLiveLink    = allocator->Allocate(6332, "NFSLiveLink")
//                  ? NFSLiveLink(block, this) : 0;            (+0x0C)  [FLAG: deferred]
//
// FLAG (PC boundary): the X360 Allocate is the mixer-system vtable slot 2
// (size, debug-name, 0); the project MixerAllocator matches that shape (alignment arg
// added). Sizes 128/32/6332 are the exact X360 immediates (0x80/0x20/0x18BC).
// ---------------------------------------------------------------------------
void IDynamicMixer::CreateInstance(const MAP_CREATE_PARAMS* lpParams)
{
    // off_83250004 = the create-params' allocator (asm stores it UNCONDITIONALLY).
    g_pMixerAllocator = lpParams->MixerAllocator;

    // --- NFSMixMaster (0x80 == 128 bytes on X360) ---
    // The asm GUARDS the construction (`Allocate ? new(block) NFSMixMaster : 0`) --
    // an earlier revision dropped the ternary and placement-new'd unconditionally
    // (UB on a null block); restored 2026-08-25 (wave 1). The mNumStates store is
    // unconditional in the asm (a null master would crash there on console too);
    // kept under the same guard as the sane host equivalent of that crash path.
    // FLAG PC-platform leaf: allocate the native object size. Its runtime pointer
    // members widen on x64; carving ARTIST's 0x80 bytes corrupts the host heap.
    void* lpMasterMem = g_pMixerAllocator->Allocate(
        static_cast<unsigned int>(sizeof(NFSMixMaster)), 16, "NFSMixMaster");
    mMixMaster = lpMasterMem ? new (lpMasterMem) NFSMixMaster() : 0;
    if (mMixMaster)
        mMixMaster->mNumStates = lpParams->NumMixStates; // *(v4+8) = *(a2+4)

    // --- SnapshotMixer (0x20 == 32 bytes on X360) ---
    // FLAG PC-platform leaf: like NFSMixMaster, its pointers widen on x64.
    void* lpSnapMem = g_pMixerAllocator->Allocate(
        static_cast<unsigned int>(sizeof(SnapshotMixer)), 16, "SnapshotMixer");
    if (lpSnapMem)
        std::memset(lpSnapMem, 0, sizeof(SnapshotMixer));
    mpSnapshot = static_cast<SnapshotMixer*>(lpSnapMem);
    if (mpSnapshot)
        mpSnapshot->Construct(this);     // owner = this

    // --- NFSLiveLink (0x18BC == 6332 bytes) ---
    // FLAG (deferred): the X360 does
    //   void* m = allocator->Allocate(6332, "NFSLiveLink");
    //   mLiveLink = m ? new(m) NFSLiveLink(this) : 0;
    // NFSLiveLink is not yet homed; mLiveLink stays null (the same result the binary
    // produces on a failed alloc). Wired in once NFSLiveLink lands.
    mLiveLink = 0;
}

// ---------------------------------------------------------------------------
// IDynamicMixer::InitMap @0x82B44C40 -- load a MixMap blob into the mix master.
//   if (mMixMaster) {
//       mMixMaster->CreateMainMainMap(lpMapData, 0);
//       mMixMaster->AssignSFXCallbacks(this);
//       mMixMaster->InitMixMap();
//       if (mpSnapshot) mpSnapshot->AttachToMixMap(mMixMaster->m_pMainMixMap);
//   }
// ---------------------------------------------------------------------------
void IDynamicMixer::InitMap(int* lpMapData)
{
    if (mMixMaster)
    {
        if (!mMixMaster->CreateMainMainMap(lpMapData, 0))
            return;
        mMixMaster->AssignSFXCallbacks(this);        // bind the SFX-callback owner
        mMixMaster->InitMixMap();                    // finish map setup
        if (mpSnapshot)
            mpSnapshot->AttachToMixMap(mMixMaster->m_pMainMixMap);
    }
}

// ---------------------------------------------------------------------------
// IDynamicMixer::DestroyMap @0x82B44D20 -- tear the loaded map / snapshots back down.
//   if (mMixMaster) mMixMaster->DestroyMainMainMap();
//   if (mpSnapshot) mpSnapshot->DestroySnapshots();
// ---------------------------------------------------------------------------
void IDynamicMixer::DestroyMap()
{
    if (mMixMaster)
        mMixMaster->DestroyMainMainMap();
    if (mpSnapshot)
        mpSnapshot->DestroySnapshots();
}

// ---------------------------------------------------------------------------
// IDynamicMixer::InitSnapshots @0x82B44D68 -- (re)build snapshots, re-attach to the map.
//   if (mpSnapshot) {
//       mpSnapshot->InitSnapshots();
//       if (mMixMaster) mpSnapshot->AttachToMixMap(mMixMaster->m_pMainMixMap);
//   }
// ---------------------------------------------------------------------------
void IDynamicMixer::InitSnapshots(void* apSnapshotData)
{
    if (mpSnapshot)
    {
        mpSnapshot->InitSnapshots(static_cast<SnapshotHeader*>(apSnapshotData));
        if (mMixMaster)
            mpSnapshot->AttachToMixMap(mMixMaster->m_pMainMixMap); // *v3 == m_pMainMixMap
    }
}

// ---------------------------------------------------------------------------
// IDynamicMixer::ProcessMixMap @0x82B44CA8 -- per-frame drive.
//   if (mLiveLink)  NFSLiveLink::ProcessLiveLink(mLiveLink, mpSnapshot);   [FLAG: deferred]
//   if (mpSnapshot) mpSnapshot->Update(dt);
//   if (mMixMaster) mMixMaster->ProcessMixMap(dt, miCamState);
// ---------------------------------------------------------------------------
void IDynamicMixer::ProcessMixMap(int /*liUnused*/, float lfDeltaTime)
{
    // FLAG (deferred): NFSLiveLink::ProcessLiveLink(mLiveLink, mpSnapshot) -- mLiveLink
    // is null until NFSLiveLink is homed, so this branch is a faithful no-op.
    if (mpSnapshot)
        mpSnapshot->Update(lfDeltaTime);                    // FLAG: DSP rodata-blocked
    if (mMixMaster)
        mMixMaster->ProcessMixMap(lfDeltaTime, miCamState); // -> NFSMixMap::ProcessMixMap
}

// ---------------------------------------------------------------------------
// IDynamicMixer::SetCameraState @0x825487C0 -- store the camera/mix state; return this.
//   *(this+4) = liState; return this;   (stw r4,4(r3); blr)
// ---------------------------------------------------------------------------
IDynamicMixer* IDynamicMixer::SetCameraState(int liState)
{
    miCamState = liState;   // +0x04
    return this;            // X360 returns r3 unchanged
}

// ---------------------------------------------------------------------------
// IDynamicMixer::SetSnapshot @0x82B44DB8 -- apply the current snapshot if one exists.
//   if (mpSnapshot) mpSnapshot->SetSnapshot();   (lwz +0x1C; beqlr; tail-call)
// ---------------------------------------------------------------------------
void IDynamicMixer::SetSnapshot(int aiSnapshot, bool abActive)
{
    if (mpSnapshot)
        mpSnapshot->SetSnapshot(aiSnapshot, abActive);
}

} // namespace Nicotine
