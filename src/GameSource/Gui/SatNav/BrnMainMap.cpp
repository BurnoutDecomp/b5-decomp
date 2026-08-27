#include "GameSource/Gui/SatNav/BrnMainMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // the one-shot gate log
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"       // CgsGui::GuiAccessPointers::GetGuiCache
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::GetAccessPointers
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"          // BrnGui::MapTransform (device space / world rect)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"      // BrnGui::GuiEventShowHideSatNav (the 213 payload)

#include "rw/math/vpu/vector4_operation.h"          // rw::math::vpu::Magnitude (the Construct assert)

// BrnGui::MainMapComponent - the sat-nav main-map screen component bodies.
//
// This TU reconstructs Construct @0x8245E228, Prepare @0x8244F4A8,
// SetStandardDefZoomParams @0x82447ED8 and RecvEvent @0x82458370, and defines the two
// class-static zoom-scale tables the DWARF places inside the class (h:218 / h:219).
// The remaining ledger functions (Update, SnapToLocation, ApplyZoom, SetZoom) are still
// held back because they depend on entities that are not yet recovered/homed:
//   - the hand-written VMX world-rect transform pipeline runs through sub_8245A080
//     (external/unknown vector helper) plus the still-todo CalculateViewPaddingOffset
//     and CalculatePositionedWorldRect methods (Update / SnapToLocation / ApplyZoom);
//   - SetZoom needs CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiAudioTriggerEvent>
//     and Update needs OutputViewState<GuiEventRenderMainMap>, neither of which has a
//     reconstructed home.
//
// ⭐ THE ZOOM-SCALE TABLE VALUES ARE NO LONGER UNKNOWN. The old banner said the two
// file-static float tables "are not present in the dossier or the IDA export and must not
// be guessed". They ARE present -- as .data in the image, which the export naturally does
// not carry. Read out of scratch/postfx_step9_final/envfix/work/image.bin (file offset =
// VA - 0x82000000), big-endian:
//     flt_82F259DC = 45CB2000 455AC000 451C4000 00000000 = { 6500, 3500, 2500, 0 }
//     flt_82F259EC = 459C4000 452BE000 447A0000 00000000 = { 5000, 2750, 1000, 0 }
// flt_82F259DC is the LIVE table (mfZoomScalFactors) and flt_82F259EC the standard-def
// source (mfStandardDefZoomScalFactors) -- pinned by SetStandardDefZoomParams @0x82447ED8,
// which copies EC -> DC element by element, and by Construct's `lfs f0,
// (flt_82F259E4 - 0x82F259DC)(r10)` == mfZoomScalFactors[E_ZOOMFACTOR_HIGH] (index 2).
//
// ⚠️ MOUNT NOTE FOR THE CONDUCTOR (measured, stunt-race UI wave 2026-08-27). This TU is
// UNMOUNTED today and GameSource/Gui/SatNav/BrnMainMapLinkGates.cpp currently carries an
// inert stand-in for MainMapComponent::RecvEvent. Those two must NEVER coexist -- the
// real RecvEvent lives HERE (:below) and the gate would be a second definition (LNK2005).
// Mounting this TU closes MainMapComponent::{Construct, Prepare, RecvEvent} and opens
// exactly ONE new external, BrnGui::MapManager::RecvEvent @0x8244F898 (declared
// BrnMapManager.h:68, bodied only in the unmounted BrnMapManager.cpp). MapManager::Construct
// @0x82458590 -- the OTHER MapManager entry this TU would need -- is neither declared in
// BrnMapManager.h nor bodied anywhere, so it is routed through the local FLAG boundary
// below rather than left as a second hole.

namespace BrnGui
{
    // ---- the two class-static zoom-scale tables (DWARF BrnMainMap.h:218 / :219) --------
    // X360 .data flt_82F259DC / flt_82F259EC; see the banner for the raw bit patterns.
    // mfZoomScalFactors is indexed by ZoomFactor: LOW / MEDIUM / HIGH / CUSTOM, and the
    // CUSTOM slot is a genuine 0.0f on the console (SetZoom overwrites it with the
    // caller's custom scale before use) -- NOT a placeholder.
    f32 MainMapComponent::mfZoomScalFactors[4]            = { 6500.0f, 3500.0f, 2500.0f, 0.0f };
    f32 MainMapComponent::mfStandardDefZoomScalFactors[4] = { 5000.0f, 2750.0f, 1000.0f, 0.0f };

