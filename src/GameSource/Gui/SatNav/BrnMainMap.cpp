#include "GameSource/Gui/SatNav/BrnMainMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // the one-shot gate log
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"       // CgsGui::GuiAccessPointers::GetGuiCache
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::GetAccessPointers / OutputViewState / OutputGuiEvent
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"          // BrnGui::MapTransform (device space / world rect / rect-to-rect Transform)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"      // BrnGui::GuiEventShowHideSatNav (the 213 payload)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"            // BrnGui::GuiAudioTriggerEvent (the 201 payload)

#include "rw/math/vpu/vector4_operation.h"          // rw::math::vpu::Magnitude (4-lane, the world-rect asserts)
#include "rw/math/vpu/vector2_operation.h"          // rw::math::vpu::Magnitude (2-lane, the centre asserts/distances)

#include <cmath>                                    // fabsf (ApplyZoom's fabs on the zoom delta)

// BrnGui::MainMapComponent - the sat-nav main-map screen component bodies.
//
// This TU reconstructs the WHOLE ledger set: Construct @0x8245E228, Prepare @0x8244F4A8,
// SetStandardDefZoomParams @0x82447ED8, RecvEvent @0x82458370, and — main-map slice
// 2026-08-27 — Update @0x824696E8, SnapToLocation @0x8245EBA0, SetZoom @0x82469A38,
// ApplyZoom @0x8245EE78, CalculateViewPaddingOffset @0x82447D38,
// CalculatePositionedWorldRect @0x8245E5F0 and CalculateOffsetWorldCentre (inlined
// everywhere on console; DWARF-attested .cpp body, see its banner). It also defines the
// two class-static zoom-scale tables the DWARF places inside the class (h:218 / h:219).
// The old "still held back" blockers are all retired: sub_8245A080 turned out to BE
// MapTransform::Transform(Vector2, Vector4, Vector4) (now bodied in BrnMapUtils.cpp),
// OutputGuiEvent<GuiAudioTriggerEvent> / OutputViewState<GuiEventRenderMainMap> are both
// header-inline templates with committed explicit-instantiation TUs.
//
// ⭐ VMX DECODE CONVENTION FOR THIS TU (pinned three independent ways — the reciprocal
// and rsqrt Newton-Raphson idioms and the rect-centre folds all agree): the export's
// plain `vmaddfp vD, vA, vB, vC` lines print the RAW FIELD ORDER, i.e. vD = vA*vC + vB
// (printed op2 * op4 + op3), and `vnmsubfp vD, vA, vB, vC` likewise = vB - vA*vC. The
// *128 forms print the repeated destination as their extra operand. Every
// `vrefp`/`vrsqrtefp` + two-refinement-step block below is folded to the exact divide /
// sqrt it computes, per the BrnMapUtils.cpp precedent.
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
// ⭐ MOUNT STATE (main-map slice 2026-08-27): this TU IS MOUNTED (build_game_exe.bat,
// SatNav block, alongside BrnMainMapLinkGates.cpp), and BrnMapManager.cpp is mounted too
// ([map arm 2026-08-27] bat note), so the MapManager methods the new bodies call —
// SetZoomLevel @0x8244F768, CalculateCurrentTileSet @0x8244FA80,
// RefreshActiveTextureArray @0x82448540 — resolve to their REAL bodies. The two
// MainMapComponent gates that covered Update and SetZoom in BrnMainMapLinkGates.cpp are
// DELETED in this same slice (LNK2005 otherwise). The MapManager::Construct FLAG
// boundary below predates the map-arm mount and is retirement-ready (its DELETE-WHEN
// condition — BrnMapManager.h declaring Construct and BrnMapManager.cpp bodying it — is
// now met); the map-arm work owns that swap, so it is left intact here.

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
        // The vperm/vaddfp rect-centre halving constant in Prepare (flt_82001DA0 = 0.5f);
        // the same address feeds every half-width/half-height fmuls in
        // CalculateViewPaddingOffset and CalculatePositionedWorldRect.
        const f32 KF_RECT_CENTRE_SCALE = 0.5f;

        // ---- main-map slice constants, all read off the decrypted XEX (big-endian) ----
        // The world rect's x-max stretch: every world-rect rebuild multiplies LANE z alone
        // (the vperm lane-2 insert through the rw::math::vpu permute table entry
        // unk_8327F140 + 0xA0 == "insert source word 2 into lane 2").
        // flt_82F25AD4 = 0x3FE38E39 = 1.7777778f (16:9).
        const f32 KF_WORLD_ASPECT_RATIO = 1.7777778f;
        // ApplyZoom's per-frame zoom-scale step AND its settle threshold (the same f12 is
        // both the fabs compare operand and the fmadds multiplier). flt_82F25C6C = 100.0f.
        const f32 KF_ZOOM_STEP = 100.0f;
        // ApplyZoom's step direction pair (fsel-free two-compare pick): flt_82001C98 = 1.0f
        // when the delta is positive, flt_820037C8 = -1.0f otherwise (compare vs
        // flt_82001CC0 = 0.0f).
        const f32 KF_ZOOM_STEP_UP   = 1.0f;
        const f32 KF_ZOOM_STEP_DOWN = -1.0f;
        // ApplyZoom's centre-ease divisors (each a vrefp + two-Newton-Raphson exact
        // reciprocal, folded to the divide per the BrnMapUtils.cpp precedent): while the
        // zoom scale still steps, the centre moves a QUARTER of the remaining distance per
        // frame (flt_82004EF4 = 4.0f); once the scale has settled it moves the full
        // distance (flt_82001C98 = 1.0f -- yes, the console really divides by 1).
        const f32 KF_CENTRE_EASE_DIVISOR_ZOOMING = 4.0f;
        const f32 KF_CENTRE_EASE_DIVISOR_SETTLED = 1.0f;
        // ApplyZoom's "the view centre has arrived" distance, in world units
        // (flt_82F25C68 = 1.0f).
        const f32 KF_CENTRE_SETTLE_DISTANCE = 1.0f;
        // GuiAudioTriggerEvent::Construct's action argument at both SetZoom sites (r4 = 7,
        // same literal as every other map audio chirp -- BrnPreRaceFlyBy_wJ_01.cpp:419
        // precedent).
        const s32 KI_AUDIO_TRIGGER_ACTION = 7;

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

    // (The MainMapMapManagerBoundary FLAG boundary is RETIRED -- [map arm 2026-08-27]
    // BrnMapManager.h declares `void Construct(CgsGui::StateInterface*)`, BrnMapManager.cpp
    // bodies @0x82458590, and the TU is MOUNTED, so Construct below calls
    // `mMapManager.Construct(lpStateInterface)` directly, exactly the console's
    // `bl BrnGui__MapManager__Construct` @0x8245E3AC on `this + 0x8C`. ⚠️ The old
    // boundary's "recovered semantics" note misattributed two stores -- the asm-pinned
    // truth in the real body is: the {0,0,1,1} block lands at +0x24 (mLowResTexture.mBB,
    // `addi r7, r31, 0x24`), the two normalised->world corner transforms land at +0x34
    // (mLowResTexture.mBBWorld, `addi r10, r31, 0x34`), and there is NO +0x560
    // muTextureCount store.)

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

        // The embedded MapManager sub-construct (X360 `bl BrnGui__MapManager__Construct`
        // @0x8245E3AC on `this + 0x8C`) -- direct now the real body is mounted [map arm].
        mMapManager.Construct(lpStateInterface);

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

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::Update
    //
    // X360 ARTIST @0x824696E8 (json name field verified; original body BrnMainMap.cpp:165
    // per the DWARF). The per-frame animated map view:
    //   1. advance the zoom/centre animation (ApplyZoom with the 16:9 lane-z stretch),
    //   2. re-position mv4WorldRect around the padding-offset centre
    //      (CalculateOffsetWorldCentre IN -> CalculatePositionedWorldRect),
    //   3. publish the world-space padding rect into the embedded MapManager's world rect
    //      (`stfs` x4 into this+0x8C+0x00..0x0C == MapManager::mWorldRect),
    //   4. when the map is enabled, rebuild the tile working set and emit the
    //      GuiEventRenderMainMap view-state (channel 41) through
    //      OutputViewState<GuiEventRenderMainMap> @0x82465E50,
    //   5. install the zoomed map spaces (SetZoomedWorldRect on three corners of
    //      mv4WorldRect + SetZoomedViewportRect on mv4ViewRect),
    //   6. return the re-positioned centre taken back OUT of the padding offset.
    //
    // The mbEnabled byte (this+0x5F0 == MapManager+0x564) is tested TWICE -- two separate
    // `lbz`/`beq` arms in the asm, not one guarded block -- so two ifs are transcribed.
    // The three SetZoomedWorldRect corners are assembled by the unk_82CDA350 perm mask as
    // {x,y}, {z,y}, {x,w} of mv4WorldRect: origin, x-adjacent, y-adjacent -- the exact
    // (A, B, C) order SetZoomedWorldRect's banner pins.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::Update(Vector2 lv2WorldCentre)
    {
        // cpp:167 -- static FireAssert, the console's own text.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        // f1 = flt_82F25AD4 (1.7777778f): the world-rect x-max stretch rides in as
        // ApplyZoom's float argument.
        const Vector2 lv2AnimatedCentre = ApplyZoom(lv2WorldCentre, KF_WORLD_ASPECT_RATIO);

        const Vector2 lv2OffsetCentre =
            CalculateOffsetWorldCentre(lv2AnimatedCentre, E_OFFSETPADDING_IN);
        const Vector2 lv2PositionedCentre = CalculatePositionedWorldRect(lv2OffsetCentre);

        // The padding rect's two corners, taken from normalised space into the CURRENT
        // (freshly positioned) world window, become the MapManager's world rect
        // (X360 `stfs` x4 at 0x82469874..0x82469880 into MapManager+0x00..0x0C).
        const Vector2 lv2PaddingMin = { mv4PaddingRect.x, mv4PaddingRect.y, 0.0f, 0.0f };
        const Vector2 lv2PaddingMax = { mv4PaddingRect.z, mv4PaddingRect.w, 0.0f, 0.0f };
        const Vector2 lv2WorldPadMin =
            MapTransform::Transform(lv2PaddingMin, MapTransform::GetNormalisedRect(), mv4WorldRect);
        const Vector2 lv2WorldPadMax =
            MapTransform::Transform(lv2PaddingMax, MapTransform::GetNormalisedRect(), mv4WorldRect);
        mMapManager.mWorldRect.mfLeft   = lv2WorldPadMin.x;
        mMapManager.mWorldRect.mfTop    = lv2WorldPadMin.y;
        mMapManager.mWorldRect.mfRight  = lv2WorldPadMax.x;
        mMapManager.mWorldRect.mfBottom = lv2WorldPadMax.y;

        // First mbEnabled arm: refresh the tile working set.
        if (mMapManager.mbEnabled)
        {
            mMapManager.CalculateCurrentTileSet();
            mMapManager.RefreshActiveTextureArray();
        }

        // Second mbEnabled arm: emit the per-frame render event. The console fills the
        // payload member-by-member on the stack -- no GuiEventRenderMainMap::Construct
        // call exists in the body.
        if (mMapManager.mbEnabled)
        {
            GuiEventRenderMainMap lRenderEvent;
            lRenderEvent.mv4MapRect       = mv4WorldRect;                 // stack var_A0
            lRenderEvent.mv4ViewRect      = mv4ViewRect;                  // stack var_90
            lRenderEvent.mpActiveTextures = &mMapManager.mActiveTextures; // this+0x340 == MapManager+0x2B4
            lRenderEvent.mfZoomLevel      = mfWorldZoomScaleFactor;       // this+0x660
            lRenderEvent.meMapType        = meMapType;                    // this+0x66C
            lRenderEvent.mbIsActive       = mbIsActive;                   // this+0x67C

            // cpp:203 -- a STREAMED assert on the console (StrStreamBase << into
            // gpcMessageBuffer, then FireAssert on the buffer); lowered to the static
            // sequence with the recovered literal per the project convention.
            CGS_ASSERT(mpStateInterface != 0, "State interface is invalid");

            mpStateInterface->OutputViewState(lRenderEvent);
        }

        // The zoomed map spaces: three corners of the positioned world rect (origin,
        // x-adjacent, y-adjacent) + the on-screen view rect.
        const Vector2 lv2CornerOrigin = { mv4WorldRect.x, mv4WorldRect.y, 0.0f, 0.0f };
        const Vector2 lv2CornerX      = { mv4WorldRect.z, mv4WorldRect.y, 0.0f, 0.0f };
        const Vector2 lv2CornerY      = { mv4WorldRect.x, mv4WorldRect.w, 0.0f, 0.0f };
        MapTransform::SetZoomedWorldRect(lv2CornerOrigin, lv2CornerX, lv2CornerY);
        MapTransform::SetZoomedViewportRect(mv4ViewRect);

        return CalculateOffsetWorldCentre(lv2PositionedCentre, E_OFFSETPADDING_OUT);
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::CalculateOffsetWorldCentre
    //
    // Original body BrnMainMap.cpp:273 (DWARF); the console INLINES it at every call site
    // -- there is no out-of-line X360 address -- but each expansion is byte-recognisable:
    // CalculateViewPaddingOffset, then the rect-to-rect Transform @0x8245A080 world ->
    // normalised, a full-quad vaddfp128/vsubfp128 of the offset, and the Transform back
    // normalised -> world (Update @0x82469780/@0x82469790 + @0x82469A08/@0x82469A18,
    // ApplyZoom @0x8245F1D4..@0x8245F228, SnapToLocation @0x8245EE08..@0x8245EE5C).
    //
    // [FLAG enum-name mapping] Which OffsetPadding enumerator selects + and which - is NOT
    // recoverable: the method is inlined everywhere, so the enumerator constants are erased
    // from the binary. The two directions themselves ARE attested (every site applies the
    // ADD form to the centre it feeds INTO CalculatePositionedWorldRect and the SUBTRACT
    // form to the centre it hands back OUT), so E_OFFSETPADDING_IN is mapped to + and
    // E_OFFSETPADDING_OUT to -; behaviour is invariant to the naming as long as all
    // call sites (all in this TU) agree.
    //
    // The console's add/sub is a full-quad vector op; both operands carry 0 in z/w
    // (Transform and CalculateViewPaddingOffset zero them), so the x/y member form below
    // is lane-exact.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::CalculateOffsetWorldCentre(Vector2 lv2Centre, OffsetPadding lePadding)
    {
        const Vector2 lv2PaddingOffset = CalculateViewPaddingOffset();

        Vector2 lv2Normalised =
            MapTransform::Transform(lv2Centre, mv4WorldRect, MapTransform::GetNormalisedRect());

        if (lePadding == E_OFFSETPADDING_IN)
        {
            lv2Normalised.x += lv2PaddingOffset.x;
            lv2Normalised.y += lv2PaddingOffset.y;
        }
        else
        {
            lv2Normalised.x -= lv2PaddingOffset.x;
            lv2Normalised.y -= lv2PaddingOffset.y;
        }

        return MapTransform::Transform(lv2Normalised, MapTransform::GetNormalisedRect(), mv4WorldRect);
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::CalculateViewPaddingOffset
    //
    // X360 ARTIST @0x82447D38 (json name field verified; original body BrnMainMap.cpp:301).
    // The offset between the VIEW rect's centre and the PADDING rect's centre, z/w zeroed
    // (`std r11, 0(r7)` on the return quad's high half). Each centre is the console's
    // min + 0.5*(max-min) fold: the lvsl-built lane selectors split the rect into
    // splat(min)/splat(max), vsubfp forms the extent, and the vmaddfp (raw field order,
    // see the TU banner) folds 0.5*extent + min. flt_82001DA0 = 0.5f both times.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::CalculateViewPaddingOffset()
    {
        // this+0x620 = mv4ViewRect, this+0x630 = mv4PaddingRect.
        const f32 lfViewCentreX =
            (mv4ViewRect.z - mv4ViewRect.x) * KF_RECT_CENTRE_SCALE + mv4ViewRect.x;
        const f32 lfViewCentreY =
            (mv4ViewRect.w - mv4ViewRect.y) * KF_RECT_CENTRE_SCALE + mv4ViewRect.y;
        const f32 lfPaddingCentreX =
            (mv4PaddingRect.z - mv4PaddingRect.x) * KF_RECT_CENTRE_SCALE + mv4PaddingRect.x;
        const f32 lfPaddingCentreY =
            (mv4PaddingRect.w - mv4PaddingRect.y) * KF_RECT_CENTRE_SCALE + mv4PaddingRect.y;

        Vector2 lv2Offset;
        lv2Offset.x = lfViewCentreX - lfPaddingCentreX;   // fsubs @0x82447EA4
        lv2Offset.y = lfViewCentreY - lfPaddingCentreY;   // fsubs @0x82447EB8
        lv2Offset.z = 0.0f;                               // std r11(0) @0x82447EC0
        lv2Offset.w = 0.0f;
        return lv2Offset;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::CalculatePositionedWorldRect
    //
    // X360 ARTIST @0x8245E5F0 (json name field verified; original body BrnMainMap.cpp:326).
    // Re-positions mv4WorldRect (in place -- this is the member-writing heart of the view
    // pipeline) so its centre lands on lv2Centre, edge-clamped against the FIXED world
    // rect (v127 = flt_82FB31F0 == MapTransform::smv4WorldRect) wherever the stick flags
    // pin the map to a screen edge, then returns the positioned rect's actual centre.
    //
    // The clamp floor on the left/up edges is offset by the view-padding offset taken from
    // normalised space into the CURRENT world window as a POINT transform (the
    // @0x8245A080 rect-to-rect Transform on the raw CalculateViewPaddingOffset result --
    // translation included, exactly as the console does it). The right/down ceilings are
    // the plain `fixed max - extent`. Each clamp is an fsel, transcribed operand-exact:
    //   left  @0x8245E76C: fsel(rect.x - floor,   rect.x, floor)   == max
    //   right @0x8245E7B4: fsel(rect.x - ceiling, ceiling, rect.x) == min
    //   up    @0x8245E92C: fsel(rect.y - floor,   rect.y, floor)   == max
    //   down  @0x8245E978: fsel(rect.y - ceiling, ceiling, rect.y) == min
    // The stick flags are read as +0x67A (left) / +0x67B (right) / +0x678 (up) /
    // +0x679 (down) -- the header's attested flag order. Lane-insert stores go through the
    // rw::math::vpu permute table (unk_8327F140 + 0x00/0x40/0x80/0xC0 = lanes x/y/z/w),
    // lowered to the named member writes.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::CalculatePositionedWorldRect(Vector2 lv2Centre)
    {
        const Vector4& lv4FixedRect = MapTransform::GetWorldRect();   // flt_82FB31F0

        // The world-space padding offset (see the banner: a point transform, on purpose).
        const Vector2 lv2WorldPadding = MapTransform::Transform(
            CalculateViewPaddingOffset(), MapTransform::GetNormalisedRect(), mv4WorldRect);

        // ---- x half ------------------------------------------------------------------
        const f32 lfWidth = mv4WorldRect.z - mv4WorldRect.x;

        mv4WorldRect.x = lv2Centre.x - lfWidth * KF_RECT_CENTRE_SCALE;   // lane-0 insert @0x8245E73C

        if (mbStickMapLeft)    // lbz +0x67A
        {
            const f32 lfFloor = lv4FixedRect.x + lv2WorldPadding.x;
            mv4WorldRect.x =
                (mv4WorldRect.x - lfFloor >= 0.0f) ? mv4WorldRect.x : lfFloor;   // fsel @0x8245E76C
        }
        if (mbStickMapRight)   // lbz +0x67B
        {
            const f32 lfCeiling = lv4FixedRect.z - lfWidth;
            mv4WorldRect.x =
                (mv4WorldRect.x - lfCeiling >= 0.0f) ? lfCeiling : mv4WorldRect.x;   // fsel @0x8245E7B4
        }

        mv4WorldRect.z = mv4WorldRect.x + lfWidth;   // lane-2 insert @0x8245E854

        // ---- y half ------------------------------------------------------------------
        const f32 lfHeight = mv4WorldRect.w - mv4WorldRect.y;

        mv4WorldRect.y = lv2Centre.y - lfHeight * KF_RECT_CENTRE_SCALE;   // lane-1 insert @0x8245E8FC

        if (mbStickMapUp)      // lbz +0x678
        {
            const f32 lfFloor = lv4FixedRect.y + lv2WorldPadding.y;
            mv4WorldRect.y =
                (mv4WorldRect.y - lfFloor >= 0.0f) ? mv4WorldRect.y : lfFloor;   // fsel @0x8245E930
        }
        if (mbStickMapDown)    // lbz +0x679
        {
            const f32 lfCeiling = lv4FixedRect.w - lfHeight;
            mv4WorldRect.y =
                (mv4WorldRect.y - lfCeiling >= 0.0f) ? lfCeiling : mv4WorldRect.y;   // fsel @0x8245E980
        }

        mv4WorldRect.w = mv4WorldRect.y + lfHeight;   // lane-3 insert @0x8245EA20

        // cpp:376 -- static FireAssert, the console's own text.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        // The positioned rect's centre, z/w zeroed (`std r31, 0(r11)` @0x8245EB70).
        Vector2 lv2PositionedCentre;
        lv2PositionedCentre.x = mv4WorldRect.x + lfWidth * KF_RECT_CENTRE_SCALE;
        lv2PositionedCentre.y = mv4WorldRect.y + lfHeight * KF_RECT_CENTRE_SCALE;
        lv2PositionedCentre.z = 0.0f;
        lv2PositionedCentre.w = 0.0f;
        return lv2PositionedCentre;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::SnapToLocation
    //
    // X360 ARTIST @0x8245EBA0 (json name field verified; original body BrnMainMap.cpp:395).
    // Centre the map on a location with NO animation: snap the zoom scale to its desired
    // value, park the desired centre on the location (full-quad store, like
    // SetDesiredWorldCentre), rebuild the world rect at that scale, and run the
    // padding/positioning pipeline once so mv4WorldRect lands where Update would have
    // eased it. ⭐ THE TWO CalculateOffsetWorldCentre RESULTS ARE DISCARDED ON THE CONSOLE
    // TOO -- v1 is overwritten right after each inlined expansion (@0x8245EE1C the
    // positioned-rect call reloads the RAW location, and the final expansion's result dies
    // at the blr). The calls are kept because that is what the original source did (the
    // opaque Transform calls kept the compiler from deleting them); only
    // CalculatePositionedWorldRect's mv4WorldRect side effect matters here.
    // -------------------------------------------------------------------------
    void MainMapComponent::SnapToLocation(Vector2 lv2Location)
    {
        // cpp:397 -- static FireAssert, the console's own text.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        mfWorldZoomScaleFactor = mfDesiredWorldZoomFactor;   // stfs @0x8245EC0C
        mv2DesiredCentre       = lv2Location;                // full-quad stvx @0x8245EC3C

        // The world rect at the desired scale (the shared rebuild idiom; the 16:9 stretch
        // is the flt_82F25AD4 constant here, not a parameter).
        mv4WorldRect.x = mv4ViewRect.x * mfDesiredWorldZoomFactor;
        mv4WorldRect.y = mv4ViewRect.y * mfDesiredWorldZoomFactor;
        mv4WorldRect.z = mv4ViewRect.z * mfDesiredWorldZoomFactor * KF_WORLD_ASPECT_RATIO;
        mv4WorldRect.w = mv4ViewRect.w * mfDesiredWorldZoomFactor;

        // cpp:405 / cpp:406 -- static FireAsserts, the console's own strings. The first is
        // the TWO-lane magnitude (the console sums lanes x/y only -- see the
        // vector2_operation.h grow note), the second the full-4.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv2DesiredCentre) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv2DesiredCentre) < 1000000.0f");
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        CalculateOffsetWorldCentre(lv2Location, E_OFFSETPADDING_IN);    // result discarded (console too)
        CalculatePositionedWorldRect(lv2Location);                      // side effect: mv4WorldRect
        CalculateOffsetWorldCentre(lv2Location, E_OFFSETPADDING_OUT);   // result discarded (console too)

        mbIsZooming = false;   // stb @0x8245EE60
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::SetZoom
    //
    // X360 ARTIST @0x82469A38 (json name field verified; original body BrnMainMap.cpp:429).
    // Select a zoom factor. On E_ZOOMFACTOR_CUSTOM the caller's scale overwrites the
    // table's CUSTOM slot, with an audio chirp keyed on which way the zoom moved relative
    // to the slot's previous value ("CodeMapZoomIn" when the new zoom is tighter,
    // "CodeMapZoomOut" when wider; equal = silent). With lbApplyNow the world rect is
    // rebuilt IMMEDIATELY -- ⭐ from the OLD mfWorldZoomScaleFactor (the console loads
    // this+0x660 before the tail stores update it; the new scale is applied by the next
    // Update's ApplyZoom pass) -- and both zoom scalars snap to the new table value.
    // Without it only the desired factor is set and ApplyZoom animates there.
    // -------------------------------------------------------------------------
    void MainMapComponent::SetZoom(ZoomFactor leZoomFactor, float lfCustomZoom, bool lbApplyNow)
    {
        meCurrentZoomFactor = leZoomFactor;   // stw @0x82469A60

        if (leZoomFactor == E_ZOOMFACTOR_CUSTOM)
        {
            const f32 lfPreviousCustomZoom = mfZoomScalFactors[leZoomFactor];
            if (lfCustomZoom < lfPreviousCustomZoom)
            {
                // r4 = 7, r5 = "" (the shared empty-string sentinel unk_820046A7),
                // r6 = "CodeMapZoomIn", r7 = "" -- (action, component, label, movie).
                GuiAudioTriggerEvent lAudioEvent;
                lAudioEvent.Construct(KI_AUDIO_TRIGGER_ACTION, "", "CodeMapZoomIn", "");
                mpStateInterface->OutputGuiEvent(lAudioEvent);
            }
            else if (lfCustomZoom > lfPreviousCustomZoom)
            {
                GuiAudioTriggerEvent lAudioEvent;
                lAudioEvent.Construct(KI_AUDIO_TRIGGER_ACTION, "", "CodeMapZoomOut", "");
                mpStateInterface->OutputGuiEvent(lAudioEvent);
            }

            mfZoomScalFactors[meCurrentZoomFactor] = lfCustomZoom;   // stfsx @0x82469AC8
        }

        if (lbApplyNow)
        {
            const f32 lfNewZoom = mfZoomScalFactors[meCurrentZoomFactor];   // lfsx @0x82469AE0

            // cpp:456 -- static FireAssert, the console's own text.
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

            // The shared world-rect rebuild -- on the OLD scale (see the banner).
            mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
            mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
            mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * KF_WORLD_ASPECT_RATIO;
            mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

            // cpp:461 -- static FireAssert, the console's own text.
            CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                       "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

            mfDesiredWorldZoomFactor = lfNewZoom;   // stfs @0x82469C3C
            mfWorldZoomScaleFactor   = lfNewZoom;   // stfs @0x82469C40
        }
        else
        {
            mfDesiredWorldZoomFactor = mfZoomScalFactors[meCurrentZoomFactor];   // stfs @0x82469C54
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::ApplyZoom
    //
    // X360 ARTIST @0x8245EE78 (json name field verified; original body BrnMainMap.cpp:545).
    // The per-frame zoom/centre animation step, returning the eased world centre. lfZoom
    // is the world-rect lane-z stretch (Update passes flt_82F25AD4 == 16:9). Three phases:
    //
    //   A. SCALE STEPPING (|desired - current| > 100): the scale moves a fixed 100.0 per
    //      frame toward the desired factor (fmadds step*dir + scale), mbIsZooming latches
    //      true, and the centre eases a QUARTER of its remaining distance (the
    //      vrefp+2xN-R reciprocal of flt_82004EF4=4, vmaddcfp128 delta*(1/4) + centre).
    //      The arrived-block is skipped (r9 = 0).
    //
    //   B. SCALE SETTLED: if the centre still sits > 1.0 world units from the desired
    //      centre, the world rect is rebuilt at the current scale, the centre takes a
    //      FULL step (divisor flt_82001C98 = 1.0), and the padding/positioning pipeline
    //      runs (IN -> CalculatePositionedWorldRect -> OUT); mbIsZooming = whether the
    //      re-positioned centre still differs from the input by > 1.0 (the vcmpgtfp bit
    //      stored straight to +0x670 @0x8245F2BC), and "arrived" is its negation (the
    //      cntlzw/extrwi pair). If the centre was already within 1.0, "arrived" is
    //      immediate (r9 stays 1) and the input centre is returned unchanged.
    //
    //   C. ARRIVED: park mv2DesiredCentre on the eased centre when their x/y differ (the
    //      vrlimi128 lane-duplication + vcmpeqfp. all-lanes test reduces to that pair),
    //      snap the scale to the desired factor (re-latching mbIsZooming when it actually
    //      moved), and push the zoom level into the embedded MapManager. ⭐ The switch's
    //      jump table is FOUR IDENTICAL entries: every defined ZoomFactor lands on
    //      MapManager::SetZoomLevel(&mMapManager, 0) (r4 = 0 == E_ZOOM_MEDIUM); only the
    //      out-of-range default differs (the "Unexpected zoom factor" assert).
    //
    // The tail ALWAYS rebuilds mv4WorldRect from the (possibly stepped/snapped) current
    // scale with the lfZoom stretch, and re-checks the magnitude guard.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::ApplyZoom(Vector2 lv2WorldCentre, float lfZoom)
    {
        Vector2 lv2Centre     = lv2WorldCentre;   // v123
        bool lbCentreArrived  = true;             // r9 (seeded 1 @0x8245EEB8)

        const f32 lfZoomDelta = mfDesiredWorldZoomFactor - mfWorldZoomScaleFactor;

        if (fabsf(lfZoomDelta) > KF_ZOOM_STEP)
        {
            // ---- phase A: the scale still steps -------------------------------------
            const f32 lfDirection =
                (lfZoomDelta > 0.0f) ? KF_ZOOM_STEP_UP : KF_ZOOM_STEP_DOWN;
            // fmadds @0x8245EF24: step*direction + scale.
            mfWorldZoomScaleFactor = KF_ZOOM_STEP * lfDirection + mfWorldZoomScaleFactor;
            mbIsZooming = true;   // stb @0x8245EF34

            // vmaddcfp128 @0x8245EF94: centre + (desired - centre)/4.
            lv2Centre.x = (mv2DesiredCentre.x - lv2WorldCentre.x) / KF_CENTRE_EASE_DIVISOR_ZOOMING
                          + lv2WorldCentre.x;
            lv2Centre.y = (mv2DesiredCentre.y - lv2WorldCentre.y) / KF_CENTRE_EASE_DIVISOR_ZOOMING
                          + lv2WorldCentre.y;

            lbCentreArrived = false;   // mr r9, r31 @0x8245EF3C
        }
        else
        {
            // ---- phase B: the scale has settled; does the centre still travel? ------
            Vector2 lv2ToDesired;
            lv2ToDesired.x = mv2DesiredCentre.x - lv2WorldCentre.x;
            lv2ToDesired.y = mv2DesiredCentre.y - lv2WorldCentre.y;
            lv2ToDesired.z = 0.0f;
            lv2ToDesired.w = 0.0f;

            if (rw::math::vpu::Magnitude(lv2ToDesired) > KF_CENTRE_SETTLE_DISTANCE)
            {
                // The shared world-rect rebuild, at the current scale.
                mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
                mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
                mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * lfZoom;
                mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

                // vmaddcfp128 @0x8245F144: a FULL step (divisor 1.0 -- see the constants).
                lv2Centre.x = lv2ToDesired.x / KF_CENTRE_EASE_DIVISOR_SETTLED + lv2WorldCentre.x;
                lv2Centre.y = lv2ToDesired.y / KF_CENTRE_EASE_DIVISOR_SETTLED + lv2WorldCentre.y;

                // cpp:579 -- static FireAssert, the console's own text.
                CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                           "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

                // The padding/positioning pipeline the eased centre runs through.
                const Vector2 lv2OffsetCentre =
                    CalculateOffsetWorldCentre(lv2Centre, E_OFFSETPADDING_IN);
                const Vector2 lv2PositionedCentre = CalculatePositionedWorldRect(lv2OffsetCentre);
                const Vector2 lv2AdjustedCentre =
                    CalculateOffsetWorldCentre(lv2PositionedCentre, E_OFFSETPADDING_OUT);

                Vector2 lv2Moved;
                lv2Moved.x = lv2AdjustedCentre.x - lv2WorldCentre.x;
                lv2Moved.y = lv2AdjustedCentre.y - lv2WorldCentre.y;
                lv2Moved.z = 0.0f;
                lv2Moved.w = 0.0f;

                // stb @0x8245F2BC: the compare bit IS the member value.
                mbIsZooming = rw::math::vpu::Magnitude(lv2Moved) > KF_CENTRE_SETTLE_DISTANCE;
                lbCentreArrived = !mbIsZooming;   // cntlzw/extrwi @0x8245F2B8/@0x8245F2C0
            }
            // (else: within 1.0 already -- lbCentreArrived stays true, lv2Centre stays
            //  the input centre.)
        }

        if (lbCentreArrived)
        {
            // ---- phase C: park the animation ----------------------------------------
            // vrlimi128 x/y-duplication + vcmpeqfp. @0x8245F2E8: store only when the x/y
            // lanes differ; the store itself is the full quad.
            if (mv2DesiredCentre.x != lv2Centre.x || mv2DesiredCentre.y != lv2Centre.y)
            {
                mv2DesiredCentre = lv2Centre;   // stvx @0x8245F2FC
            }

            if (mfWorldZoomScaleFactor != mfDesiredWorldZoomFactor)
            {
                mfWorldZoomScaleFactor = mfDesiredWorldZoomFactor;   // stfs @0x8245F310
                mbIsZooming = true;                                  // stb @0x8245F314
            }
            else
            {
                mbIsZooming = false;                                 // stb @0x8245F31C
            }

            switch (meCurrentZoomFactor)
            {
                // jpt_8245F340: four identical entries -- every defined factor takes the
                // same arm, r4 = 0 == MapManager::E_ZOOM_MEDIUM.
                case E_ZOOMFACTOR_LOW:
                case E_ZOOMFACTOR_MEDIUM:
                case E_ZOOMFACTOR_HIGH:
                case E_ZOOMFACTOR_CUSTOM:
                    mMapManager.SetZoomLevel(MapManager::E_ZOOM_MEDIUM);
                    break;
                default:
                    // cpp:635 -- static FireAssert, the console's own text.
                    CGS_ASSERT(false, "Unexpected zoom factor");
                    break;
            }
        }

        // ---- the unconditional tail: the world rect at the current scale ------------
        mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
        mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
        mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * lfZoom;
        mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

        // cpp:645 -- static FireAssert, the console's own text.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        return lv2Centre;   // stvx v123 -> [r20] @0x8245F48C
    }
}
