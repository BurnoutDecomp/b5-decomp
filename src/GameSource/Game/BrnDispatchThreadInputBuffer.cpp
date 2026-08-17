#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"  // CgsModule::IOBufferStack (the manager's buffer home)

#include <cstring>   // std::memset (the opaque-payload clears in Construct)

// BrnGame::DispatchThreadInputBuffer -- out-of-line accessors/mutators the X360 build emitted for
// the particle/effects dispatch-thread input buffer. Each body tests the inherited IOBuffer status
// flag then acts on the named member:
//   - const getters assert the read-lock  ("Not locked for reading\n")  and return &member / value;
//   - mutable getters / Append / Set mutators assert the write-lock ("Not locked for writing\n").
// (The X360 rodata carries the trailing \n on both lock strings -- matched verbatim.)
//
// FLAG (benign parity gap): the X360 inlined a StrStream (BeginAssert/BasePriorityQueue::Clear/the
// stream operator/FireAssert/EndAssert) to compose each assert message; we deliberately use the
// committed CGS_ASSERT macro instead and do NOT reproduce that StrStream machinery. Parity YELLOW
// for the missing assert-stream calls is the expected/committed convention.
namespace BrnGame
{
    // ---- particle data ---------------------------------------------------------------

    // GetParticleData() const @ 0x8227F4F0 (DWARF h:97). Read-lock, return &mParticleData (+16).
    const BrnParticle::ParticleModule::DispatchThreadUpdateData* DispatchThreadInputBuffer::GetParticleData() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mParticleData;
    }

    // GetParticleData() @ 0x8227F598 (DWARF h:98, NON-const; IDA "GetP"). Write-lock, return
    // &mParticleData (+16).
    BrnParticle::ParticleModule::DispatchThreadUpdateData* DispatchThreadInputBuffer::GetParticleData()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mParticleData;
    }

    // GetParticleRenderData() @ 0x8227F6E8 (DWARF h:101, NON-const; IDA "GetParticl"). Write-lock,
    // return &mParticleRenderData (+0x38A0).
    BrnParticle::ParticleModule::ParticleRenderData* DispatchThreadInputBuffer::GetParticleRenderData()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mParticleRenderData;
    }

    // GetParticleRenderData() const @ 0x8227F640 (DWARF h:100; IDA leaves it as sub_8227F640).
    // Read-lock, return &mParticleRenderData (+0x38A0 == the `a1 + 14496` its body returns). Its
    // assert quotes source line 100, one below the write twin's 101, which is what identifies the
    // unnamed body as this overload.
    const BrnParticle::ParticleModule::ParticleRenderData* DispatchThreadInputBuffer::GetParticleRenderData() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mParticleRenderData;
    }

    // ---- crash-triangle cache --------------------------------------------------------

    // GetBufferCrashTriangleCache() const @ 0x8227F790 (DWARF h:103). Read-lock, &member (+0x3B00).
    const BrnEffects::BrnCrashTriangleCache* DispatchThreadInputBuffer::GetBufferCrashTriangleCache() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mBufferCrashTriangleCache;
    }

    // GetBufferCrashTriangleCache() @ 0x8227F838 (DWARF h:104, NON-const). Write-lock, &member.
    BrnEffects::BrnCrashTriangleCache* DispatchThreadInputBuffer::GetBufferCrashTriangleCache()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mBufferCrashTriangleCache;
    }

    // ---- particle inter-thread event queue -------------------------------------------

    // GetParticleInterThreadEventQueue() const @ 0x8227F8E0 (DWARF h:106; IDA "GetParticle").
    // Read-lock, return &mParticleInterThreadEventQueue (+0x5980).
    const DispatchThreadInputBuffer::CappedInterThreadEventQueue* DispatchThreadInputBuffer::GetParticleInterThreadEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mParticleInterThreadEventQueue;
    }

    // GetParticleInterThreadEventQueue() @ 0x8227F988 (DWARF h:107, NON-const; IDA
    // "GetParticleInterT"). Write-lock, return &mParticleInterThreadEventQueue (+0x5980).
    DispatchThreadInputBuffer::CappedInterThreadEventQueue* DispatchThreadInputBuffer::GetParticleInterThreadEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mParticleInterThreadEventQueue;
    }

    // AppendParticleInterThreadEventQueue @ 0x8228FDE8 (DWARF h:108). Write-lock, then bulk-append
    // the source inter-thread event queue's packed bytes into the embedded particle queue (+0x5980).
    // X360 forwards to CgsModule::VariableEventQueue<16384,16>::Append<16384,16>(source); the
    // committed template's Append<SRCBUF,SRCALIGN> deduces <16384,16> from the source type.
    void DispatchThreadInputBuffer::AppendParticleInterThreadEventQueue(const CappedInterThreadEventQueue* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mParticleInterThreadEventQueue.Append(*lpSource);
    }

    // ---- renderer flags / brightness / contrast --------------------------------------

    // SetRendererFlags @ 0x827BBDA0 (DWARF h:95). Write-lock, stw a2 at this+4 (mxRendererFlags).
    void DispatchThreadInputBuffer::SetRendererFlags(uint32_t xRendererFlags)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mxRendererFlags = xRendererFlags;
    }

    // SetBrightness @ 0x823B4280 (DWARF h:111). Write-lock, stw at +8 (miBrightness).
    void DispatchThreadInputBuffer::SetBrightness(int32_t liBrightness)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        miBrightness = liBrightness;
    }

    // GetBrightness() const @ 0x823FB970 (DWARF h:110). Read-lock, return miBrightness (+8).
    int32_t DispatchThreadInputBuffer::GetBrightness() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return miBrightness;
    }

    // SetContrast @ 0x823B4328 (DWARF h:114). Write-lock, stw at +12 (miContrast).
    void DispatchThreadInputBuffer::SetContrast(int32_t liContrast)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        miContrast = liContrast;
    }

    // GetContrast() const @ 0x823FBA18 (DWARF h:113). Read-lock, return miContrast (+12).
    int32_t DispatchThreadInputBuffer::GetContrast() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return miContrast;
    }

    // ---- calibration texture / post-fx / stall / disk-error --------------------------

    // SetCalibrationTextureHandle @ 0x823B4480 (DWARF h:120). Write-lock, 8-byte store at +0x9994
    // (mhCalibrationTextureHandle == ResourceHandle 8-byte opaque handle).
    void DispatchThreadInputBuffer::SetCalibrationTextureHandle(CgsResource::ResourceHandle lhCalibrationTextureHandle)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mhCalibrationTextureHandle = lhCalibrationTextureHandle;
    }

    // GetCalibrationTextureHandle() const @ 0x823FBB70 (DWARF h:119). Read-lock, return the 8-byte
    // handle (+0x9994) by value (both handle dwords copied into the sret slot).
    CgsResource::ResourceHandle DispatchThreadInputBuffer::GetCalibrationTextureHandle() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mhCalibrationTextureHandle;
    }

    // SetCalibrationUnfriendlyEnablePostFx @ 0x823B43D0 (DWARF h:117). Write-lock, stb at +0x99BB.
    void DispatchThreadInputBuffer::SetCalibrationUnfriendlyEnablePostFx(bool lbEnablePostFx)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbCalibrationUnfriendlyEnablePostFx = lbEnablePostFx;
    }

    // GetCalibrationUnfriendlyEnablePostFx() const @ 0x823FBAC0 (DWARF h:116). Read-lock, return
    // the byte at +0x99BB.
    bool DispatchThreadInputBuffer::GetCalibrationUnfriendlyEnablePostFx() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbCalibrationUnfriendlyEnablePostFx;
    }

    // SetIsStalled @ 0x823B4530 (DWARF h:123). Write-lock, stb a2 at +0x99BC (mbIsStalled).
    void DispatchThreadInputBuffer::SetIsStalled(bool bIsStalled)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsStalled = bIsStalled;
    }

    // GetIsStalled() const @ 0x823FBC30 (DWARF h:122). Read-lock, return the byte at +0x99BC.
    bool DispatchThreadInputBuffer::GetIsStalled() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsStalled;
    }

    // SetIsDiskError @ 0x823B45E0 (DWARF h:131). Write-lock, stb at +0x99BD (mbIsDiskError).
    void DispatchThreadInputBuffer::SetIsDiskError(bool lbIsDiskError)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsDiskError = lbIsDiskError;
    }

    // ---- snapshot request ------------------------------------------------------------

    // GetSnapShotRequest() @ 0x823B4690 (DWARF h:134, NON-const). Write-lock, return
    // &mSnapShotRequest (+0x999C).
    DispatchThreadInputBuffer::SnapShotRequest* DispatchThreadInputBuffer::GetSnapShotRequest()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mSnapShotRequest;
    }

    // ---- env-map face-render flags ---------------------------------------------------

    // SetEnvMapFaceRender (DWARF h:141, dwarfdump _compile/BrnWorldUnity.cpp:10926
    // `SetEnvMapFaceRender(uint32_t luIndex, bool lbRender)` -- see the rename note on the
    // declaration).
    // INLINED on the console: there is no standalone X360 body, and the ONE writer,
    // WorldModule::GenerateDispatchLists @0x827D1CE8, emits the byte store directly at
    // buffer + 39348 + face. 39348 == 0x99B4 == &mabEnvMapFaceRender[0], which is pinned
    // from the other side by Construct @0x823C5BB8: that body stores 0x99B0/0x99B1/0x99B2/
    // 0x99B3 (mbIsRenderingAtFullFrameRate + mabThreeThreadsOverBudget[3]) and then jumps
    // straight to 0x99BB, skipping 0x99B4..0x99B9 -- exactly the six bytes of this array.
    //
    // The write-lock assert is the sibling convention in this file, not an attested part
    // of this store (the console's inline sits inside GenerateDispatchLists' own
    // LockForWrite/UnlockForWrite bracket, so the invariant it checks is the console's).
    void DispatchThreadInputBuffer::SetEnvMapFaceRender( u32 luIndex, bool lbRender )
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(luIndex < 6, "luIndex < BrnGraphics::E_FACE_NUM");
        mabEnvMapFaceRender[ luIndex ] = lbRender;
    }

    // ---- lifecycle -------------------------------------------------------------------

    // Construct @ 0x823C5BB8. The X360 body, store-for-store by named member: the IOBuffer
    // base construct (status word), the renderer flags / particle-frame words, the
    // inter-thread queue construct (+0x5980), then the frame defaults -- full-frame-rate
    // TRUE, thread-over-budget flags clear, the snapshot take flag clear, post-fx TRUE,
    // the calibration handle cleared, disk-error clear, and the one-shot loading-screen
    // command back to NONE (this is the per-frame refill the manager's Swap drives, so a
    // posted command fires exactly once at the dispatch side).
    void DispatchThreadInputBuffer::Construct()
    {
        mxRendererFlags = 0;                              // +4
        CgsModule::IOBuffer::Construct();                 // *(+0) status = constructed
        std::memset(&mParticleData, 0, sizeof(mParticleData));   // +24 = 0 (the particle-frame word; the
                                                          // opaque model zeroes the whole payload)
        // ⚠ mParticleRenderData IS DELIBERATELY NOT CLEARED HERE, and that is the console
        // (BrnGame::DispatchThreadInputBuffer::Construct @0x823C5BB8 writes +4, +0, +24, the queue
        // at +22912 and then only the 39312..39357 tail -- nothing in the +0x38A0 payload). It is
        // safe there because BrnParticle::ParticleModule::GenerateRenderRequests @0x82281BD8 -- the
        // sole caller of the write-locked accessor -- fills it every frame before any reader runs.
        // ON PC IT IS NOT SAFE TO READ: that producer is not reconstructed, so nothing fills it,
        // and since the 2026-08-15 perf wave CreateIOBuffer<T> no longer zero-fills either -- so
        // its camera matrices and time step are UNINITIALISED bytes, not zeros. Anything that reads
        // them must gate on a producer having run; see the [FLAG PC bring-up] note on
        // BrnRendererUpdatePostFxMotionBlur, which is the only reader in this tree.
        mParticleInterThreadEventQueue.Construct();       // +0x5980
        mbIsRenderingAtFullFrameRate = true;              // +0x99B0 = 1
        mabThreeThreadsOverBudget[0] = false;             // +0x99B1..B3 = 0
        mabThreeThreadsOverBudget[1] = false;
        mabThreeThreadsOverBudget[2] = false;
        // ⚠ [FLAG PC deviation, reflections step 1 2026-08-17] THE CONSOLE DOES NOT WRITE
        // THESE SIX BYTES HERE. Construct @0x823C5BB8 stores +0x99B0..+0x99B3 and then
        // goes straight to +0x99AC / +0x99BB / +0x9994 / +0x9998 / +0x99BD / +0x9990 --
        // 0x99B4..0x99B9 (mabEnvMapFaceRender) is skipped, because on the console the
        // producer writes all six every dispatch (GenerateDispatchLists @0x827D1CE8, this
        // tree's BrnWorldModule.cpp:4018) and the IO-buffer pool memory started zeroed.
        // NEITHER holds on PC: the live producer is GenerateDispatchListsBringUp, whose
        // env-map arm parks until the face cameras have been positioned, and since the
        // 2026-08-15 perf wave CreateIOBuffer<T> no longer zero-fills -- so without this
        // the renderer's new six-face loop would read UNINITIALISED bytes and try to draw
        // faces whose mesh lists nobody filled. Seeded to the state the console's memory
        // always had. DELETE-WHEN the real GenerateDispatchLists is the live producer.
        for ( s32 liFace = 0; liFace < 6; ++liFace )
        {
            mabEnvMapFaceRender[ liFace ] = false;
        }
        std::memset(&mSnapShotRequest, 0, sizeof(mSnapShotRequest));  // the take flag (+0x99AC) = 0
        mbCalibrationUnfriendlyEnablePostFx = true;       // +0x99BB = 1
        mhCalibrationTextureHandle.Clear();               // +0x9994/+0x9998 = 0 (ResourceHandle::Clear)
        mbIsDiskError = false;                            // +0x99BD = 0
        meLoadingScreenCommand = E_LSC_NONE;              // +0x9990 = 0
    }

    // ---- the double-buffered manager -------------------------------------------------

    // DispatchThreadInputBufferManager::Construct @ 0x823CBCE0. Create the two buffers on
    // the supplied IO-buffer stack ("DispatchThreadIOBuffer0"/"1"), seed write = [0] /
    // read = [1] / index 0, Construct both, and stamp the write/read flags.
    void DispatchThreadInputBufferManager::Construct(CgsModule::IOBufferStack* lpIOBufferStack)
    {
        lpIOBufferStack->CreateIOBuffer<DispatchThreadInputBuffer>(&mapBuffers[0], "DispatchThreadIOBuffer0");
        lpIOBufferStack->CreateIOBuffer<DispatchThreadInputBuffer>(&mapBuffers[1], "DispatchThreadIOBuffer1");
        mpReadBuffer      = mapBuffers[1];
        mpWriteBuffer     = mapBuffers[0];
        muWriteBufferIndex = 0;
        mpReadBuffer->Construct();
        mpWriteBuffer->Construct();
        mpReadBuffer->SetIsWriteBuffer(false);            // *([1] + 39354) = 0
        mpWriteBuffer->SetIsWriteBuffer(true);            // *([0] + 39354) = 1
    }

    // Swap (DWARF h:354; the X360 inlines it in BrnGameModule::OnEndOfUpdateFrame
    // @0x823DBBA0): the buffer just written becomes the read buffer, the other becomes the
    // write buffer and is Destructed + re-Constructed (the base destruct then the derived
    // construct, exactly as the inline), then the write/read flags are restamped.
    void DispatchThreadInputBufferManager::Swap()
    {
        const u32 luWritten = muWriteBufferIndex;
        muWriteBufferIndex  = 1u - luWritten;
        mpReadBuffer  = mapBuffers[luWritten];
        mpWriteBuffer = mapBuffers[muWriteBufferIndex];
        mpWriteBuffer->CgsModule::IOBuffer::Destruct();
        mpWriteBuffer->Construct();
        mpWriteBuffer->SetIsWriteBuffer(true);            // *(write + 39354) = 1
        mpReadBuffer->SetIsWriteBuffer(false);            // *(read  + 39354) = 0
    }
}
