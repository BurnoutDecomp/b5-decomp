// ============================================================================
// GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.cpp
//
// BrnDirector::BrnDirectorVehicleInputInterface -- the world -> director "a car entered the
// simulation" seam. Three ledger functions, all bodied here:
//   Construct                 (DWARF :54)   -- bring the embedded NewVehicleEvent<50> queue up
//   GetNewVehicleEventQueue   (DWARF :57)
//   NewVehicle                (DWARF :63)   @ X360 0x822CBA90
//
// ⭐⭐ WHY THIS TU EXISTS NOW. The two SHARED gameplay cameras
// (BehaviourGameplayExternal / BehaviourGameplayBumper) do nothing at all until their
// Parameters::mbIsValid is true, and the ONLY writers of that byte are the two
// Parameters::Set functions -- which are fed, ultimately, from an event pushed HERE.
// The full chain is written out in BrnBehaviourGameplayExternal.h's Update FLAG; this file
// is its first link that actually carries data.
//
// ⚠️ THE PRODUCER SIDE IS A PC BRING-UP STAND-IN, and it is flagged where it lives, not
// here: the console's only caller of NewVehicle is
// RaceCarEntityModule::ProcessCreateVehicleEvents @0x822FF620, which drains
// VehicleManagerOutputInterface::mCreateVehicleResultQueue -- and the ONLY producer of THAT
// queue in the whole XEX is BrnPhysics::Vehicle::VehicleManager::ProcessCreateEvents
// @0x82616770, which this build does not have (BrnVehicleManager.cpp is not on the build
// list). See RaceCarEntityModule::PublishNewVehicleToDirectorWithoutPhysicsBringUp.
// THIS function is the console's own, verbatim.
//
// ---- X360 body @0x822CBA90, statement for statement -------------------------------------
//   NewVehicle(this, r4 = lAttribsKey (64-bit, `std` into the stack event), r5 = liEntityIndex)
//     std  r30, var_A0(r1)                    ; lEvent.mAttribsKey
//     stw  r21, var_98(r1)                    ; lEvent.miEntityIndex  (var_A0 + 8)
//     sub_82204998(&lCarAsset, key, 0)        ; Attrib::Gen::burnoutcarasset(key, owner)
//     assert lCarAsset.IsValid()              ; :135  "Invalid car asset, key:"
//     camerabumperbehaviour  (RefSpec(carData + 0x1B8).GetCollection(), 0)
//     cameraexternalbehaviour(RefSpec(carData + 0x1A0).GetCollection(), 0)
//     lpcName = *(carData + 0x1E8)
//     assert lBumperCam.IsValid()             ; :141  "Invalid bumpercam asset, key:"
//     assert bumperData[0x18] > 0.0f          ; :142  "Invalid bumpercam Boost FOV, key:"
//     assert lExternalCam.IsValid()           ; :143  "Invalid externalcam asset, key:"
//     assert externalData[0x40] > 0.0f        ; :144  "Invalid externalcam Boost FOV, key:"
//     return NewVehicleEventQueue::AddEvent(this, &lEvent)
//     ~Instance x3
//
// ⭐ THE FOUR ATTRIB ASSERTS ARE THE SAME FOUR MainDirector::ProcessNewVehicleEvents
// @0x8221A6B0 repeats at .cpp:1795..:1798 -- the console checks the car's camera attribs at
// BOTH ends of the queue. The +0x18 / +0x40 operands are BehaviourGameplayBumper::
// Parameters' and BehaviourGameplayExternal::Parameters' own `mfBoostFOV <- source[...]`
// slots, which is independent confirmation that both Parameters::Source blocks model the
// right thing.
//
// ⚠️ The console's assert TEXT is a streamed composite ("Invalid car asset, key:" + the
// 64-bit key + " name:" + the asset name + ", model index:" + the index). CGS_ASSERT takes a
// literal, so -- as everywhere else in this tree (e.g. HandleResetPlayerCarAction's "Invalid
// Number of Palettes: ") -- the literal PREFIX is kept and the streamed operands are dropped.
// ============================================================================

#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"                 // Attrib::Gen::burnoutcarasset
#include "GameSource/AttribSys/Generated/classes/camerabumperbehaviour.h"           // Attrib::Gen::camerabumperbehaviour
#include "GameSource/AttribSys/Generated/classes/cameraexternalbehaviour.h"         // Attrib::Gen::cameraexternalbehaviour

