// ============================================================================
// b5-decomp/src/GameSource/Physics/BrnPhysicsModuleIO.h
//
// Canonical (DWARF) home for BrnPhysics::PhysicsModuleIO::OutputBuffer
// (BrnPhysicsModuleIO.h:260). MINIMAL-COMPLETE slice covering ONLY the OutputBuffer's
// X360-emitted accessors owned by the IO-OutputBuffers group:
//   GetVehicleOutputRequestInterface() @ 0x8259FF30 write (bit 3) -> +16     (DWARF :349)
//   GetVehicleOutputInterface() const  @ 0x8279F598 read  (bit 4) -> +44128  (DWARF :354)
//   GetVehicleOutputInterface()        @ 0x825A0080 write (bit 3) -> +44128  (DWARF :355)
//   GetPropManagerOutputInterface() const @ 0x8279F640 read (bit 4) -> +71792 (DWARF :357)
//   GetPropManagerOutputInterface()    @ 0x825C0DC8 write (bit 3) -> +71792  (DWARF :358)
//   GetDeformationOutputInterface()    @ 0x825A0128 write (bit 3) -> +148656 (DWARF :361)
//   GetContactSpyInterface()           @ 0x825A0320 write (bit 3) -> +998192 (DWARF :370)
//
// LAYOUT (DWARF :260 member order + X360 getter return-offsets, authoritative):
//   base   CgsModule::IOBuffer                                  (1-byte status; +1..+15 pad)
//   +16     VehicleOutputRequestInterface mVehicleOutputRequestInterface         :376
//   +...    VehicleManagerOutputInterface mVehicleManagerOutputInterface         :378
//   +44128  VehicleOutputInterface        mVehicleOutputInterface                :379
//   +71792  PropOutputInterface           mPropManagerOutputInterface            :381
//   +148656 DeformationOutputInterface    mDeformationOutputInterface            :383
//   +...    DeformationOutputInterfaceForEntityModules mDeformationOutputInterfaceForEntityModules :384
//   +...    SceneInputInterface           mSceneInputInterface                   :385
//   +998192 ContactSpyInterface           mContactSpyInterface                   :386
//
// FLAG (foreign types): every interface member here has its own owning home elsewhere
// and is NOT reconstructed in this slice. The five X360-pinned return offsets (+16,
// +44128, +71792, +148656, +998192) are made exact via correctly-sized opaque storage;
// the intervening members whose offsets are not separately X360-attested
// (mVehicleManagerOutputInterface, mDeformationOutputInterfaceForEntityModules,
// mSceneInputInterface) are folded into the padding (named in comments). Adopt the named
// interface types additively when their homes land.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    class OutputBuffer : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above). Each is a 1-byte placeholder; the
        // member offsets are pinned by the explicit padding arrays in the private section.
        struct VehicleOutputRequestInterfaceStorage { unsigned char maBytes[1]; };
        struct VehicleOutputInterfaceStorage        { unsigned char maBytes[1]; };
        struct PropOutputInterfaceStorage           { unsigned char maBytes[1]; };
        struct DeformationOutputInterfaceStorage    { unsigned char maBytes[1]; };
        struct ContactSpyInterfaceStorage           { unsigned char maBytes[1]; };

        // ---- accessors owned/bodied by this group --------------------------------------
        VehicleOutputRequestInterfaceStorage*       GetVehicleOutputRequestInterface();       // +16,     write
        const VehicleOutputInterfaceStorage*        GetVehicleOutputInterface() const;        // +44128,  read
        VehicleOutputInterfaceStorage*              GetVehicleOutputInterface();              // +44128,  write
        const PropOutputInterfaceStorage*           GetPropManagerOutputInterface() const;    // +71792,  read
        PropOutputInterfaceStorage*                 GetPropManagerOutputInterface();          // +71792,  write
        DeformationOutputInterfaceStorage*          GetDeformationOutputInterface();          // +148656, write
        ContactSpyInterfaceStorage*                 GetContactSpyInterface();                 // +998192, write

        static void _AssertLayout();

    private:
        u8                                   maStatusPad[15];                 // +1..+15 (force +16)
        VehicleOutputRequestInterfaceStorage mVehicleOutputRequestInterface;  // +16     :376
        // mVehicleManagerOutputInterface (:378) folded into this padding: +17 .. +44128.
        unsigned char                        maVehicleManagerAndPad[44128 - 17]; // ...  :378
        VehicleOutputInterfaceStorage        mVehicleOutputInterface;          // +44128  :379
        // gap to mPropManagerOutputInterface: +44129 .. +71792.
        unsigned char                        maPropMgrPad[71792 - 44129];      // ...
        PropOutputInterfaceStorage           mPropManagerOutputInterface;      // +71792  :381
        // gap to mDeformationOutputInterface: +71793 .. +148656.
        unsigned char                        maDeformationPad[148656 - 71793]; // ...
        DeformationOutputInterfaceStorage    mDeformationOutputInterface;      // +148656 :383
        // mDeformationOutputInterfaceForEntityModules (:384) + mSceneInputInterface (:385)
        // folded into this padding: +148657 .. +998192.
        unsigned char                        maEntityAndSceneAndPad[998192 - 148657]; // ... :384/:385
        ContactSpyInterfaceStorage           mContactSpyInterface;             // +998192 :386
    };
}
}
