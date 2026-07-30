#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"            // AptAuxPointer::mpAptAuxInst (the singleton)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"  // AptRenderHandler (Render + the transform/caches/pool)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"  // CgsResource::GuiGeometryFile / GuiGeometryMesh / GuiGeometryObject
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (the mask/command writers)
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"              // CgsGraphics::TextRenderer::RenderString / TextObject
#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"              // CgsGui::FontCollection (the empty-collection guard)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"   // CgsGraphics::Im2dRenderBuffer (RenderString arg)

#include <cstdlib>   // bsearch
#include <cmath>     // std::fabs

// =============================================================================
// CgsGui::AptCallbackRender / AptCallbackRenderFlags -- the render host-callback family the
// Apt player drives. EVERY callback reaches the committed render handler through the AptAux
// singleton: CgsGui::AptAuxPointer::mpAptAuxInst->mRenderHandler. Decompiled from the PS3
// External ELF dossiers (address per method). The bodies REUSE the committed
// AptRenderHandler::Render / GetCommandBuffer / GetTextRenderer / DestroyAptString and the
// ImRenderBuffer<V> command API; this TU only bridges the engine's hook arguments to them.
//
// NOTE: this TU lives in the OPAQUE-CgsAptString world (AptRenderHandler.h models the pooled
// CgsAptString as a 128-byte slot). AllocateString -- the one callback that needs the REAL
// CgsGui::CgsAptString text object (to call CgsAptString::Prepare) -- is in its own TU
// (CgsAptCallbackRenderAllocateString.cpp), which cannot include this header. The two
// definitions of CgsGui::CgsAptString are mutually exclusive in a single TU, so they are
// kept apart by the file split (NOT forked -- they are the same guest type).
// =============================================================================

namespace CgsGui
{
    // Shorthand for the 2D command buffer's vertex type.
    typedef CgsGraphics::Basic2dColouredTexturedVertex Im2dVertex;
    typedef CgsGraphics::ImRenderBuffer<Im2dVertex>     Im2dCommandBuffer;

