#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (SetTransform/SetProgram/SetTextureState/SetTexture/RenderFromStaticVertexBuffer)
#include "rw/rwcore_structs.h"                        // rw::ResourceAllocatorRegistry::GetDefaultAllocator + rw::Resource

#include <cmath>   // std::fabs
#include <vectorintrinsics.h>

// CgsGui::AptRenderHandler member functions.
//   AptRenderHandler()    X360 0x827DFBD8
//   SetWhiteTexture(tex)  X360 0x828466E0
//   GetIm2dRendererType() X360 0x82846780
//   Render(geom, shader)  PS3  0x5CB230
//   GetUnusedAptString()  PS3  0x5BAC6C
//   DestroyAptString(str) PS3  0x5BACF8
//
// Render is the heart of the Apt rasteriser: it submits a GUI shape geometry (one Apt movie
// shape's mesh list) as a sequence of textured 2D primitive batches through the Im2d render
// buffer. The texture state per mesh is memoised in one of two HashTable<textureId,TextureState*,25>
// caches (a CLAMP-addressed one for mesh texture-mode 1, a WRAP-addressed one for mode 2; mode 0
// uses the white fallback texture). On a cache miss it builds the sampler Parameters, sizes +
// allocates the state through the RenderWare default resource allocator, Initializes it, and
// sorted-inserts it into the cache. Every command append (the batch transform, the program id,
// the texture-state bind, the static vertex draw) goes through the committed ImRenderBuffer<V>
// command API (CgsImRenderBufferTemplate.{h,cpp}); the GPU draw-flush that ultimately consumes
// those commands is the platform leaf documented (and FLAG'd) in that buffer's Dispatch path.

namespace CgsContainers
{
    template<> void HashTable<u32, renderengine::TextureState*, 25>::Insert(
        HashTable<u32, renderengine::TextureState*, 25>::Element* lpElement);
}

namespace CgsGui
{
    // -------------------------------------------------------------------------
    // AptRenderHandler() - X360 0x827DFBD8.
    // -------------------------------------------------------------------------
    // ARTIST827DFBD8: resource, hash-table and array members perform all stores.
    AptRenderHandler::AptRenderHandler()
        : mBackgroundColour(rw::RGBA::NoInitTag())
    {
    }

