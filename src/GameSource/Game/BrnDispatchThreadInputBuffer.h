#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer base + IsBufferLockedForReading()
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<N,16> (inter-thread queue Append)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle (calibration texture handle, by value)
#include "GameSource/Game/BrnLoadingScreenRenderer.h"    // BrnGame::ELoadingScreenCommand (meLoadingScreenCommand)
#include "BrnCommonTypes.h"                              // Matrix44 (rw::math::vpu::Matrix44, the occlusion view-projection)

namespace CgsModule { struct IOBufferStack; }            // the manager Construct's stack (pointer-only here)

// BrnGame::DispatchThreadInputBuffer -- the per-frame input payload the particle/effects dispatch
// thread reads. It derives the shared CgsModule::IOBuffer (status-flag-guarded read/write locking)
// and carries the particle-update data the simulation produced, plus brightness/contrast/renderer
// flags and the render data, crash-triangle cache, inter-thread event queue, calibration/loading
// state, and the snapshot request.
//
// Layout + member order recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Game/BrnDispatchThreadInputBuffer.h): struct
// BrnGame::DispatchThreadInputBuffer : public IOBuffer, with the members being (in DWARF order)
// mxRendererFlags (h:191), miBrightness (h:192), miContrast (h:193), mParticleData (h:194),
// mParticleRenderData (h:195), mBufferCrashTriangleCache (h:196), the inter-thread event queue
// (h:197), the calibration-texture handle / flags / loading command, and mSnapShotRequest.
//
// FLAG (semantic parity, NOT byte-exact full object): the render-payload member TYPES that have no
// committed home yet (DispatchThreadUpdateData, ParticleRenderData, BrnCrashTriangleCache,
// CappedInterThreadEventQueue, SnapShotRequest) are modelled here as opaque/minimal complete types
// -- their true internal size/layout is UNKNOWN. They are embedded BY VALUE so that &member yields
// the correct pointer type for the accessor return (the accessor bodies return &member / forward to
// a member method). The X360 32-bit byte offsets (this+16 mParticleData, this+0x38A0
// mParticleRenderData, this+0x3B00 mBufferCrashTriangleCache, this+0x5980 mParticleInterThreadEvent
// Queue, this+0x9994 mhCalibrationTextureHandle, this+0x999C mSnapShotRequest, ...) are documented
// for reference only; the host x64 layout differs (pointers/queues widen) so parity is BY NAMED
// MEMBER, per the x64-gate rule. When each payload's canonical home lands this file includes it and
// drops the local opaque model (do NOT fork the type).
namespace BrnParticle
{
namespace ParticleModule
{
    // FLAG (ad-hoc opaque): BrnParticle::ParticleModule::DispatchThreadUpdateData is NOT committed
    // anywhere yet. It is modelled here as a minimal opaque struct ONLY so it can be a complete
    // member type by value (so mParticleData has a real address and &mParticleData yields the
    // correct pointer type for GetParticleData's return). The real size/layout is UNKNOWN -- the
    // 1-byte placeholder storage is a stand-in, NOT the true size. When DispatchThreadUpdateData is
    // reconstructed in its own home this definition is replaced (and this file includes it instead).
    struct DispatchThreadUpdateData
    {
        u8 maStorage[1];   // FLAG: opaque, true size unknown
    };

    // FLAG (ad-hoc opaque): ParticleRenderData has no committed home yet (DWARF h:195). Same
    // by-value opaque model as DispatchThreadUpdateData so &mParticleRenderData yields the correct
    // pointer type. True size/layout UNKNOWN; replaced when its canonical home lands.
    struct ParticleRenderData
    {
        u8 maStorage[1];   // FLAG: opaque, true size unknown
    };
}
}

