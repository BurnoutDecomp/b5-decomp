// BrnTrafficMiscRuntimeClasses.cpp  (body home for BrnTraffic::PhysicalRequestInfo)
//
// Out-of-line body for the single X360 method emitted from this element-type home:
//   BrnTraffic::PhysicalRequestInfo::Construct  @ 0x829175B0
//
// The PhysicalRequestInfo POD layout (u16 muVehicle @+0, i8 miReason @+2, pad @+3,
// EntityId mTargetEntityId @+4; sizeof==8) and the PhysicalRequestInfoList : Array<...,25>
// alias are declared in BrnTrafficMiscRuntimeClasses.h. Store-for-store parity with the
// X360 asm: sth@+0 (muVehicle), stb@+2 (miReason), stw@+4 (mTargetEntityId), guarded by a
// range assert on luVehicle. The de-inlined BeginAssert/FireAssert pair collapses to one
// CGS_ASSERT; the rodata message is reproduced verbatim (no trailing newline in rodata).
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTraffic
{
    // DWARF BrnTrafficMiscRuntimeClasses.h:73. Assert @ line 138.
    void PhysicalRequestInfo::Construct(u16 luVehicle, PhysicalReason leReason, EntityId lTargetEntityId)
    {
        // KU_MAX_STANDARD_TRAFFIC == 0x190 (400): cmplwi 0x190 / blt at 0x829175D0.
        CGS_ASSERT(luVehicle < 0x190u, "luVehicle < KU_MAX_STANDARD_TRAFFIC");

        muVehicle       = luVehicle;                     // sth  r28, 0(r29)
        miReason        = static_cast<s8>(leReason);     // stb  r27, 2(r29)
        mTargetEntityId = lTargetEntityId;               // stw  r26, 4(r29)
    }
}