    // -------------------------------------------------------------------------
    // DrawRenderingUnit - PS3 0x5CBA30. The main shape-draw bridge.
    //
    // leMaskOperation dispatch (recovered from the compare-immediates):
    //   == 0  (Normal)   : draw the shape via AptRenderHandler::Render(unit, shader 0).
    //   == -1 (Subtract) : end/clear the current stencil mask (push a muType-19 command).
    //   >  0  (Add)      : begin a stencil mask -- per mesh, find the screen-space AABB, transform
    //                      its two corners through the batch transform, and push a muType-18
    //                      mask-geometry command carrying the bound texture id + the 2-corner run.
    // (Other negatives are a no-op, matching the asm's `bne exit`.)
    // -------------------------------------------------------------------------
    void AptCallbackRender::DrawRenderingUnit(AptAssetRenderingUnit lpRenderingUnit,
                                              AptMaskRenderOperation leMaskOperation, s32 liLevel)
    {
        (void)liLevel;   // the apt mask level is carried for parity; this path keys off the op only

        AptRenderHandler& lrHandler = AptAuxPointer::mpAptAuxInst->mRenderHandler;

        // ---- Normal: just draw the shape through the committed render path -------------------
        if (leMaskOperation == AptMaskRenderOperation_Normal)
        {
            lrHandler.Render(reinterpret_cast<CgsResource::GuiGeometryFile*>(lpRenderingUnit), 0);
            return;
        }

        // ---- Subtract: end/clear the current stencil mask -----------------------------------
        if (leMaskOperation == AptMaskRenderOperation_Subtract)
        {
            lrHandler.GetCommandBuffer()->EndMask();
            return;
        }

        // Any negative op other than Subtract is a no-op (the asm `bne exit`).
        if (leMaskOperation < AptMaskRenderOperation_Normal)
            return;

        // ---- Add (op > 0): build a stencil mask from each mesh's screen-space AABB ----------
        CgsResource::GuiGeometryFile* lpFile =
            reinterpret_cast<CgsResource::GuiGeometryFile*>(lpRenderingUnit);
        Im2dCommandBuffer*            lpBuffer = lrHandler.GetCommandBuffer();
        const CgsGraphics::Im2dTransform& lrTransform = lrHandler.GetVertexTransform();

        // FLAG (x64 native-8 fork): 8-byte-strided mesh table + 8-byte mppGeometryMeshes (rebased
        // offset->pointer by AptFixupGeometryFileNative8), indexed as uintptr_t entries.
        const uintptr_t* lpMeshTable =
            reinterpret_cast<const uintptr_t*>(lpFile->mppGeometryMeshes);

        for (u32 luMesh = 0; luMesh < lpFile->muNumberOfMeshes; ++luMesh)
        {
            CgsResource::GuiGeometryMesh* lpMesh =
                reinterpret_cast<CgsResource::GuiGeometryMesh*>(lpMeshTable[luMesh]);

            // The bound texture id: the mesh's mpTexture when its texture mode (mesh+4) is set,
            // else the render handler's white fallback (guest p_mRenderHandler+128). The console
            // stores this word at the mask command's +8 slot.
            uintptr_t luTextureId;
            if (lpMesh->miTextureMode != 0)
                luTextureId = static_cast<uintptr_t>(lpMesh->mpTexture);
            else
                luTextureId = reinterpret_cast<uintptr_t>(lrHandler.GetWhiteTexture());

            // The mesh's static vertex run (mppVerticies table, first entry).
            const Im2dVertex* const* lppVertexTable = reinterpret_cast<const Im2dVertex* const*>(
                static_cast<uintptr_t>(lpMesh->mppVerticies));
            const Im2dVertex* lpVertices = *lppVertexTable;

            const s32 liVertexCount = static_cast<s32>(lpMesh->muNumberOfVerticies);

            // Find the screen-space AABB corners: lpMin = smallest (x, then y), lpMax = largest
            // (x, then y). Mirrors the guest's compare chain (v14 == min corner, v13 == max corner):
            //   not-a-new-min (vx > minX || vy > minY) AND vx >= maxX AND vy >= maxY -> new max
            //   else -> new min.
            const Im2dVertex* lpMin = lpVertices;
            const Im2dVertex* lpMax = lpVertices;
            f32 lfMinX = lpVertices->mv2Pos.x;
            f32 lfMinY = lpVertices->mv2Pos.y;
            for (s32 liVert = 1; liVert < liVertexCount; ++liVert)
            {
                const Im2dVertex& lrV = lpVertices[liVert];
                if (lrV.mv2Pos.x > lfMinX || lrV.mv2Pos.y > lfMinY)
                {
                    if (lrV.mv2Pos.x >= lpMax->mv2Pos.x && lrV.mv2Pos.y >= lpMax->mv2Pos.y)
                        lpMax = &lrV;
                }
                else
                {
                    lfMinX = lrV.mv2Pos.x;
                    lfMinY = lrV.mv2Pos.y;
                    lpMin  = &lrV;
                }
            }

            // Transform each corner into screen space through the batch transform. The guest's VPU
            // sequence (vmaddfp with a zeroed scale lane v31) reduces to the transform's additive
            // origin/right/up basis applied to the corner; reconstructed as the standard Im2d
            // screen transform: pos = origin + cornerX*right + cornerY*up (xy lanes). The corner's
            // colour + UV are copied through verbatim.
            Im2dVertex laCorners[2];
            const f32 lfMinPosX = lpMin->mv2Pos.x;
            const f32 lfMinPosY = lpMin->mv2Pos.y;
            const f32 lfMaxPosX = lpMax->mv2Pos.x;
            const f32 lfMaxPosY = lpMax->mv2Pos.y;

            laCorners[0].mv2Pos.x = lrTransform.mOriginXYZ.x
                                  + lfMinPosX * lrTransform.mRightUp.x
                                  + lfMinPosY * lrTransform.mRightUp.z;
            laCorners[0].mv2Pos.y = lrTransform.mOriginXYZ.y
                                  + lfMinPosX * lrTransform.mRightUp.y
                                  + lfMinPosY * lrTransform.mRightUp.w;
            laCorners[0].mv4Colour = lpMin->mv4Colour;
            laCorners[0].mv2Tex0UV = lpMin->mv2Tex0UV;

            laCorners[1].mv2Pos.x = lrTransform.mOriginXYZ.x
                                  + lfMaxPosX * lrTransform.mRightUp.x
                                  + lfMaxPosY * lrTransform.mRightUp.z;
            laCorners[1].mv2Pos.y = lrTransform.mOriginXYZ.y
                                  + lfMaxPosX * lrTransform.mRightUp.y
                                  + lfMaxPosY * lrTransform.mRightUp.w;
            laCorners[1].mv4Colour = lpMax->mv4Colour;
            laCorners[1].mv2Tex0UV = lpMax->mv2Tex0UV;

            // Push the mask-geometry command (the muType-18 record + the 2-corner vertex run).
            lpBuffer->PushMaskGeometry(luTextureId, laCorners[0], laCorners[1]);
        }
    }

