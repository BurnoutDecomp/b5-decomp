#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (SetTransform/SetProgram/SetTextureState/SetTexture/RenderFromStaticVertexBuffer)
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState (Initialize + GetResourceDescriptor)
#include "rw/rwcore_structs.h"                        // rw::ResourceAllocatorRegistry::GetDefaultAllocator + rw::Resource

#include <cmath>   // std::fabs

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

namespace CgsGui
{
    // -------------------------------------------------------------------------
    // TextureStateCache::InsertSorted - the ordered insert AptRenderHandler::Render inlines.
    // Bump-allocate the next free node from the cache's external pool, stamp {key, state}, and
    // splice it into its bin (key % 25) keeping the bin sorted ascending by key. This reproduces
    // the guest's compare-and-(InternalAddHead / InternalAddTail / InternalAddBefore) chain:
    //   - empty bin            -> InternalAddHead
    //   - key > tail's key     -> InternalAddTail
    //   - otherwise walk to the first node whose key >= key and InternalAddBefore it
    // (Render reads each candidate's key via the node+8 slot; here it is node->mData.mKey.)
    // -------------------------------------------------------------------------
    renderengine::TextureState* AptRenderHandler::TextureStateCache::InsertSorted(
        u32 luKey, renderengine::TextureState* lpState)
    {
        // Bump-allocate the node from the pool (the guest's "16 * count++" + write-back of count).
        Node* lpNode = &maNodePool[muNodeCount];
        ++muNodeCount;
        lpNode->mData.mKey   = luKey;
        lpNode->mData.mValue = lpState;

        Table::Bin& lrBin = mBins.GetBin(luKey);

        CgsContainers::BaseLinkedListNode* lpHead = lrBin.InternalGetHead();
        if (!lpHead)
        {
            lrBin.InternalAddHead(lpNode);
            return lpState;
        }

        // tail's key < new key  => append at the tail.
        Node* lpTail = static_cast<Node*>(lrBin.InternalGetTail());
        if (lpTail->mData.mKey < luKey)
        {
            lrBin.InternalAddTail(lpNode);
            return lpState;
        }

        // Walk from the head to the first node whose key is >= the new key, insert before it.
        while (static_cast<Node*>(lpHead)->mData.mKey <= luKey)
        {
            CgsContainers::BaseLinkedListNode* lpNext = lpHead->GetNextNode();
            lpHead = lpNext;
            if (!lpNext)
                return lpState;   // reached the end mid-walk (guest's null-mpNext early-out)
        }
        lrBin.InternalAddBefore(lpHead, lpNode);
        return lpState;
    }

    // -------------------------------------------------------------------------
    // AptRenderHandler() - X360 0x827DFBD8.
    // -------------------------------------------------------------------------
    AptRenderHandler::AptRenderHandler()
        : mpWhiteTexture(nullptr)
        , mpImRenderers(nullptr)
    {
        // Both per-shape texture-state caches start empty (HashTable bins live, pool empty).
        mWrapTextureCache.Init();
        mClampTextureCache.Init();

        // The batch transform starts cleared (the guest's leading zeroed dword runs).
        mVertexTransform.mOriginXYZ.SetZero();
        mVertexTransform.mRightUp.SetZero();
        mVertexTransform.mColourShift.SetZero();
        mVertexTransform.mColourScale.SetZero();

        // Every AptString slot starts FREE (flag != 0 means available). The slot char-pointer
        // table starts cleared.
        for (u32 luIndex = 0; luIndex < KU_NUM_APT_STRINGS; ++luIndex)
        {
            mabUnusedAptStrings[luIndex] = 1;
            maacAptStringChars[luIndex]  = 0;
        }
    }