    // ARTIST82854430; native VMX reciprocal refinement and transforms.
    void AptRenderHandler::Construct(ImRendererSet* lpImRenderers,
        CgsGraphics::TextRenderer* lpTextRenderer, CgsLanguage::LanguageManager* lpLanguageManager,
        const FontCollection* lpFonts, f32 lfAspectRatio,
        const ::RGBA* lpAlternateTextColours, s32 liNumAlternateColours)
    {
        CGS_ASSERT(lpImRenderers != 0, "Invalid renderer sent to AptRenderHandler::Construct");
        mpImRenderers = lpImRenderers;
        CGS_ASSERT(lpTextRenderer != 0, "Invalid text renderer sent to AptRenderHandler::Construct");
        mpTextRenderer = lpTextRenderer;
        CGS_ASSERT(lpFonts != 0, "lpFonts");
        mpFontCollection = lpFonts;
        CGS_ASSERT(lpLanguageManager != 0, "Invalid language manager sent to AptRenderHandler::Construct");
        mpLanguageManager = lpLanguageManager;

        CGS_ALIGN(16) const f32 laResolution[4] = {1280.0f, 720.0f, 0.0f, 0.0f};
        const __vector4 lvResolution = __lvx(laResolution, 0);
        __stvx(lvResolution, &mAptResolution, 0);
        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lvWidth = __vspltw(lvResolution, 0);
        const __vector4 lvHeight = __vspltw(lvResolution, 1);
        __vector4 lvInverseHeight = __vrefp(lvHeight);
        for (int i = 0; i < 2; ++i)
            lvInverseHeight = __vmaddfp(lvInverseHeight,
                __vnmsubfp(lvInverseHeight, lvHeight, lvOne), lvInverseHeight);
        CGS_ALIGN(16) f32 laRatio[4];
        __stvx(__vmulfp(lvInverseHeight, lvWidth), laRatio, 0);
        const f32 lfScale = lfAspectRatio / laRatio[0];
        CGS_ALIGN(16) const f32 laInputs[4] = {1.0f / 1280.0f, -1.0f / 720.0f, lfScale, 2.0f};
        const __vector4 lvInputs = __lvx(laInputs, 0);
        const __vector4 lvInvWidth = __vspltw(lvInputs, 0);
        const __vector4 lvNegHeight = __vmulfp(__vspltw(lvInputs, 1), __vspltw(lvInputs, 2));
        static const CGS_ALIGN(16) u32 kauMaskA[4] = {0x08090A0B, 0x1C1D1E1F, 0x00010203, 0x00010203};
        static const CGS_ALIGN(16) u32 kauMaskB[4] = {0x00010203, 0x00010203, 0x00010203, 0x14151617};
        const __vector4 lvA = __vperm(lvInvWidth, lvNegHeight, __lvx(kauMaskA, 0));
        const __vector4 lvB = __vperm(lvInvWidth, lvNegHeight, __lvx(kauMaskB, 0));
        __stvx(__vmulfp(__vsldoi(lvB, lvA, 8), __vspltw(lvInputs, 3)), &mPlatformTransform, 0);
        CGS_ASSERT(mPlatformTransform.x == mPlatformTransform.x && mPlatformTransform.y == mPlatformTransform.y &&
                   mPlatformTransform.z == mPlatformTransform.z && mPlatformTransform.w == mPlatformTransform.w,
                   "IsValid(mPlatformTransform)");
        const __vector4 lvHalf = __vcfsx(__vspltisw(1), 1);
        __stvx(__vmulfp(lvResolution, lvHalf), &mOriginTransform, 0);
        CGS_ASSERT(mOriginTransform.x == mOriginTransform.x && mOriginTransform.y == mOriginTransform.y &&
                   mOriginTransform.z == mOriginTransform.z && mOriginTransform.w == mOriginTransform.w,
                   "IsValid(mOriginTransform)");
        for (u32 i = 0; i < KU_NUM_APT_STRINGS; ++i)
        {
            mabUnusedAptStrings[i] = 1;
            maAptStrings[i].Construct(lpAlternateTextColours, liNumAlternateColours);
        }
        maWrapTextureStateTableElements.Construct();
        mWrapTextureStateTable.Init();
        maClampTextureStateTableElements.Construct();
        mClampTextureStateTable.Init();
        miTextEffect = 0;
        mfFontSizeScale = 1.0f;
        SetWhiteTexture(CgsGraphics::ImRendererBase::GetStateLibrary().mpTexture_White);
    }

    // -------------------------------------------------------------------------
    // SetWhiteTexture - X360 0x828466E0.
    // -------------------------------------------------------------------------
    void AptRenderHandler::SetWhiteTexture(renderengine::Texture* lpWhiteTexture)
    {
        CGS_ASSERT(lpWhiteTexture != 0,
                   "Invalid texture pointer sent to AptRenderHandler::SetWhiteTexture");
        mpWhiteTexture = lpWhiteTexture;
    }

    // -------------------------------------------------------------------------
    // GetIm2dRendererType - X360 0x82846780. Asserts the renderer-set pointer (guest +108588) is
    // non-null, then returns **(this+108588) -- the set's leading slot, i.e. the active 2D renderer
    // (Im2dRenderBuffer) BASE POINTER (NOT a type word; the name is the verbatim EA symbol). The
    // asm: `lwz r11, 0(this+108588)` (set ptr) then `lwz r3, 0(r11)` (renderer base ptr); returns r3.
    // -------------------------------------------------------------------------
    AptIm2dRenderBuffer* AptRenderHandler::GetIm2dRendererType() const
    {
        CGS_ASSERT(mpImRenderers != 0, "mpImRenderers");
        return mpImRenderers->mpIm2dRenderBuffer;
    }

