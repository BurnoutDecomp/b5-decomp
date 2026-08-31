#ifndef BRN_SOUND_VEHICLES_VEHICLE_STATE_H
#define BRN_SOUND_VEHICLES_VEHICLE_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"                 // BrnState (the DWARF base)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::RaceCarState
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"                   // CgsSound::Utils::DataPoint<bool>
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attribute::Key

// =============================================================================
// BrnSound::Vehicles::VehicleState
//   GameSource/Sound/Vehicles/BrnVehicleState.h (DWARF home) +
//   GameSource/Sound/Vehicles/BrnVehicleState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF.
//
// (2026-08-25, audio-faithfulness wave 5 RECONCILIATION): this header used to model
// VehicleState as a NAMESPACE hosting AttachInfo -- but the DWARF
// (BrnVehicleState.h:41) is unambiguous:
//   struct BrnSound::Vehicles::VehicleState : public BrnSound::Logic::BrnState
// with EEngineComponentType (:48) and AttachInfo (:137) NESTED, and
// GetEngineComponentName/GetEngineComponentKey as CONST MEMBERS (:191/:200). The
// same class also had a SECOND rival definition inside GameShared CgsState.h
// (struct : State direct, ctor-derived numeric member names). Both are folded
// here; the ctor-derived tail decodes EXACTLY onto the DWARF names:
//   +96    mVehiclePhysicsData   (ctor: RaceCarState::Clear @0x8229FFC8)
//   +1216  mAttachInfo           (the old mi1216/mi1220/mu1224 = asset/index/token)
//   +1232  mVehicleBoostInfo     (BoostOutputInfo, 36B -- untouched by the ctor)
//   +1268  mcaEngineComponentName[2][13] (ctor zeroes byte 1268 + 1281 = the two
//          strings' first chars)
//   +1296  mEngineComponentKey[2] (8-byte elements -- the old mu1296/mu1304; the
//          PhysicsControl (type+0xA2)*8 walk lands exactly here: 0xA2*8 == 0x510)
//   +1312  mfMaxRpm              (the old mf1312)
//   +1316  mbCollisionOccuredFlag
//   +1317  bIsRaceCarActive      (DataPoint<bool>, the old mu1317/mu1318 pair)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// the console offsets above are comments, not asserted.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// VehicleData is the sound-side name for the RaceCarState snapshot published by
// the vehicle manager. The X360 producer copies exactly that 1120-byte record and
// every sound consumer reads its named RaceCarState fields.
typedef BrnPhysics::Vehicle::RaceCarState VehicleData;

// DWARF BrnVehicleState.h:41. The per-vehicle engine-sound state.
struct VehicleState : public BrnSound::Logic::BrnState
{
    // DWARF BrnVehicleState.h:48 (nested). The engine-component selector.
    enum EEngineComponentType
    {
        E_ENGINE    = 0,
        E_EXHAUST   = 1,
        E_MAX_TYPES = 2,
    };

    // BrnVehicleState.h (assert-cited region). Upper bound for the active-race-car
    // index the attach path validates against (cmpwi r30, 8). The assert text is
    // "liVehicleIndex >= 0 && liVehicleIndex < static_cast<int32_t>(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)".
    static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

    // DWARF BrnVehicleState.h:137 (NESTED -- the old namespace-hosted copy folded
    // in; :275/:276 assert sites). The per-attach record.
    struct AttachInfo
    {
        // BY NAME. The vehicle asset being attached (asserted non-null @:276;
        // DWARF :149 `const VehicleListEntry*`, held opaque). 4-byte pointer on
        // X360; widened on host.
        void* mpVehicleAsset;

        // The active-race-car index (asserted [0, KI_MAX_ACTIVE_RACE_CARS) @:275).
        u32 muVehicleIndex;

        // The 64-bit attach token (std-store; full 8 bytes on X360; DWARF :151 CgsID).
        u64 mAttachToken;

        // @ 0x82681FC8 -- validate the index and asset, then store all three fields.
        // Signature mirrors the X360 fastcall register order: r4=token, r5=asset,
        // r6=vehicleIndex. Returns `this`.
        AttachInfo* Construct( u64 aAttachToken, void* apVehicleAsset, u32 auVehicleIndex );
    };

