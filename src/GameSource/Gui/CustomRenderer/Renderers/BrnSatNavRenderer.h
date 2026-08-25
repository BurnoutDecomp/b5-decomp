#ifndef BRN_SATNAV_RENDERER_H
#define BRN_SATNAV_RENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                    // Vector2/Vector3
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h" // base
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                // GuiEventEnableSatNavIcons::EIconDisplayType
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer (== Im2d), Im2dTransform
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // CgsGraphics::ImRenderBuffer<V> (the draw-target type)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"   // CgsGraphics::Im2dTransform (mTransform)
#include "GameShared/GameClasses/Core/CgsID.h"                                // CgsID (GetID)
#include "rw/rwcore_structs.h"                                                // rw::Resource (texture-state backing)
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"                         // BrnGui::GuiEventRenderSatNav (the 212 payload)

// BrnGui::SatNavRenderer -- the GUI custom-render component that draws the sat-nav /
// mini-map event icons and the player's route highlight on the live map.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008) against
// references/DecFIGS/dwarfdump/.../BrnSatNavRenderer.h (full DWARF class layout).
// The ledger functions of this TU are reconstructed in BrnSatNavRenderer.cpp:
//   Construct, Destruct, Prepare, Release, RecvEvent, GetID,
//   GetNumIcons, GetIconInformation, InitResources, InitEventTypeUvs,
//   InitSatNavIcons, RefreshSatNavIconInfo, RenderIconsForSatNav, plus the
//   class:BrnGui::SatNavRenderer group: SatNavRenderer (ctor @0x827DD468),
//   RenderSatNavIcon (@0x8245A3A0) and UpdateRendererTransform (@0x8245F4D8).
// (Update / RenderComponent / SetTransparency are sibling TUs / out of scope and are
//  reached here only as declared callees.)
//
// LAYOUT POLICY (matches the sibling renderers BoostBarRenderer / CrashNavIconRenderer):
// the compile gate is a per-TU `cl /c` on a 64-bit host, so guest byte offsets are NOT
// load-bearing (pointers widen 4->8). Members are declared BY NAME from the DWARF; no
// raw offset casts are used anywhere in the .cpp. Member TYPES that are still uncommitted
// (RGBA, Im2dTransform, GuiEventRenderSatNav, the renderengine Resource/TextureState/
// BlendState pairs, ImRendererSet) are modelled as named opaque slots / primitive storage
// so the in-scope bodies type-check; their full layouts are intentionally OMITTED. FLAG:
// opaque-typed members + SIMD render math reconstructed at the semantic level (the X360
// vectorised VMX128 transform/clamp math has no portable PC equivalent -- same policy as
// BrnMapUtils.h / rw/math/vpu/types.h).

namespace renderengine
{
    // Uncommitted renderengine types reached only as opaque handles by this slice.
    class TextureState;
    class BlendState;
}

// CgsGraphics::Im2dRenderBuffer (== Im2d) and Im2dTransform come from the immediate-mode
// headers included above; the in-scope RenderSatNavIcon / UpdateRendererTransform bodies
// drive them by name.
namespace CgsModule   { struct Event; }

namespace BrnGui
{
    class GuiCache;
    class MapTransform;

    // The sat-nav route/event-icon renderer. Derives from the GUI custom-renderer base
    // (DWARF: SatNavRenderer : public CgsGui::CustomRenderComponentInterface).
    class SatNavRenderer : public CgsGui::CustomRenderComponentInterface
    {
        // The sat-nav debug component (BrnGuiSatNavDebugComponent.cpp) rebuilds the renderer's
        // transform when its rect/alpha sliders change (X360 TriggerSatNavPositionUpdate calls the
        // private UpdateRendererTransform directly).
        friend struct SatNavDebugComponent;

    public:
        // ---- DWARF nested enums (BrnSatNavRenderer.h:53/61/68/104) ----

        // mePrepareStage state machine driven by Prepare().
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_LOAD  = 1,
            E_PREPARESTAGE_INIT  = 2,
            E_PREPARESTAGE_DONE  = 3,
        };

