// ===================================================================================
// BrnGui::ViewModule  --  GameSource/Gui/BrnGuiViewModule.cpp
//
//   BrnGui::ViewModule::Construct                       @ 0x824F13B8  (virtual; EXECUTED)
//   BrnGui::ViewModule::Prepare                         @ 0x824F1468  (virtual)
//   BrnGui::ViewModule::Release                         @ 0x824F15B0  (virtual)
//   BrnGui::ViewModule::Destruct                        @ 0x824F16F0  (virtual)
//   BrnGui::ViewModule::Update                          @ 0x824F1730  (virtual)
//   BrnGui::ViewModule::RenderInternal                  @ 0x824F1770  (virtual)
//   BrnGui::ViewModule::ProcessIncomingLoadNotification @ 0x824F9468  (virtual)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX for SEMANTIC PARITY. The Burnout GUI view
// module extends the shared CgsGui::ViewModule by owning a BrnFlapt::FlaptManager and
// driving it in lock-step with the base module's lifecycle, plus turning a "FLAPT load"
// resource-load notification into a FlaptManager file registration.
//
// All seven methods are non-static members (asm: r3 = this). The dev-assert
// Begin/Fire/End sequences fold into CGS_ASSERT(cond,"msg") per house style; the
// default-case "bad stage" asserts that stream the stage value into the message buffer
// are modelled as an unreachable CGS_ASSERT(false, "...") (the streamed integer is
// debug-only formatting -- the semantic content is the unreachable tripwire).
//
// MEMBER ACCESS is BY NAME throughout: this module's own four members are real named
// fields (meBrnPrepareStage / meBrnReleaseStage / mFlaptManager / mpGuiModule); the base
// view module's owned sub-objects + per-frame time-step are reached through the base's
// named accessors (GetImRendererSet() / GetTextRenderer() / GetLanguageManager() /
// GetFontCollection() / GetUpdateTimeStep()); the owning GuiModule's always-available
// components manager is reached through GuiModule::GetAlwaysAvailableComponentsManager().
// ===================================================================================
#include "GameSource/Gui/BrnGuiViewModule.h"

// NOTE: BrnGuiModule.h is intentionally NOT included -- the owning GuiModule is held by
// pointer only and the always-available components manager is reached through the free
// BrnGui::GetAlwaysAvailableComponentsManager(GuiModule*) accessor declared in
// BrnGuiAlwaysAvailableComponentsManager.h (it takes a forward-declared GuiModule*), so
// this TU avoids GuiModule.h's heavy transitive includes.
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h" // AlwaysAvailableComponentsManager + accessor
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                  // BrnFlapt::FileRef (GetFile out-buffer)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiEventLoadNotification
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT

namespace BrnGui
{
namespace
{
    // X360 load-notification request type that triggers a FLAPT registration. The asm
    // compares meRequestType to the literal 10 (`cmpwi r11, 0xA`). NOTE: in the PS3
    // DecFIGS ResourceRequestTypes enum value 10 is E_GUI_RESOURCETYPE_TEXTURE while
    // FLAPT_PERSISTENT is 9 -- a merge-window enum drift between PS3 and the X360 ARTIST
    // build. The X360 numeric is authoritative here; named for the role it plays in this
    // consumer rather than asserting which PS3 enumerator it equals.
    const CgsGui::ResourceRequestTypes KE_REQUESTTYPE_FLAPT_LOAD =
        static_cast<CgsGui::ResourceRequestTypes>(10);