    // -------------------------------------------------------------------------
    // DrawString - PS3 0x5C9364. Batch a laid-out text object's glyphs through the render
    // handler's TextRenderer. Skipped when the batch colour-scale alpha (mVertexTransform.
    // mColourScale.w) is zero -- the guest's `vcmpeqfp mColourScale.w, 0` early-out.
    // -------------------------------------------------------------------------
    void AptCallbackRender::DrawString(AptAssetString lpAssetString,
                                       AptMaskRenderOperation leMaskOperation, s32 liLevel)
    {
        (void)leMaskOperation;
        (void)liLevel;

        AptRenderHandler& lrHandler = AptAuxPointer::mpAptAuxInst->mRenderHandler;

        // x64 render-data handle: the engine passes the ZID (slotIndex + 1, or the raw slot
        // pointer on the console). Map it back to the pooled CgsAptString (the TextObject is its
        // leading member). A 0 / out-of-range id (or a null TextRenderer on the shape-only bring-up)
        // -> nothing to draw. See AptRenderHandler::ZIDToAptString.
        const int liZID = static_cast<int>(reinterpret_cast<intptr_t>(lpAssetString));
        CgsAptString* lpSlot = lrHandler.ZIDToAptString(liZID);
        if (lpSlot == nullptr || lrHandler.GetTextRenderer() == nullptr)
            return;

        // Fully-transparent batch -> nothing to draw. The guest's early-out is `vcmpeqfp` on the
        // colour-scale ALPHA lane, which is .w: SetColourTransform rotates the CXForm's
        // (A,R,G,B) into (R,G,B,A) on the way into the Im2dTransform, and
        // AptRenderHandler::Render @0x82859300 reads the same row at +0xC (the fourth float).
        // CORRECTED 2026-07-30 -- this read .x, so the text path skipped any batch that merely
        // had no RED as though it were invisible.
        if (lrHandler.GetVertexTransform().mColourScale.w == 0.0f)
            return;

        // The glyphs ride the SAME dispatched command buffer as the Apt shapes, under this text
        // item's CURRENT batch transform (PushMatrices -> pfnSetVertexMatrix stamped it into the
        // handler's mVertexTransform just before this callback; the shape path pushes the same
        // command at the head of AptRenderHandler::Render). Dispatch folds the transform into
        // every glyph vertex, so the field's local-space layout box lands exactly where the apt
        // matrix places it -- and the glyph draw keeps its walk-order depth among the shapes.
        // FLAG (PC fold): on the console the Im2dRenderBuffer this path drives IS this buffered
        // command buffer (RenderStart @0x57E0A0 / RenderEnd @0x57E5DC); the PC debug fold made the
        // Im2dRenderBuffer typedef the IMMEDIATE Im2d wrapper, so the text renderer is driven
        // through its buffered entry (TextRenderer::RenderStringBuffered) here.
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpBuffer =
            lrHandler.GetCommandBuffer();
        lpBuffer->SetTransform(lrHandler.GetVertexTransform());

        // The pooled CgsAptString's leading member IS the CgsGraphics::TextObject (same address).
        const CgsGraphics::TextObject* lpTextObject =
            reinterpret_cast<const CgsGraphics::TextObject*>(lpSlot);

        lrHandler.GetTextRenderer()->RenderStringBuffered(lpBuffer, *lpTextObject);
    }

