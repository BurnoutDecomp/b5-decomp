#ifndef BRN_FLAPT_MANAGER_H
#define BRN_FLAPT_MANAGER_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"          // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"     // BrnFlapt::FlaptFileInstance (52-byte stride)
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"         // BrnFlapt::FlaptRenderer (the embedded renderer @ console +0x40)

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptManager.h
//
// BrnFlapt::FlaptManager — owns the table of live FlaptFileInstances.
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Attested layout (this group):
//   +0x08  maFlaptFileInstances[]   (X360 GetFile: element = this + 8 + 52*index)
//          element stride 0x34 (52 bytes) == sizeof(FlaptFileInstance).
//
// Only the array base proven by GetFile is named; the leading 8 bytes and the
// array length are not attested here, so the array is modeled as a declared-only
// element block reached via the attested base offset. Grow this header additively
// as the other FlaptManager TUs (Construct / Prepare / Update / Render /
// RegisterFlaptFile / Release / Destruct / SetSoundTriggerHandler) land.
// ============================================================================

// Pointer-only parameter types for the lifecycle methods declared below. Forward-declared
// (incomplete-type use is sufficient for the declarations; their real homes would force a
// transitive header cascade into every includer of this header). DWARF Construct signature:
// Construct(ImRendererSet*, TextRenderer*, CgsLanguage::LanguageManager*,
//           const CgsGui::FontCollection*, const RGBA*, int32_t).
struct RGBA;
namespace CgsGui { struct ImRendererSet; struct FontCollection; }
namespace CgsGraphics { struct TextRenderer; }
namespace CgsLanguage { class LanguageManager; }
namespace CgsMemory { class LinearMalloc; }
namespace CgsResource { struct ResourceHandle; }

namespace BrnFlapt
{
    // DWARF BrnFlaptManager.h:45 -- the flapt files the manager can host. The X360 build
    // registers/loads exactly one (the HUD flapt) under E_FLAPTFILE_HUD == 0.
    enum FlaptFiles
    {
        E_FLAPTFILE_HUD     = 0,
        E_FLAPTFILES_COUNT  = 1,
    };

    struct FlaptManager
    {
        enum PrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE = 1,
        };

        enum ReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE = 1,
        };

        // GetFile @ 0x82473078 : index maFlaptFileInstances[luFile], assert the
        // entry IsActive(), and return a FileRef {&entry} by value into the
        // caller-provided out buffer.
        FileRef* GetFile(FileRef* lpOutRef, u32 luFile);

        // --- Lifecycle / per-frame methods (declared for the BrnGui::ViewModule caller;
        // bodies live in this class's own ledger TUs -- the per-TU `cl /c` gate does not
        // link, so the declarations suffice). All X360-attested. ---

        // @0x82472??? Construct : build the manager's renderer + file table from the owning
        // view module's sub-objects (ImRendererSet, TextRenderer, LanguageManager,
        // FontCollection) plus the GUI clear colour and a count/flag int.
        void Construct(CgsGui::ImRendererSet* lpImRenderers, CgsGraphics::TextRenderer* lpTextRenderer,
                       CgsLanguage::LanguageManager* lpLanguageManager,
                       const CgsGui::FontCollection* lpFonts, const RGBA* lpColour, int liArg5);

        // @0x82471??? Prepare : two-phase resource prepare; true once the manager is ready.
        bool Prepare(CgsMemory::LinearMalloc* lpLinear);

        // @0x82472??? Release : tear down the manager's prepared resources; true once done.
        bool Release();

        // @0x824716F8 Destruct : release the renderer + file instances.
        void Destruct();

        // @0x82472120 Update : advance every live file instance by lfTimeStep.
        void Update(f32 lfTimeStep);

        // @0x82472908 Render : draw the live flapt files through the view module's renderers.
        void Render();

        // @0x82472188 RegisterFlaptFile : bind a loaded resource handle to a flapt file
        // slot (already-active assert + FlaptFileInstance::SetData with the embedded renderer).
        void RegisterFlaptFile(FlaptFiles leFile, CgsResource::ResourceHandle lResourceHandle);

        PrepareStage      mePrepareStage;
        ReleaseStage      meReleaseStage;
        FlaptFileInstance maFlaptFileInstances[E_FLAPTFILES_COUNT];
        // [c:+0x40] the embedded renderer Render() drives (StartRenderingFrame + the
        // per-frame texture/blend cache it resets). Named member -- the console byte
        // offset folds away on x64. Render reads mRenderer.mpImRenderSet->mpIm2dRenderBuffer
        // for EndRendering and clears mRenderer.mpCurrentTexture/mpCurrentBlendState.
        FlaptRenderer     mRenderer;                // [c:+0x40]
    };
}

#endif // BRN_FLAPT_MANAGER_H
