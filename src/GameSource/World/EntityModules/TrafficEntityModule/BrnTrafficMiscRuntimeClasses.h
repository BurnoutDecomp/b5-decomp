#ifndef BRN_TRAFFIC_MISC_RUNTIME_CLASSES_H
#define BRN_TRAFFIC_MISC_RUNTIME_CLASSES_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // ::EntityId { u32 muValue } (4-byte packed handle)
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> (base of PhysicalRequestInfoList)

// =============================================================================
// BrnTrafficMiscRuntimeClasses.h  (element-type home)
//
// Homes BrnTraffic::PhysicalRequestInfo and its fixed-capacity list alias
// PhysicalRequestInfoList : Array<PhysicalRequestInfo, 25>. Reconstructed from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h).
//
// PhysicalRequestInfo is an 8-byte POD (DWARF :61-65):
//   uint16_t muVehicle       @+0x00  (:63)
//   int8_t   miReason        @+0x02  (:64)   -- one of PhysicalReason
//   pad                      @+0x03
//   EntityId mTargetEntityId @+0x04  (:65)   -- ::EntityId { u32 muValue }
// sizeof == 8. The 8-byte stride is X360-attested by Array<PhysicalRequestInfo,25>::
// Append @0x82918B98 (slwi r11,r11,3 == index*8; two-word stw@+0/stw@+4 element copy;
// count word @+0xC8 == 200 == 25*8). Members accessed by name only.
// =============================================================================

namespace BrnTraffic
{
    // DWARF BrnTrafficMiscRuntimeClasses.h:37.
    enum PhysicalReason
    {
        E_PHYSICALREASON_INVALID              = -1,
        E_PHYSICALREASON_CRASHED              = 0,
        E_PHYSICALREASON_CHECKED              = 1,
        E_PHYSICALREASON_SLAMMED              = 2,
        E_PHYSICALREASON_SYMPATHETIC_CRASHING = 3,
        E_PHYSICALREASON_SWERVING             = 4,
        E_PHYSICALREASON_NORMAL               = 5,
    };

    // DWARF BrnTrafficMiscRuntimeClasses.h:61. 8-byte POD.
    struct PhysicalRequestInfo
    {
        u16      muVehicle;        // +0x00  :63
        s8       miReason;         // +0x02  :64  (PhysicalReason, stored as i8)
        EntityId mTargetEntityId;  // +0x04  :65

        // DWARF :73.
        void Construct(u16 luVehicle, PhysicalReason leReason, EntityId lTargetEntityId);
    };

    // DWARF BrnTrafficMiscRuntimeClasses.h:80. Fixed 25-slot request list.
    struct PhysicalRequestInfoList : public Array<PhysicalRequestInfo, 25>
    {
    };
}

#endif // BRN_TRAFFIC_MISC_RUNTIME_CLASSES_H
