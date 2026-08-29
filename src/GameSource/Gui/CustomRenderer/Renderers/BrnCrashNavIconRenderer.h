#ifndef BRN_CRASH_NAV_ICON_RENDERER_H
#define BRN_CRASH_NAV_ICON_RENDERER_H

// ============================================================================
// b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.h
//
// BrnGui::CrashNavIconRenderer -- the GUI custom-render component (CustomRendererManager
// slot "CrashNavIcon") that draws the CRASH-NAV MAP furniture: the event icons, the map
// cursor, the drive-throughs, the road signs, the start/finish markers and the rival
// icons, on top of the main-map quad.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX against the full DWARF class layout
// (references/DecFIGS/dwarfdump/GameSource/Gui/CustomRenderer/Renderers/
//  BrnCrashNavIconRenderer.h). Every member name below is the DWARF's; every X360 byte
// offset in the comments was gated against the loads/stores of Construct @0x82463520,
// the C++ ctor @0x827E0B28, Prepare @0x82463848, InitResources @0x8245CD20,
// RecvEvent @0x82456168, Release @0x824470A8, Destruct @0x824470C8 and
// SetRenderEnabled @0x827E0CA0.
//
// ---------------------------------------------------------------------------
// ⭐ HEADER CONTRACT (for the sibling RENDER-half TU, BrnCrashNavIconRenderer_wK_01.cpp)
// ---------------------------------------------------------------------------
//  * This header is COMPLETE: all members + all method declarations (core AND render
//    family) are here. The render half must not add members or re-declare methods.
//  * OWNERSHIP. The CORE half (BrnCrashNavIconRenderer.cpp, this agent) bodies:
//        ctor, Construct, Prepare, Release, Destruct, RecvEvent, GetID,
//        SetRenderEnabled, InitResources, InitEventTypeUvs.
//    The RENDER half (BrnCrashNavIconRenderer_wK_01.cpp) bodies:
//        RenderComponent, RenderIcons, RenderEventIcon, RenderCursor,
//        RenderDriveThroughs, RenderDriveThrough, RenderRoadSigns, RenderRoadSign,
//        RenderStartFinish, RenderRivals, RenderQuad, RotatateRect,
//        CalculateUVsForIndex, GetNumIcons, GetIconInformation,
//        GetActiveIconType, IsIgnoredIcon.
//  * The UV tables (mav2IconUv* / mav2MiniIconUv*) are FILLED by InitEventTypeUvs (core
//    half) and READ by the render half. Their indexing is [icon-type row][event-type
//    column]: row = ECrashNavIconType (NOTATTEMPTED / COMPLETED), column = the event-type
//    index 0..KU_ICON_EVENT_TYPE_COUNT-1 for the big icons, and the
//    ECrashNavEventTypeMiniIconIndex for the mini icons.
//  * ACCESS MEMBERS BY NAME. The X360 offsets in the comments are DOCUMENTATION: the gate
//    compiles for a 64-bit host, so every pointer member widens 4 -> 8 and NO console
//    offset is reproducible. Never apply one with a raw cast.
//  * TEXTURE STATES. The five `u32 m*Resource[5]` spans are the console's 5-dword
//    renderengine resource descriptors, kept as the documented X360 shape. The live PC
//    texture states are created through renderengine::TextureState::Initialize over the
//    `rw::Resource m*Backing` members (the BrnSatNavRenderer / CgsFont precedent) --
//    read `mp*TextureState`, never the descriptor words.
//
// FLAGS carried by this slice:
//  * FLAG (opaque render types): renderengine::TextureState / BlendState and
//    CgsGui::ImRendererSet are reached as handles only.
//  * FLAG (atlas constants): KAF_TEXTURE_* / KAF_ICON_* / KAF_MINI_ICON_* live in the
//    X360 .rdata (flt_8205502C..flt_8205505C) which the symbol export does not carry --
//    see the values + FLAG in the .cpp.
//  * (RETIRED 2026-08-29, wave G3) mRoadSignList: BrnGui::RoadSignList::Construct
//    @0x8244BC90 -- the 65-entry literal data table -- IS NOW RECONSTRUCTED, in
//    CustomRenderer/Renderers/BrnRoadSign.cpp (the RoadSign family's own TU, already
//    mounted). All 1,690 fields were replayed out of the function's asm and read from the
//    raw image; see that file's banner for the two structural cross-checks.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                    // Vector3 / Vector4 / CgsID
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"  // CustomRenderComponentInterface base + ImRendererSet
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"    // CgsGraphics::Im2dTransform (mTextTransform)
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"              // CgsGraphics::TextObject / TextRenderer
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // CgsGraphics::Vector2 / RGBA8
#include "GameShared/GameClasses/Containers/CgsArray.h"                        // Array<u32,5> (mOnlineStartpointsToIgnore)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"    // CgsGraphics::Im2dRenderBuffer (== Im2d)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // CgsGraphics::ImRenderBuffer<V> -- the render half's ACTUAL draw target
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                // GuiEventDrawEventIcons + the three map-status payloads
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                          // GuiEventSetHoveredEventIcon (id 559)
#include "GameSource/Gui/SatNav/BrnMainMap.h"                                  // BrnGui::GuiEventRenderMainMap (mRenderMainMapEvent)
#include "GameSource/Gui/SatNav/BrnSatNavIcon.h"                               // BrnGui::CrashNavMapIcon (rival / start-finish icons)
#include "GameSource/Gui/SatNav/BrnEventIconManager.h"                         // BrnGui::EventIconManager::EventIcon2D (RenderEventIcon arg)
#include "GameSource/Gui/CustomRenderer/Renderers/BrnRoadSign.h"               // BrnGui::RoadSign (RoadSignList element)
#include "rw/rwcore_structs.h"                                                 // rw::Resource (texture-state backing), rw::IResourceAllocator