    // @ 0x826C9E70 (was homed in the GameShared CgsState.cpp rival). Clears the
    // physics blob via RaceCarState::Clear and seeds the tail fields. Bodied in
    // BrnVehicleState.cpp.
    VehicleState();
    virtual ~VehicleState() {}

    // BrnVehicleState.cpp:238/62/254.  Attach is the base State forwarder;
    // UpdateParams refreshes the live vehicle/boost snapshots and watches for a
    // streamed-asset replacement; Detach performs the console's attached-only
    // transition before clearing the vehicle-specific payload.
    virtual void Attach(void* apvAttachment);
    virtual void UpdateParams(f32 af32DeltaTime);
    virtual bool Detach();

    // DWARF :191. Resolve the human-readable component name for a component type.
    // FLAG (DEFER): declared-only -- its body is a separate recon slice.
    const char* GetEngineComponentName( EEngineComponentType aeComponentType ) const;

    // DWARF :200. The component attribute key (the (type+0xA2)*8 walk == this
    // member array, by name) with the console's non-zero guard. Bodied in
    // BrnVehicleState.cpp (the PhysicsControl forwarder inlines it on X360).
    u64 GetEngineComponentKey( EEngineComponentType aeComponentType ) const;

    BrnPhysics::Vehicle::RaceCarState* GetVehicleData() { return &mVehiclePhysicsData; }
    const BrnPhysics::Vehicle::RaceCarState* GetVehicleData() const { return &mVehiclePhysicsData; }
    const u8* GetBoostInfo() const { return mauVehicleBoostInfo; }
    u64 GetAttribKey() const { return mVehiclePhysicsData.mCarAssetAttribKey; }
    u8 GetVehicleIndex() const { return static_cast<u8>(mAttachInfo.muVehicleIndex); }
    f32 GetMaxRPM() const { return mfMaxRpm; }
    void SetCollisionOccured(bool abOccurred) { mbCollisionOccuredFlag = abOccurred; }
    bool GetCollisionOccured() const { return mbCollisionOccuredFlag; }
    AttachInfo GetAttachInfo() const { return mAttachInfo; }
    virtual bool IsAttachedToThis(void* apvAttachment);

    // ---- members (DWARF order/names; console offsets in the banner) ----
protected:
    // DWARF :169 `VehicleData mVehiclePhysicsData` -- modelled as its leading
    // RaceCarState (see the VehicleData FLAG above).
    BrnPhysics::Vehicle::RaceCarState mVehiclePhysicsData;   // +96

    // DWARF :171.
    AttachInfo mAttachInfo;                                  // +1216

    // DWARF :173 `BoostInfo mVehicleBoostInfo` (BoostInfo = BoostOutputInfo,
    // BrnSoundLogicSharedIO.h:49). FLAG: held as its attested 36-byte span until
    // the BrnSound-side BoostOutputInfo home is includable here.
    u8 mauVehicleBoostInfo[36];                              // +1232

    // DWARF :175. The two component-name strings (engine / exhaust).
    char mcaEngineComponentName[2][13];                      // +1268

    // DWARF :176 `Attribute::Key[2]` -- the console elements stride 8 (keys at
    // +1296/+1304, the mu1296/mu1304 pair of the old rival model; the attested
    // PhysicsControl walk reads the LEADING 4-byte Key of each 8-byte element,
    // and the committed Attribute::Key is u32). Modelled as the 8-byte element
    // with the leading key named. FLAG: the trailing word is un-attested.
    struct EngineComponentKeyElem
    {
        Attribute::Key mKey;   // +0x00 -- the component attribute key
        u32            muPad;  // +0x04 -- un-attested trailing word
    };
    EngineComponentKeyElem mEngineComponentKey[2];           // +1296

    // DWARF :177.
    f32 mfMaxRpm;                                            // +1312

private:
    // DWARF :181.
    bool mbCollisionOccuredFlag;                             // +1316

    // DWARF :183.
    CgsSound::Utils::DataPoint<bool> bIsRaceCarActive;       // +1317

    void Clear();
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_VEHICLE_STATE_H