    // -------------------------------------------------------------------------
    // BindTexture - PS3 0x5BAE00. Across every loaded gui-geometry file, find each mesh whose id
    // (mesh+8 == miTextureId) matches liID and stamp lpAssetTexture into its mpTexture (mesh+12).
    // lpUserData+12 -> the GuiGeometryObject ([0]=file count, [8]=GuiGeometryFile* table).
    // -------------------------------------------------------------------------
    void AptCallbackRender::BindTexture(AptAnimationUserData lpUserData, s32 liID,
                                        AptAssetTexture lpAssetTexture)
    {
        CgsResource::GuiGeometryObject* lpObject =
            *reinterpret_cast<CgsResource::GuiGeometryObject**>(
                reinterpret_cast<u8*>(lpUserData) + 12);

        const u32 luNumFiles = lpObject->muNumberOfFiles;
        if (luNumFiles == 0)
            return;

        const u32* lpFileTable = reinterpret_cast<const u32*>(
            static_cast<uintptr_t>(lpObject->mppGeometryFiles));

        for (u32 luFile = 0; luFile != luNumFiles; ++luFile)
        {
            CgsResource::GuiGeometryFile* lpFile =
                reinterpret_cast<CgsResource::GuiGeometryFile*>(
                    static_cast<uintptr_t>(lpFileTable[luFile]));
            if (lpFile->muNumberOfMeshes == 0)
                continue;

            const u32* lpMeshTable = reinterpret_cast<const u32*>(
                static_cast<uintptr_t>(lpFile->mppGeometryMeshes));
            for (u32 luMesh = 0; luMesh < lpFile->muNumberOfMeshes; ++luMesh)
            {
                CgsResource::GuiGeometryMesh* lpMesh =
                    reinterpret_cast<CgsResource::GuiGeometryMesh*>(
                        static_cast<uintptr_t>(lpMeshTable[luMesh]));
                if (lpMesh->miTextureId == liID)
                    lpMesh->mpTexture = static_cast<u32>(reinterpret_cast<uintptr_t>(lpAssetTexture));
            }
        }
    }

