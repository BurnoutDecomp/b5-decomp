// BrnSatNavIcon.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The out-of-line BrnGui::CrashNavMapIcon
// members the X360 emits as real functions:
//   operator=                 @0x8244BA18 -- copy-assign (memberwise; embedded TextField)
//   Construct`adjustor{144}'  @0x827DD610 -- base-adjusting thunk forwarding to Construct
//   Construct                 @0x824481A8 -- the ELEMENT ctor (CrashNavIconComponent::Construct)
//
// The icon's inline Set*/Get* mutators stay in the header (the X360 emits them inline at
// the call sites).

#include "GameSource/Gui/SatNav/BrnSatNavIcon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // [H3c] CGS_ASSERT (the Construct/Prepare asserts)
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"            // [H3c] BrnFlapt::FileRef (Prepare)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // [F3] CgsCore::SPrintf (the Update commit)

#include <limits>    // [H3c] FLT_MAX poison (Construct @0x82448340 stores 3.4028235e38)

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

// @ 0x827DD610  (`CrashNavMapIcon::Construct'adjustor{144}'`)
//   addi r3, r3, -0x90 ; b CrashNavMapIcon::Construct
//   The MSVC base-adjusting thunk that occupies the ICON subobject's vtable Construct
//   slot: walk `this` from the icon back to the hosting element (0x90 == 144 == the
//   icon's offset inside CrashNavIconComponent) and tail-call the real element ctor
//   @0x824481A8. The console's `b` is a DIRECT branch, so the forward below is
//   explicitly qualified -- an unqualified virtual call would re-enter this thunk.
void CrashNavMapIcon::Construct(const char* lpcName, CgsGui::StateInterface* lpStateInterface,
                                const char* lpcParentName)
{
    CrashNavIconComponent* lpComponent = reinterpret_cast<CrashNavIconComponent*>(
        reinterpret_cast<char*>(this) - offsetof(CrashNavIconComponent, mIcon));
    lpComponent->CrashNavIconComponent::Construct(lpcName, lpStateInterface, lpcParentName);
}

// @ 0x824481A8 [F3 2026-08-29 LANDED] -- the crash-nav icon ELEMENT constructor. Every
// offset in the X360 body is element-relative, so `this` is the CrashNavIconComponent:
//
//   asserts        name / state interface non-null           (:146 / :147 on the console)
//   0x824482B8     CgsGui::GuiComponent::Construct(this, name, iface, parentName)
//   0x824482DC     (**(this+0xC4))(this+0xC4, "Icon", iface, this+4)
//                    -- the embedded TextField's OWN virtual Construct (slot 0), parented
//                       to the element's composed name (this+4 == macName, which the base
//                       Construct has just filled in).
//   0x824482E4     stw   0,        0xB8   -> icon meState             = 0 (INVISIBLE)
//   0x824482F4     stfs  flt_82001CC0,  0xB0   -> icon mfRotationInRadians = 0.0f
//   0x824482F8/FC  stb   1,        0x1EC/0x1ED -> icon mbIsDirty / mbDirtyIconState
//   0x82448304     stfs  flt_820037C8,  0xB4   -> icon mfAlpha             = -1.0f
//
// Both float constants were read out of the image (0x82001CC0 == 0x00000000 == 0.0f,
// 0x820037C8 == 0xBF800000 == -1.0f), not inferred from the pseudocode.
//
// The -1.0f alpha is the SAME poison Update tests against (`if (mfAlpha > -1.0f)`): a
// freshly constructed icon publishes no _alpha until something sets a real one. Store
// order is the console's, not "tidied".
void CrashNavIconComponent::Construct(const char* lpcName,
                                      CgsGui::StateInterface* lpStateInterface,
                                      const char* lpcParentName)
{
    CGS_ASSERT(lpcName != 0,
               "Invalid name passed to CrashNavMapIcon::Construct()");            // :146 (non-gating)
    CGS_ASSERT(lpStateInterface != 0,
               "Invalid StateInterface passed to CrashNavMapIcon::Construct()");  // :147 (non-gating)

    CgsGui::GuiComponent::Construct(lpcName, lpStateInterface, lpcParentName);

    mIcon.mIconText.Construct("Icon", lpStateInterface, GetName());

    mIcon.meState             = MapIconBrnBase::E_ICONSTATE_INVISIBLE;   // stw 0, 0xB8
    mIcon.mfRotationInRadians = 0.0f;                                    // stfs flt_82001CC0, 0xB0
    mIcon.mbIsDirty           = true;                                    // stb 1, 0x1EC
    mIcon.mbDirtyIconState    = true;                                    // stb 1, 0x1ED
    mIcon.mfAlpha             = -1.0f;                                   // stfs flt_820037C8, 0xB4
}