    // ARTIST82859704..798 / 82859844..8D0: key is the native texture pointer.
    static renderengine::TextureState* ResolveTextureState(
        AptRenderHandler::TextureStateTable& lrTable,
        Array<AptRenderHandler::TextureStateElement, 256>& lrElements,
        renderengine::Texture* lpTexture, bool lbClamp)
    {
        const u32 luKey = static_cast<u32>(reinterpret_cast<uintptr_t>(lpTexture));
        renderengine::TextureState** lppState = lrTable.Get(luKey);
        if (lppState)
            return *lppState;
        renderengine::TextureState::Parameters lParams;
        lParams.muAddressU = lbClamp ? 2 : 0;
        lParams.muAddressV = lbClamp ? 2 : 0;
        lParams.muMagFilter = 1;
        lParams.muMinFilter = 1;
        lParams.mpTexture = lpTexture;
        rw::IResourceAllocator* lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();
        rw::ResourceDescriptor lDescriptor;
        renderengine::TextureState::GetResourceDescriptor(&lDescriptor);
        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        renderengine::TextureState* lpState = renderengine::TextureState::Initialize(&lResource, &lParams);
        AptRenderHandler::TextureStateElement lElement;
        lrElements.Append(lElement);
        AptRenderHandler::TextureStateElement* lpElement = &lrElements[lrElements.GetLength() - 1];
        lpElement->Set(luKey, lpState);
        lrTable.Insert(lpElement);
        return lpState;
    }

    // -------------------------------------------------------------------------
    // Render - PS3 0x5CB230.
    // -------------------------------------------------------------------------
    void AptRenderHandler::Render(CgsResource::GuiGeometryFile* lpGeometry, int liShaderIndex)
    {
        CgsDev::PerfMonCpu::StartMonitor(BrnGui::GuiPerfmons::miCgsAptFlashRender);
        // Validate the render set up front (the X360 fires these five debug asserts BEFORE the
        // alpha early-out): the set pointer (this+108588), the 2D renderer (*set), and the 3D
        // renderer (*(set+16)) must all be non-null. (The duplicate "mpImRenderers" asserts are the
        // inlined GetIm2dRendererType null-checks the guest emits at CgsAptRender.h:424/443.)
        CGS_ASSERT(mpImRenderers != 0, "Invalid renderer set in AptRenderHandler::Render");
        CGS_ASSERT(mpImRenderers != 0, "mpImRenderers");
        CGS_ASSERT(mpImRenderers->mpIm2dRenderBuffer != 0, "Invalid 2D renderer in AptRenderHandler::Render");
        CGS_ASSERT(mpImRenderers != 0, "mpImRenderers");
        CGS_ASSERT(mpImRenderers->mpIm3dRenderBufferMenusAndHud != 0, "Invalid 3D renderer in AptRenderHandler::Render");

        // Skip a fully-transparent batch: the colour-scale ALPHA below the epsilon means nothing
        // this batch draws is visible. Channel order is (Red, Green, Blue, Alpha) in
        // (x, y, z, w) -- alpha is .w. CORRECTED 2026-07-30: this read .x, so the test was
        // gating on RED, and any batch that merely had no red was skipped as if it were
        // fully transparent. AptCallbackRender::SetColourTransform does NOT copy the CXForm
        // through unchanged -- the CXForm's mfVal[0..3] are (A,R,G,B) and it explicitly
        // ROTATES them into (R,G,B,A). The X360 original reads +0xC here, i.e. the fourth
        // lane, which is the direct attestation. Full evidence: CgsIm2d.cpp's
        // FoldIm2dColourChannel note.
        if (!(std::fabs(mVertexTransform.mColourScale.w) >= 0.00000011920929f))
        {
            CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miCgsAptFlashRender);
            return;
        }

