#include "GameSource/Gui/Flapt/BrnFlaptManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"          // CgsGraphics::Im2d::EndRendering (the render-buffer frame flush)

// BrnFlapt::FlaptManager member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::FlaptManager) bodies the one
// X360-emitted function:
//
//   GetFile @ 0x82473078
//
// X360 body: forms the element address `&maFlaptFileInstances[luFile]`
// (mulli r5,0x34 ; add base ; addi 8 — i.e. this + 8 + 52*index), asserts the
// entry is active ("maFlaptFileInstances[leFile].IsActive()") and that the
// element pointer is non-null ("lpFileInst", the inlined FileRef constructor),
// then writes the FileRef into the caller-provided out buffer
// (`*out = &maFlaptFileInstances[luFile]`). The X360-baked file/line cites are
// discarded per project convention.
//
// Note the asm reads the IsActive byte from element+0 (lbz 0(r31)); since the
// element pointer is taken as &maFlaptFileInstances[luFile], element+0 is the
// FlaptFileInstance's mbIsActive flag.

namespace BrnFlapt
{

// The two enclosing flapt CPU monitors ("      FLApt - Update"/"      FLApt - Render",
// GUI page 3, budget 0.5f) are registered by BrnGui::GuiPerfmons::Initialise
// @0x824EF050 into dword_82F2765C/dword_82F27680 -- that TU is not yet homed, so the
// handles stay -1 here (Start/StopMonitor no-op on an invalid handle) until it lands.
s32 giFlaptUpdateMonitor = -1;
s32 giFlaptRenderMonitor = -1;
// The inner "FUpdate"/"FRender" pair (X360 dword_82FB3B0C/dword_82FB3B10) is
// registered below by FlaptManager::Construct.
s32 giFlaptUpdateMonitorTotal = -1;
s32 giFlaptRenderMonitorTotal = -1;
// The renderer's per-phase monitors, also registered by Construct (X360
// dword_82FB3AE0..dword_82FB3B1C). Their Start/Stop bracket sites live in the
// FlaptRenderer bodies (RenderMasks/RenderChildren/RenderMeshes/...); extern
// them from here as those TUs land.
s32 giFlaptRenderMasksMonitor = -1;        // dword_82FB3AF4
s32 giFlaptRenderChildrenMonitor = -1;     // dword_82FB3B14
s32 giFlaptRenderMeshesMonitor = -1;       // dword_82FB3B18
s32 giFlaptRenderMeshMonitor = -1;         // dword_82FB3AE0
s32 giFlaptGetTextureMonitor = -1;         // dword_82FB3B08
s32 giFlaptNotMaskMonitor = -1;            // dword_82FB3B00
s32 giFlaptMaskMonitor = -1;               // dword_82FB3AF0
s32 giFlaptRenderTextFieldsMonitor = -1;   // dword_82FB3AFC
s32 giFlaptPushTransformMonitor = -1;      // dword_82FB3AF8
s32 giFlaptPopTransformMonitor = -1;       // dword_82FB3AE4
s32 giFlaptSetTextureMonitor = -1;         // dword_82FB3AEC
s32 giFlaptSetBlendMonitor = -1;           // dword_82FB3B1C
s32 giFlaptRenderStaticMonitor = -1;       // dword_82FB3B04
s32 giFlaptSetTransformsMonitor = -1;      // dword_82FB3AE8

void FlaptManager::Construct(CgsGui::ImRendererSet* lpImRenderers,
                             CgsGraphics::TextRenderer* lpTextRenderer,
                             CgsLanguage::LanguageManager* lpLanguageManager,
                             const CgsGui::FontCollection* lpFonts,
                             const RGBA* lpAlternateTextColours,
                             int liNumAlternateColours)
{
    CGS_ASSERT(lpImRenderers != 0, "lpImRenderers");
    CGS_ASSERT(lpTextRenderer != 0, "lpTextRenderer");
    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");
    CGS_ASSERT(lpFonts != 0, "lpFonts");

    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_DONE;

    for (u32 luFile = 0; luFile < E_FLAPTFILES_COUNT; ++luFile)
        maFlaptFileInstances[luFile].Construct(lpAlternateTextColours,
                                               liNumAlternateColours);

    // FLAG (type fork): FlaptRenderSet models slot 0 of CgsGui::ImRendererSet (the
    // shared immediate-mode render buffer); reconcile the two types when the
    // AptIm2dRenderBuffer / CgsGraphics::Im2d render-buffer web unifies.
    mRenderer.Construct(reinterpret_cast<FlaptRenderSet*>(lpImRenderers),
                        lpTextRenderer, lpLanguageManager, lpFonts);

    // The 16 flapt CPU monitors the X360 Construct registers (@0x82472694..), in the
    // attested order: the inner update/render brackets, then the renderer's
    // per-phase monitors (read by the FlaptRenderer bodies).
    giFlaptUpdateMonitorTotal = CgsDev::PerfMonCpu::AddMonitor(
        "FUpdate", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderMonitorTotal = CgsDev::PerfMonCpu::AddMonitor(
        "FRender", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderMasksMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FR: RenderMasks", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderChildrenMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FR: RenderChildren", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderMeshesMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FR: RenderMeshes", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderMeshMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "    FRM: RenderMesh", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptGetTextureMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "      FRM: GetTexture", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptNotMaskMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "      FRM: NotMask", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptMaskMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "      FRM: Mask", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderTextFieldsMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FR: RenderTextFields", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptPushTransformMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FR: PushTransform", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptPopTransformMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FR: PopTransform", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptSetTextureMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FRState: SetTexture", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.0f, true);
    giFlaptSetBlendMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FRState: SetBlend", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.0f, true);
    giFlaptRenderStaticMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FRState: RenderStatic", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.0f, true);
    giFlaptSetTransformsMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "  FRState: SetTransforms", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.0f, true);
}

bool FlaptManager::Prepare(CgsMemory::LinearMalloc* lpLinear)
{
    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        for (u32 luFile = 0; luFile < E_FLAPTFILES_COUNT; ++luFile)
            maFlaptFileInstances[luFile].Prepare(lpLinear);
        mePrepareStage = E_PREPARESTAGE_DONE;
        // fall through -- the X360 START path (@0x8246D5EC) drops straight into the
        // DONE tail: a first call prepares AND reports ready in one step.

    case E_PREPARESTAGE_DONE:
        meReleaseStage = E_RELEASESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Got bad prepare stage in FlaptManager::Prepare");
        return false;
    }
}

bool FlaptManager::Release()
{
    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
        meReleaseStage = E_RELEASESTAGE_DONE;
        // fall through -- mirrors Prepare: the X360 START path (@0x8246D610) drops
        // into the DONE tail, so a first call releases AND reports done in one step.

    case E_RELEASESTAGE_DONE:
        mePrepareStage = E_PREPARESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Got bad release stage in FlaptManager::Release");
        return false;
    }
}