// ======================= H3c: the sat-nav icon-pool bind surface =======================

// @ 0x82448340 -- the pool-bind constructor (this = the icon; the X360 constructs the
// HOSTING FlaptIconComponent at `this - 32`, then poisons the icon's cached transform
// state so the first real Set* always pushes through to the clip).
void SatNavMapIcon::Construct(const char* lpcName, CgsGui::StateInterface* lpStateInterface,
                              const char* lpcParentName)
{
    CGS_ASSERT(lpcName != 0,
               "Invalid name passed to SatNavMapIcon::Construct()");            // :291 (non-gating)
    CGS_ASSERT(lpStateInterface != 0,
               "Invalid StateInterface passed to SatNavMapIcon::Construct()");  // :292 (non-gating)

    SatNavIconComponent* lpComponent = reinterpret_cast<SatNavIconComponent*>(
        reinterpret_cast<char*>(this) - offsetof(SatNavIconComponent, mIcon));
    lpComponent->FlaptIconComponent::Construct(lpcName, lpStateInterface, lpcParentName);

    mIconText.SetInvalid();                       // X360 stw 0 @+0x30/+0x34/+0x38
    mfRotationInRadians = 3.4028235e38f;          // X360 stfs FLT_MAX @+0x20 (flt_8204F664)
    mfAlpha             = -1.0f;                  // X360 stfs -1.0 @+0x24 (flt_820037C8)
    meState             = E_ICONSTATE_COUNT;      // X360 stw 53 @+0x28
}

// @ 0x82448488 -- the pool-bind Prepare (this = the ELEMENT; IDA labels it
// "SatNavMapIcon::Prepare" but the X360 call site passes icon-32). Bind the named clip,
// then reset the embedded icon through its own virtuals: the direct state=COUNT store
// forces SetState(INVISIBLE) to fire the "Invisible" label jump; SetRotation(0) and
// SetPosition(zero) push the reset transform to the freshly-bound clip.
void SatNavIconComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
{
    FlaptIconComponent::Prepare(lacName, lFile, 0);

    // X360 order: stw 53 -> icon state, then the four virtual resets.
    mIcon.MapIconBrnBase::SetState(MapIconBrnBase::E_ICONSTATE_COUNT);   // the raw store (no label jump)
    mIcon.SetRotation(0.0f);
    mIcon.SetAlpha(0.0f);
    mIcon.SetState(MapIconBrnBase::E_ICONSTATE_INVISIBLE);
    Vector2 lv2Zero;
    lv2Zero.x = 0.0f; lv2Zero.y = 0.0f; lv2Zero.z = 0.0f; lv2Zero.w = 0.0f;
    mIcon.SetPosition(lv2Zero);
}

