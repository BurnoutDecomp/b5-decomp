#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 (rw::math::vpu alias) -- render method params
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"     // CgsDev::DebugUI::Window (base), Metrics, InputEvent
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"                 // CgsResource::ID (GetSelectedTextureId return)

namespace rw { struct IResourceAllocator; }   // AllocateDebugMemory param (pointer only)

// CgsResource::DebugPoolTextures -- the in-game "texture pool browser" debug window. It snapshots
// the loaded-texture entries of a resource Pool into a flat TextureEntry array, lets the user page
// through them (sorted by area / width / height / data-size / model-usage / instance-usage), and
// renders the selected texture (at actual size, stretched, or as a thumbnail grid) with a readout
// of its dimensions / mip / video memory / how many models + instances reference it.
//
// SOURCE-OF-TRUTH:
//   * Member NAMES, the two enums, the nested TextureEntry record and the method SHAPE come from the
//     DecFIGS DWARF (GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h).
//   * Member LAYOUT (byte offsets) is pinned to the X360 ARTIST asm. The base CgsDev::DebugUI::Window
//     occupies +0..+0x24 (vtable +0, mpcCaption +4, mxFlags +8, mfTransparency +0xC, mfX +0x10,
//     mfY +0x14, mfWidth +0x18, mfHeight +0x1C, mpNext +0x20); the subclass members below start at
//     +0x24 and every offset matches a load/store in the asm (e.g. RegisterTexture stores into
//     mpTextureEntries at +0x44, Update reads meSortMode at +0x4C / meRenderMode is read in
//     RenderTextureDisplay at +0x50).
//
// This build's ARTIST function set bodies 17 of this class's methods in CgsDebugPoolTextures.cpp
// (the Render/Update virtuals, the texture analysis + registration helpers, the three render-mode
// draws and the six qsort comparators). The remaining DWARF-declared methods (Reset, the inc/dec
// index helpers, SetSortMode/SetRenderMode, GetSelectedTextureId, IsPartOfFilter, GetSizeString,
// FreeDebugMemory, ...) are header-surface only here -- declared so the type is complete, bodied by
// their own TUs.

namespace renderengine { class Texture; }

namespace CgsResource
{
    class Pool;

    // Render callback the resource debug component hands the window: draws one texture quad given its
    // texture + screen-space (normalised) position/size + user data + two flags. Matches the X360
    // DebugComponentParams callback void(*)(renderengine::Texture*, Vector2, Vector2, void*, bool, bool)
    // (the Vector2 pair travels in vector registers on X360; passed by value here).
    typedef void (*DebugTextureRenderCallback)(renderengine::Texture* lpTexture,
                                               Vector2 lv2Position, Vector2 lv2Size,
                                               void* lpUserData, bool lbFlag0, bool lbFlag1);

    // CgsDebugPoolTextures.h:48 (DWARF) -- which key the entry array is qsort'd by each frame.
    enum EDebugSortMode
    {
        E_DEBUGSORTMODE_NONE          = 0,
        E_DEBUGSORTMODE_AREA          = 1,
        E_DEBUGSORTMODE_WIDTH         = 2,
        E_DEBUGSORTMODE_HEIGHT        = 3,
        E_DEBUGSORTMODE_DATASIZE      = 4,
        E_DEBUGSORTMODE_MODELUSAGE    = 5,
        E_DEBUGSORTMODE_INSTANCEUSAGE = 6,
        E_DEBUGSORTMODE_COUNT         = 7,
    };

    // CgsDebugPoolTextures.h:60 (DWARF) -- how the selected texture is drawn on the overlay.
    enum EDebugTextureRenderMode
    {
        E_DEBUGTEXTURERENDERMODE_ACTUALSIZE      = 0,
        E_DEBUGTEXTURERENDERMODE_STRETCH         = 1,
        E_DEBUGTEXTURERENDERMODE_THUMBNAILS8x8   = 2,
        E_DEBUGTEXTURERENDERMODE_THUMBNAILS16x16 = 3,
        E_DEBUGTEXTURERENDERMODE_THUMBNAILS32x32 = 4,
        E_DEBUGTEXTURERENDERMODE_COUNT           = 5,
    };

    class DebugPoolTextures : public CgsDev::DebugUI::Window
    {
    public:
        // CgsDebugPoolTextures.h:85 (DWARF) -- the texture-entry snapshot array capacity.
        static const s32 KI_MAX_TEXTURE_ENTRIES = 2048;