        // meReleaseStage state machine driven by Release().
        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 1,
        };

        // Which UV row a cached icon uses: not-attempted vs completed event icon.
        enum ESatNavIconType
        {
            E_SATNAVICON_EVENT_NOTATTEMPTED = 0,
            E_SATNAVICON_EVENT_COMPLETED    = 1,
            E_SATNAVICON_NUM                = 2,
        };

        // Mini-icon row index per event type.
        enum ESatNavEventTypeMiniIconIndex
        {
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_CURRENT_BURNINGROUTE = 0,
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_ROADRAGE            = 1,
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_BURNINGROUTE        = 2,
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_RACE               = 3,
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_MARKEDMAN          = 4,
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_STUNT              = 5,
            E_SATNAVICON_EVENTTYPE_MINI_INDEX_COUNT              = 6,
        };

        // DWARF BrnSatNavRenderer.h:77 -- one cached on-map icon. InitSatNavIcons /
        // RefreshSatNavIconInfo / GetIconInformation fill an array of these and
        // RenderIconsForSatNav reads them.
        struct IconRendererSatNavIconInfo
        {
            Vector3         mv3Position;       // world-space icon position
            s32             miEventId;         // owning event id
            u32             muEventTypeIndex;  // UV column (event-type -> icon index)
            ESatNavIconType meSatNavIconType;  // UV row (not-attempted / completed)
        };

        // ---- class-scope constants (DWARF h:221-227) ----
        static const u32 KU_ICON_EVENT_TYPE_COUNT            = 11;
        static const u32 KU_ICON_EVENT_TYPE_ONLINE_OFFSET    = 6;
        static const u32 KU_ICON_EVENT_TYPE_ADDITIONAL_OFFSET = 9;
        static const u32 KU_MAX_SATNAV_ICONS                 = 150;
        static const u8  KI_ALPHA                            = 90;
        static const u8  KU_BURNING_ROUTE_ICON_ALPHA         = 100;

    public:
        SatNavRenderer();

        // ---- virtual overrides driven by the GUI CustomRendererManager ----
        // ⭐ H3b: Prepare/RecvEvent/GetID previously re-declared with LOCAL signatures
        // (void* args / a u32 "GetComponentID") -- shadowing redeclarations, NOT
        // overrides: the manager's virtual dispatch would have hit the base defaults
        // and the renderer would never prepare or receive an event (the
        // shadowing-redeclaration defect class; only a link/mount surfaces it).
        virtual void   Construct();
        virtual bool   Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                               rw::IResourceAllocator* lpHeapAllocator,
                               rw::IResourceAllocator* lpTextureAllocator);
        virtual bool   Release();
        virtual void   Destruct();
        virtual void   RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        virtual CgsID  GetID() const;   // @0x82445888 -- the component CgsID
        // RenderComponent @0x82465EC0 -- the per-frame sat-nav draw (map quad + mask +
        // icons), dispatched by the base's non-virtual Render(set).
        virtual void   RenderComponent(CgsGui::ImRendererSet* lpRendererSet);

        // ---- private helpers (this TU's non-virtual leaves) ----
    private:
        void InitResources();
        void InitEventTypeUvs();
        void InitSatNavIcons();
        void RefreshSatNavIconInfo(s32 liEventId);
        u32  GetNumIcons() const;
        void GetIconInformation(u32 luIndex, IconRendererSatNavIconInfo* lpInfo) const;
        // PC fold: the draw target is the Apt 2D COMMAND buffer (the ticker precedent;
        // the console's v6+4 subobject) -- the buffered ImRenderBuffer, not the direct Im2d.
        void RenderIconsForSatNav(
            CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer);

        // Build the screen-space mTransform from the viewport descriptor + aspect ratio
        // (X360 @0x8245F4D8; bodied in this TU). Called by Construct() and the debug component.
        void UpdateRendererTransform();
        // Draw one sat-nav icon quad through the immediate-mode render buffer (X360 @0x8245A3A0;
        // bodied in this TU). X360 float-arg ABI order (f1..f4) is (x, halfWidth, y, halfHeight):
        // the call sites in RenderIconsForSatNav pass f1/f3 = the transformed device position and
        // f2/f4 = the zoom-scaled per-icon half-extents (see @0x8245FD00/0x82460108).
        void RenderSatNavIcon(f32 lfX, f32 lfHalfWidth, f32 lfY, f32 lfHalfHeight,
                              ESatNavIconType leIconType, u32 luEventTypeIndex,
                              CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer);

        // ---- member state (DWARF h:238-306; declared by name, opaque where uncommitted) ----
        // NOTE: mbRenderEnabled lives in the base (CgsGui::CustomRenderComponentInterface).

        u32  mMapQuadColour;                 // h:238 RGBA (packed colour; type uncommitted)
        CgsGraphics::Im2dTransform mTransform; // h:240 the screen-space Im2dTransform
                                              // UpdateRendererTransform builds in place (64 bytes,
                                              // 4 x Vector4; X360 @0x8245F6Bx writes lanes 0/16/32/48).
        EPrepareStage mePrepareStage;        // h:242
        EReleaseStage meReleaseStage;        // h:243
        // h:245 -- the latched GuiEventRenderSatNav payload (RecvEvent 212). ⭐ H3b: was
        // an opaque u8[48] copied with the X360's 48-byte memcpy -- on this host the
        // record is native-width (three 8-byte texture pointers), so the fixed-size copy
        // TRUNCATED it (the PlayAptMovie precedent). Typed + copied by assignment now.
        GuiEventRenderSatNav mRenderSatNavEvent;
        GuiCache* mpGuiCache;                // h:246
        void* mpHeapAllocator;               // h:248 rw::IResourceAllocator*

        // h:254-272 -- six TextureState/BlendState resource pairs + the icon array. Each
        // "Resource" is the renderengine resource descriptor (5 dwords) the InitResources
        // copy loop fills; modelled as named state blocks. Types uncommitted -> opaque.
        u32  mMapTextureStateResource[5];    // h:254 Resource
        renderengine::TextureState* mpMapTextureState;        // h:255
        u32  mMapBlendStateResource[5];      // h:256 Resource
        renderengine::BlendState*   mpMapBlendState;          // h:257
        u32  mMaskTextureStateResource[5];   // h:259 Resource
        renderengine::TextureState* mpMaskTextureState;       // h:260
        u32  mMaskBlendStateResource[5];     // h:261 Resource
        renderengine::BlendState*   mpMaskBlendState;         // h:262
        u32  mRouteSegmentTextureStateResource[5];  // h:264 Resource
        renderengine::TextureState* mpRouteSegmentTextureState; // h:265
        u32  mRouteSegmentBlendStateResource[5];    // h:267 Resource
        renderengine::BlendState*   mpRouteSegmentBlendState;   // h:268

        u32  maIconResources[2][5];          // h:271 Resource[2]  (InitResources fills these)
        renderengine::TextureState* mapIconTextureStates[2];   // h:272

        // PC fold: backing storage for the runtime-created texture states (the font-path
        // precedent -- renderengine::TextureState::Initialize(rw::Resource*, Parameters*)).
        // The console's u32[5] descriptor slots above stay as the documented X360 shape.
        rw::Resource mMapTextureStateBacking;
        rw::Resource mMaskTextureStateBacking;
        rw::Resource maIconTextureStateBacking[2];

        GuiEventEnableSatNavIcons::EIconDisplayType meIconDisplayType; // h:275 (Construct=COUNT)
        s32  meGameModeFilter;               // h:278 BrnProgression::RaceEventData::EModeType

        // h:281-289 -- per (icon-type-row, event-type-column) UV corner tables. Each entry
        // is a Basic2dColouredVertex::Vector2 (u/v floats). InitEventTypeUvs fills them.
        Vector2 mav2IconUvTopLeft[2][KU_ICON_EVENT_TYPE_COUNT];     // h:281
        Vector2 mav2IconUvBottomLeft[2][KU_ICON_EVENT_TYPE_COUNT];  // h:282
        Vector2 mav2IconUvTopRight[2][KU_ICON_EVENT_TYPE_COUNT];    // h:283
        Vector2 mav2IconUvBottomRight[2][KU_ICON_EVENT_TYPE_COUNT]; // h:284
        Vector2 mav2MiniIconUvTopLeft[2][E_SATNAVICON_EVENTTYPE_MINI_INDEX_COUNT];     // h:286
        Vector2 mav2MiniIconUvBottomLeft[2][E_SATNAVICON_EVENTTYPE_MINI_INDEX_COUNT];  // h:287
        Vector2 mav2MiniIconUvTopRight[2][E_SATNAVICON_EVENTTYPE_MINI_INDEX_COUNT];    // h:288
        Vector2 mav2MiniIconUvBottomRight[2][E_SATNAVICON_EVENTTYPE_MINI_INDEX_COUNT]; // h:289

        bool mbRenderEventStarts;            // h:294
        bool mbRefreshSatNavIcons;           // h:297
        IconRendererSatNavIconInfo maCachedSatNavIcons[KU_MAX_SATNAV_ICONS]; // h:298
        u32  muNumberOfSatNavIcons;          // h:299
        f32  mfZoomLevel;                    // h:301
        bool mbDrawRoute;                    // h:303
        s32  miSatNavRendererPM;             // h:306 perf-monitor handle
    };
}

#endif // BRN_SATNAV_RENDERER_H