namespace renderengine
{
    // Uncommitted renderengine types this slice only ever holds as handles.
    class TextureState;
    class BlendState;
}

namespace CgsLanguage { class  LanguageManager; }
namespace CgsModule   { struct Event; }

namespace BrnGui
{
    class GuiCache;

    // ------------------------------------------------------------------------
    // DWARF BrnCrashNavIconRenderer.h:81 -- the renderer's fixed table of 65 authored
    // road-sign records. Construct @0x8244BC90 is one huge literal initialiser
    // (65 x { text-box bounds, offsets, sign size, font sizes, two text lines, road id }).
    // ⭐ RECONSTRUCTED 2026-08-29 (wave G3). The body -- all 65 records, every float read
    // from the raw image and every string read at its target VA -- lives in
    // CustomRenderer/Renderers/BrnRoadSign.cpp, next to the RoadSign copy functions it
    // fills. This declaration stays here because the DWARF homes RoadSignList on THIS
    // header (BrnCrashNavIconRenderer.h:81).
    // ------------------------------------------------------------------------
    struct RoadSignList
    {
        static const s32 KI_NUM_ROAD_SIGNS = 65;   // DWARF h:83

        RoadSign maRoadSigns[KI_NUM_ROAD_SIGNS];   // DWARF h:84 (X360 stride 104)

        void Construct();                          // @0x8244BC90 -- body in BrnRoadSign.cpp
    };

    // ------------------------------------------------------------------------
    // DWARF BrnCrashNavIconRenderer.h:97
    //   struct CrashNavIconRenderer : public CgsGui::CustomRenderComponentInterface
    // ------------------------------------------------------------------------
    class CrashNavIconRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // ---- DWARF nested enums (h:51 lives on RoadSign; h:100/108/115/144 here) ----

