#pragma once

#include "types.hpp"
#include "SDKs/EATech/include/Apt/Apt.h"            // AptAssetString, AptAllocateStringParameters
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h" // AptMatrix (SetVertexMatrix arg)
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h" // AptCXForm (SetColourTransform arg)
#include "GameShared/GameClasses/Fonts/CgsUnicode.h" // CgsUnicode::CgsUtf8 (AllocateString bridge out-param)

// =============================================================================
// CgsGui::AptCallbackRender / CgsGui::AptCallbackRenderFlags
//
// The 16 render + 2 render-flag host callbacks the Apt player drives. They are the
// free functions CgsGui::AptAux::ConstructApt @0x5BA0F8 installs into the Apt user-
// function table (gAptFuncs); the Apt engine calls them as it walks a movie's render
// tree. Every one reaches the committed render handler through the AptAux singleton
// (CgsGui::AptAuxPointer::mpAptAuxInst->mRenderHandler) -- so this family is the BRIDGE
// from the engine's hook table to CgsGui::AptRenderHandler::Render.
//
// Recovered from the PS3 External ELF dossiers (addresses on each method). The
// engine-side asset handles the Apt player passes are the SDK's own opaque types:
//   AptAnimationUserData  -- the per-animation user data; its +12 word points at the
//                            loaded gui-geometry object (a CgsResource::GuiGeometryObject
//                            whose [0]=file count, [8]=GuiGeometryFile* table).
//   AptAssetRenderingUnit -- a loaded shape's GuiGeometryFile* (LoadRenderingUnit hands
//                            one back; DrawRenderingUnit draws it).
//   AptAssetTexture       -- a GuiTexture id (a 32-bit handle).
//   AptAssetString        -- a CgsGraphics::TextObject* (DrawString) / CgsAptString*
//                            (AllocateString hands one back). Apt.h aliases it to void*.
// They are modelled as void* here (the SDK's opaque-handle convention); the callback
// bodies cast them to the concrete loaded-resource layout they read.
// =============================================================================

namespace CgsGraphics { struct TextObject; }

// The mask operation the Apt render tree requests for a rendering unit. Values recovered
// from the compare-immediates in DrawRenderingUnit @0x5CBA30:
//   == 0  -> Normal   : draw the shape (AptRenderHandler::Render).
//   >  0  -> Add      : begin a stencil mask from the shape's AABB (push mask geometry).
//   == -1 -> Subtract : end/clear the current stencil mask.
// (Other negative values fall through to a no-op, matching the asm's `bne exit`.)
enum AptMaskRenderOperation : int
{
    AptMaskRenderOperation_Subtract = -1,
    AptMaskRenderOperation_Normal   = 0,
    AptMaskRenderOperation_Add      = 1,
};

// The opaque per-animation user data the Apt player threads through the load/bind/draw
// callbacks. Only the +12 slot (the loaded gui-geometry object) is in scope.
typedef void* AptAnimationUserData;

// A loaded shape's gui-geometry file handle (LoadRenderingUnit result / DrawRenderingUnit arg).
typedef void* AptAssetRenderingUnit;

// A GuiTexture id handle (LoadTexture result / BindTexture + FreeTexture arg).
typedef void* AptAssetTexture;

namespace CgsGui
{
    // The render-callback family. Static methods (the gAptFuncs slots are plain function
    // pointers): the Apt engine supplies all state through the arguments + the AptAux singleton.
    class AptCallbackRender
    {
    public:
        // PS3 0x5CBA30. The main shape-draw bridge: dispatch on the mask op. Normal -> draw the
        // rendering unit through AptRenderHandler::Render; Add -> push a stencil mask built from
        // each mesh's screen-space AABB; Subtract -> end the current mask.
        static void DrawRenderingUnit(AptAssetRenderingUnit lpRenderingUnit,
                                      AptMaskRenderOperation leMaskOperation, s32 liLevel);

        // PS3 0x5C9364. Batch a laid-out text object's glyphs through the render handler's
        // TextRenderer (skipped when the batch colour-scale alpha is zero).
        static void DrawString(AptAssetString lpAssetString,
                               AptMaskRenderOperation leMaskOperation, s32 liLevel);

        // PS3 0x5BAE00. Bind a loaded texture id into every gui-geometry mesh that references it.
        static void BindTexture(AptAnimationUserData lpUserData, s32 liID,
                                AptAssetTexture lpAssetTexture);

        // PS3 0x5BAD58. Look a texture id up across the loaded gui-geometry files; return the
        // bound texture handle (or null).
        static AptAssetTexture LoadTexture(AptAnimationUserData lpUserData, s32 liID);

        // PS3 0x5BADFC. No-op (the texture lifetime is owned by the resource system).
        static void FreeTexture(AptAssetTexture lpAssetTexture);

