// Compile-time layout attestations for the BrnPhysics-deform group's four TUs.
// Asserts the X360 element strides / member roles that the recovered bodies depend on.

#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// --- PropZoneData (X360 GetRespawnTypeForProp @0x822BB1F0 walks 12-byte cells) ---
static_assert(sizeof(BrnPhysics::Props::PropCellId)   == 4,  "PropCellId is two u16 == 4 bytes");
static_assert(sizeof(BrnPhysics::Props::PropCellData) == 12, "PropCellData stride is 12 (asm `addi r11,r11,0xC`)");

// --- StreamedDeformationSpec (X360 GetGlassPaneSpec @0x825B32D8 / GetWheelSpec @0x822A0328) ---
static_assert(sizeof(BrnPhysics::Deformation::GlassPaneSpec) == 112, "GlassPaneSpec stride 112 (asm `112 * liIndex`)");
static_assert(sizeof(BrnPhysics::Deformation::WheelSpec)     == 48,  "WheelSpec stride 48 (asm `48 * liWheel`)");

// --- DetachedPartNotificationEvent event queue (X360 AddEventSafe/Append, 32-byte stride) ---
static_assert(sizeof(BrnPhysics::Deformation::DetachedPartNotificationEvent) == 32,
              "DetachedPartNotificationEvent stride 32 (asm `slwi r11,r11,5` / 4-qword copy)");

// --- PhysicalTrafficState event queue (X360 Append, 816-byte stride) ---
static_assert(sizeof(BrnPhysics::Vehicle::PhysicalTrafficState) == 816,
              "PhysicalTrafficState stride 816 == 0x330 (asm `mulli r5, srcLen, 0x330`)");
