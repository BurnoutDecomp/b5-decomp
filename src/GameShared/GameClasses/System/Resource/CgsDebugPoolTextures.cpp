#include "GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h"

#include <cstdlib>   // qsort (the per-frame entry sort)

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                           // CgsCore::SPrintf
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"               // CgsResource::Pool / Entry / Type / SmallResource
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"               // CgsResource::Type::GetCachedId
#include "GameShared/GameClasses/Graphics/CgsModel.h"                             // CgsGraphics::Model::UsesTexture
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"      // CgsDev::DebugUI::Metrics
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // Debug2DImmediateRender, RGBA
#include "pc/gcm/renderengine/texture.h"                                          // renderengine::Texture::GetWidth/GetHeight
#include "rw/rwcore_structs.h"                                                    // rw::IResourceAllocator / Resource / ResourceDescriptor

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Semantic parity, not byte-match. Source-of-truth is the
// X360 pseudocode/asm for behaviour + calling convention; the DecFIGS DWARF for declaration shape.
//
// The selected file/line strings in the assert sites are the ones the X360 .rdata carries (the
// original d:\p4\b5_main\... source path); CGSDPT_FIRE_ASSERT preserves them rather than substituting
// __FILE__/__LINE__, matching the project's "keep the original assert coordinates" precedent.
#define CGSDPT_FIRE_ASSERT(condition, msg, file, line)            \
    do                                                            \
    {                                                             \
        if (!(condition))                                         \
        {                                                         \
            CgsDev::Assert::BeginAssert();                        \
            CgsDev::Assert::FireAssert((msg), (file), (line));    \
            CgsDev::Assert::EndAssert();                          \
        }                                                         \
    } while (0)

namespace
{
    const char* const KPC_SOURCE_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsDebugPoolTextures.cpp";

    // The pool stores its in-use bit at bit 1 of the per-entry status byte (the X360 `& 2` test).
    const u8  KU8_ENTRY_STATUS_INUSE = 2;

    // Resource-type registry ids the texture-pool browser keys off (X360 cmpwi cr6, typeId, {0,0x23,0x2A}).
    const u32 KU_TYPEID_TEXTURE  = 0;     // a texture pool entry (Update only registers these)
    const u32 KU_TYPEID_INSTANCE = 35;    // 0x23: an instance-list entry
    const u32 KU_TYPEID_MODEL    = 42;    // 0x2A: a model entry

    // Render layout constants (X360 .rdata floats).
    const f32 KF_TEXT_LINE_STEP = 12.0f;   // flt_82013FB0  vertical step between stat lines
    const f32 KF_TEXT_SCALE     = 12.0f;   // flt_82004A20  stat-line text scale (== flt_82013FB0)
    const f32 KF_DISPLAY_INSET  = 100.0f;  // flt_820049E0  texture-display box inset from the window edges

    // The X360 packs an opaque-black text colour (li r8, 0xFF000000 == -16777216) for the stat lines.
    const CgsDev::RGBA KU_TEXT_COLOUR = 0xFF000000u;

    // FLAG: minimal file-private slice of the (NOT committed) CgsGraphics::InstanceList element that
    // CountInstancesThatUseTexture walks. The same three-field shape is already attested by the
    // committed CgsInstance.cpp / CgsInstanceListResourceType.cpp reconstructions (an on-disk resource
    // body whose 80-byte Instance element is not committed to a shared header); reproduced here at the
    // X360 offsets so the count loop reads named fields rather than raw displacements. The instance's
    // model pointer is the first word of each 80-byte element (X360 `*(*v8 + v10)`, v10 += 80).
    struct InstanceListView
    {
        CgsGraphics::Model** mppaInstanceModels; // +0x00  base of the per-instance model-pointer buffer (stride 80)
        u32                  muArraySize;        // +0x04  total slots (bounds-check bound: v9 >= v8[1])
        u32                  muNumInstances;     // +0x08  live instances (iteration count: v9 < v8[2])
    };
    // The on-disk Instance element is 80 bytes; its model pointer is the first word (X360 v10 += 80).
    const u32 KU_INSTANCE_STRIDE_BYTES = 80;

    // Read the i-th instance's model: element base + i*80 bytes, first word = the model pointer.
    inline CgsGraphics::Model* GetInstanceModel(const InstanceListView* lpList, u32 luIndex)
    {
        const u8* lpElement = reinterpret_cast<const u8*>(lpList->mppaInstanceModels)
                            + luIndex * KU_INSTANCE_STRIDE_BYTES;
        return *reinterpret_cast<CgsGraphics::Model* const*>(lpElement);
    }
}