        // h:100 -- mePrepareStage, driven by Prepare().
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_LOAD  = 1,
            E_PREPARESTAGE_INIT  = 2,
            E_PREPARESTAGE_DONE  = 3,
        };

        // h:108 -- meReleaseStage.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 1,
        };

        // h:115 -- which UV row (and which loaded atlas) an event icon uses.
        enum ECrashNavIconType
        {
            E_CRASHNAVICON_EVENT_NOTATTEMPTED = 0,   // texture id 204
            E_CRASHNAVICON_EVENT_COMPLETED    = 1,   // texture id 205
            E_CRASHNAVICON_NUM                = 2,
        };

        // h:144 -- mini-icon atlas row per event type.
        enum ECrashNavEventTypeMiniIconIndex
        {
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_CURRENT_BURNINGROUTE = 0,
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_ROADRAGE             = 1,
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_BURNINGROUTE         = 2,
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_RACE                 = 3,
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_MARKEDMAN            = 4,
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_STUNT                = 5,
            E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT                = 6,
        };

        // ---- class-scope constants (DWARF h:357-376) ----
        static const u32 KU_ICON_EVENT_TYPE_COUNT             = 11;   // h:357
        static const u32 KU_ICON_EVENT_TYPE_ONLINE_OFFSET     = 6;    // h:358
        static const u32 KU_ICON_EVENT_TYPE_ADDITIONAL_OFFSET = 9;    // h:359
        static const u8  KU_BURNING_ROUTE_ICON_ALPHA          = 100;  // h:360
        static const s32 KI_MAX_RIVALS                        = 8;    // h:374
        static const s32 KI_PLAYER_ICON_INDEX                 = 5;    // h:375
        static const s32 KI_PLAYER_ICON_OVERLAY_INDEX         = 7;    // h:376

        // The two start/finish markers the CrashNav pre-race view draws. Not a DWARF
        // constant (the DWARF spells the member `CrashNavMapIcon mStartFinishIcons[2]`);
        // named here so the render half can bound its loops without a bare 2.
        static const s32 KI_NUM_STARTFINISH_ICONS             = 2;

        CrashNavIconRenderer();   // @0x827E0B28 (compiler-generated: vtable + sub-objects)

        // ---- virtual overrides driven by the GUI CustomRendererManager ----------
        // The signatures MUST stay identical to CgsGui::CustomRenderComponentInterface's;
        // a shadowing redeclaration silently unbinds the override (the SatNavRenderer H3b
        // lesson) and the component then never prepares and never receives an event.
        virtual void  Construct();                                            // @0x82463520
        virtual bool  Prepare(CgsGui::GuiEventQueueSmall* lpOutputEventQueue,
                              rw::IResourceAllocator* lpHeapAllocator,
                              rw::IResourceAllocator* lpTextureAllocator);    // @0x82463848
        virtual bool  Release();                                              // @0x824470A8
        virtual void  Destruct();                                             // @0x824470C8
        virtual void  RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType); // @0x82456168
        virtual CgsID GetID() const;                                          // @0x824470D8
        virtual void  SetRenderEnabled(bool lbRenderEnabled);                 // @0x827E0CA0

        // ---- the two DWARF overrides recovered from the COMPONENT VTABLE -------------
        // ⭐ ADDED 2026-08-29 (FIX2). The DWARF declares both on this class
        // (BrnCrashNavIconRenderer.h:505 Update, :508 GetRenderLayer) and the header carried
        // neither, so both silently inherited the base -- GetRenderLayer is load-bearing
        // (BrnCustomRenderer.cpp:599 draws a component only when GetRenderLayer() == leLayer).
        //
        // SLOT RECOVERY, exactly as BrnInGameMessageRenderer.h documents it. The component
        // vtable is off_820D0034 (named by the ctor @0x827E0B28); read out of the raw image
        // (file offset = VA - 0x82000000, big-endian) its first fourteen slots are
        //   0 Construct 0x82463520   1 Prepare 0x82463848   2 Release 0x824470A8
        //   3 Destruct 0x824470C8    4 GetRenderOutput 0x828476C0
        //   5 RecvEvent 0x82456168   6 Update 0x8284CB38    7 SetRenderEnabled 0x827E0CA0
        //   8 GetRenderLayer 0x82C296C8   9 GetID 0x824470D8   10 GetNumTextures 0x82C296C8
        //   11 StartFade / 12 ClearFadeState 0x8284CB38      13 RenderComponent 0x8246AE38
        // -- five of which are the addresses this header already names, which is what pins
        // the mapping. The two recovered bodies are ICF-folded leaves, read byte-for-byte:
        //   0x82C296C8 = { 38 60 00 01, 4E 80 00 20 } == `li r3,1 ; blr`  -> returns 1
        //   0x8284CB38 = { 4E 80 00 20 }              == `blr`            -> empty
        // So GetRenderLayer() is E_CUSTOMRENDERLAYER_1 (== 1) -- the SAME value as the base
        // default, which is why nothing visibly broke; the crash-nav map draws in layer 1, NOT
        // layer 2 like the ticker and the boost bar. Update() is genuinely EMPTY on this build
        // (its slot is the shared empty leaf that StartFade / ClearFadeState also fold onto).
        // ⚠️ CONSEQUENCE, recorded rather than papered over: the hover/pulse animation members
        // (mfHoveredIconScaleFactor / ...EndTime / ...Growing / mfPlayerIconPulseScale /
        // ...EndTime) are advanced ONLY inside the Render* bodies that read them, never by a
        // per-frame Update. That is the console's own behaviour, not a gap.
        virtual void  Update();                                               // vtable slot 6
        virtual CgsGui::eCustomRenderLayer GetRenderLayer() const;            // vtable slot 8

        // h:562 / h:487 -- header-inline setters the GUI view module drives when it binds
        // the shared text renderer / language manager onto every custom renderer.
        void SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer) { mpTextRenderer = lpTextRenderer; }
        void SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager) { mpLanguageManager = lpLanguageManager; }

    private:
        // ---- RENDER half (BrnCrashNavIconRenderer_wK_01.cpp) --------------------
        // Dispatched by the base's non-virtual Render(set).
        //
        // ⭐ SIGNATURES CORRECTED 2026-08-29 by the render half (E2), against the X360
        //    bodies + call sites. Three things the header-first pass could not see, all
        //    of them load-bearing:
        //  (a) DRAW TARGET. It is CgsGraphics::ImRenderBuffer<Basic2dColouredTexturedVertex>
        //      (the buffered command stream reached as `*lpRendererSet -> mCommandBuffer`),
        //      NOT CgsGraphics::Im2dRenderBuffer (== the direct Im2d). The X360 `v6 + 4`
        //      subobject IS mCommandBuffer, BrnGui::SetMaskRect only takes this type, and
        //      it is the type BrnSatNavRenderer.h / BrnBoostBarRenderer.cpp already commit
        //      for exactly the same seam.
        //  (b) RenderQuad's four Vector2 are the quad's screen-space CORNER POSITIONS
        //      (RotatateRect's outputs, X360 r5..r8), and its trailing Vector4Template<float>&
        //      is the UV RECT {u0,v0,u1,v1} the four vertices sample by lane (a stack POINTER
        //      arg; asm @0x8245DBF8-0x8245DC50). The colour rides v1, i.e. BY VALUE, and by
        //      DWARF it sits SIXTH -- before both state pointers, see the row quoted below.
        //  (c) RenderEventIcon / GetIconInformation carry the console's parameter ORDER:
        //      the first bool is "use the MINI icon", the second is "this icon is filtered
        //      out"; the event ID precedes the icon type, which precedes the event-type
        //      column. (@0x8246A6EC-0x8246A814 pins every register.)
        typedef CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>
                Im2dCommandBuffer;

        virtual void RenderComponent(CgsGui::ImRendererSet* lpRendererSet);   // @0x8246AE38

        void RenderIcons(Im2dCommandBuffer* lpRenderBuffer);                  // @0x8246A410
        // @0x8245D210. The four f32* are the per-icon-type half-extent tables the caller
        // scaled by 1/zoom once for the whole frame (see the RenderIcons call sites).
        // ⭐ PARAMETER ORDER CORRECTED 2026-08-29 (FIX2) to the DWARF row
        // (BrnCrashNavIconRenderer.h:542): the Vector3 rides BY VALUE immediately after the
        // two u32s and BEFORE the four f32* half-extent tables --
        //   (Im2dRenderBuffer*, bool, bool, ECrashNavIconType, uint32_t, uint32_t,
        //    Vector3, float32_t*, float32_t*, float32_t*, float32_t*, RGBA8,
        //    EventIcon2D*, int32_t*)
        // The header had it interleaved (mini-width, Vector3, mini-height, width, height),
        // which the X360 register allocation does not support: the position is a whole VMX
        // lane (v1) and the four table pointers are four consecutive GPRs.
        void RenderEventIcon(Im2dCommandBuffer* lpRenderBuffer,
                             bool lbUseMiniIcon, bool lbFiltered,
                             ECrashNavIconType leIconType,
                             u32 luEventId, u32 luEventTypeIndex,
                             Vector3 lv3Position,
                             const f32* lpafMiniHalfWidth,
                             const f32* lpafMiniHalfHeight,
                             const f32* lpafHalfWidth,
                             const f32* lpafHalfHeight,
                             u32 luColour,
                             EventIconManager::EventIcon2D* lpaIcon2D,
                             s32* lpiIconCount);
        u32  GetNumIcons() const;                                             // @0x82456C68
        void GetIconInformation(u32 luIndex, bool* lpbFiltered, bool* lpbUseMiniIcon,
                                u32* lpuEventID, ECrashNavIconType* lpeIconType,
                                u32* lpuEventTypeIndex, Vector3& lrv3Position) const; // @0x82456D80
        // DWARF cpp:2797 -- inlined into InitEventTypeUvs / RenderEventIcon on the X360.
        void CalculateUVsForIndex(s32 liIndex,
                                  CgsGraphics::Vector2& lrv2TopLeft,
                                  CgsGraphics::Vector2& lrv2BottomLeft,
                                  CgsGraphics::Vector2& lrv2TopRight,
                                  CgsGraphics::Vector2& lrv2BottomRight,
                                  f32 lfTextureWidth, f32 lfTextureHeight,
                                  f32 lfIconWidth, f32 lfIconHeight);
        void RenderCursor(Im2dCommandBuffer* lpRenderBuffer);                 // @0x8245D8C0
        void RenderDriveThroughs(Im2dCommandBuffer* lpRenderBuffer, f32 lfScale);          // @0x82469038
        void RenderDriveThrough(Im2dCommandBuffer* lpRenderBuffer, s32 liIndex, f32 lfScale); // @0x824639F8
        void RenderRoadSigns(Im2dCommandBuffer* lpRenderBuffer);              // @0x824692D8
        void RenderRoadSign(Im2dCommandBuffer* lpRenderBuffer, s32 liIndex);  // @0x82463E78
        void RenderStartFinish(Im2dCommandBuffer* lpRenderBuffer);            // @0x82465210
        void RenderRivals(Im2dCommandBuffer* lpRenderBuffer);                 // @0x82465468
        // @0x8245DB90 -- one textured quad through the immediate-mode buffer.
        // ⭐ PARAMETER ORDER CORRECTED 2026-08-29 (FIX2) to the DWARF row
        // (BrnCrashNavIconRenderer.h:575): the COLOUR comes 6th, BY VALUE, and the UV rect is
        // LAST, after both state pointers --
        //   (Im2dRenderBuffer*, const Vector2&, const Vector2&, const Vector2&,
        //    const Vector2&, Vector4, const TextureState*, const BlendState*,
        //    const Vector4Template<float>&)
        // The X360 allocation is unambiguous and agrees: at RenderRoadSign's second call site
        // (@0x82464DE8-0x82464E94) r5..r8 are the four corner pointers, r9 = this+0x128
        // (mpRoadSignsTextureState), r10 = dword_83010F20 (the blend state), the UV rect goes
        // out as a POINTER in the first stack slot (`addi r11,r1,var_210 / stw r11,var_21C`),
        // and the colour rides v1 (`vmr128 v1, v126`).
        void RenderQuad(Im2dCommandBuffer* lpRenderBuffer,
                        const CgsGraphics::Vector2& lrv2TopLeft,
                        const CgsGraphics::Vector2& lrv2BottomLeft,
                        const CgsGraphics::Vector2& lrv2TopRight,
                        const CgsGraphics::Vector2& lrv2BottomRight,
                        Vector4 lv4Colour,
                        const renderengine::TextureState* lpTextureState,
                        const renderengine::BlendState* lpBlendState,
                        const Vector4& lrv4UvRect);
        // @0x8245DD28 -- DWARF spelling (sic: "Rotatate"). Rotate a screen rect about its
        // centre into four corner points.
        void RotatateRect(Vector4 lv4Rect, f32 lfRotationInRadians,
                          CgsGraphics::Vector2& lrv2TopLeft,
                          CgsGraphics::Vector2& lrv2BottomLeft,
                          CgsGraphics::Vector2& lrv2TopRight,
                          CgsGraphics::Vector2& lrv2BottomRight);
        // h:512 / h:536 -- header-inline helpers on the console.
        GuiEventDrawEventIcons::EIconDisplayType GetActiveIconType() const;
        bool IsIgnoredIcon(u32 luEventId) const;

        // ---- CORE half (this header's .cpp) ------------------------------------
        void InitResources();      // @0x8245CD20
        void InitEventTypeUvs();   // @0x824566F0

        // ---- member state (DWARF h:380-476; X360 offsets are DOCUMENTATION) -----
        // NOTE: mbRenderEnabled lives in the base (CustomRenderComponentInterface, +0x04).

        CgsGraphics::Im2dTransform    mTextTransform;        // h:380  X360 +0x010 (4 x Vector4)
        CgsLanguage::LanguageManager* mpLanguageManager;     // h:382  X360 +0x050
        EPrepareStage                 mePrepareStage;        // h:384  X360 +0x054
        EReleaseStage                 meReleaseStage;        // h:385  X360 +0x058
        // h:387 X360 +0x060 (48 B). RecvEvent case 223 memcpy's the whole record; on this
        // host the record is native-width, so it is copied by ASSIGNMENT (the PlayAptMovie /
        // SatNav-212 truncation precedent), never by a literal 48-byte memcpy.
        GuiEventRenderMainMap         mRenderMainMapEvent;
        GuiCache*                     mpGuiCache;            // h:388  X360 +0x090
        rw::IResourceAllocator*       mpHeapAllocator;       // h:389  X360 +0x094
        CgsGui::GuiEventQueueSmall*   mpOutputEventQueue;    // h:394  X360 +0x098

        // h:397-411 -- five renderengine resource descriptors + their texture states. Each
        // "Resource" is the console's 5-dword descriptor the InitResources copy loop fills.
        u32                           mBackgroundMaskTextureStateResource[5]; // h:397 X360 +0x09C
        renderengine::TextureState*   mpBackgroundMaskTextureState;           // h:398 X360 +0x0B0 (texture id 203)
        u32                           mPreRaceMaskTextureStateResource[5];    // h:400 X360 +0x0B4
        renderengine::TextureState*   mpPreRaceMaskTextureState;              // h:401 X360 +0x0C8 (texture id 202)
        u32                           maIconResources[E_CRASHNAVICON_NUM][5]; // h:404 X360 +0x0CC
        renderengine::TextureState*   mapIconTextureStates[E_CRASHNAVICON_NUM];// h:405 X360 +0x0F4 (ids 204/205)
        u32                           mIconsTextureStateResource[5];          // h:407 X360 +0x0FC
        renderengine::TextureState*   mpIconsTextureState;                    // h:408 X360 +0x110 (texture id 206)
        u32                           mRoadSignsTextureStateResource[5];      // h:410 X360 +0x114
        renderengine::TextureState*   mpRoadSignsTextureState;                // h:411 X360 +0x128 (texture id 235)

        // PC fold (BrnSatNavRenderer precedent): backing storage for the runtime-created
        // texture states -- renderengine::TextureState::Initialize(rw::Resource*, Parameters*).
        // The console's u32[5] descriptor slots above stay as the documented X360 shape.
        rw::Resource                  mBackgroundMaskTextureStateBacking;
        rw::Resource                  mPreRaceMaskTextureStateBacking;
        rw::Resource                  maIconTextureStateBacking[E_CRASHNAVICON_NUM];
        rw::Resource                  mIconsTextureStateBacking;
        rw::Resource                  mRoadSignsTextureStateBacking;

        GuiEventDrawEventIcons::EIconDisplayType meIconDisplayType; // h:414 X360 +0x12C (Construct/Release/Destruct = COUNT)
        // h:417 X360 +0x130 (count word @+0x144). The DWARF types the element uint32_t, but
        // the X360 MANGLING is Array<int,5> (Append @0x82448C28 / Contains @0x8244FE48, the
        // instantiation committed in CgsArrayInt5.cpp), so the element is s32 here.
        Array<s32, 5>                 mOnlineStartpointsToIgnore;
        u32                           muInspectedEventID;           // h:420 X360 +0x148 (event 558)
        GuiEventSetHoveredEventIcon   mHoveredEventIcon;            // h:423 X360 +0x150 (24 B, event 559)
        GuiEventSetHoveredEventIcon   mHoveredEventIconLastFrame;   // h:424 X360 +0x168 (24 B)
        s32                           miSelectedIndex;              // h:425 X360 +0x180
        f32                           mfHoveredIconScaleFactor;     // h:426 X360 +0x184 (Construct = 1.0)
        f32                           mfHoveredIconScaleEndTime;    // h:427 X360 +0x188
        bool                          mfHoveredIconGrowing;         // h:428 X360 +0x18C (sic: DWARF spells the bool "mf...")
        f32                           mfPlayerIconPulseScale;       // h:429 X360 +0x190 (Construct = 1.0)
        f32                           mfPlayerIconPulseEndTime;     // h:430 X360 +0x194
        GuiEventMapCursorStatus       mGuiEventMapCursorStatus;     // h:432 X360 +0x1A0 (32 B, event 560)
        // h:433 X360 +0x1C0. ⚠️ NOT initialised by Construct on the console (the ctor's
        // store set has no +0x1C0 write) -- reproduced faithfully; the render half must
        // treat it as valid only after the first event-560 arrival.
        f32                           mfCursorScaleFactor;
        GuiEventMapIconStatus         mGuiEventMapIconStatus;       // h:434 X360 +0x1C4 (8 B, event 561)
        CrashNavMapIcon               mRivalIcons[KI_MAX_RIVALS];   // h:436 X360 +0x1D0 (stride 0x1F0)
        s32                           miRivalIconsCount;            // h:437 X360 +0x1150
        CrashNavMapIcon               mStartFinishIcons[KI_NUM_STARTFINISH_ICONS]; // h:439 X360 +0x1160
        s32                           miStartFinishIconCount;       // h:440 X360 +0x1540
        GuiEventRoadSignIconStatus    mRoadSignIconStatus;          // h:443 X360 +0x1544 (8 B, event 562)
        RoadSignList                  mRoadSignList;                // h:444 X360 +0x154C (65 x 104)
        CgsGraphics::TextObject       mTextObject;                  // h:446 X360 +0x2FB4
        CgsGraphics::TextRenderer*    mpTextRenderer;               // h:447 X360 +0x3030
        // h:450 X360 +0x3034 -- BrnProgression::RaceEventData::EModeType. Held as s32
        // (the SatNavRenderer convention) so this header does not drag in the progression
        // headers; Construct seeds it with 6 and RecvEvent case 557 overwrites it.
        s32                           meGameModeFilter;

        // h:453-461 -- per (icon-type row, event-type column) UV corner tables, filled by
        // InitEventTypeUvs. The DWARF types the entry Basic2dColouredVertex::Vector2, i.e.
        // the 8-byte (u,v) pair -- CgsGraphics::Vector2, NOT the 16-byte VMX Vector2.
        CgsGraphics::Vector2 mav2IconUvTopLeft    [E_CRASHNAVICON_NUM][KU_ICON_EVENT_TYPE_COUNT]; // h:453 X360 +0x3038
        CgsGraphics::Vector2 mav2IconUvBottomLeft [E_CRASHNAVICON_NUM][KU_ICON_EVENT_TYPE_COUNT]; // h:454 X360 +0x30E8
        CgsGraphics::Vector2 mav2IconUvTopRight   [E_CRASHNAVICON_NUM][KU_ICON_EVENT_TYPE_COUNT]; // h:455 X360 +0x3198
        CgsGraphics::Vector2 mav2IconUvBottomRight[E_CRASHNAVICON_NUM][KU_ICON_EVENT_TYPE_COUNT]; // h:456 X360 +0x3248
        CgsGraphics::Vector2 mav2MiniIconUvTopLeft    [E_CRASHNAVICON_NUM][E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT]; // h:458 X360 +0x32F8
        CgsGraphics::Vector2 mav2MiniIconUvBottomLeft [E_CRASHNAVICON_NUM][E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT]; // h:459 X360 +0x3358
        CgsGraphics::Vector2 mav2MiniIconUvTopRight   [E_CRASHNAVICON_NUM][E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT]; // h:460 X360 +0x33B8
        CgsGraphics::Vector2 mav2MiniIconUvBottomRight[E_CRASHNAVICON_NUM][E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT]; // h:461 X360 +0x3418

        u32  mauIconsToIgnore[GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE]; // h:466 X360 +0x3478
        s32  miNumIconsToIgnore;                                  // h:467 X360 +0x34A0
        bool mbRenderEventStarts;                                 // h:470 X360 +0x34A4
        bool mbOldTypeFading;                                     // h:473 X360 +0x34A5
        GuiEventDrawEventIcons::EIconDisplayType meFadingIconDisplayType; // h:474 X360 +0x34A8
        f32  mfIconFadeStartTime;                                 // h:475 X360 +0x34AC
        f32  mfIconFadeEndTime;                                   // h:476 X360 +0x34B0

    public:
        // Never called; pins the pointer-invariant facts the X360 bodies depend on. These
        // are counts/strides, not byte offsets -- the offsets above legitimately differ on
        // the 64-bit gate and are documentary only.
        static void _AssertLayout()
        {
            // The InitResources loop runs exactly E_CRASHNAVICON_NUM times over
            // maIconResources / mapIconTextureStates (X360 end-pointer bound
            // &unk_82F25878 == &maResourcesToLoad[2]).
            static_assert(E_CRASHNAVICON_NUM == 2,
                          "InitResources walks exactly two icon atlases (ids 204/205)");
            // Construct's five int_5_::Append calls must fit the array (X360 +0x130).
            static_assert(Array<s32, 5>::KU_SIZE == 5,
                          "Construct appends five online start-points to ignore");
            // The four big UV tables are 2 x 11 entries; the four mini tables 2 x 6.
            static_assert(KU_ICON_EVENT_TYPE_COUNT == 11,
                          "InitEventTypeUvs fills 11 event-type columns per icon-type row");
            static_assert(E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT == 6,
                          "InitEventTypeUvs fills 6 mini-icon columns per icon-type row");
            // RecvEvent case 554 hands GetIgnoreIcons the whole mauIconsToIgnore span.
            static_assert(GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE == 10,
                          "mauIconsToIgnore is the event's own 10-entry ignore list");
            // The rival / start-finish icon banks (X360 +0x1D0 and +0x1160).
            static_assert(KI_MAX_RIVALS == 8, "eight rival icons (X360 +0x1D0, stride 0x1F0)");
            static_assert(KI_NUM_STARTFINISH_ICONS == 2, "two start/finish icons (X360 +0x1160)");
            // The road-sign table (X360 +0x154C, 65 x 104 bytes ending at mTextObject).
            static_assert(RoadSignList::KI_NUM_ROAD_SIGNS == 65,
                          "65 authored road signs (X360 +0x154C..+0x2FB4 == 65 x 104)");
            // The 24-byte hover record RecvEvent case 559 copies as three doublewords, and
            // the 32-byte cursor record case 560 copies as four -- both are pointer-free
            // on the console but the road-name pointer widens here, so only the FIELD SET
            // is pinned (by name, at compile time, through the assignments in the .cpp).
        }
    };
}

#endif // BRN_CRASH_NAV_ICON_RENDERER_H
