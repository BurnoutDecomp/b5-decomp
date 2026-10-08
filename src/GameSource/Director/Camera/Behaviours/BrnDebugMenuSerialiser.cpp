// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.cpp
//
// The out-of-line members of BrnDirector::Camera::DebugMenuSerialiser: AddToPath, the vec3
// leaf visitor and the four Process<T> instances. Everything the console inlines into the
// Parameters walkers lives in the header.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"  // complete BrnDirector::DebugComponent (RegisterVariable/UnregisterVariable/SetStep)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                            // CgsCore::StrCat

#include <cstring>   // strlen

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// AddToPath -- append "<lpacPath>/" to the menu path and advance the stack position by the
// pushed length. The two appends are the inlined CgsCore::StrCat (each asserts the result
// still fits the 64-byte path stack); the position advance is strlen(lpacPath) + 1, the
// exact amount RemoveFromPath takes back off.
// ----------------------------------------------------------------------------
void DebugMenuSerialiser::AddToPath(const char* lpacPath)
{
    CgsCore::StrCat(macPathStack, KI_PATHSTACKSIZE, lpacPath);
    CgsCore::StrCat(macPathStack, KI_PATHSTACKSIZE, "/");
    muStackPos += static_cast<u32>(strlen(lpacPath)) + 1;
}

// ----------------------------------------------------------------------------
// Process<T> -- the per-leaf worker. ADD registers &lrValue as a debug-menu variable named
// lpcName in the group macPathStack; REMOVE strips that variable back out (the variable
// manager keys the removal off the value address). Each instance binds a distinct typed
// CgsDev::DebugComponent::RegisterVariable overload, which is why the console has one
// out-of-line body per leaf type.
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

template void DebugMenuSerialiser::Process<float>(const char* lpcName, float& lrValue);
template void DebugMenuSerialiser::Process<int>(const char* lpcName, int& lrValue);
template void DebugMenuSerialiser::Process<unsigned int>(const char* lpcName, unsigned int& lrValue);
template void DebugMenuSerialiser::Process<bool>(const char* lpcName, bool& lrValue);

// ----------------------------------------------------------------------------
// Serialise(const char*, Vector3&) -- each component in turn as a menu float under the same
// label, each with the 0.01 adjust step.
// ----------------------------------------------------------------------------
void DebugMenuSerialiser::Serialise(const char* lpcName, Vector3& lrValue)
{
    Process<float>(lpcName, lrValue.x);
    mpDebugComponent->SetStep(&lrValue.x, KF_DEBUG_ADJUST_STEP);

    Process<float>(lpcName, lrValue.y);
    mpDebugComponent->SetStep(&lrValue.y, KF_DEBUG_ADJUST_STEP);

    Process<float>(lpcName, lrValue.z);
    mpDebugComponent->SetStep(&lrValue.z, KF_DEBUG_ADJUST_STEP);
}

} // namespace Camera
} // namespace BrnDirector
