// BrnSatNavIcon.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The out-of-line BrnGui::CrashNavMapIcon
// members the X360 emits as real functions:
//   operator=                 @0x8244BA18 -- copy-assign (memberwise; embedded TextField)
//   Construct`adjustor{144}'  @0x827DD610 -- base-adjusting thunk forwarding to Construct
//
// The icon's inline Set*/Get* mutators stay in the header (the X360 emits them inline at
// the call sites). Construct itself (the icon's full constructor) is a separate, not-yet-
// reconstructed TU; the adjustor here only adjusts `this` and forwards to it.

#include "GameSource/Gui/SatNav/BrnSatNavIcon.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [H3b] the Construct park print

namespace BrnGui
{

// @ 0x82F25A00 -- the per-IconState Flapt animation-label table SatNavMapIcon::SetState
// indexes (53 entries, E_ICONSTATE_COUNT). Strings read off the X360 image (H3b
// h3b_dump.txt); repeated pointers in the image (e.g. the five "Invisible" rows) are
// faithful -- those states share a label.
const char* const gaSatNavStateLabels[MapIconBrnBase::E_ICONSTATE_COUNT] =
{
    "Invisible",                        //  0 E_ICONSTATE_INVISIBLE
    "Offline_Player_Yellow",            //  1 E_ICONSTATE_PLAYER_OFFLINE
    "Player_Yellow",                    //  2 E_ICONSTATE_PLAYER_ONLINE
    "Player_Yellow",                    //  3 E_ICONSTATE_PLAYER_YELLOW
    "Player_Red",                       //  4
    "Player_Blue",                      //  5
    "Player_Pink",                      //  6
    "Player_Green",                     //  7
    "Player_Orange",                    //  8
    "Player_Purple",                    //  9
    "Player_Cyan",                      // 10
    "Player_White",                     // 11
    "Player_Gray",                      // 12
    "Player_Black",                     // 13
    "Offline_Rival_Red",                // 14 E_ICONSTATE_RIVAL
    "Rival_Yellow",                     // 15
    "Rival_Red",                        // 16
    "Rival_Blue",                       // 17
    "Rival_Pink",                       // 18
    "Rival_Green",                      // 19
    "Rival_Orange",                     // 20
    "Rival_Purple",                     // 21
    "Rival_Cyan",                       // 22
    "Rival_White",                      // 23
    "Rival_Gray",                       // 24
    "Rival_Black",                      // 25
    "SatNav_Landmark",                  // 26
    "SatNav_LandmarkBeaten",            // 27
    "SatNav_TrackedLandmark",           // 28
    "SatNav_TrackedLandmarkBeaten",     // 29
    "SatNav_FinSmallLandmark",          // 30
    "SatNav_PendingFinSmallLandmark",   // 31
    "SatNav_StartLandmark",             // 32
    "SatNav_PendingLandmark",           // 33
    "CrashNav_landmarkFin",             // 34
    "CrashNav_landmarkStart",           // 35
    "Invisible",                        // 36 E_ICONSTATE_CRASHNAV_JUNKYARD
    "Invisible",                        // 37 E_ICONSTATE_CRASHNAV_BODYSHOP
    "Invisible",                        // 38 E_ICONSTATE_CRASHNAV_GAS_STATION
    "Invisible",                        // 39 E_ICONSTATE_CRASHNAV_PAINT_SHOP
    "CrashNav_landmarkFin",             // 40 E_ICONSTATE_SATNAV_FREEBURN_CHALLENGE
    "SatNav_JunkyardSmall",             // 41
    "SatNav_AutoPartSmall",             // 42 E_ICONSTATE_SATNAV_CAR_PARK
    "SatNav_BodyShopSmall",             // 43
    "SatNav_GasStationSmall",           // 44
    "SatNav_PaintShopSmall",            // 45
    "CrashNav_StartPoint",              // 46
    "CrashNav_FinishPoint",             // 47
    "CrashNav_Checkpoint",              // 48
    "PreRace_StartPoint",               // 49
    "PreRace_FinishPoint",              // 50
    "invisible",                        // 51 E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_START_POINT
    "invisible",                        // 52 E_ICONSTATE_CRASHNAV_CUSTOMRENDERED_FINISH_POINT
};

// @ 0x8244BA18
//   The X360 byte-copies the icon starting at this+0x04 (the +0x00 vtable slot is NOT
//   copied), copies the embedded TextField via TextField::operator=, then the two
//   trailing dirty flags @+0x1EC/+0x1ED. Reproduced as a member-by-name copy of the
//   modelled fields:
//     - the MapIconBrnBase data lane (position / rotation / alpha / state) [+0x10..+0x2B]
//     - muId                                                                [+0x2C]
//     - mIconText (TextField::operator=)                                    [+0xC4]
//     - mbIsDirty / mbDirtyIconState                                        [+0x1EC/+0x1ED]
//   The vtable pointer is left untouched (matching the X360 byte-copy anchor at +0x04 and
//   C++ copy-assign of a polymorphic object).
CrashNavMapIcon& CrashNavMapIcon::operator=(const CrashNavMapIcon& lrSource)
{
    // ---- MapIconBrnBase data members (protected; copied field-by-field, NOT the vtable) ----
    mv2Position         = lrSource.mv2Position;
    mfRotationInRadians = lrSource.mfRotationInRadians;
    mfAlpha             = lrSource.mfAlpha;
    meState             = lrSource.meState;

    // ---- CrashNavMapIcon own fields ----
    muId      = lrSource.muId;
    mIconText = lrSource.mIconText;          // TextField::operator= (X360 @0x824470F0)
    mbIsDirty        = lrSource.mbIsDirty;
    mbDirtyIconState = lrSource.mbDirtyIconState;

    return *this;
}

// @ 0x827DD610  (Construct`adjustor{144}')
//   addi r3, r3, -0x90 ; b CrashNavMapIcon::Construct
//   A compiler-emitted base-adjusting thunk: it shifts `this` back by 0x90 (the offset of
//   the secondary base whose vtable carries this Construct slot) to the full
//   CrashNavMapIcon object, then tail-calls the real constructor. Construct is a separate
//   not-yet-reconstructed TU (declared-only); the adjustor faithfully reproduces the
//   `this`-adjust + forward (the X360 `addi r3,r3,-0x90; b Construct`).
CrashNavMapIcon* CrashNavMapIcon::ConstructAdjustor144(s32 liA, s32 liB, s32 liC)
{
    // The X360's only behaviour here is the `this -= 0x90` base adjust + tail-call into
    // the full constructor. On the host build the full object is reached directly, so the
    // adjust collapses to a plain forward to Construct (the out-of-scope real ctor TU).
    return Construct(liA, liB, liC);
}


// @ 0x824481A8 -- the crash-nav icon component's full constructor (name/iface/parent
// per the X360 asm; the committed adjustor forwards here). [H3b NAMED GATE]: the
// component half (apt binding, embedded TextField construction) belongs to the parked
// crash-nav icon-pool slice; its only callers are that pool's SetOwnerParameters pass
// (itself gated) and the adjustor. One-shot-logged, not a silent stub.
CrashNavMapIcon* CrashNavMapIcon::Construct(s32 liA, s32 liB, s32 liC)
{
    (void)liA; (void)liB; (void)liC;
    static bool sbLogged = false;
    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
    {
        sbLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] PARK: CrashNavMapIcon::Construct @0x824481A8 (the crash-nav "
               "icon-pool component ctor) is unreconstructed\n";
    }
    return this;
}

} // namespace BrnGui