    // The flapt file the HUD notification registers/reads (asm passes the file index 0).
    const BrnFlapt::FlaptFiles KE_HUD_FLAPT_FILE = BrnFlapt::E_FLAPTFILE_HUD;
}

// ---- Construct @ 0x824F13B8 -------------------------------------------------------
// Build the base view module, store the owning GuiModule, construct the embedded
// FlaptManager from the base module's sub-objects, then seed the staging enums.
void ViewModule::Construct(BrnGui::GuiModule* lpGuiModule, const char* lpcName, int liArg2,
                           f32 lfArg3, const RGBA* lpColour, int liArg5)
{
    // The base view-module bring-up (the float lfArg3 rides the FP arg register straight
    // through to the base; the X360 reuses the same name/int/colour/int args).
    CgsGui::ViewModule::Construct(lpcName, liArg2, lfArg3, lpColour, liArg5);

    CGS_ASSERT(lpGuiModule != 0, "lpGuiModule");
    mpGuiModule = lpGuiModule;

    // Build the FlaptManager from the base module's owned sub-objects (the ImRendererSet,
    // the TextRenderer, this module's LanguageManager and its FontCollection), plus the GUI
    // clear colour and the trailing count/flag int. All reached BY NAME through the base's
    // sub-object accessors.
    mFlaptManager.Construct(GetImRendererSet(), GetTextRenderer(), GetLanguageManager(),
                            GetFontCollection(), lpColour, liArg5);

    meBrnPrepareStage = E_BRNPREPARESTAGE_START;
    meBrnReleaseStage = E_BRNRELEASESTAGE_DONE;
}

// ---- Prepare @ 0x824F1468 ---------------------------------------------------------
// Advance the prepare state machine one step per call; true only once it reaches DONE.
// START falls through to BASE_CLASS; BASE_CLASS prepares the base view module; FLAPT
// prepares the FlaptManager; DONE finishes and resets the release stage to START.
bool ViewModule::Prepare(CgsMemory::HeapMalloc* lpHeap, rw::IResourceAllocator* lpResAlloc,
                         CgsMemory::HeapMalloc* lpHeap2, CgsMemory::LinearMalloc* lpLinear)
{
    switch (meBrnPrepareStage)
    {
    case E_BRNPREPARESTAGE_START:
        meBrnPrepareStage = E_BRNPREPARESTAGE_BASE_CLASS;
        // fall through
    case E_BRNPREPARESTAGE_BASE_CLASS:
        if (!CgsGui::ViewModule::Prepare(lpHeap, lpResAlloc, lpHeap2, lpLinear))
            return false;
        meBrnPrepareStage = E_BRNPREPARESTAGE_FLAPT;
        // fall through
    case E_BRNPREPARESTAGE_FLAPT:
        if (!mFlaptManager.Prepare(lpLinear))
            return false;
        meBrnPrepareStage = E_BRNPREPARESTAGE_DONE;
        // fall through
    case E_BRNPREPARESTAGE_DONE:
        // Re-arm the release stage so a matching Release() runs from the top.
        meBrnReleaseStage = E_BRNRELEASESTAGE_START;
        return true;
    default:
        CGS_ASSERT(false, "Get bad prepare stage in BrnGui::ViewModule::Prepare");
        return false;
    }
}

// ---- Release @ 0x824F15B0 ---------------------------------------------------------
// Mirror of Prepare in reverse: release the FlaptManager first, then the base view
// module; true once DONE, which re-arms the prepare stage to START.
bool ViewModule::Release()
{
    switch (meBrnReleaseStage)
    {
    case E_BRNRELEASESTAGE_START:
        meBrnReleaseStage = E_BRNRELEASESTAGE_FLAPT;
        // fall through
    case E_BRNRELEASESTAGE_FLAPT:
        if (!mFlaptManager.Release())
            return false;
        meBrnReleaseStage = E_BRNRELEASESTAGE_BASE_CLASS;
        // fall through
    case E_BRNRELEASESTAGE_BASE_CLASS:
        if (!CgsGui::ViewModule::Release())
            return false;
        meBrnReleaseStage = E_BRNRELEASESTAGE_DONE;
        // fall through
    case E_BRNRELEASESTAGE_DONE:
        meBrnPrepareStage = E_BRNPREPARESTAGE_START;
        return true;
    default:
        CGS_ASSERT(false, "Get bad release stage in BrnGui::ViewModule::Release");
        return false;
    }
}

// ---- Destruct @ 0x824F16F0 --------------------------------------------------------
// Destruct the embedded FlaptManager, then chain to the base view module's destructor.
void ViewModule::Destruct()
{
    mFlaptManager.Destruct();
    CgsGui::ViewModule::Destruct();
}

// ---- Update @ 0x824F1730 ----------------------------------------------------------
// Tick the base view module then the FlaptManager, fed the base module's current
// per-frame time-step (base member read by name via GetUpdateTimeStep()).
void ViewModule::Update(CgsGui::ViewIO::IOBufferStack* lpInStack,
                        CgsGui::ViewIO::IOBufferStack* lpOutStack,
                        const CgsGui::ViewIO::InputBuffer* lpInput,
                        CgsGui::ViewIO::OutputBuffer* lpOutput)
{
    CgsGui::ViewModule::Update(lpInStack, lpOutStack, lpInput, lpOutput);
    mFlaptManager.Update(GetUpdateTimeStep());
}

// ---- RenderInternal @ 0x824F1770 --------------------------------------------------
// Clear the screen, render the shared view content, then draw the flapt overlay on top.
void ViewModule::RenderInternal(const CgsGui::ViewIO::InputBuffer* lpInput)
{
    CgsGui::ViewModule::RenderBlackScreen();
    CgsGui::ViewModule::RenderInternal(lpInput);
    mFlaptManager.Render();
}

// ---- ProcessIncomingLoadNotification @ 0x824F9468 ---------------------------------
// A FLAPT load notification (request type 10) registers the loaded file with the
// FlaptManager and (re)prepares the owning GuiModule's always-available components
// against it; every other notification defers to the base handler.
void ViewModule::ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent)
{
    CGS_ASSERT(lpEvent != 0, "Invalid event in ViewModule::ProcessIncomingLoadNotification");

    const CgsGui::GuiEventLoadNotification* lpNotification =
        reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);

    if (lpNotification->meRequestType != KE_REQUESTTYPE_FLAPT_LOAD)
    {
        CgsGui::ViewModule::ProcessIncomingLoadNotification(lpEvent);
        return;
    }

    // Register the just-loaded HUD flapt file with the manager.
    mFlaptManager.RegisterFlaptFile(KE_HUD_FLAPT_FILE, lpNotification->mResourceHandle);

    CGS_ASSERT(mpGuiModule != 0, "mpGuiModule");

    // Fetch the registered file and (re)prepare the GuiModule's always-available components.
    BrnFlapt::FileRef lFileRef;
    mFlaptManager.GetFile(&lFileRef, KE_HUD_FLAPT_FILE);

    GetAlwaysAvailableComponentsManager(mpGuiModule)->PrepareFlapt(lFileRef);
}

} // namespace BrnGui