namespace BrnEffects
{
    // FLAG (ad-hoc opaque): BrnEffects::BrnCrashTriangleCache is bodied only in
    // GameSource/Effects/BrnCrashTriangleCache.cpp (no includable committed struct home yet, DWARF
    // h:196). Modelled here as a by-value opaque so &mBufferCrashTriangleCache yields the correct
    // pointer type for GetBufferCrashTriangleCache's const/non-const overloads. True size UNKNOWN;
    // replaced when BrnCrashTriangleCache gets a committed header home.
    struct BrnCrashTriangleCache
    {
        u8 maStorage[1];   // FLAG: opaque, true size unknown
    };
}

namespace BrnGame
{
    // DWARF h:68 -- struct BrnGame::DispatchThreadInputBuffer : public IOBuffer (IOBuffer here is
    // CgsModule::IOBuffer, the committed shared base).
    struct DispatchThreadInputBuffer : public CgsModule::IOBuffer
    {
        // The particle dispatch thread's inter-thread event queue (DWARF h:197). The X360
        // AppendParticleInterThreadEventQueue forwards to
        // CgsModule::VariableEventQueue<16384,16>::Append<16384,16>(source). Reused by name; the
        // template's Append<SRCBUF,SRCALIGN> deduces <16384,16> from the source type.
        typedef CgsModule::VariableEventQueue<16384, 16> CappedInterThreadEventQueue;

        // FLAG (ad-hoc opaque): SnapShotRequest has no committed home yet. Modelled as a by-value
        // opaque so &mSnapShotRequest yields the correct pointer type for GetSnapShotRequest. True
        // size/layout UNKNOWN; replaced when its canonical home lands.
        struct SnapShotRequest
        {
            u8 maStorage[1];   // FLAG: opaque, true size unknown
        };

        // ---- particle data ---------------------------------------------------------------
        // GetParticleData @ 0x8227F4F0 / DWARF h:97 (const) -- assert read-lock, hand back the
        // embedded particle-update data; @ 0x8227F598 / DWARF h:98 (non-const) asserts write-lock.
        const BrnParticle::ParticleModule::DispatchThreadUpdateData* GetParticleData() const;   // 0x8227F4F0
        BrnParticle::ParticleModule::DispatchThreadUpdateData*       GetParticleData();          // 0x8227F598

        // GetParticleRenderData @ 0x8227F6E8 / DWARF h:101 (non-const) -- write-lock, &member.
        BrnParticle::ParticleModule::ParticleRenderData* GetParticleRenderData();               // 0x8227F6E8

        // ---- crash-triangle cache --------------------------------------------------------
        const BrnEffects::BrnCrashTriangleCache* GetBufferCrashTriangleCache() const;           // 0x8227F790 (h:103, R)
        BrnEffects::BrnCrashTriangleCache*       GetBufferCrashTriangleCache();                 // 0x8227F838 (h:104, W)

        // ---- particle inter-thread event queue -------------------------------------------
        const CappedInterThreadEventQueue* GetParticleInterThreadEventQueue() const;            // 0x8227F8E0 (h:106, R)
        CappedInterThreadEventQueue*       GetParticleInterThreadEventQueue();                  // 0x8227F988 (h:107, W)
        void AppendParticleInterThreadEventQueue(const CappedInterThreadEventQueue* lpSource);  // 0x8228FDE8 (h:108, W)

        // ---- renderer flags / brightness / contrast --------------------------------------
        void    SetRendererFlags(uint32_t xRendererFlags);                                      // 0x827BBDA0 (h:95, W)
        void    SetBrightness(int32_t liBrightness);                                            // 0x823B4280 (h:111, W)
        int32_t GetBrightness() const;                                                          // 0x823FB970 (h:110, R)
        void    SetContrast(int32_t liContrast);                                                // 0x823B4328 (h:114, W)
        int32_t GetContrast() const;                                                            // 0x823FBA18 (h:113, R)