        // PS3 0x5BAEA8. bsearch the loaded gui-geometry object's file table for the file whose id
        // matches liID; return the GuiGeometryFile* (or null).
        static AptAssetRenderingUnit LoadRenderingUnit(AptAnimationUserData lpUserData, s32 liID);

        // PS3 0x5BAEEC. No-op (the rendering-unit lifetime is owned by the resource system).
        static void FreeRenderingUnit(AptAssetRenderingUnit lpRenderingUnit);

        // PS3 0x5BDF54. Fold the apt 2x2 matrix into the batch transform's screen-space basis.
        static void SetVertexMatrix(AptMatrix* lpAptMatrix);

        // PS3 0x5BE228. Fold the apt colour transform into the batch transform's colour shift/scale.
        static void SetColourTransform(AptCXForm* lpAptColourTransform);

        // PS3 0x5C0E2C. Set the stage clear colour (rotate the apt ARGB word to RGBA).
        static void SetBackgroundColour(u32 luAptColour);

        // PS3 0x5C7260. Allocate + lay out a text string from the parameters: free the previous
        // string if the flags say so, hand out a pooled CgsAptString, and Prepare it.
        static AptAssetString AllocateString(AptAllocateStringParameters* lpParameters);

        // The opaque-world half of AllocateString @0x5C7260, implemented in CgsAptCallbackRender.cpp
        // (which owns the opaque pool CgsAptString + the render handler). AllocateString itself lives
        // in CgsAptCallbackRenderAllocateString.cpp -- the TU that owns the REAL CgsGui::CgsAptString
        // text object (for CgsAptString::Prepare) and so cannot include the render-handler header.
        // This half frees the previous string, hands out a free pool slot + its char buffer, and
        // surfaces the font collection / text effect / size scale + the slot's x64 ZID (slotIndex+1,
        // stored in AptRenderItemDynamicText::mZID; 0 == unresolved) that AllocateString forwards to
        // CgsAptString::Prepare. Returns the pool slot as a raw void* (the caller casts it to CgsAptString*).
        static void* AcquireStringSlot(AptAllocateStringParameters* lpParameters,
                                       CgsUnicode::CgsUtf8** lpcStringBufferOut,
                                       const void** lppFontsOut,
                                       s32* lpiEffectOut,
                                       f32* lpfSizeScaleOut,
                                       s32* lpiZIDOut);

        // PS3 0x5BDF3C. Return a pooled CgsAptString to the render handler's free pool.
        static void DeallocateString(AptAssetString lpAssetString, u32 leFlags);

        // PS3 0x5BE028 / 0x5BE010. The stage dimensions the apt player lays out against.
        static f32 GetStageWidth();
        static f32 GetStageHeight();
    };

    // The render-flag push/pop hooks (no-ops in this build; the render-flag stack is unused).
    class AptCallbackRenderFlags
    {
    public:
        static void Push(const char* lpacRenderFlags);   // PS3 0x5BA8A4 (empty)
        static void Pop(const char* lpacRenderFlags);    // PS3 0x5BA8A8 (empty)
    };

    // ---------------------------------------------------------------------------------
    // The Apt CUSTOM-CONTROL callback family (gAptFuncs.pfnCustomControlRender & friends).
    //
    // An Apt custom control is a movieclip the movie itself types at author time. The
    // engine (AptCIH::ProcessCustomControls @0x82B07788 -> AptRenderItemCustomControl::
    // Render @0x82AEF8F8) hands the host the clip's `_type` / `_target` / property string
    // and its geometry, and the host decides what to draw there. Burnout uses exactly one
    // such control on the boot path: B5LicenseRank0's photo slot, authored
    // `_type='PlayerImage', _index=1`.
    //
    // Body lives in CgsAptAux.cpp (the console's own assert strings name that file:
    // "..\\..\\..\\GameShared\\GameClasses\\Gui/View/AptInterface/CgsAptAux.cpp", lines
    // 990/992/995/1002/1030/1039).
    // ---------------------------------------------------------------------------------
    class AptCallbackCustom
    {
    public:
        // X360 0x8285BFA0. Resolve the control's `_type` to a CgsID, pull `_index=` out of
        // the property string, ask the GUI's custom-renderer manager for that component's
        // texture, and -- only if one comes back -- bind it into every mesh of the control's
        // geometry, stamp the standard quad UVs, and draw through AptRenderHandler::Render
        // with the shader program the component chose.
        static void ControlRender(char* lpcType, char* lpcTarget,
                                  AptAssetRenderingUnit lpRenderingUnit,
                                  const char* lpcProperties,
                                  AptMaskRenderOperation leMaskOperation, int liLevel);
    };

}