void FlaptManager::Destruct()
{
    for (u32 luFile = 0; luFile < E_FLAPTFILES_COUNT; ++luFile)
        maFlaptFileInstances[luFile].Destruct();
}

// ---- RegisterFlaptFile @ 0x82472188 ---------------------------------------
// Bind a loaded FlaptFile resource handle to a flapt file slot: assert the slot
// is not already live (the X360 streams the slot index into the message; the
// StrStream-built text collapses to the plain string per project convention),
// then hand the handle + the embedded renderer to the instance's SetData.
void FlaptManager::RegisterFlaptFile(FlaptFiles leFile,
                                     CgsResource::ResourceHandle lResourceHandle)
{
    FlaptFileInstance* lpFileInst = &maFlaptFileInstances[leFile];

    CGS_ASSERT(!lpFileInst->mbIsActive,
               "Tried to register FLApt file  more than once");

    lpFileInst->SetData(lResourceHandle, &mRenderer);
}

// ---- GetFile @ 0x82473078 ------------------------------------------------
FileRef* FlaptManager::GetFile(FileRef* lpOutRef, u32 luFile)
{
    FlaptFileInstance* lpFileInst = &maFlaptFileInstances[luFile];

    CGS_ASSERT(lpFileInst->mbIsActive, "maFlaptFileInstances[leFile].IsActive()");
    CGS_ASSERT(lpFileInst != 0, "lpFileInst");

    lpOutRef->mpFileInstance = lpFileInst;
    return lpOutRef;
}

// ---- Update @ 0x82472120 -------------------------------------------------
// The X360 brackets the region in two monitors: the enclosing GUI-page handle
// (dword_82F2765C, registered by the un-homed BrnGui::GuiPerfmons::Initialise --
// still -1 here) and the "FUpdate" handle Construct registers (dword_82FB3B0C).
// Per-frame tick: bracket the work in the two CPU perf monitors, then -- if the single
// (HUD) file instance is active -- advance it by the time step. The X360 returns the
// inner StopMonitor's r3; the caller (BrnGui::ViewModule::Update) ignores it, and the
// header declares Update void, so the return is dropped.
void FlaptManager::Update(f32 lfTimeStep)
{
    CgsDev::PerfMonCpu::StartMonitor(giFlaptUpdateMonitor);
    const s32 liTotalMonitor = giFlaptUpdateMonitorTotal;
    CgsDev::PerfMonCpu::StartMonitor(liTotalMonitor);

    if (maFlaptFileInstances[0].mbIsActive)
        maFlaptFileInstances[0].Update(lfTimeStep);

    CgsDev::PerfMonCpu::StopMonitor(giFlaptUpdateMonitor);
    CgsDev::PerfMonCpu::StopMonitor(liTotalMonitor);
}

// ---- Render @ 0x82472908 -------------------------------------------------
// Bracketed like Update: the enclosing GUI-page handle (dword_82F27680, registered by
// the un-homed BrnGui::GuiPerfmons::Initialise -- still -1 here) plus the "FRender"
// handle Construct registers (dword_82FB3B10).
// Per-frame draw: bracket in the two CPU perf monitors; start the frame on the embedded
// renderer; draw the single active (HUD) file instance through it; flush the immediate-
// mode render buffer (EndRendering); then clear the renderer's per-frame texture/blend
// cache so the next frame re-binds. The X360 reaches the render buffer as
// mRenderer.mpImRenderSet->mpIm2dRenderBuffer + 4 (the command sub-object), which folds
// to the named buffer on the PC Im2d; its int return is dropped (header declares void).
void FlaptManager::Render()
{
    CgsDev::PerfMonCpu::StartMonitor(giFlaptRenderMonitor);
    const s32 liTotalMonitor = giFlaptRenderMonitorTotal;
    CgsDev::PerfMonCpu::StartMonitor(liTotalMonitor);

    mRenderer.StartRenderingFrame();

    if (maFlaptFileInstances[0].mbIsActive)
        maFlaptFileInstances[0].Render(&mRenderer);

    mRenderer.mpImRenderSet->mpIm2dRenderBuffer->EndRendering();

    // Clear the renderer's per-frame bound-texture / blend-state cache (X360 a1+0x50/0x54).
    mRenderer.mpCurrentTexture    = 0;
    mRenderer.mpCurrentBlendState = 0;

    CgsDev::PerfMonCpu::StopMonitor(giFlaptRenderMonitor);
    CgsDev::PerfMonCpu::StopMonitor(liTotalMonitor);
}

}
