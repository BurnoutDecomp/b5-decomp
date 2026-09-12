// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.cpp
//
// BrnDirector::DebugComponent -- the director module's in-game debug menu component
// (registered under the name "Camera" -- see GetName). Of the 7 recovered functions of
// this class, 3 are reconstructed here:
//   - Construct      (runs on every boot)
//   - GetName
//   - TakePanorama
//   - (Destruct is declared in the original header but has no recovered body)
//
// OnActivate, RenderHUD, StartEditor and UpdatePanoramaScreenshots remain declaration-only
// -- see the per-function BLOCKED comments in the header. All four reach into
// BrnDirector::DirectorModule's still-unmodelled multi-megabyte layout
// (BrnDirectorModule.h is constructor-only) by raw byte offset off mpDirectorModule, which
// the project convention forbids reconstructing as a raw offset-cast; RenderHUD and
// UpdatePanoramaScreenshots additionally involve heavy vector-unit matrix/quaternion math
// with no recovered semantic names for their coefficient tables.
// ============================================================================

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// DebugComponent::Construct  (called by BrnDirector::DirectorModule::Construct)
//
// The first call in the recovered prologue resolves to a name from an unrelated class
// (CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct), but a Construct
// calling an unrelated class's Destruct makes no sense -- this is the well-known linker
// ICF folding pattern documented for this class family (see CgsDebugComponent.cpp
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
// DebugComponent::GetName
// ----------------------------------------------------------------------------
const char* DebugComponent::GetName() const
{
    return "Camera";
}

// ----------------------------------------------------------------------------
// DebugComponent::TakePanorama
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