    // -------------------------------------------------------------------------
    // LoadTexture - PS3 0x5BAD58. Same walk as BindTexture, but RETURN the bound texture handle
    // (mesh+12) for the first mesh whose id matches liID; null if none match.
    // -------------------------------------------------------------------------
    AptAssetTexture AptCallbackRender::LoadTexture(AptAnimationUserData lpUserData, s32 liID)
    {
        CgsResource::GuiGeometryObject* lpObject =
            *reinterpret_cast<CgsResource::GuiGeometryObject**>(
                reinterpret_cast<u8*>(lpUserData) + 12);

        const u32 luNumFiles = lpObject->muNumberOfFiles;
        if (luNumFiles == 0)
            return nullptr;

        const u32* lpFileTable = reinterpret_cast<const u32*>(
            static_cast<uintptr_t>(lpObject->mppGeometryFiles));

        for (u32 luFile = 0; luFile != luNumFiles; ++luFile)
        {
            CgsResource::GuiGeometryFile* lpFile =
                reinterpret_cast<CgsResource::GuiGeometryFile*>(
                    static_cast<uintptr_t>(lpFileTable[luFile]));
            if (lpFile->muNumberOfMeshes == 0)
                continue;

            const u32* lpMeshTable = reinterpret_cast<const u32*>(
                static_cast<uintptr_t>(lpFile->mppGeometryMeshes));
            for (u32 luMesh = 0; luMesh < lpFile->muNumberOfMeshes; ++luMesh)
            {
                CgsResource::GuiGeometryMesh* lpMesh =
                    reinterpret_cast<CgsResource::GuiGeometryMesh*>(
                        static_cast<uintptr_t>(lpMeshTable[luMesh]));
                if (lpMesh->miTextureId == liID)
                    return reinterpret_cast<AptAssetTexture>(
                        static_cast<uintptr_t>(lpMesh->mpTexture));
            }
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // FreeTexture - PS3 0x5BADFC. FLAG PC-platform leaf: host render-callback shim -- the
    // resource system owns the texture lifetime, so there is nothing for the Apt free
    // callback to release (the console body is itself empty).
    // -------------------------------------------------------------------------
    void AptCallbackRender::FreeTexture(AptAssetTexture /*lpAssetTexture*/)
    {
    }

    // The bsearch comparator (PS3 0x5BAE94, CgsGui::_FindGeometryFile): compare the two records'
    // leading id words (`**a - **b`). Both the search key and each table entry are a one-word
    // record whose first word is the file id.
    static int FindGeometryFileCompare(const void* lpA, const void* lpB)
    {
        const s32 liA = **reinterpret_cast<const s32* const*>(lpA);
        const s32 liB = **reinterpret_cast<const s32* const*>(lpB);
        return liA - liB;
    }

    // -------------------------------------------------------------------------
    // LoadRenderingUnit - PS3 0x5BAEA8. bsearch the loaded gui-geometry object's GuiGeometryFile*
    // table for the file whose id matches lnID; return the matching GuiGeometryFile* (or null).
    // lpUserData+12 -> the GuiGeometryObject; its [8] is the file pointer table, [0] the count.
    // -------------------------------------------------------------------------
    AptAssetRenderingUnit AptCallbackRender::LoadRenderingUnit(AptAnimationUserData lpUserData,
                                                              s32 liID)
    {
        CgsResource::GuiGeometryObject* lpObject =
            *reinterpret_cast<CgsResource::GuiGeometryObject**>(
                reinterpret_cast<u8*>(lpUserData) + 12);

        // The key is a one-word record holding the requested id (the guest's `v3 = lnID; &v3`).
        s32 liKey = liID;
        const s32* lpKey = &liKey;

        // The file pointer table + count (object[8] table, object[0] count).
        const void* lpTable = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(lpObject->mppGeometryFiles));

        void* lpFound = bsearch(&lpKey, lpTable,
                                static_cast<size_t>(lpObject->muNumberOfFiles),
                                12u, &FindGeometryFileCompare);
        return reinterpret_cast<AptAssetRenderingUnit>(lpFound);
    }

    // -------------------------------------------------------------------------
    // FreeRenderingUnit - PS3 0x5BAEEC. FLAG PC-platform leaf: host render-callback shim --
    // the rendering-unit lifetime is owned by the resource system, so there is nothing for the
    // Apt free callback to release (the console body is itself empty).
    // -------------------------------------------------------------------------
    void AptCallbackRender::FreeRenderingUnit(AptAssetRenderingUnit /*lpRenderingUnit*/)
    {
    }

    // -------------------------------------------------------------------------
    // SetVertexMatrix - PS3 0x5BDF54. Fold the apt 2x2 matrix (a,b,c,d at +0..+0xC; tx,ty at
    // +0x10/+0x14) into the batch transform's screen-space basis. The guest:
    //   right/up basis (transform+0x10) = (matrix.tx, matrix.ty - originBias) ... add originBias
    //   origin       (transform+0x20)   = (matrix.a, matrix.b, matrix.c, matrix.d) ... add base
    // with the bias/base vectors loaded from a module-static (dword_8DB150). The VPU multiply uses
    // a zeroed scale lane (vspltisw v12, 0), so the multiply-add reduces to the additive bias.
    // Reconstructed as the observable in-place stores: the right/up basis takes (tx,ty) and the
    // origin row takes (a,b,c,d), each offset by the static bias the guest adds.
    // FLAG: the module-static bias vector (dword_8DB150) is not recovered from rodata here; it is
    // modelled as zero (the natural identity bias), so the basis/origin take the raw apt-matrix
    // components. The exact bias is a follow-on rodata dump (see Rodata-extraction memory).
    // -------------------------------------------------------------------------
    void AptCallbackRender::SetVertexMatrix(AptMatrix* lpAptMatrix)
    {
        CgsGraphics::Im2dTransform& lrTransform =
            AptAuxPointer::mpAptAuxInst->mRenderHandler.GetVertexTransform();

        // transform+0x10 (mOriginXYZ here) <- the translation (tx, ty) + bias (0).
        lrTransform.mOriginXYZ.x = lpAptMatrix->tx;
        lrTransform.mOriginXYZ.y = lpAptMatrix->ty;

        // transform+0x20 (mRightUp here) <- the 2x2 (a, b, c, d) + base (0).
        lrTransform.mRightUp.x = lpAptMatrix->a;
        lrTransform.mRightUp.y = lpAptMatrix->b;
        lrTransform.mRightUp.z = lpAptMatrix->c;
        lrTransform.mRightUp.w = lpAptMatrix->d;
    }

    // -------------------------------------------------------------------------
    // SetColourTransform - PS3 0x5BE228. Fold the apt colour transform into the batch transform's
    // colour shift (transform+0x30 == mColourShift) and colour scale (transform+0x40 ==
    // mColourScale). The guest copies the CXForm's translate ARGB (the four floats at CXForm+0x14:
    // skipping the helper vptr, the AdditiveColorHelper channels at +0x18..+0x24) into mColourShift,
    // the scale ARGB (CXForm+0x04..+0x10) into mColourScale, and multiply-adds them by a zeroed scale
    // lane against a splatted constant (vspltw of a module-static 1.0). Reconstructed as the
    // observable copy: mColourShift <- the translate channels, mColourScale <- the scale channels.
    // -------------------------------------------------------------------------
    void AptCallbackRender::SetColourTransform(AptCXForm* lpAptColourTransform)
    {
        CgsGraphics::Im2dTransform& lrTransform =
            AptAuxPointer::mpAptAuxInst->mRenderHandler.GetVertexTransform();

        // LANE ORDER (corrected 2026-07-30): the Im2dTransform colour rows are
        // (R, G, B, A) -- alpha is .w -- NOT the CXForm's own (A,R,G,B) order. The guest
        // ROTATES on the way in; that is what the "multiply-add against a splatted 1.0"
        // shuffle in the guest body is doing. Attestations: AptRenderHandler::Render
        // @0x82859300 takes its fully-transparent early-out on the row's +0xC, i.e. the
        // FOURTH float; and the sibling FLAPT path culls on `vspltw v0,v0,3`
        // (MovieClipInstance::Render @0x824718F0) -- both agree alpha is lane 3.
        // This TU previously wrote Alpha->.x, which made the PC Apt chain self-consistent
        // with a consumer that also read .x, so pixels came out right for the wrong
        // reason. Both ends are now on the real console convention.

        // mColourShift <- the additive (translate) channels, rotated to (R,G,B,A).
        lrTransform.mColourShift.x = lpAptColourTransform->translate.GetValuef(AptColorHelper::Red);
        lrTransform.mColourShift.y = lpAptColourTransform->translate.GetValuef(AptColorHelper::Green);
        lrTransform.mColourShift.z = lpAptColourTransform->translate.GetValuef(AptColorHelper::Blue);
        lrTransform.mColourShift.w = lpAptColourTransform->translate.GetValuef(AptColorHelper::Alpha);

        // mColourScale <- the multiplicative (scale) channels, rotated to (R,G,B,A).
        lrTransform.mColourScale.x = lpAptColourTransform->scale.GetValuef(AptColorHelper::Red);
        lrTransform.mColourScale.y = lpAptColourTransform->scale.GetValuef(AptColorHelper::Green);
        lrTransform.mColourScale.z = lpAptColourTransform->scale.GetValuef(AptColorHelper::Blue);
        lrTransform.mColourScale.w = lpAptColourTransform->scale.GetValuef(AptColorHelper::Alpha);
    }

    // -------------------------------------------------------------------------
    // SetBackgroundColour - PS3 0x5C0E2C. Set the stage clear colour, rotating the apt ARGB word
    // to the render handler's RGBA byte order: result = (argb >> 24) | (argb << 8).
    // -------------------------------------------------------------------------
    void AptCallbackRender::SetBackgroundColour(u32 luAptColour)
    {
        AptAuxPointer::mpAptAuxInst->mRenderHandler.GetBackgroundColour().m_rgba =
            (luAptColour >> 24) | (luAptColour << 8);
    }

    // -------------------------------------------------------------------------
    // DeallocateString - PS3 0x5BDF3C. Return a pooled CgsAptString to the render handler's free
    // pool (a tail-call to AptRenderHandler::DestroyAptString). The opaque pool CgsAptString* is
    // exactly what DestroyAptString takes -- no text-object access here, so this stays in the
    // opaque-CgsAptString TU. leFlags is carried for signature parity (unused by DestroyAptString).
    // -------------------------------------------------------------------------
    void AptCallbackRender::DeallocateString(AptAssetString lpAssetString, u32 /*leFlags*/)
    {
        // x64 render-data handle: map the ZID (slotIndex + 1) back to the pool slot, then free it.
        // A 0 / out-of-range id is a no-op (an unresolved / already-freed handle).
        AptRenderHandler& lrHandler = AptAuxPointer::mpAptAuxInst->mRenderHandler;
        const int liZID = static_cast<int>(reinterpret_cast<intptr_t>(lpAssetString));
        CgsAptString* lpSlot = lrHandler.ZIDToAptString(liZID);
        if (lpSlot != nullptr)
            lrHandler.DestroyAptString(lpSlot);
    }

    // -------------------------------------------------------------------------
    // GetStageWidth / GetStageHeight - PS3 0x5BE028 / 0x5BE010. The stage dimensions
    // (mAptResolution.y == width, mAptResolution.x == height).
    // -------------------------------------------------------------------------
    f32 AptCallbackRender::GetStageWidth()
    {
        return AptAuxPointer::mpAptAuxInst->mRenderHandler.GetStageWidth();
    }

    f32 AptCallbackRender::GetStageHeight()
    {
        return AptAuxPointer::mpAptAuxInst->mRenderHandler.GetStageHeight();
    }

    // -------------------------------------------------------------------------
    // AptCallbackRender::AcquireStringSlot -- the opaque-world half of AllocateString @0x5C7260
    // (see the header). AllocateString lives in the REAL-CgsAptString TU (which cannot include
    // AptRenderHandler.h / the opaque pool CgsAptString), so this half runs here: (1) free the
    // previous string when the flags say so, (2) hand out a free pool slot + its preallocated char
    // buffer, (3) surface the text-layout inputs (font collection / effect / size scale) the caller
    // forwards to CgsAptString::Prepare. Returns the pool slot as a raw void* (the caller
    // reinterprets it as the REAL CgsAptString*).
    // -------------------------------------------------------------------------
    void* AptCallbackRender::AcquireStringSlot(AptAllocateStringParameters* lpParameters,
                                               CgsUnicode::CgsUtf8** lpcStringBufferOut,
                                               const void** lppFontsOut,
                                               s32* lpiEffectOut,
                                               f32* lpfSizeScaleOut,
                                               s32* lpiZIDOut)
    {
        AptRenderHandler& lrHandler = AptAuxPointer::mpAptAuxInst->mRenderHandler;

        // Free the previous string unless one of the "keep" flag bits (0x406) is set.
        if ((lpParameters->eFlags & 0x406u) == 0)
            AptCallbackRender::DeallocateString(lpParameters->pCurrString, 0);

        // Hand out the first free pool slot + its preallocated char storage.
        void* lpSlot = lrHandler.GetUnusedAptString(lpcStringBufferOut, lpParameters->szString);

        // Surface the text-layout inputs the guest reads from the render handler.
        // An EMPTY font collection (no typeface registered yet -- the bring-up's text system
        // not loaded) is surfaced as NULL so AllocateString returns the unresolved handle
        // instead of letting FindFont's degenerate empty-slot result AV inside Prepare.
        const FontCollection* lpFonts =
            reinterpret_cast<const FontCollection*>(lrHandler.GetFontCollection());
        if (lpFonts != nullptr && lpFonts->maFonts[0].IsNull())
            lpFonts = nullptr;
        *lppFontsOut     = lpFonts;
        *lpiEffectOut    = lrHandler.GetTextEffect();
        *lpfSizeScaleOut = lrHandler.GetFontSizeScale();

        // The x64 render-data handle for this slot (slotIndex + 1); the AllocateString TU returns
        // it as the AptAssetString the engine stores in mZID (see the header note).
        *lpiZIDOut = lrHandler.AptStringToZID(reinterpret_cast<const CgsAptString*>(lpSlot));
        return lpSlot;
    }

    // -------------------------------------------------------------------------
    // AptCallbackRenderFlags::Push / Pop - PS3 0x5BA8A4 / 0x5BA8A8. Both are empty in this build
    // (the named render-flag stack is unused; the engine still calls them, so the slots exist).
    // -------------------------------------------------------------------------
    void AptCallbackRenderFlags::Push(const char* /*lpacRenderFlags*/)
    {
    }

    void AptCallbackRenderFlags::Pop(const char* /*lpacRenderFlags*/)
    {
    }
}