    namespace
    {
        // BrnMainMap.cpp:96 -- the console's own guard constant (flt_820068C0 = 1000000.0f).
        const f32 KF_MAX_WORLD_RECT_MAGNITUDE = 1000000.0f;
        // The vperm/vaddfp rect-centre halving constant in Prepare (flt_82001DA0 = 0.5f).
        const f32 KF_RECT_CENTRE_SCALE = 0.5f;

        // Same one-shot logger shape as BrnMainMapLinkGates.cpp / BrnFriendsListLinkGates.cpp:
        // an inert stand-in must never be scored as a silent success.
        void LogGateOnce(bool& lrbLogged, const char* lpacSymbol)
        {
            if (lrbLogged)
            {
                return;
            }
            lrbLogged = true;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[mainmap-link-gate] " << lpacSymbol
                    << ": inert stand-in, no body anywhere in the tree [FLAG link scaffold]\n";
            }
        }

    }

    // -------------------------------------------------------------------------
    // FLAG BOUNDARY -- BrnGui::MapManager::Construct @0x82458590.
    //
    // MainMapComponent::Construct's `bl BrnGui__MapManager__Construct` (0x8245E3AC, on
    // `this + 0x8C` == the embedded mMapManager, with the StateInterface in r4) has NO
    // reachable target on the host: the method is not declared in BrnMapManager.h and not
    // defined in BrnMapManager.cpp. Calling it directly would not even compile, so the call
    // is routed through this boundary, exactly as BrnBootLegal.cpp routes its cache entries.
    //
    // THE RECOVERED SEMANTICS, so the follow-on wave can lift them verbatim (X360 pseudocode
    // dumped this wave; offsets are MapManager-relative and match BrnMapManager.h):
    //     CGS_ASSERT(lpStateInterface != NULL, "lpStateInterface != NULL")  // BrnMapManager.cpp:62
    //     mpStateInterface = lpStateInterface;                              // +0x56C (1388)
    //     mpAllocator      = lpStateInterface->GetAllocator();              // +0x574 (1396)
    //                                       // (the CgsGuiStateInterface.h:337 "mpAllocator
    //                                       //  != NULL" assert is that accessor's own)
    //     mActiveTextures.muTextureCount = 0;                               // +0x560 (1376)
    //     mbEnabled                      = 0;                               // +0x564 (1380)
    //     muTilesRequestedCount          = 0;                               // +0x568 (1384)
    //     meZoomLevel                    = E_ZOOM_MEDIUM;                   // +0x570 (1392)
    //     mapDirectories[E_ZOOM_MEDIUM]  = 0; mapDirectories[E_ZOOM_HIGH] = 0; // +0x2AC/+0x2B0
    //     memset(&maRequestedTiles, 0, 528);                                // +0x9C (156)
    //     mLowResTexture / mLowResTextureCache head words zeroed;           // +0x20 / +0x44
    //     mWorldRect  = { 0, 0, 1, 1 };                                     // +0x00..+0x0F
    //     mScreenRect = MapTransform::Transform(worldSpace -> normalisedSpace) of the two
    //                   unit corners (the two `bl BrnGui::MapTransform::Transform` calls on
    //                   the smm33WorldSpace @0x82FB3610 / smm33NormalisedSpace @0x82FB2FA0
    //                   copies), stored at +0x34..+0x40.
    //
    // [FLAG PC-only link gate] Inert. On the stunt-run fly-by path this costs nothing: the
    // map is not applicable to STUNT_ATTACK, so nothing ever reads the tile working set
    // (S1 §4). It IS load-bearing for any mode that draws the map.
    // DELETE-WHEN BrnMapManager.h declares `void Construct(CgsGui::StateInterface*)` and
    // BrnMapManager.cpp bodies it (the S1 §2 ctor-split mount) -- then this whole namespace
    // goes and Construct calls `mMapManager.Construct(lpStateInterface)` directly.
    // -------------------------------------------------------------------------
    namespace MainMapMapManagerBoundary
    {
        void Construct(MapManager* /*lpMapManager*/, CgsGui::StateInterface* /*lpStateInterface*/)
        {
            static bool sbLogged = false;
            LogGateOnce(sbLogged, "BrnGui::MapManager::Construct");
        }
    }

    // -------------------------------------------------------------------------
    // FLAG BOUNDARY -- the GuiCache high-definition byte at X360 GuiCache +0x4B49 (19273).
    //
    // Construct's tail reads it (`lbz r11, 0x4B49(r10)` @0x8245E5D4) and takes the
    // standard-def zoom table when it is CLEAR. BrnGuiCache.h has no member there yet: the
    // byte falls inside `mPad_4B44[6]` (BrnGuiCache.h:1172), and this TU may not carve the
    // cache header, so the read is routed through this boundary.
    //
    // WHAT THE BYTE IS, measured across all four consumers in the export set -- every one of
    // them picks an HD constant when SET and an SD constant when CLEAR:
    //   * MainMapComponent::Construct @0x8245E5D4 -- clear => SetStandardDefZoomParams(),
    //     i.e. the live zoom table becomes {5000, 2750, 1000, 0} instead of {6500, 3500,
    //     2500, 0}. This is the strongest witness: the SD table is literally named.
    //   * CrashNavMapMain::HandleCrashNavInputPressed @0x824CCC74 -- set => SetZoom custom
    //     9000.0f, clear => 12000.0f.
    //   * CrashNavDriverDetails::UpdateWFInit @0x824BFEB8 -- set => LicenseComponent
    //     position unk_82FB4A90, clear => unk_82FB4C00.
    //   * RoadSignIconManager::Update @0x82517014 -- set => the alternate distance-fade pair
    //     at unk_82F27FA8/AC; BootLegal::Update @0x824778D8 gates the HD-composite transin.
    // There is NO writer anywhere in the export set (grepped every `st?` to 0x4B49 across
    // .ida-exports: zero hits), so the producer is a dyn-init or an unexported path -- the
    // recurring "a .bss zero does not mean the console value is zero" trap. The stand-in
    // therefore does NOT read the byte.
    //
    // [FLAG PC-only stand-in] Returns true because this host IS the HD path:
    // GuiModule::Construct installs the HD sat-nav rect and constructs the GUI resource
    // module with HighDef == true (BrnGuiModule.cpp:1062 / :1140). Returning false here
    // would silently retune every map zoom to the SD table.
    // DELETE-WHEN BrnGuiCache.h carves that byte as a named member (suggested
    // `bool mbIsHighDef;  // +0x4B49 (19273)`, out of mPad_4B44, WITHOUT shifting
    // mbInEventColouringGate at +0x4B4A) with an accessor -- then this reads
    // `lpGuiCache->IsHighDef()` and the boundary goes.
    // -------------------------------------------------------------------------
    namespace MainMapCacheBoundary
    {
        bool IsHighDef(const GuiCache* /*lpGuiCache*/)
        {
            static bool sbLogged = false;
            LogGateOnce(sbLogged, "BrnGui::GuiCache high-definition byte (+0x4B49)");
            return true;
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::Construct
    //
    // X360 ARTIST @0x8245E228 (json name field verified: BrnGui::MainMapComponent::Construct).
    // Brings the map component up: the two argument asserts, the GuiComponent base
    // construction under the name "MainMap", the GuiCache latch, the embedded MapManager,
    // the default zoom selection, the caller's view/padding rects and map type, the
    // device-space display rect derived from the padding rect, and the world rect + its
    // sanity assert. Finishes with the flag block and the standard-def zoom fallback.
    //
    // ⭐ THE WORLD RECT. The X360 reads flt_82FB31F0 == MapTransform::smv4WorldRect (the
    // fixed, const world-space rect {-4375.42, -5842.42, 5363.15, 3904.74}, already homed
    // at BrnMapUtils.cpp) and copies it into the instance member the DWARF declares. Every
    // other `flt_82FB31F0` site in the export set (Prepare 0x8244F4D0, this function
    // 0x8245E514, CalculatePositionedWorldRect 0x8245E61C, CrashNavMap::MoveCursor
    // 0x824BF2F4, RoadSignIconManager::SetupComponent 0x8250AEFC) is an `addi` address
    // formation feeding an `lvx`/`lfs` -- i.e. a READ. Nothing writes it. (The old
    // BrnMainMap.h banner called three of them WRITERS; that is corrected there this wave.)
    //
    // ⭐ THE DISPLAY RECT. The 40-instruction VMX block at 0x8245E430..0x8245E518 is two
    // MapTransform::Transform applications, inlined and interleaved: the padding rect's two
    // corners (x,y) and (z,w) are each promoted to the homogeneous (px, py, 1) form by
    // `vrlimi128 v,v13,2,0` (v13 == vcfsx of vspltisw 1 == 1.0f) and multiplied against the
    // three rows of unk_82FB3050 == MapTransform::smm33DeviceSpace, exactly the
    // `p.x*xAxis + p.y*yAxis + zAxis` that BrnMapUtils.cpp's Transform performs. The two
    // results are re-interleaved by the perm masks at unk_82CDA350 / unk_82CDA3C0 plus a
    // `vsldoi ...,8` into {minX, minY, maxX, maxY}. Mask bytes read from the image:
    // 82CDA350 = {0,1,2,3, 20,21,22,23, 0,1,2,3, 0,1,2,3} (word0 of A, word1 of B, word0,
    // word0) and 82CDA3C0 = {0,1,2,3, 0,1,2,3, 0,1,2,3, 20,21,22,23}.
    // -------------------------------------------------------------------------
    void MainMapComponent::Construct(CgsGui::StateInterface* lpStateInterface,
                                     MainMapParameterBundle* lpParameters)
    {
        // cpp:65 / cpp:66 -- both are streamed asserts on the console (BeginAssert +
        // StrStreamBase + FireAssert); the message text is the console's own.
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface pointer");
        CGS_ASSERT(lpParameters != 0, "Invalid data pointer");

        // X360 `bl CgsGui__GuiComponent__Construct` with r6 == 0: no parent name.
        CgsGui::GuiComponent::Construct("MainMap", lpStateInterface, 0);

        // The X360 inlines both accessors (the "mpAccessPointers != NULL" assert at
        // CgsGuiStateInterface.h:344 and the "mpGuiCache" assert at CgsGuiShared.h:201 are
        // theirs); restored as the real calls, which already carry those asserts.
        mpGuiCache = lpStateInterface->GetAccessPointers()->GetGuiCache();

        MainMapMapManagerBoundary::Construct(&mMapManager, lpStateInterface);

        // The console's default: E_ZOOMFACTOR_HIGH, with both zoom scalars seeded from that
        // slot of the live table (`lfs f0, (flt_82F259E4 - flt_82F259DC)(r10)` -- index 2).
        meCurrentZoomFactor      = E_ZOOMFACTOR_HIGH;
        mfDesiredWorldZoomFactor = mfZoomScalFactors[E_ZOOMFACTOR_HIGH];
        mfWorldZoomScaleFactor   = mfZoomScalFactors[E_ZOOMFACTOR_HIGH];

        // The caller's bundle, in the console's store order (view rect, padding rect, type).
        mv4ViewRect    = lpParameters->mv4ViewRect;
        mv4PaddingRect = lpParameters->mv4PaddingRect;
        meMapType      = lpParameters->meMapType;

        // The padding rect, taken into device space corner by corner (see the banner).
        const Matrix33 lm33DeviceSpace = MapTransform::GetDeviceSpace();
        const Vector2  lv2PaddingMin   = { mv4PaddingRect.x, mv4PaddingRect.y, 0.0f, 0.0f };
        const Vector2  lv2PaddingMax   = { mv4PaddingRect.z, mv4PaddingRect.w, 0.0f, 0.0f };
        const Vector2  lv2DisplayMin   = MapTransform::Transform(lv2PaddingMin, lm33DeviceSpace);
        const Vector2  lv2DisplayMax   = MapTransform::Transform(lv2PaddingMax, lm33DeviceSpace);

        mv4DisplayRect.x = lv2DisplayMin.x;
        mv4DisplayRect.y = lv2DisplayMin.y;
        mv4DisplayRect.z = lv2DisplayMax.x;
        mv4DisplayRect.w = lv2DisplayMax.y;

        mv4WorldRect = MapTransform::GetWorldRect();

        // cpp:96 -- the console's own sanity check on the world rect it just installed. The
        // vendor Magnitude (vector4_operation_inline.h) carries the console's exact semantics
        // including the vsel zero-dot guard (sqrt(0) == 0).
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        mbIsZooming  = false;
        mpMapManager = 0;

        // The map starts pinned to every screen edge and active; PreRaceFlyByState::OnEnter
        // immediately clears all four via SetStickMapToScreenEdges(false, false, false, false).
        mbStickMapUp    = true;
        mbStickMapDown  = true;
        mbStickMapLeft  = true;
        mbStickMapRight = true;
        mbIsActive      = true;

        // X360 `lbz r11, 0x4B49(mpGuiCache)`: the cache's high-definition byte. Clear ==
        // standard definition, which swaps the whole zoom-scale table for the SD one.
        if (!MainMapCacheBoundary::IsHighDef(mpGuiCache))
        {
            SetStandardDefZoomParams();
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::Prepare
    //
    // X360 ARTIST @0x8244F4A8 (json name field verified: BrnGui::MainMapComponent::Prepare).
    // Marks the component active and parks the desired world centre at the centre of the
    // fixed world rect. Then returns true unconditionally (`li r3, 1` at 0x8244F4DC).
    //
    // The whole VMX body is one rect-centre: `lvx` of flt_82FB31F0 ==
    // MapTransform::smv4WorldRect, four `vspltw` splats of its lanes, `vaddfp` of lane0+lane2
    // and lane1+lane3, and a multiply by the scalar at flt_82001DA0. That scalar is read from
    // the image (file offset 0x1DA0, big-endian 3F000000) == 0.5f -- NOT a placeholder.
    //
    // ⭐ THE STORE IS A FULL QUADWORD WITH A DUPLICATED X LANE, and it is transcribed as such
    // rather than tidied to {x, y, 0, 0}. The final `vperm v0, v0, v13, v7` uses the mask at
    // unk_82CDA350 = {0,1,2,3, 20,21,22,23, 0,1,2,3, 0,1,2,3}: word0 and word1 take the two
    // centre components, and words 2 AND 3 take a SECOND copy of the x component. The
    // `stvx128 v0, r8, 0x650` then commits all four lanes to mv2DesiredCentre. Nothing in the
    // recovered set reads .z/.w of that member, but the console value is x, not 0.
    // -------------------------------------------------------------------------
    bool MainMapComponent::Prepare()
    {
        mbIsActive = true;

        const Vector4& lrv4WorldRect = MapTransform::GetWorldRect();

        const f32 lfCentreX = (lrv4WorldRect.x + lrv4WorldRect.z) * KF_RECT_CENTRE_SCALE;
        const f32 lfCentreY = (lrv4WorldRect.y + lrv4WorldRect.w) * KF_RECT_CENTRE_SCALE;

        mv2DesiredCentre.x = lfCentreX;
        mv2DesiredCentre.y = lfCentreY;
        mv2DesiredCentre.z = lfCentreX;   // vperm lane 2 -- the console's duplicated x
        mv2DesiredCentre.w = lfCentreX;   // vperm lane 3 -- ditto

        return true;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::SetStandardDefZoomParams
    //
    // X360 ARTIST @0x82447ED8. Copies the standard-definition zoom-scale table over the live
    // one, element by element. The loop carries an inlined array-accessor bounds assert
    // (BrnMainMap.h:265) whose counter is pre-incremented, so it is checked against
    // E_ZOOMFACTOR_COUNT AFTER the store and can never fire for the four in-range indices --
    // reproduced as written rather than hoisted, because the console's check order is the
    // observable part.
    // -------------------------------------------------------------------------
    void MainMapComponent::SetStandardDefZoomParams()
    {
        s32 liEnumIndex = 0;
        for (s32 liZoom = 0; liZoom < E_ZOOMFACTOR_COUNT; ++liZoom)
        {
            ++liEnumIndex;
            mfZoomScalFactors[liZoom] = mfStandardDefZoomScalFactors[liZoom];
            CGS_ASSERT(liEnumIndex <= E_ZOOMFACTOR_COUNT,
                       "leEnumIndex <= MainMapComponent::E_ZOOMFACTOR_COUNT");
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::RecvEvent
    //
    // X360 ARTIST @0x82458370. Asserts the event is non-null, then -- on the show/hide-satnav
    // event (BrnGui::GuiEventShowHideSatNav, id 213: `cmpwi cr6, r26, 0xD5`) targeting the
    // MAIN map (`lwz r11,0(event); cmpwi cr6, r11, 0` == GetMapType() == E_MAPTYPE_MAIN) --
    // latches the event's show flag (`lbz r11,8(event)` == GetShow()) into the embedded
    // MapManager's mbEnabled (`stb r11,0x5F0(this)`), and finally forwards the event to
    // MapManager::RecvEvent unconditionally. The 12-byte payload's members are DWARF-attested
    // (BrnGuiDemangledEventTypes.h:751); the wJ tree constructs this exact type by name at
    // BrnPreRaceFlyBy_wJ_04.cpp:287.
    //
    // ⚠️ MapManager::RecvEvent @0x8244F898 is this TU's ONE unresolved external (bodied only
    // in the unmounted BrnMapManager.cpp; an inert gate covers it in BrnMainMapLinkGates.cpp).
    // It is left as a REAL call deliberately -- the mount shape is the conductor's call.
    // -------------------------------------------------------------------------
    void MainMapComponent::RecvEvent(const CgsModule::Event* lpEvent, int32_t liEventType)
    {
        CGS_ASSERT(lpEvent != NULL, " invalid event passed ");

        const GuiEventShowHideSatNav* lpSatNav =
            reinterpret_cast<const GuiEventShowHideSatNav*>(lpEvent);
        if (liEventType == lpSatNav->GetEventType() &&
            lpSatNav->GetMapType() == GuiEventShowHideSatNav::E_MAPTYPE_MAIN)
        {
            mMapManager.mbEnabled = lpSatNav->GetShow();
        }

        mMapManager.RecvEvent(lpEvent, liEventType);
    }
}