namespace BrnDirector
{
namespace
{
    // The two Boost-FOV slots the console's :142 / :144 tripwires read out of the resolved
    // camera-attrib data areas. They are the SAME two offsets BehaviourGameplayBumper::
    // Parameters::Set and BehaviourGameplayExternal::Parameters::Set copy mfBoostFOV from,
    // which is why they are spelled as source-array offsets rather than as struct members
    // (the attrib data area is the serialised source block, not either Parameters block).
    const u32 KU_BUMPER_SOURCE_BOOST_FOV_OFFSET   = 0x18;   // `lfs f0, 0x18(r11)`
    const u32 KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET = 0x40;   // `lfs f0, 0x40(r11)`

    f32 ReadSourceFloat(const void* lpAttributeData, u32 luByteOffset)
    {
        if (lpAttributeData == 0)
        {
            return 0.0f;
        }
        return *reinterpret_cast<const f32*>(
            static_cast<const u8*>(lpAttributeData) + luByteOffset);
    }
}

    // DWARF :54 -- point the embedded queue at its own inline event buffer. Nothing else
    // brings it up: the three IO buffers that embed this interface
    // (DirectorIO::InputBuffer, BrnWorldIO::UpdateOutputBuffer and
    // RaceCarEntityModuleIO::OutputBuffer_PostPhysics) each zero-fill their tail, so without
    // this call mpEvents is NULL and the first AddEvent writes through it.
    void BrnDirectorVehicleInputInterface::Construct()
    {
        mNewVehicleQueue.Construct();
        mNewVehicleQueue.Clear();
    }

    // DWARF :57.
    const BrnDirectorVehicleInputInterface::NewVehicleEventQueue*
    BrnDirectorVehicleInputInterface::GetNewVehicleEventQueue() const
    {
        return &mNewVehicleQueue;
    }

    // @0x822CBA90 -- see the file banner.
    s32 BrnDirectorVehicleInputInterface::NewVehicle(u64 lAttribsKey, s32 liEntityIndex)
    {
        NewVehicleEvent lEvent;
        lEvent.mAttribsKey   = lAttribsKey;
        lEvent.miEntityIndex = liEntityIndex;

        // The car's top-level attribute asset, resolved by its collection key.
        Attrib::Gen::burnoutcarasset lCarAsset(lAttribsKey, 0);
        CGS_ASSERT(lCarAsset.IsValid(), "Invalid car asset, key:");                    // :135

        // Its two gameplay-camera sub-collections, reached through the asset's own RefSpecs.
        Attrib::RefSpec* lpBumperRef   = lCarAsset.GetBumperCamRefSpec();
        Attrib::RefSpec* lpExternalRef = lCarAsset.GetExternalCamRefSpec();

        Attrib::Gen::camerabumperbehaviour lBumperCam(
            (lpBumperRef != 0)
                ? const_cast<Attrib::Collection*>(lpBumperRef->GetCollection()) : 0, 0);
        Attrib::Gen::cameraexternalbehaviour lExternalCam(
            (lpExternalRef != 0)
                ? const_cast<Attrib::Collection*>(lpExternalRef->GetCollection()) : 0, 0);

        // The console loads the asset name here (it only feeds the assert messages below).
        (void)lCarAsset.GetAssetName();

        CGS_ASSERT(lBumperCam.IsValid(), "Invalid bumpercam asset, key:");             // :141
        CGS_ASSERT(ReadSourceFloat(lBumperCam.GetLayoutPointer(),
                                   KU_BUMPER_SOURCE_BOOST_FOV_OFFSET) > 0.0f,
                   "Invalid bumpercam Boost FOV, key:");                               // :142
        CGS_ASSERT(lExternalCam.IsValid(), "Invalid externalcam asset, key:");         // :143
        CGS_ASSERT(ReadSourceFloat(lExternalCam.GetLayoutPointer(),
                                   KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET) > 0.0f,
                   "Invalid externalcam Boost FOV, key:");                             // :144

        return mNewVehicleQueue.AddEvent(lEvent) ? 1 : 0;
    }
}