        // Fetch the active 2D renderer base (GetIm2dRendererType == **(this+108588)) and reach its
        // command buffer at `base + 4` (the ImRenderBuffer<V> base). The guest stamps the transform on the base
        // (Im2dRenderBuffer::SetTransform, which itself does +4) and the per-mesh ops on base+4
        // (v18 = ret + 4); the committed ImRenderBuffer<V> template is that base+4 command object, so
        // every command here goes through it. REUSE: the committed ImRenderBuffer<V> command API.
        AptIm2dRenderBuffer* lpRenderer = GetIm2dRendererType();
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpBuffer =
            lpRenderer;

        // Stamp the per-batch transform (the inlined 80-byte SET_TRANSFORM command writer).
        lpRenderer->SetTransform(mVertexTransform);

        // Bind the shader program for this batch (sub_4F6DB4 == ImRenderBuffer<V>::SetProgram).
        lpBuffer->SetProgram(static_cast<s8>(liShaderIndex));

        // The per-mesh primitive topology (X360 v37 @ [sp+50h]). The guest initialises it to 4
        // (0x82859548: `li r10, 4; stw r10, var_250`) BEFORE the loop and carries it across
        // iterations, only restoring it where the meshtype dispatch stores -- so a meshtype that
        // hits the assert leaves it at its prior value (the init 4, or the previous mesh's value).
        renderengine::PrimitiveType lePrimitiveType = static_cast<renderengine::PrimitiveType>(4);

        for (u32 luMesh = 0; luMesh < lpGeometry->muNumberOfMeshes; ++luMesh)
        {
            CgsResource::GuiGeometryMesh* lpMesh =
                lpGeometry->mppGeometryMeshes[luMesh];
            CGS_ASSERT(lpMesh != 0, "Invalid Mesh in AptRenderHandler::Render");

            // Pick the primitive topology from the mesh type (miMeshType == mesh+0). The X360
            // (0x82859300 @0x8285962C) dispatch stores: type 0 -> 4, type 1 -> 6 (TRIANGLESTRIPS),
            // type 2 -> 2 (LINES). A type >= 3 fires the "Unexpected meshtype" assert and stores
            // NOTHING -- the asm branches past the store (0x8285965C `b loc_82859678`), so the
            // primitive type is LEFT UNCHANGED (it keeps whatever the prior iteration / the init 4
            // left in v37). Faithfully reproduced: assert, then leave lePrimitiveType alone.
            const s32 liMeshType = lpMesh->mMeshHeader.miMeshType;
            if (liMeshType == 0)
            {
                lePrimitiveType = static_cast<renderengine::PrimitiveType>(4);
            }
            else if (liMeshType == 1)
            {
                lePrimitiveType = static_cast<renderengine::PrimitiveType>(6); // PRIMITIVETYPE_TRIANGLESTRIPS
            }
            else if (static_cast<u32>(liMeshType) < 3u)
            {
                lePrimitiveType = static_cast<renderengine::PrimitiveType>(2); // PRIMITIVETYPE_LINES (type 2)
            }
            else
            {
                // meshType >= 3: assert and leave lePrimitiveType at its prior value (the asm
                // skips the v37 store on this path).
                CGS_ASSERT(static_cast<u32>(liMeshType) < 3u, "Unexpected meshtype in AptRenderHandler::Render");
            }

            // Resolve the bound texture/state from the mesh's texture mode (miTextureMode == mesh+4):
            //   1 -> CLAMP-addressed cache, 2 -> WRAP-addressed cache, 0 -> white fallback texture,
            //   >= 3 -> invalid. (X360 routes mode 1/2 through SetState(TextureState*), mode 0
            //   through SetTexture(white).)
            const s32 liTextureMode = lpMesh->mMeshHeader.miTextureMode;

            if (liTextureMode == 1 || liTextureMode == 2)
            {
                renderengine::Texture* lpTexture = lpMesh->mMeshHeader.mpTexture;
                CGS_ASSERT(lpTexture != 0, "lpGuiMesh->mMeshHeader.mpTexture!=NULL");
                renderengine::TextureState* lpState = liTextureMode == 1
                    ? ResolveTextureState(mClampTextureStateTable, maClampTextureStateTableElements, lpTexture, true)
                    : ResolveTextureState(mWrapTextureStateTable, maWrapTextureStateTableElements, lpTexture, false);
                lpBuffer->SetState(lpState);
            }
            else if (liTextureMode == 0)
            {
                // SetTexture: bind the white fallback (guest +128).
                lpBuffer->SetTexture(mpWhiteTexture);
            }
            else
            {
                CGS_ASSERT(static_cast<u32>(liTextureMode) < 3u, "Unexpected texturemode in AptRenderHandler::Render");
            }

            const CgsGraphics::Basic2dColouredTexturedVertex* lpVertices =
                lpMesh->mppVerticies[0];
            lpBuffer->RenderFromStaticVertexBuffer(lePrimitiveType, lpVertices,
                                                   lpMesh->muNumberOfVerticies);
        }

