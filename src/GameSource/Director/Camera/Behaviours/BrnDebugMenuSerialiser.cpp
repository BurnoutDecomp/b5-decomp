// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.cpp
//
// Compilation home for the BrnDirector::Camera::DebugMenuSerialiser slice this TU owns:
//   - DebugMenuSerialiser::Serialise(const char*, Vector3&) @0x82219A50
//
// The vec3 leaf visitor: for each component (x, y, z) it registers the field with the debug
// menu via the Process<float> worker under the current menu path, then gives that menu variable
// a 0.01 adjust step through the bound debug component (CgsDev::DebugComponent::SetStep,
// inherited by BrnDirector::DebugComponent).
//
// Store-for-store from the asm at 0x82219A50 (r31=this, r29=name, r30=&value):
//     Process<float>(name, value.x);                     ; bl Process<float>(this, name, &value)
//     mpDebugComponent->SetStep(&value.x, 0.01f);        ; lwz r3,0x48(this); f1=flt_82002138; bl SetStep
//     Process<float>(name, value.y);                     ; r30 += 4 -> &value.y
//     mpDebugComponent->SetStep(&value.y, 0.01f);
//     Process<float>(name, value.z);                     ; r30 += 8 -> &value.z
//     result = mpDebugComponent->SetStep(&value.z, 0.01f);
//
// flt_82002138 == 0.0099999998f == 0.01f (the per-component debug-menu adjust step). The X360
// keeps SetStep's return register live into the tail (`b __restgprlr_28`); the DWARF signature
// returns void, so the value is dropped (a leftover-register artifact). Vector3's x/y/z sit at
// +0x00/+0x04/+0x08 (rw::math::vpu::Vector3), matching the asm's r30 / r30+4 / r30+8.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"  // complete BrnDirector::DebugComponent (SetStep)

namespace BrnDirector
{
namespace Camera
{

void DebugMenuSerialiser::Serialise(const char* lpcName, Vector3& lrValue)
{
    static const f32 KF_DEBUG_ADJUST_STEP = 0.01f;   // flt_82002138 (0.0099999998f)

    Process<float>(lpcName, lrValue.x);
    mpDebugComponent->SetStep(&lrValue.x, KF_DEBUG_ADJUST_STEP);

    Process<float>(lpcName, lrValue.y);
    mpDebugComponent->SetStep(&lrValue.y, KF_DEBUG_ADJUST_STEP);

    Process<float>(lpcName, lrValue.z);
    mpDebugComponent->SetStep(&lrValue.z, KF_DEBUG_ADJUST_STEP);
}

} // namespace Camera
} // namespace BrnDirector
