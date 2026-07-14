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
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"  // complete BrnDirector::DebugComponent (RegisterVariable/UnregisterVariable/SetStep)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT (BeginAssert/FireAssert/EndAssert)

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// The per-leaf worker (Serialisation.h:265, `template<class T> void Process<T>(const char*, T&)`).
// In ADD mode it registers &value as a debug-menu variable under the current menu path (the path
// stack macPathStack is the group); in REMOVE mode it strips that same variable back out (the
// variable manager keys the removal off the value address, not the label). One typed
// CgsDev::DebugComponent::RegisterVariable overload is selected per instantiation
// (float/int/uint/bool), which is why each Process<T> has its own out-of-line body in the X360
// image (0x821FE078/108/198/228) -- each calls a distinct RegisterVariable.
//
// Store-for-store from Process<float> @0x821FE078 (r31=this, a2=name, a3=&value):
//     if ( meMode == E_MODE_ADD_TO_MENU )                              ; if ( !*this )
//         mpDebugComponent->RegisterVariable(&value, macPathStack, name);  ; RegisterVariable(this[0x48], a3, this+8, a2)
//     else if ( meMode == E_MODE_REMOVE_FROM_MENU )                    ; if ( *this == 1 )
//         mpDebugComponent->UnregisterVariable(&value);                ; UnregisterVariable(this[0x48], a3)
//     else
//         CGS_ASSERT(false, "unhandled case");                         ; Serialisation.h:283
//
// meMode(+0x00)/macPathStack(+0x08)/mpDebugComponent(+0x48) match the frozen class layout; the
// DWARF signature returns void, so the X360's live return register from the tail call is dropped.
// ----------------------------------------------------------------------------
template<class T>
void DebugMenuSerialiser::Process(const char* lpcName, T& lrValue)
{
    if (meMode == E_MODE_ADD_TO_MENU)
        mpDebugComponent->RegisterVariable(&lrValue, macPathStack, lpcName);
    else if (meMode == E_MODE_REMOVE_FROM_MENU)
        mpDebugComponent->UnregisterVariable(&lrValue);
    else
        CGS_ASSERT(false, "unhandled case");
}

// Explicit instantiations owned by this TU -- the four leaf types the camera-tunings/playlist
// Parameters trees drive Process<T> with: 0x821FE078 float, 0x821FE108 int, 0x821FE198 unsigned
// int, 0x821FE228 bool. Each &value resolves to the matching CgsDev::DebugComponent overload.
template void DebugMenuSerialiser::Process<float>(const char* lpcName, float& lrValue);
template void DebugMenuSerialiser::Process<int>(const char* lpcName, int& lrValue);
template void DebugMenuSerialiser::Process<unsigned int>(const char* lpcName, unsigned int& lrValue);
template void DebugMenuSerialiser::Process<bool>(const char* lpcName, bool& lrValue);

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