        // CgsDebugPoolTextures.h:88 (DWARF) -- one snapshotted texture (16 bytes; X360 stride 16).
        // RegisterTexture fills it from a pool Entry; the qsort comparators sort the array by these.
        struct TextureEntry
        {
            renderengine::Texture* mpTexture;                 // +0x00  the texture resource
            u32                    muDataSize;                // +0x04  resource data size (bytes)
            u16                    muEntryIndex;              // +0x08  source pool-entry index
            u16                    muModelsThatUseTexture;    // +0x0A  # models referencing it
            u16                    muInstancesThatUseTexture; // +0x0C  # instances referencing it
        };

        // --- the 17 ARTIST-attested bodies (CgsDebugPoolTextures.cpp) -----------------------------

        // @0x828EED50  Allocate the texture-entry array (KI_MAX_TEXTURE_ENTRIES * 16 = 0x8000 bytes,
        // 16-aligned) through the debug resource allocator; assert it succeeded.
        void AllocateDebugMemory(rw::IResourceAllocator* lpAllocator);

        // @0x828FFB78  Window::Render override: print the selected texture's stats then draw it.
        virtual void Render(CgsDev::Debug2DImmediateRender* lpRender);

        // @0x828EEB58  Window::Update override: re-snapshot the pool's textures, sort, clamp selection.
        virtual void Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent);

        // GetSelectedTextureId @0x82969... (DWARF :969) -- header surface, bodied by its own TU.
        ID GetSelectedTextureId();

    private:
        // @0x828E5C68  Count instances (in the pool) whose model uses lpTexture.
        s32  CountInstancesThatUseTexture(renderengine::Texture* lpTexture);
        // @0x828E5D78  Count models (in the pool) that use lpTexture.
        s32  CountModelsThatUseTexture(renderengine::Texture* lpTexture);
        // @0x828E5E28  Snapshot pool entry liPoolEntryIndex into slot liSlot (optionally counting usage).
        void RegisterTexture(s32 liSlot, u16 luPoolEntryIndex, bool lbDoFullAnalysis);

        // @0x828E5B28  Draw one texture quad via the registered render callback, mapping the supplied
        // screen-space position/size into normalised-by-screen coords first.
        void RenderTexture(renderengine::Texture* lpTexture, Vector2 lv2Position, Vector2 lv2Size,
                           bool lbFlag0, bool lbFlag1);
        // @0x828F82E0  Dispatch the selected texture to the active render mode's draw.
        void RenderTextureDisplay(CgsDev::Debug2DImmediateRender* lpRender, Vector2 lv2Position, Vector2 lv2Size);
        // @0x828EEE08  Draw the texture at 1:1 pixels (centred), falling back to stretched if too big.
        void RenderTextureActualSize(renderengine::Texture* lpTexture, Vector2 lv2Position, Vector2 lv2Size);
        // @0x828E5F10  Draw the texture stretched to fit the box, preserving aspect ratio.
        void RenderTextureStretched(renderengine::Texture* lpTexture, Vector2 lv2Position, Vector2 lv2Size,
                                    bool lbFlag0, bool lbFlag1);
        // @0x828E60A8  Draw a grid of texture thumbnails (liGridSize per page).
        void RenderThumbnails(CgsDev::Debug2DImmediateRender* lpRender, Vector2 lv2Position, Vector2 lv2Size,
                              s32 liGridSize);

        // The six qsort comparators (each a-b, descending: larger key first).
        static s32 SortByAreaCallback(const void* lpA, const void* lpB);          // @0x828DAD30
        static s32 SortByWidthCallback(const void* lpA, const void* lpB);         // @0x828DADA0
        static s32 SortByHeightCallback(const void* lpA, const void* lpB);        // @0x828DAE08
        static s32 SortByDataSizeCallback(const void* lpA, const void* lpB);      // @0x828DAE70
        static s32 SortByModelUsageCallback(const void* lpA, const void* lpB);    // @0x828DAE98
        static s32 SortByInstanceUsageCallback(const void* lpA, const void* lpB); // @0x828DAEC0

        // --- members (offsets pinned to the X360 asm; subclass region starts at +0x24) ------------
        const Pool*                mpPool;                              // +0x24
        s32                        miTextureIndex;                       // +0x28  selected entry (-1 = none)
        s32                        miTotalTextures;                      // +0x2C  live entry count this frame
        u16                        muTextureEntryIndex;                  // +0x30  selected entry's pool index
        DebugTextureRenderCallback mpRenderTextureCallback;             // +0x34
        void*                      mpCallbackUserData;                   // +0x38
        s32                        miNumModelsThatUseCurrentTexture;     // +0x3C
        s32                        miNumInstancesThatUseCurrentTexture;  // +0x40
        TextureEntry*              mpTextureEntries;                     // +0x44  the KI_MAX_TEXTURE_ENTRIES array
        bool                       mbDoFullAnalysisNextFrame;            // +0x48
        EDebugSortMode             meSortMode;                           // +0x4C
        EDebugTextureRenderMode    meRenderMode;                         // +0x50
    };
}
