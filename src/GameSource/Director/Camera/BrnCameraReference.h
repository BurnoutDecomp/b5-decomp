#pragma once

#include "types.hpp"
#include "GameSource/Director/Camera/Camera.h"                // BrnDirector::Camera::Camera (by value)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"   // BehaviourHelperIndex / BehaviourManager / BehaviourControllerLockInterface

namespace BrnDirector { class ICEWrapper; }                   // GameSource/Director/BrnDirectorICEWrapper.h (pointer-only here)

// BrnDirector::Camera::CameraReference - a polymorphic "where does this camera come
// from" record (a cached camera, a live behaviour's camera, or an ICE movie
// wrapper's camera) the interpolation behaviour reads through. Class shape /
// member names / method set verbatim from the DecFIGS DWARF
// (BrnCameraReference.h:45/:85-:102); gated on the X360 ledger. This TU bodies
// Setup(ICEWrapper)/GetCamera/Prepare/Release; Construct, the other two Setup
// overloads and Unlock are their own ledger functions (declaration-only,
// not X360-exported).
namespace BrnDirector
{
namespace Camera
{

struct CameraReference
{
    // DWARF BrnCameraReference.h:90.
    enum EType
    {
        E_TYPE_INVALID   = 0,
        E_TYPE_CACHED    = 1,
        E_TYPE_BEHAVIOUR = 2,
        E_TYPE_ICE       = 3,
        E_TYPE_COUNT     = 4,
    };

    // DWARF :49/:74 -- declared-only (their own ledger functions).
    void Construct();
    void Unlock(const BehaviourControllerLockInterface& lLockInterface);

    // ⭐ @0x8223E990 (this TU, DWARF :54 / cpp:~58) -- BODIED 2026-08-01. It IS in the export
    // set, as the unnamed sub_8223E990; the header note that used to call this and the
    // by-value Setup(Camera) "not X360-exported" was stale. (Setup(Camera) is @0x8223EA08,
    // still declaration-only.)
    void Setup(BehaviourHelperIndex lBehaviourHelperIndex,
               const BehaviourManager* lpBehaviourController);

    // @0x8223EA08 (DWARF :58) -- the by-value overload; the CALLER materialises the copy
    // (Camera::Camera @0x8224EF44 runs before the call), which confirms the by-value
    // signature. Sets meType = E_TYPE_CACHED. Declaration-only.
    void Setup(Camera lCamera);

    // @0x821F8508 (this TU, DWARF :62 / cpp:93).
    void Setup(const BrnDirector::ICEWrapper* lpIceWrapper);

    // @0x8223EA80 (this TU, DWARF :66 / cpp:172).
    const Camera& GetCamera(const BehaviourManager* lpBehaviourController);

    // @0x822523F0 (this TU, DWARF :70 / cpp:110).
    void Prepare(const BehaviourControllerLockInterface& lLockInterface);

    // @0x822524A8 (this TU, DWARF :78 / cpp:155).
    void Release(const BehaviourControllerLockInterface& lLockInterface);

    // DWARF :81 -- X360 header-inline (the `0 < meType < 4` range check every
    // bodied function opens with).
    bool IsValid() const { return meType > E_TYPE_INVALID && meType < E_TYPE_COUNT; }

private:
    Camera                         mCamera;                // :85  +0x000 (X360 sizeof(Camera) == 0x160)
    const BrnDirector::ICEWrapper* mpIceWrapper;           // :87  +0x160
    BehaviourHelperIndex           mBehaviourHelperIndex;  // :88  +0x164
    EType                          meType;                 // :101 +0x168
    bool                           mbBehaviourLocked;      // :102 +0x16C
};

}
}
