// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.cpp
//
// BrnDirector::DebugComponent -- the director module's in-game debug menu component
// (registered under the name "Camera" -- see GetName). Of the 7 functions this build's
// ARTIST export recovers for this TU, 4 are reconstructed here:
//   - Construct                @ 0x821F6FD8  [EXECUTED in boot trace]
//   - GetName                  @ 0x821F7050
//   - TakePanorama             @ 0x821F7060
//   - (Destruct is DWARF-declared but has no recovered body in this export)
//
// OnActivate (0x82275F68), RenderHUD (0x82209A68), StartEditor (0x821F7080) and
// UpdatePanoramaScreenshots (0x8221B470) remain declaration-only -- see the per-function
// BLOCKED comments in the header. All four reach into BrnDirector::DirectorModule's
// still-unmodelled multi-megabyte layout (BrnDirectorModule.h is constructor-only) by raw
// byte offset off mpDirectorModule, which the project convention forbids reconstructing as
// a raw offset-cast; RenderHUD and UpdatePanoramaScreenshots additionally involve heavy VMX
// matrix/quaternion math with no recovered semantic names for their constant tables.
// ============================================================================

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// DebugComponent::Construct @ 0x821F6FD8  (called by BrnDirector::DirectorModule::Construct)
//
// The X360 prologue's first call demangles as
// CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct, but a Construct
// calling an unrelated class's Destruct makes no sense -- this is the well-known
// linker ICF folding pattern documented for this class family (see CgsDebugComponent.cpp
// DebugComponent::Construct: two zero-stores, mbActive=false / mpDebugLinkedListNext=
// nullptr). It is the base CgsDev::DebugComponent::Construct() call, folded at link time
// with a byte-identical trivial function from an unrelated TU.
// ----------------------------------------------------------------------------
void DebugComponent::Construct(DirectorModule* lpDirectorModule)
{
    CgsDev::DebugComponent::Construct();

    CGS_ASSERT(lpDirectorModule != 0, "lpCameraModule != NULL");

    mpDirectorModule          = lpDirectorModule;
    mbShowCameraPos           = false;
    mbShowCrashShotInfo       = false;
    mbTakePanoramaScreenshot  = false;
}

// ----------------------------------------------------------------------------
// DebugComponent::GetName @ 0x821F7050
// ----------------------------------------------------------------------------
const char* DebugComponent::GetName() const
{
    return "Camera";
}

// ----------------------------------------------------------------------------
// DebugComponent::TakePanorama @ 0x821F7060
//
// Debug-menu action callback (OnActivate registers it with userData=this -- see the
// header FLAG comment). Arms the panorama screenshot request on the rising edge
// (resets the pitch/yaw step counters); a no-op while a request is already pending.
// ----------------------------------------------------------------------------
void DebugComponent::TakePanorama(void* lpUserData)
{
    DebugComponent* lpThis = reinterpret_cast<DebugComponent*>(lpUserData);

    if (!lpThis->mbTakePanoramaScreenshot)
    {
        lpThis->miPanoramaStepPitch      = 0;
        lpThis->miPanoramaStepYaw        = 0;
        lpThis->mbTakePanoramaScreenshot = true;
    }
}

}