        // ---- calibration texture / post-fx / stall / disk-error --------------------------
        void                          SetCalibrationTextureHandle(CgsResource::ResourceHandle lhCalibrationTextureHandle); // 0x823B4480 (h:120, W)
        CgsResource::ResourceHandle   GetCalibrationTextureHandle() const;                      // 0x823FBB70 (h:119, R)
        void SetCalibrationUnfriendlyEnablePostFx(bool lbEnablePostFx);                         // 0x823B43D0 (h:117, W)
        bool GetCalibrationUnfriendlyEnablePostFx() const;                                      // 0x823FBAC0 (h:116, R)
        void SetIsStalled(bool bIsStalled);                                                     // 0x823B4530 (h:123, W)
        bool GetIsStalled() const;                                                              // 0x823FBC30 (h:122, R)
        void SetIsDiskError(bool lbIsDiskError);                                                // 0x823B45E0 (h:131, W)

        // ---- snapshot request ------------------------------------------------------------
        SnapShotRequest* GetSnapShotRequest();                                                  // 0x823B4690 (h:134, W)

        // ---- loading-screen command --------------------------------------------------------
        // DWARF h:166..h:188. The five setters are the commands BridgeGuiToGame @0x823CB758
        // writes into this buffer on the GUI-out events (19/20/138/589/590); the X360 inlines
        // them as the raw meLoadingScreenCommand (+0x9990) stores visible in that body, and
        // BrnRendererModule::Render @0x8240BFA8 forwards GetLoadingScreenCommand() into
        // LoadingScreenRenderer::AddCommand each frame (`lwzx r4, r26, 0x9990` at 0x8240C17C).
        // A posted command fires exactly once at the dispatch side: the manager's Swap
        // (inlined in BrnGameModule::OnEndOfUpdateFrame @0x823DBBA0) Destructs and
        // re-Constructs the new write buffer every frame, and Construct @0x823C5BB8
        // zeroes meLoadingScreenCommand -- so the slot never persists across frames
        // (a persisting BLACKFADEIN would re-arm the overlay fade at 1.2 forever).
        void ShowLoadingScreen()           { meLoadingScreenCommand = E_LSC_SHOW; }             // h:166
        void ShowLoadingScreenSaveLoadBG() { meLoadingScreenCommand = E_LSC_SHOWSAVELOADBG; }   // h:169
        void HideLoadingScreen()           { meLoadingScreenCommand = E_LSC_HIDE; }             // h:172
        void BlackOverlayFadeIn()          { meLoadingScreenCommand = E_LSC_BLACKFADEIN; }      // h:175
        void BlackOverlayFadeOut()         { meLoadingScreenCommand = E_LSC_BLACKFADEOUT; }     // h:178
        ELoadingScreenCommand GetLoadingScreenCommand() const { return meLoadingScreenCommand; } // h:188

        // ---- lifecycle ---------------------------------------------------------------------
        // Construct @ 0x823C5BB8 -- base construct, queue construct, then the named-member
        // defaults (the per-frame write-buffer refill rides this via the manager's Swap).
        void Construct();

        // Manager::Construct sets the write/read mbIsWriteBuffer flags after Construct.
        void SetIsWriteBuffer(bool lbIsWriteBuffer) { mbIsWriteBuffer = lbIsWriteBuffer; }      // h:88/92 pair

        // ---- full-frame-rate flag ----------------------------------------------------------
        // DWARF h:181/h:185; X360 inlined (DoDispatch stores +0x99B0 from the camera flags,
        // Render reads it for the present-interval select).
        bool GetIsRenderingAtFullFrameRate() const   { return mbIsRenderingAtFullFrameRate; }   // h:181
        void SetIsRenderingAtFullFrameRate(bool lbFullRate) { mbIsRenderingAtFullFrameRate = lbFullRate; } // h:185

