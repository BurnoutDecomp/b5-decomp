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

s32 giFlaptUpdateMonitor = -1;
s32 giFlaptUpdateMonitorTotal = -1;
s32 giFlaptRenderMonitor = -1;
s32 giFlaptRenderMonitorTotal = -1;

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

    mRenderer.Construct(reinterpret_cast<FlaptRenderSet*>(lpImRenderers),
                        lpTextRenderer, lpLanguageManager, lpFonts);

    giFlaptUpdateMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "FUpdate", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptRenderMonitor = CgsDev::PerfMonCpu::AddMonitor(
        "FRender", static_cast<CgsDev::PerfMonCpuPage>(23), false, 1.5f, true);
    giFlaptUpdateMonitorTotal = giFlaptUpdateMonitor;
    giFlaptRenderMonitorTotal = giFlaptRenderMonitor;
}

bool FlaptManager::Prepare(CgsMemory::LinearMalloc* lpLinear)
{
    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        for (u32 luFile = 0; luFile < E_FLAPTFILES_COUNT; ++luFile)
            maFlaptFileInstances[luFile].Prepare(lpLinear);
        mePrepareStage = E_PREPARESTAGE_DONE;
        return false;

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
        return false;

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

// ---- GetFile @ 0x82473078 ------------------------------------------------
FileRef* FlaptManager::GetFile(FileRef* lpOutRef, u32 luFile)
{
    FlaptFileInstance* lpFileInst = &maFlaptFileInstances[luFile];

    CGS_ASSERT(lpFileInst->mbIsActive, "maFlaptFileInstances[leFile].IsActive()");
    CGS_ASSERT(lpFileInst != 0, "lpFileInst");

    lpOutRef->mpFileInstance = lpFileInst;
    return lpOutRef;
}

// FLAG: the two CPU perf-monitor handles bracketing the flapt UPDATE region (the X360
// read them from the globals dword_82F2765C / dword_82FB3B0C). They are registered via
// CgsDev::PerfMonCpu::AddMonitor by the perf-monitor setup TU; declared extern here so
// the bracket compiles (the per-TU gate does not link). The console wraps the same
// region in both monitors (a specific + an enclosing total).
// ---- Update @ 0x82472120 -------------------------------------------------
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

// FLAG: the two CPU perf-monitor handles bracketing the flapt RENDER region (X360
// globals dword_82F27680 / dword_82FB3B10). Registered by the perf-monitor setup TU;
// extern here so the bracket compiles.
// ---- Render @ 0x82472908 -------------------------------------------------
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