// ================== [F3 2026-08-29] the crash-nav icon COMMIT ==================
// @ 0x8244F5D0 -- CrashNavMapIcon::Update. Store-for-store from the asm:
//
//   r11 = meState                                            (lwz 0x28(this))
//   r28 = r11                                                (the apt_state label index)
//   states 1..25 and 36..39 take the 0x8244F60C arm: label index forced to 0
//     ("Invisible") and mbDirtyIconState raised -- the crash-nav component does NOT draw
//     the player/rival colour icons (1..25) or the crash-nav drive-through icons (36..39)
//     itself; a sibling renderer owns them. Transcribed as the console has it.
//   state 0 (INVISIBLE) skips the transform block entirely (both branches fall through
//     the `beq -> 0x8244F728` / `cmpwi r28,0; beq` tests) and only the state commit runs.
//   every other state (26..35, 40..52) publishes the transform block, then clears
//     mbIsDirty (`stb r27, 0x15C`, r27 == 0).
//   finally, if mbDirtyIconState, publish "apt_state" NON-immediate and clear the flag.
//
// The four view-state writes go to the HOSTING component (`addi r29, r31, -0x90`).
// Constants: 57.29578 == flt_82054BD8 (radians -> degrees); the alpha gate compares
// against flt_820037C8 == -1.0 (the poison Construct seeds); "%3.3f" is the shared format
// pointer r30 all four SPrintf calls reuse; the buffer is 64 bytes (`li r4, 0x40`).
//
// FLAG (constant provenance): the first two apt variable names are exported as
// `unk_820550C4` / `unk_820550C8` -- unnamed .rdata, four bytes apart, taking the
// formatted position lanes icon+0x10 (x) and icon+0x14 (y) in that order, immediately
// before the named "_rotation" and "_alpha". "_x" / "_y" is the apt member vocabulary
// every sibling component in the tree already uses for exactly this pair (see
// BrnRivalTableCell.cpp:94 and BrnLicenseComponent.cpp:717), and a 4-byte gap fits
// "_x\0" / "_y\0" exactly -- but the strings themselves were NOT read out of the image
// here. Re-read them off the XEX before treating them as pinned.
void CrashNavMapIcon::Update()
{
    CrashNavIconComponent* lpComponent = reinterpret_cast<CrashNavIconComponent*>(
        reinterpret_cast<char*>(this) - offsetof(CrashNavIconComponent, mIcon));

    const s32 liState = static_cast<s32>(meState);
    s32 liLabelIndex  = liState;

    const bool lbForcedInvisibleLabel = ((liState <= 25 && liState != 0)
                                         || (liState >= 36 && liState <= 39));

    if (lbForcedInvisibleLabel)
    {
        liLabelIndex      = 0;                 // 0x8244F60C: mr r28, r27 (r27 == 0)
        mbDirtyIconState  = true;              // 0x8244F61C: stb 1, 0x15D(this)
    }
    else if (liLabelIndex != 0)
    {
        char lacValue[64];

        CgsCore::SPrintf(lacValue, 64, "%3.3f", mv2Position.x);          // icon+0x10
        lpComponent->AddOutputAptViewState("_x", lacValue, true);

        CgsCore::SPrintf(lacValue, 64, "%3.3f", mv2Position.y);          // icon+0x14
        lpComponent->AddOutputAptViewState("_y", lacValue, true);

        CgsCore::SPrintf(lacValue, 64, "%3.3f",
                         mfRotationInRadians * 57.29578f);               // icon+0x20
        lpComponent->AddOutputAptViewState("_rotation", lacValue, true);

        if (mfAlpha > -1.0f)                                             // icon+0x24
        {
            CgsCore::SPrintf(lacValue, 64, "%3.3f", mfAlpha);
            lpComponent->AddOutputAptViewState("_alpha", lacValue, true);
        }

        mbIsDirty = false;                     // 0x8244F724: stb r27, 0x15C(this)
    }

    if (mbDirtyIconState)
    {
        lpComponent->AddOutputAptViewState("apt_state", gaSatNavStateLabels[liLabelIndex],
                                           false);   // li r6, 0 -- NOT immediate
        mbDirtyIconState = false;
    }
}

} // namespace BrnGui