    private:
        // Members in DWARF declaration order. With the committed CgsModule::IOBuffer base (a single
        // 1-byte FlagSet8 status field, padded to 4), mxRendererFlags lands at +4, miBrightness at
        // +8, miContrast at +12, and mParticleData at +16 -- matching the asm's `return a1 + 16`
        // (= &mParticleData). FLAG: semantic parity; the trailing X360 offsets (h:195 +0x38A0,
        // h:196 +0x3B00, h:197 +0x5980, +0x9994.., +0x999C) are implied by the (unknown) member
        // sizes, not asserted byte-exactly here (the opaque models are NOT the true sizes).
        uint32_t mxRendererFlags;                                       // DWARF h:191  (X360 +4)
        int32_t  miBrightness;                                          // DWARF h:192  (X360 +8)
        int32_t  miContrast;                                            // DWARF h:193  (X360 +12)
        BrnParticle::ParticleModule::DispatchThreadUpdateData mParticleData;             // DWARF h:194  (X360 +16)
        BrnParticle::ParticleModule::ParticleRenderData       mParticleRenderData;       // DWARF h:195  (X360 +0x38A0)
        BrnEffects::BrnCrashTriangleCache                     mBufferCrashTriangleCache; // DWARF h:196  (X360 +0x3B00)
        CappedInterThreadEventQueue                           mParticleInterThreadEventQueue; // DWARF h:197  (X360 +0x5980)
        ELoadingScreenCommand       meLoadingScreenCommand;             // DWARF h:198  (X360 +0x9990 == 39312, the BridgeGuiToGame slot)
        CgsResource::ResourceHandle mhCalibrationTextureHandle;         // DWARF h:204  (X360 +0x9994, 8 bytes)
        SnapShotRequest             mSnapShotRequest;                   // DWARF h:205  (X360 +0x999C)
        bool mbIsRenderingAtFullFrameRate;                              // DWARF h:207  (X360 +0x99B0 == 39344, the present-interval select)
        bool mabThreeThreadsOverBudget[3];                              // DWARF h:208  (the three thread-monitor squares)
        bool mabEnvMapFaceRender[6];                                    // DWARF h:209
        bool mbIsWriteBuffer;                                           // DWARF h:210  (X360 +0x99BA == 39354, the IsReadBuffer assert)
        bool mbCalibrationUnfriendlyEnablePostFx;                       // DWARF h:211  (X360 +0x99BB)
        bool mbIsStalled;                                               // DWARF h:212  (X360 +0x99BC)
        bool mbIsDiskError;                                             // DWARF h:213  (X360 +0x99BD)
        Matrix44 mOcclusionViewProjectionMatrix;                        // DWARF h:215  (X360 +0x99C0 == 39360, the Render vector loads)
    };

    // BrnGame::DispatchThreadInputBufferManager (DWARF h:341) -- the double-buffered pair the
    // game module owns (BrnGameModule.h:531, X360 module +10097064): the update side fills
    // mpWriteBuffer, the dispatch/render side reads mpReadBuffer, and the end-of-frame Swap
    // (inlined in BrnGameModule::OnEndOfUpdateFrame @0x823DBBA0) rotates them -- the buffer
    // just written becomes the read buffer, and the other is Destructed + re-Constructed
    // (the per-frame refill that clears the one-shot loading-screen command).
    struct DispatchThreadInputBufferManager
    {
        void Construct(CgsModule::IOBufferStack* lpIOBufferStack);      // 0x823CBCE0 (h:346)
        void Swap();                                                    // h:354 (X360 inlined @0x823DBBA0)
        DispatchThreadInputBuffer* GetWriteBuffer() { return mpWriteBuffer; }   // h:357
        DispatchThreadInputBuffer* GetReadBuffer()  { return mpReadBuffer; }    // h:360

    private:
        DispatchThreadInputBuffer* mapBuffers[2];                       // DWARF h:363  (X360 +0)
        DispatchThreadInputBuffer* mpWriteBuffer;                       // DWARF h:364  (X360 +8)
        DispatchThreadInputBuffer* mpReadBuffer;                        // DWARF h:365  (X360 +12)
        u32                        muWriteBufferIndex;                  // DWARF h:366  (X360 +16)
    };
}