// MaybeDrawText (X360 0x82824048) -- the shared "draw debug text iff on-screen" helper used by the HUD
// readouts. External to this TU (its own TU); declared here so the compile gate sees its shape. The
// extra register args the decompiler shows are artifacts; the real call is the renderer's
// DrawText(text, x, y, scale, colour) gated on an on-screen test. Signature recovered from the
// sibling DebugComponent reconstructions: (display, text, x, y, scale, colour, centred).
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

// DebugDraw2DBox (X360 sub_8281C3E0) -- the free-function corner-based debug 2D box primitive used by
// the thumbnail selection highlight. External to this TU (its own helper); declared here so the
// compile gate sees its shape. The X360 takes two screen-space corners (x1,y1)-(x2,y2), forms the box
// position {x1,y1} and size {x2-x1,y2-y1}, and hands them to CgsDev::Debug2DImmediateRender::DrawBox.
// The trailing decompiler register args are artifacts; the recovered call is (display, x1, y1, x2, y2,
// colour). Under the per-TU compile gate the declaration suffices (its body lives in its own TU).
void DebugDraw2DBox(CgsDev::Debug2DImmediateRender* lpDisplay, f32 lfX1, f32 lfY1, f32 lfX2, f32 lfY2,
                    CgsDev::RGBA lColour);

namespace CgsResource
{
    // ================================================================================================
    // AllocateDebugMemory  @0x828EED50
    // Carve the texture-entry snapshot array out of the debug resource allocator. The X360 builds a
    // single-entry resource descriptor { m_size = 0x8000 (= KI_MAX_TEXTURE_ENTRIES * 16), m_alignment
    // = 0x10 } on the stack, dispatches the allocator's resource-allocate virtual, takes the
    // main-memory base of the carved resource into mpTextureEntries (this+0x44), and asserts it.
    // ================================================================================================
    void DebugPoolTextures::AllocateDebugMemory(rw::IResourceAllocator* lpAllocator)
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 li = 0; li < 4; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = KI_MAX_TEXTURE_ENTRIES * sizeof(TextureEntry); // 0x8000
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;

        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        mpTextureEntries = static_cast<TextureEntry*>(lResource.m_baseResources[0]);