    // -------------------------------------------------------------------------
    // SetWhiteTexture - X360 0x828466E0.
    // -------------------------------------------------------------------------
    void AptRenderHandler::SetWhiteTexture(void* lpWhiteTexture)
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
        return mpImRenderers->mpIm2dRenderer;
    }

    // -------------------------------------------------------------------------
    // Build the sampler Parameters for one mesh's texture state. luTextureId is the GuiTexture id
    // the mesh references (the cache key + the bound raster); lbClampAddressing picks the addressU/V
    // value (mesh texture-mode 1 => the CLAMP-cache fill, mode 2 => the WRAP-cache fill). The two
    // fills are byte-identical apart from addressU/addressV. Decompiled verbatim from the v44 (mode-1)
    // and v51 (mode-2) sampler-Parameters store blocks in AptRenderHandler::Render -- X360 ARTIST
    // 0x82859300 @ 0x82859844 (mode 1, cache this+104176) and @0x82859704 (mode 2, cache this+99772).
    //
    // The numeric values below are transcribed directly from those X360 stores (the field index ==
    // the guest's word index into the 16-word Parameters block; see renderstates.h):
    //   addressU/addressV (word 0/1) : mode-1 fill stores 2, mode-2 fill stores 0  (the ONLY field
    //                                  that differs between the two caches; r25==2 vs r31==0)
    //   addressW          (word 2)   : 0
    //   magFilter         (word 3)   : 1     minFilter (word 4) : 1
    //   mipFilter         (word 5)   : 2     (r25; NOTE the Font path 0x82835658 stores 1 here -- the
    //                                        Apt path differs, it stores 2)
    //   maxAnisotropy     (word 8)   : 13    (r22 == 0xD)
    //   word10            (word 10)  : 1
    //   mipLodBias        (word 11)  : flt_82001CC0 == 0.0f (verified from the decrypted ARTIST XEX
    //                                  rodata; the Font path uses flt_82004C78 == -0.5f instead)
    //   word12            (word 12)  : flt_82001CC0 == 0.0f
    //   words 6,7,9,13,14,15         : 0
    //   field40..42                  : 0     field43,field44 : 1
    //   mpTexture                    : the GuiTexture id (the raster the import resolves; the guest
    //                                  stores the id at word offset 18, here Parameters::mpTexture)
    //
    // The committed PC consumer (renderengine::TextureState::Initialize, texturestate.cpp) memcpy's
    // the leading 32-byte sampler block (words 0..7: addressU/V/W, mag/min/mipFilter, words 6/7) into
    // the TextureState and binds mpTexture -- so reproducing those exact words makes the produced PC
    // state genuinely identical to the block the console marshals. (On X360 the same words are
    // marshalled into a Xenos GPU sampler descriptor; that GPU marshal is the platform leaf documented
    // in texturestate.cpp -- NOT reproduced here.) The SamplerState enum NAMES the console source used
    // for these literals are not recovered, so they are given as the authoritative integers, not by
    // a guessed enum constant.
    // -------------------------------------------------------------------------
    static void BuildTextureStateParameters(renderengine::TextureState::Parameters* lpParamsOut,
                                            u32 luTextureId, bool lbClampAddressing)
    {
        // addressU/addressV: the mode-1 (CLAMP cache) fill stores 2, the mode-2 (WRAP cache) fill
        // stores 0. This is the sole field that differs between the guest's two fill blocks.
        const u32 luAddress = lbClampAddressing ? 2u : 0u;
        lpParamsOut->muAddressU      = luAddress;  // word 0
        lpParamsOut->muAddressV      = luAddress;  // word 1
        lpParamsOut->muAddressW      = 0u;   // word 2
        lpParamsOut->muMagFilter     = 1u;   // word 3
        lpParamsOut->muMinFilter     = 1u;   // word 4
        lpParamsOut->muMipFilter     = 2u;   // word 5  (Apt fill stores 2; Font path stores 1)
        lpParamsOut->muField6        = 0u;   // word 6
        lpParamsOut->muField7        = 0u;   // word 7
        lpParamsOut->muMaxAnisotropy = 13u;  // word 8  (r22 == 0xD)
        lpParamsOut->muField9        = 0u;   // word 9
        lpParamsOut->muField10       = 1u;   // word 10
        lpParamsOut->mfMipLodBias    = 0.0f; // word 11 (flt_82001CC0 == 0.0f)
        lpParamsOut->mfField12       = 0.0f; // word 12 (flt_82001CC0 == 0.0f)
        lpParamsOut->muField13       = 0u;   // word 13
        lpParamsOut->muField14       = 0u;   // word 14
        lpParamsOut->muField15       = 0u;   // word 15
        lpParamsOut->mu8Field40      = 0u;
        lpParamsOut->mu8Field41      = 0u;
        lpParamsOut->mu8Field42      = 0u;
        lpParamsOut->mu8Field43      = 1u;
        lpParamsOut->mu8Field44      = 1u;
        // The bound raster: the GuiTexture id reinterpreted as the raster the import resolves
        // (the guest stores `v50/v57 = textureId` then Initialize binds it as the state's raster).
        lpParamsOut->mpTexture = reinterpret_cast<renderengine::Texture*>(
            static_cast<uintptr_t>(luTextureId));
    }

    // -------------------------------------------------------------------------
    // Resolve (and memoise) the texture state for one mesh: look the texture id up in the cache;
    // on a miss build the Parameters, size the state, allocate it through the RW default resource
    // allocator, Initialize it, and sorted-insert it into the cache. Returns the resolved state.
    // Decompiled from the two (mode-1 / mode-2) miss paths in AptRenderHandler::Render @0x5CB230
    // (identical apart from the cache + the address mode).
    // -------------------------------------------------------------------------
    static renderengine::TextureState* ResolveTextureState(
        AptRenderHandler::TextureStateCache& lrCache, u32 luTextureId, bool lbClampAddressing)
    {
        renderengine::TextureState* lpCached = lrCache.Find(luTextureId);
        if (lpCached)
            return lpCached;

        // ---- cache miss: build the sampler params ----
        renderengine::TextureState::Parameters lParams;
        BuildTextureStateParameters(&lParams, luTextureId, lbClampAddressing);

        // Size the texture-state resource (the guest's GetResourceDescriptor + the descriptor the
        // allocator consumes). renderengine::TextureState::GetResourceDescriptor writes a 10-word
        // (5-entry {size,align}) descriptor.
        u32 laDescriptor[10];
        renderengine::TextureState::GetResourceDescriptor(laDescriptor);

        // Allocate the backing resource through the RenderWare default allocator (the guest's
        // rw::ResourceAllocatorRegistry::GetDefaultAllocator() then the allocator vtable slot
        // DoAllocate). The descriptor's slot-0 {size, alignment} drives the carve.
        rw::IResourceAllocator* lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();
        rw::ResourceDescriptor lAllocDescriptor;
        lAllocDescriptor.m_baseResourceDescriptors[0].m_size      = laDescriptor[0];
        lAllocDescriptor.m_baseResourceDescriptors[0].m_alignment = laDescriptor[1];
        lAllocDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;
        rw::Resource lResource = lpAllocator->DoAllocate(lAllocDescriptor, nullptr);

        // Initialize the sampler+raster state in the carved memory (the guest's
        // renderengine::TextureState::Initialize). [PC: the committed Initialize keeps the sampler
        // config + bound raster for draw-time application instead of marshalling a Xenos GPU
        // descriptor -- see texturestate.cpp.]
        renderengine::TextureState* lpState = renderengine::TextureState::Initialize(
            reinterpret_cast<rw::Resource*>(lResource.m_baseResources[0]), &lParams);

        // Memoise it (the ordered insert into the bin key % 25).
        lrCache.InsertSorted(luTextureId, lpState);
        return lpState;
    }

    // -------------------------------------------------------------------------
    // Render - PS3 0x5CB230.
    // -------------------------------------------------------------------------
    void AptRenderHandler::Render(CgsResource::GuiGeometryFile* lpGeometry, int liShaderIndex)
    {
        // Validate the render set up front (the X360 fires these five debug asserts BEFORE the
        // alpha early-out): the set pointer (this+108588), the 2D renderer (*set), and the 3D
        // renderer (*(set+16)) must all be non-null. (The duplicate "mpImRenderers" asserts are the
        // inlined GetIm2dRendererType null-checks the guest emits at CgsAptRender.h:424/443.)
        CGS_ASSERT(mpImRenderers != 0, "Invalid renderer set in AptRenderHandler::Render");
        CGS_ASSERT(mpImRenderers != 0, "mpImRenderers");
        CGS_ASSERT(mpImRenderers->mpIm2dRenderer != 0, "Invalid 2D renderer in AptRenderHandler::Render");
        CGS_ASSERT(mpImRenderers != 0, "mpImRenderers");
        CGS_ASSERT(mpImRenderers->mp3dRenderer != 0, "Invalid 3D renderer in AptRenderHandler::Render");

        // Skip a fully-transparent batch: the colour-scale alpha (mColourScale.w == mColourScale
        // + 0xC) below the epsilon means nothing this batch draws is visible.
        if (std::fabs(mVertexTransform.mColourScale.w) < 0.00000011920929f)
            return;

        // Fetch the active 2D renderer base (GetIm2dRendererType == **(this+108588)) and reach its
        // command buffer at `base + 4` (mCommandBuffer). The guest stamps the transform on the base
        // (Im2dRenderBuffer::SetTransform, which itself does +4) and the per-mesh ops on base+4
        // (v18 = ret + 4); the committed ImRenderBuffer<V> template is that base+4 command object, so
        // every command here goes through it. REUSE: the committed ImRenderBuffer<V> command API.
        AptIm2dRenderBuffer* lpRenderer = GetIm2dRendererType();
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpBuffer =
            &lpRenderer->mCommandBuffer;

        // Stamp the per-batch transform (the inlined 80-byte SET_TRANSFORM command writer).
        lpBuffer->SetTransform(mVertexTransform);

        // Bind the shader program for this batch (sub_4F6DB4 == ImRenderBuffer<V>::SetProgram).
        lpBuffer->SetProgram(static_cast<s8>(liShaderIndex));

        // Walk the geometry's meshes. mppGeometryMeshes is a pointer table (one entry per mesh);
        // each entry points at a GuiGeometryMesh header {miMeshType, miTextureMode, miTextureId/
        // mpTexture, mpTexturePtr, muNumberOfVerticies, mppVerticies}. (The console reads the table
        // as 32-bit words; on the x64 host the resource fix-up has already rebased them into real
        // pointers, so they are read through the typed table.)
        const u32* lpMeshTable = reinterpret_cast<const u32*>(
            static_cast<uintptr_t>(lpGeometry->mppGeometryMeshes));

        // The per-mesh primitive topology (X360 v37 @ [sp+50h]). The guest initialises it to 4
        // (0x82859548: `li r10, 4; stw r10, var_250`) BEFORE the loop and carries it across
        // iterations, only restoring it where the meshtype dispatch stores -- so a meshtype that
        // hits the assert leaves it at its prior value (the init 4, or the previous mesh's value).
        renderengine::PrimitiveType lePrimitiveType = static_cast<renderengine::PrimitiveType>(4);

        for (u32 luMesh = 0; luMesh < lpGeometry->muNumberOfMeshes; ++luMesh)
        {
            CgsResource::GuiGeometryMesh* lpMesh =
                reinterpret_cast<CgsResource::GuiGeometryMesh*>(
                    static_cast<uintptr_t>(lpMeshTable[luMesh]));
            CGS_ASSERT(lpMesh != 0, "Invalid Mesh in AptRenderHandler::Render");

            // Pick the primitive topology from the mesh type (miMeshType == mesh+0). The X360
            // (0x82859300 @0x8285962C) dispatch stores: type 0 -> 4, type 1 -> 6 (TRIANGLESTRIPS),
            // type 2 -> 2 (LINES). A type >= 3 fires the "Unexpected meshtype" assert and stores
            // NOTHING -- the asm branches past the store (0x8285965C `b loc_82859678`), so the
            // primitive type is LEFT UNCHANGED (it keeps whatever the prior iteration / the init 4
            // left in v37). Faithfully reproduced: assert, then leave lePrimitiveType alone.
            const s32 liMeshType = lpMesh->miMeshType;
            if (liMeshType == 0)
            {
                lePrimitiveType = static_cast<renderengine::PrimitiveType>(4);
            }
            else if (liMeshType == 1)
            {
                lePrimitiveType = static_cast<renderengine::PrimitiveType>(6); // PRIMITIVETYPE_TRIANGLESTRIPS
            }
            else if (liMeshType < 3)
            {
                lePrimitiveType = static_cast<renderengine::PrimitiveType>(2); // PRIMITIVETYPE_LINES (type 2)
            }
            else
            {
                // meshType >= 3: assert and leave lePrimitiveType at its prior value (the asm
                // skips the v37 store on this path).
                CGS_ASSERT(liMeshType < 3, "Unexpected meshtype in AptRenderHandler::Render");
            }

            // Resolve the bound texture/state from the mesh's texture mode (miTextureMode == mesh+4):
            //   1 -> CLAMP-addressed cache, 2 -> WRAP-addressed cache, 0 -> white fallback texture,
            //   >= 3 -> invalid. (X360 routes mode 1/2 through SetState(TextureState*), mode 0
            //   through SetTexture(white).)
            const s32 liTextureMode = lpMesh->miTextureMode;
            const u32 luTextureId   = lpMesh->mpTexture;   // mesh+12 (the GuiTexture id / raster key)

            if (liTextureMode == 1)
            {
                CGS_ASSERT(luTextureId != 0, "lpGuiMesh->mMeshHeader.mpTexture!=NULL");
                renderengine::TextureState* lpState =
                    ResolveTextureState(mClampTextureCache, luTextureId, /*clamp*/ true);
                lpBuffer->SetTextureState(lpState);   // SetState == 16-byte SET_STATE_TEXTURE command
            }
            else if (liTextureMode == 2)
            {
                CGS_ASSERT(luTextureId != 0, "lpGuiMesh->mMeshHeader.mpTexture!=NULL");
                renderengine::TextureState* lpState =
                    ResolveTextureState(mWrapTextureCache, luTextureId, /*clamp*/ false);
                lpBuffer->SetTextureState(lpState);
            }
            else if (liTextureMode == 0)
            {
                // SetTexture: bind the white fallback (guest +128).
                lpBuffer->SetTexture(static_cast<renderengine::Texture*>(mpWhiteTexture));
            }
            else
            {
                CGS_ASSERT(liTextureMode < 3, "Unexpected texturemode in AptRenderHandler::Render");
            }

            // Draw the mesh's static vertex run (the vertices already live in the resource's vertex
            // buffer, so RenderFromStaticVertexBuffer points straight at them -- no copy). The X360
            // reads the run pointer as **(mesh+20): mppVerticies is a pointer table whose first
            // entry is the vertex run; muNumberOfVerticies (mesh+16) is the count.
            u32* const* lppVertexTable = reinterpret_cast<u32* const*>(
                static_cast<uintptr_t>(lpMesh->mppVerticies));
            const CgsGraphics::Basic2dColouredTexturedVertex* lpVertices =
                reinterpret_cast<const CgsGraphics::Basic2dColouredTexturedVertex*>(*lppVertexTable);
            lpBuffer->RenderFromStaticVertexBuffer(lePrimitiveType, lpVertices,
                                                   lpMesh->muNumberOfVerticies);
        }

        // Reset the program to 0 at the end of the batch (sub_4F6DB4 with 0).
        lpBuffer->SetProgram(static_cast<s8>(0));
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
                *lpcStringPointer = reinterpret_cast<CgsUnicode::CgsUtf8*>(
                    static_cast<uintptr_t>(maacAptStringChars[luSlot]));
                mabUnusedAptStrings[luSlot] = 0;
                return &maAptStrings[luSlot];
            }
        }
        return nullptr;
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
}