        // Reset the program to 0 at the end of the batch (sub_4F6DB4 with 0).
        lpBuffer->SetProgram(static_cast<s8>(0));
        CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miCgsAptFlashRender);
    }

    // -------------------------------------------------------------------------
    // GetUnusedAptString - PS3 0x5BAC6C. Scan the 256-slot pool for the first FREE slot (flag
    // != 0), stamp its preallocated char pointer through lpcStringPointer, mark it in-use
    // (flag = 0), and return the slot; null when the pool is exhausted.
    // -------------------------------------------------------------------------
    CgsAptString* AptRenderHandler::GetUnusedAptString(CgsUnicode::CgsUtf8** lpcStringPointer,
                                                       const char* /*lpcStringToAllocate*/)
    {
        for (u32 luSlot = 0; luSlot < KU_NUM_APT_STRINGS; ++luSlot)
        {
            if (mabUnusedAptStrings[luSlot])
            {
                // Hand back this slot's preallocated char storage + the slot itself, and claim it.
                // (x64: the storage is the slot's inline 256-byte buffer -- see the header note.)
                *lpcStringPointer = reinterpret_cast<CgsUnicode::CgsUtf8*>(maacAptStringChars[luSlot]);
                mabUnusedAptStrings[luSlot] = 0;
                return &maAptStrings[luSlot];
            }
        }
        return 0;
    }

    // -------------------------------------------------------------------------
    // DestroyAptString - PS3 0x5BACF8. Find which slot lpAptString is and mark it free (flag = 1).
    // -------------------------------------------------------------------------
    void AptRenderHandler::DestroyAptString(CgsAptString* lpAptString)
    {
        for (u32 luSlot = 0; luSlot < KU_NUM_APT_STRINGS; ++luSlot)
        {
            if (&maAptStrings[luSlot] == lpAptString)
            {
                mabUnusedAptStrings[luSlot] = 1;
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // ZID <-> pool-slot bridge (x64 render-data handle scheme; see the header note).
    // The pool array maAptStrings is contiguous, so the ZID is (slotIndex + 1) -- 0 stays
    // the engine's "unresolved handle" value (AptRenderItemDynamicText::mZID == 0).
    // -------------------------------------------------------------------------
    int AptRenderHandler::AptStringToZID(const CgsAptString* lpAptString) const
    {
        if (lpAptString == 0)
            return 0;
        const CgsAptString* lpBase = &maAptStrings[0];
        if (lpAptString < lpBase || lpAptString >= lpBase + KU_NUM_APT_STRINGS)
            return 0;
        return static_cast<int>(lpAptString - lpBase) + 1;
    }

    CgsAptString* AptRenderHandler::ZIDToAptString(int liZID)
    {
        if (liZID <= 0 || static_cast<u32>(liZID) > KU_NUM_APT_STRINGS)
            return 0;
        return &maAptStrings[static_cast<u32>(liZID) - 1u];
    }
}