        CGSDPT_FIRE_ASSERT(mpTextureEntries != 0, "Could not allocate memory for texture entries\n",
                           KPC_SOURCE_FILE, 515);
    }

    // ================================================================================================
    // CountInstancesThatUseTexture  @0x828E5C68
    // Sweep every in-use instance-list entry of the pool; for each instance's model, count how many
    // use lpTexture. The X360 walks mpResourceEntries[i] (in-use bit set), keeps the instance-list
    // entries (type id 35), then iterates the list's live instances (bounds-checked against its array
    // size) and tests CgsGraphics::Model::UsesTexture per instance model.
    // ================================================================================================
    s32 DebugPoolTextures::CountInstancesThatUseTexture(renderengine::Texture* lpTexture)
    {
        s32 liCount = 0;

        const u32 luMaxResources = mpPool->GetMaxResources();
        for (u32 luEntry = 0; luEntry < luMaxResources; ++luEntry)
        {
            if ((mpPool->GetEntryStatusDirect(luEntry) & KU8_ENTRY_STATUS_INUSE) == 0)
                continue;

            const Entry* lpEntry = mpPool->GetEntryDirect(luEntry);
            if (lpEntry->mpResourceType->GetCachedId() != KU_TYPEID_INSTANCE)
                continue;

            const InstanceListView* lpList =
                static_cast<const InstanceListView*>(lpEntry->mResource.m_baseResources[0]);
            if (lpList->muNumInstances == 0)
                continue;

            for (u32 luInstance = 0; luInstance < lpList->muNumInstances; ++luInstance)
            {
                CGSDPT_FIRE_ASSERT(luInstance < lpList->muArraySize, "Instance index out of range",
                                   "..\\..\\..\\GameShared\\GameClasses\\Graphics/Instances/CgsInstance.h", 288);

                const CgsGraphics::Model* lpModel = GetInstanceModel(lpList, luInstance);
                if (lpModel->UsesTexture(lpTexture))
                    ++liCount;
            }
        }

        return liCount;
    }

    // ================================================================================================
    // CountModelsThatUseTexture  @0x828E5D78
    // Sweep every in-use model entry (type id 42) of the pool and count how many use lpTexture.
    // ================================================================================================
    s32 DebugPoolTextures::CountModelsThatUseTexture(renderengine::Texture* lpTexture)
    {
        s32 liCount = 0;

        const u32 luMaxResources = mpPool->GetMaxResources();
        for (u32 luEntry = 0; luEntry < luMaxResources; ++luEntry)
        {
            if ((mpPool->GetEntryStatusDirect(luEntry) & KU8_ENTRY_STATUS_INUSE) == 0)
                continue;

            const Entry* lpEntry = mpPool->GetEntryDirect(luEntry);
            if (lpEntry->mpResourceType->GetCachedId() != KU_TYPEID_MODEL)
                continue;

            const CgsGraphics::Model* lpModel =
                static_cast<const CgsGraphics::Model*>(lpEntry->mResource.m_baseResources[0]);
            if (lpModel->UsesTexture(lpTexture))
                ++liCount;
        }

        return liCount;
    }

    // ================================================================================================
    // RegisterTexture  @0x828E5E28
    // Snapshot pool entry luPoolEntryIndex into texture-entry slot liSlot: store the texture pointer,
    // the source pool-entry index, and the graphics-system (video) memory size; when lbDoFullAnalysis
    // is set, also count the models/instances that use it. The X360 asserts the slot is in range.
    // ================================================================================================
    void DebugPoolTextures::RegisterTexture(s32 liSlot, u16 luPoolEntryIndex, bool lbDoFullAnalysis)
    {
        CGSDPT_FIRE_ASSERT(liSlot < KI_MAX_TEXTURE_ENTRIES, "Index out of range\n", KPC_SOURCE_FILE, 545);

        const Entry*           lpEntry = mpPool->GetEntryDirect(luPoolEntryIndex);
        renderengine::Texture* lpTexture =
            static_cast<renderengine::Texture*>(lpEntry->mResource.m_baseResources[0]);

        // The X360 takes the graphics-system descriptor's m_size (entry+0x10) as the entry's data size.
        const u32 luDataSize = lpEntry->mResourceDescriptor.m_baseResourceDescriptors[1].m_size;

        TextureEntry& lrEntry = mpTextureEntries[liSlot];
        lrEntry.mpTexture     = lpTexture;
        lrEntry.muEntryIndex  = luPoolEntryIndex;
        lrEntry.muDataSize    = luDataSize;

        if (lbDoFullAnalysis)
        {
            lrEntry.muModelsThatUseTexture    = static_cast<u16>(CountModelsThatUseTexture(lpTexture));
            lrEntry.muInstancesThatUseTexture = static_cast<u16>(CountInstancesThatUseTexture(lpTexture));
        }
    }

    // ================================================================================================
    // Render  @0x828FFB78  (Window::Render override)
    // Draw the base window chrome, then (if a pool is bound and a texture is selected) print the
    // selected texture's stats line-by-line and draw the texture itself in the display box.
    // ================================================================================================
    void DebugPoolTextures::Render(CgsDev::Debug2DImmediateRender* lpRender)
    {
        Window::Render(lpRender);

        if (mpPool == 0 || muTextureEntryIndex == 0xFFFF)
            return;

        const Entry* lpEntry = mpPool->GetEntryDirect(muTextureEntryIndex);
        renderengine::Texture* lpTexture =
            static_cast<renderengine::Texture*>(lpEntry->mResource.m_baseResources[0]);

        char lacText[512];
        f32  lfY = GetY();

        CgsCore::SPrintf(lacText, sizeof(lacText), "Texture %d of %d", miTextureIndex, miTotalTextures);
        MaybeDrawText(lpRender, lacText, GetX(), lfY, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        lfY += KF_TEXT_LINE_STEP;
        CgsCore::SPrintf(lacText, sizeof(lacText), "Width: %d", renderengine::Texture::GetWidth(lpTexture));
        MaybeDrawText(lpRender, lacText, GetX(), lfY, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        lfY += KF_TEXT_LINE_STEP;
        CgsCore::SPrintf(lacText, sizeof(lacText), "Height: %d", renderengine::Texture::GetHeight(lpTexture));
        MaybeDrawText(lpRender, lacText, GetX(), lfY, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        lfY += KF_TEXT_LINE_STEP;
        CgsCore::SPrintf(lacText, sizeof(lacText), "Models: %d", miNumModelsThatUseCurrentTexture);
        MaybeDrawText(lpRender, lacText, GetX(), lfY, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        lfY += KF_TEXT_LINE_STEP;
        CgsCore::SPrintf(lacText, sizeof(lacText), "Instances: %d", miNumInstancesThatUseCurrentTexture);
        MaybeDrawText(lpRender, lacText, GetX(), lfY, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        // "MM" = main-memory size (descriptor[0]); the X360 reads it straight off the selected entry.
        lfY += KF_TEXT_LINE_STEP;
        const u32 luMainMemSize = lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size;
        CgsCore::SPrintf(lacText, sizeof(lacText), "MM: %u", luMainMemSize);
        MaybeDrawText(lpRender, lacText, GetX(), lfY, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        // "VM" = graphics-system (video) memory, formatted MB/KB/B.
        const u32 luVideoMemSize = lpEntry->mResourceDescriptor.m_baseResourceDescriptors[1].m_size;
        const u32 luMB = luVideoMemSize >> 20;
        const u32 luKB = (luVideoMemSize - (luMB << 20)) >> 10;
        const u32 luB  = luVideoMemSize - (((luMB << 10) + luKB) << 10);
        char lacVideoMem[256];
        CgsCore::SPrintf(lacVideoMem, sizeof(lacVideoMem), "%uMB %uKB %uB", luMB, luKB, luB);
        CgsCore::SPrintf(lacText, sizeof(lacText), "VM: %s", lacVideoMem);
        MaybeDrawText(lpRender, lacText, GetX(), lfY + KF_TEXT_LINE_STEP, KF_TEXT_SCALE, KU_TEXT_COLOUR, false);

        // Draw the selected texture into the display box: from (mfX + 100, mfY) to (mfWidth - 100, mfHeight).
        const Vector2 lv2Position = { GetX() + KF_DISPLAY_INSET, GetY(),                  0.0f, 0.0f };
        const Vector2 lv2Size     = { GetWidth() - KF_DISPLAY_INSET, GetHeight(),         0.0f, 0.0f };
        RenderTextureDisplay(lpRender, lv2Position, lv2Size);
    }

    // ================================================================================================
    // RenderTextureDisplay  @0x828F82E0
    // Dispatch the selected texture to the active render mode. The thumbnail modes pass their grid
    // size (8 / 16 / 32). Out-of-range render modes draw nothing.
    // ================================================================================================
    void DebugPoolTextures::RenderTextureDisplay(CgsDev::Debug2DImmediateRender* lpRender,
                                                 Vector2 lv2Position, Vector2 lv2Size)
    {
        const Entry*           lpEntry   = mpPool->GetEntryDirect(muTextureEntryIndex);
        renderengine::Texture* lpTexture =
            static_cast<renderengine::Texture*>(lpEntry->mResource.m_baseResources[0]);

        switch (meRenderMode)
        {
        case E_DEBUGTEXTURERENDERMODE_ACTUALSIZE:
            RenderTextureActualSize(lpTexture, lv2Position, lv2Size);
            break;
        case E_DEBUGTEXTURERENDERMODE_STRETCH:
            RenderTextureStretched(lpTexture, lv2Position, lv2Size, true, true);
            break;
        case E_DEBUGTEXTURERENDERMODE_THUMBNAILS8x8:
            RenderThumbnails(lpRender, lv2Position, lv2Size, 8);
            break;
        case E_DEBUGTEXTURERENDERMODE_THUMBNAILS16x16:
            RenderThumbnails(lpRender, lv2Position, lv2Size, 16);
            break;
        case E_DEBUGTEXTURERENDERMODE_THUMBNAILS32x32:
            RenderThumbnails(lpRender, lv2Position, lv2Size, 32);
            break;
        default:
            break;
        }
    }

    // ================================================================================================
    // RenderTexture  @0x828E5B28
    // Map a screen-space (position, size) box into normalised-by-screen coordinates and hand it to the
    // registered render callback (mpRenderTextureCallback) along with the texture + user data. The
    // X360 reads the screen size from the window Metrics ({mfScreenWidth, mfScreenHeight}) and divides
    // the box by it (the vrefp/Newton reciprocal sequence == a float divide). No-op if no callback.
    // ================================================================================================
    void DebugPoolTextures::RenderTexture(renderengine::Texture* lpTexture, Vector2 lv2Position, Vector2 lv2Size,
                                          bool lbFlag0, bool lbFlag1)
    {
        const CgsDev::DebugUI::Metrics& lrMetrics = GetMetrics();
        const Vector2 lv2ScreenSize = { lrMetrics.mfScreenWidth, lrMetrics.mfScreenHeight, 0.0f, 0.0f };

        if (mpRenderTextureCallback == 0)
            return;

        // Normalise the box by the virtual screen size (X360 divides position & size by screen size).
        const Vector2 lv2NormPosition =
        {
            lv2Position.x / lv2ScreenSize.x,
            lv2Position.y / lv2ScreenSize.y,
            0.0f, 0.0f,
        };
        const Vector2 lv2NormSize =
        {
            lv2Size.x / lv2ScreenSize.x,
            lv2Size.y / lv2ScreenSize.y,
            0.0f, 0.0f,
        };

        mpRenderTextureCallback(lpTexture, lv2NormPosition, lv2NormSize,
                                mpCallbackUserData, lbFlag0, lbFlag1);
    }

    // ================================================================================================
    // RenderTextureActualSize  @0x828EEE08
    // Draw the texture at 1:1 pixels, centred in the box. If the texture is wider than the box's width
    // limit or taller than its height limit, fall back to the stretched draw instead.
    // ================================================================================================
    void DebugPoolTextures::RenderTextureActualSize(renderengine::Texture* lpTexture,
                                                    Vector2 lv2Position, Vector2 lv2Size)
    {
        const u32 luWidth  = renderengine::Texture::GetWidth(lpTexture);
        const u32 luHeight = renderengine::Texture::GetHeight(lpTexture);

        // lv2Size carries the box limits: x = max width (a27), y = max height (a28).
        if (luWidth > static_cast<u32>(lv2Size.x) || luHeight > static_cast<u32>(lv2Size.y))
        {
            RenderTextureStretched(lpTexture, lv2Position, lv2Size, true, true);
        }

        // Centre the actual-size quad inside the box: position + (size - texelSize) * 0.5.
        const Vector2 lv2TexelSize =
        {
            static_cast<f32>(renderengine::Texture::GetWidth(lpTexture)),
            static_cast<f32>(renderengine::Texture::GetHeight(lpTexture)),
            0.0f, 0.0f,
        };
        const Vector2 lv2Centre =
        {
            lv2Position.x + (lv2Size.x - lv2TexelSize.x) * 0.5f,
            lv2Position.y + (lv2Size.y - lv2TexelSize.y) * 0.5f,
            0.0f, 0.0f,
        };
        RenderTexture(lpTexture, lv2Centre, lv2TexelSize, true, true);
    }

    // ================================================================================================
    // RenderTextureStretched  @0x828E5F10
    // Draw the texture scaled to fit the box while preserving aspect ratio, centred. The X360 compares
    // the texture's width vs height (vcmpgtfp) and scales by the dominant axis so the longer side fills
    // the box, then centres the result.
    // ================================================================================================
    void DebugPoolTextures::RenderTextureStretched(renderengine::Texture* lpTexture,
                                                   Vector2 lv2Position, Vector2 lv2Size,
                                                   bool lbFlag0, bool lbFlag1)
    {
        const f32 lfTexWidth  = static_cast<f32>(renderengine::Texture::GetWidth(lpTexture));
        const f32 lfTexHeight = static_cast<f32>(renderengine::Texture::GetHeight(lpTexture));

        // Centre of the box (position + size*0.5).
        const Vector2 lv2Centre =
        {
            lv2Position.x + lv2Size.x * 0.5f,
            lv2Position.y + lv2Size.y * 0.5f,
            0.0f, 0.0f,
        };

        // Fit the longer texture axis to the box: scale the box extent by (shorter/longer) on the
        // minor axis. The X360 branch tests texWidth > texHeight and divides the box's longer side by
        // the texture aspect (vrefp reciprocal == divide).
        Vector2 lv2Fitted;
        if (lfTexWidth > lfTexHeight)
        {
            lv2Fitted.x = lv2Size.x;
            lv2Fitted.y = lv2Size.x * (lfTexHeight / lfTexWidth);
        }
        else
        {
            lv2Fitted.x = lv2Size.y * (lfTexWidth / lfTexHeight);
            lv2Fitted.y = lv2Size.y;
        }
        lv2Fitted.z = 0.0f;
        lv2Fitted.w = 0.0f;

        // Position the fitted quad centred (centre - fitted*0.5).
        const Vector2 lv2FittedPos =
        {
            lv2Centre.x - lv2Fitted.x * 0.5f,
            lv2Centre.y - lv2Fitted.y * 0.5f,
            0.0f, 0.0f,
        };
        RenderTexture(lpTexture, lv2FittedPos, lv2Fitted, lbFlag0, lbFlag1);
    }

    // ================================================================================================
    // RenderThumbnails  @0x828E60A8
    // Draw the textures as a liGridSize x liGridSize grid of thumbnails, paged so the selected texture's
    // page is shown. The X360 computes per-cell size from the box and the grid count, walks the visible
    // page's cells, draws each cell's texture stretched, and draws a highlight box (sub_8281C3E0) around
    // the selected cell.
    // ================================================================================================
    void DebugPoolTextures::RenderThumbnails(CgsDev::Debug2DImmediateRender* lpRender,
                                             Vector2 lv2Position, Vector2 lv2Size, s32 liGridSize)
    {
        // Page maths (X360 divwu/twllei guards). cellsPerPage = grid^2; the selected texture's page is
        // shown, and the selected cell within the page gets a highlight box.
        const s32 liCellsPerPage = liGridSize * liGridSize;
        const s32 liSelected     = miTextureIndex;
        const s32 liPage         = (liCellsPerPage != 0) ? (liSelected / liCellsPerPage) : 0;
        const s32 liCellInPage   = (liCellsPerPage != 0) ? (liSelected % liCellsPerPage) : 0;
        const s32 liPageStart    = liPage * liCellsPerPage;

        // The whole grid lives in the box inset by an 8px outer border (flt_82004C88, X360
        // `vsubfp128 v125,v2,{8,8}`) for the framing extent, and the top-left is pushed in by the 4px
        // inter-cell gap (flt_82004270, X360 `vaddfp v1,v1,{4,4}`) for the first cell's origin.
        const Vector2 lv2FrameSize =
        {
            lv2Size.x - 8.0f,
            lv2Size.y - 8.0f,
            0.0f, 0.0f,
        };
        const Vector2 lv2FramePosition =
        {
            lv2Position.x + 4.0f,
            lv2Position.y + 4.0f,
            0.0f, 0.0f,
        };

        // Per-cell stride (X360 v52 = `(Size-{8,8}) * (1/grid)`): the framed extent divided by the grid
        // count (the X360 reciprocal of the grid count). The cell box is the stride shrunk by the 4px
        // gap (X360 v127 = stride - {4,4}); the texture is then centred in its stride region by the 2px
        // half-gap inset (X360 v126, which the VMX `(1/2)*stride - (1/2)*cellSize` collapses to {2,2}).
        const f32 lfInvGrid = (liGridSize != 0) ? (1.0f / static_cast<f32>(liGridSize)) : 0.0f;
        const Vector2 lv2CellStride =
        {
            lv2FrameSize.x * lfInvGrid,
            lv2FrameSize.y * lfInvGrid,
            0.0f, 0.0f,
        };
        const Vector2 lv2CellSize =
        {
            lv2CellStride.x - 4.0f,
            lv2CellStride.y - 4.0f,
            0.0f, 0.0f,
        };
        const Vector2 lv2CellInset = { 2.0f, 2.0f, 0.0f, 0.0f };

        // Background frame for the whole grid (the X360's first RenderTexture call before the loop):
        // position = Position+{4,4}, size = Size-{8,8}, flags (true,false).
        RenderTexture(0, lv2FramePosition, lv2FrameSize, true, false);

        if (liGridSize > 0)
        {
            // v21 in the X360: the page-relative cell counter (0..cellsPerPage). Global entry index is
            // pageStart + cellCounter; the row/column counters bound the grid sweep. The grid stops as
            // soon as the entries run out (the X360 forces the row counter past the grid size).
            //
            // The per-cell top-left (X360 v46) starts at the framed origin and advances one cell stride
            // per column (X360 `v46.x += v52.x`, vaddfp/vrlimi128) and one stride per row (X360
            // `v46.y += v52.y`, x reset to the row-start origin, vrlimi128).
            Vector2 lv2CellOrigin = lv2FramePosition;
            s32  liCellCounter = 0;
            bool lbExhausted   = false;
            for (s32 liRow = 0; liRow < liGridSize && !lbExhausted; ++liRow)
            {
                s32 liGlobalIndex = liPageStart + liCellCounter;
                s32 liColumn      = 0;
                while (liColumn < liGridSize)
                {
                    if (liGlobalIndex >= miTotalTextures)
                    {
                        lbExhausted = true;
                        break;
                    }

                    if (liCellInPage == liCellCounter)
                    {
                        // The selected cell gets a highlight box (X360 sub_8281C3E0, colour
                        // 0xFF0000FF) spanning the full cell stride from its top-left corner.
                        DebugDraw2DBox(lpRender, lv2CellOrigin.x, lv2CellOrigin.y,
                                       lv2CellOrigin.x + lv2CellStride.x,
                                       lv2CellOrigin.y + lv2CellStride.y, 0xFF0000FFu);
                    }

                    // Draw this cell's texture, centred in its stride region (origin + {2,2}, size =
                    // cellSize); X360 `vaddfp128 v1,v0,v126` for the position, `vmr128 v2,v127` size.
                    const Vector2 lv2CellPosition =
                    {
                        lv2CellOrigin.x + lv2CellInset.x,
                        lv2CellOrigin.y + lv2CellInset.y,
                        0.0f, 0.0f,
                    };
                    RenderTextureStretched(mpTextureEntries[liGlobalIndex].mpTexture,
                                           lv2CellPosition, lv2CellSize, false, false);

                    lv2CellOrigin.x += lv2CellStride.x;   // advance one cell stride per column
                    ++liColumn;
                    ++liCellCounter;
                    ++liGlobalIndex;
                }
                // Next row: reset x to the row-start origin, advance y by one stride.
                lv2CellOrigin.x  = lv2FramePosition.x;
                lv2CellOrigin.y += lv2CellStride.y;
            }
        }

        // Closing RenderTexture call (the X360's final call after the grid): position = Position+{4,4},
        // size = Size-{8,8}, flags (false,true).
        RenderTexture(0, lv2FramePosition, lv2FrameSize, false, true);
    }

    // ================================================================================================
    // qsort comparators  @0x828DAD30 .. 0x828DAEC0
    // Each is a descending compare on its key (larger key sorts first): return -1 if a < b, else
    // (a > b). The X360 emits the same idiom: blt -> -1, else subfc/subfe/clrlwi (== a > b ? 1 : 0).
    // ================================================================================================

    s32 DebugPoolTextures::SortByAreaCallback(const void* lpA, const void* lpB)
    {
        const TextureEntry* lpEntryA = static_cast<const TextureEntry*>(lpA);
        const TextureEntry* lpEntryB = static_cast<const TextureEntry*>(lpB);
        const u32 luAreaA = renderengine::Texture::GetWidth(lpEntryA->mpTexture)
                          * renderengine::Texture::GetHeight(lpEntryA->mpTexture);
        const u32 luAreaB = renderengine::Texture::GetWidth(lpEntryB->mpTexture)
                          * renderengine::Texture::GetHeight(lpEntryB->mpTexture);
        if (luAreaA < luAreaB)
            return -1;
        return (luAreaA > luAreaB) ? 1 : 0;
    }

    s32 DebugPoolTextures::SortByWidthCallback(const void* lpA, const void* lpB)
    {
        const TextureEntry* lpEntryA = static_cast<const TextureEntry*>(lpA);
        const TextureEntry* lpEntryB = static_cast<const TextureEntry*>(lpB);
        const u32 luWidthA = renderengine::Texture::GetWidth(lpEntryA->mpTexture);
        const u32 luWidthB = renderengine::Texture::GetWidth(lpEntryB->mpTexture);
        if (luWidthA < luWidthB)
            return -1;
        return (luWidthA > luWidthB) ? 1 : 0;
    }

    s32 DebugPoolTextures::SortByHeightCallback(const void* lpA, const void* lpB)
    {
        const TextureEntry* lpEntryA = static_cast<const TextureEntry*>(lpA);
        const TextureEntry* lpEntryB = static_cast<const TextureEntry*>(lpB);
        const u32 luHeightA = renderengine::Texture::GetHeight(lpEntryA->mpTexture);
        const u32 luHeightB = renderengine::Texture::GetHeight(lpEntryB->mpTexture);
        if (luHeightA < luHeightB)
            return -1;
        return (luHeightA > luHeightB) ? 1 : 0;
    }

    s32 DebugPoolTextures::SortByDataSizeCallback(const void* lpA, const void* lpB)
    {
        const u32 luSizeA = static_cast<const TextureEntry*>(lpA)->muDataSize;
        const u32 luSizeB = static_cast<const TextureEntry*>(lpB)->muDataSize;
        if (luSizeA < luSizeB)
            return -1;
        return (luSizeA > luSizeB) ? 1 : 0;
    }

    s32 DebugPoolTextures::SortByModelUsageCallback(const void* lpA, const void* lpB)
    {
        const u32 luUsageA = static_cast<const TextureEntry*>(lpA)->muModelsThatUseTexture;
        const u32 luUsageB = static_cast<const TextureEntry*>(lpB)->muModelsThatUseTexture;
        if (luUsageA < luUsageB)
            return -1;
        return (luUsageA > luUsageB) ? 1 : 0;
    }

    s32 DebugPoolTextures::SortByInstanceUsageCallback(const void* lpA, const void* lpB)
    {
        const u32 luUsageA = static_cast<const TextureEntry*>(lpA)->muInstancesThatUseTexture;
        const u32 luUsageB = static_cast<const TextureEntry*>(lpB)->muInstancesThatUseTexture;
        if (luUsageA < luUsageB)
            return -1;
        return (luUsageA > luUsageB) ? 1 : 0;
    }

    // ================================================================================================
    // Update  @0x828EEB58  (Window::Update override)
    // Re-snapshot the pool's texture entries into mpTextureEntries, (re)sort by the active mode, then
    // clamp the selection and cache the selected texture's model/instance usage for Render to print.
    // ================================================================================================
    void DebugPoolTextures::Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent)
    {
        // The X360 first calls the base window update (the inliner folded it to the empty 0x8284CB38
        // body shared with BaseCollisionGenerator::Destruct -- reproduced as the base Update call).
        Window::Update(lfTimeStep, leEvent);

        if (mpPool == 0)
            return;

        // Re-snapshot: register every in-use texture entry (type id 0) into a fresh, packed slot list.
        s32 liSlot = -1;
        const u32 luMaxResources = mpPool->GetMaxResources();
        for (u32 luEntry = 0; luEntry < luMaxResources; ++luEntry)
        {
            if ((mpPool->GetEntryStatusDirect(luEntry) & KU8_ENTRY_STATUS_INUSE) != 0
                && mpPool->GetEntryDirect(luEntry)->mpResourceType->GetCachedId() == KU_TYPEID_TEXTURE)
            {
                ++liSlot;
                RegisterTexture(liSlot, static_cast<u16>(luEntry), mbDoFullAnalysisNextFrame);
            }
        }

        // If the live texture count changed since last frame, request a full (usage-counting) analysis.
        mbDoFullAnalysisNextFrame = (miTotalTextures != liSlot);
        miTotalTextures = liSlot;

        // (Re)sort the populated slots by the active mode (NONE leaves them in pool order).
        if (liSlot > 0)
        {
            int (*lpfnCompare)(const void*, const void*) = 0;
            switch (meSortMode)
            {
            case E_DEBUGSORTMODE_AREA:          lpfnCompare = &SortByAreaCallback;          break;
            case E_DEBUGSORTMODE_WIDTH:         lpfnCompare = &SortByWidthCallback;         break;
            case E_DEBUGSORTMODE_HEIGHT:        lpfnCompare = &SortByHeightCallback;        break;
            case E_DEBUGSORTMODE_DATASIZE:      lpfnCompare = &SortByDataSizeCallback;      break;
            case E_DEBUGSORTMODE_MODELUSAGE:    lpfnCompare = &SortByModelUsageCallback;    break;
            case E_DEBUGSORTMODE_INSTANCEUSAGE: lpfnCompare = &SortByInstanceUsageCallback; break;
            default:                            lpfnCompare = 0;                            break;
            }
            if (lpfnCompare != 0)
                qsort(mpTextureEntries, static_cast<size_t>(liSlot), sizeof(TextureEntry), lpfnCompare);
        }

        // Clamp the selected index and refresh the selected pool-entry index from it.
        if (miTextureIndex < 0 || miTextureIndex >= miTotalTextures)
        {
            if (miTotalTextures <= 0)
            {
                muTextureEntryIndex = 0xFFFF;
                miTextureIndex      = -1;
            }
            else
            {
                miTextureIndex      = 0;
                muTextureEntryIndex = mpTextureEntries[0].muEntryIndex;
            }
        }
        else
        {
            muTextureEntryIndex = mpTextureEntries[miTextureIndex].muEntryIndex;
        }

        // Cache the selected entry's model/instance usage for Render.
        if (miTextureIndex >= 0)
        {
            const TextureEntry& lrSelected = mpTextureEntries[miTextureIndex];
            miNumModelsThatUseCurrentTexture    = lrSelected.muModelsThatUseTexture;
            miNumInstancesThatUseCurrentTexture = lrSelected.muInstancesThatUseTexture;
        }
    }

    // ================================================================================================
    // GetSelectedTextureId  (DWARF :969) -- header surface; bodied by its own TU. Provided here as a
    // behaviour-faithful stub so the type is usable: the selected entry's id is the pool entry's ID.
    // ================================================================================================
}
