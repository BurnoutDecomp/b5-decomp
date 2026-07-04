// ============================================================================
// GameSource/Director/Utils/BrnPostBox.cpp
//
// Per-instantiation compilation home for the BrnDirector::PostBox<T>::GetPackage()
// const bodies the X360 ARTIST build emitted out-of-line. The generic accessor body
// is inline in BrnPostBox.h; these thin explicit instantiations force one out-of-line
// emission per template argument the build attested.
// ============================================================================

#include "GameSource/Director/Utils/BrnPostBox.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_LineTestFineResult.hpp"  // OutEventLineTestFineResult
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"                 // OutEventLineTestNearestResult

// X360 0x821FC260 / 0x821FBFA0 :
//   BrnDirector::PostBox<const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult*>::GetPackage() const
// (BrnPostBox.h:108). Asserts meState == E_STATE_GOT_PACKAGE (==2), then returns &mPackage (this+4:
// the 4-byte pointer package immediately after the 4-byte EState -- asm `addi r3,r31,4`). The two
// X360 addresses are the SAME instantiation (duplicate COMDAT copies the linker emitted twice; asm
// identical); the ONE explicit instantiation line below covers both.
// Callers: BrnDirector::Camera::BehaviourRoadRunner::PostCollisionUpdate and
// BrnDirector::Camera::VisibilityCollisionPolicy::ProcessSceneQueryResults.
template const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult* const&
    BrnDirector::PostBox<const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult*>::GetPackage() const;

// X360 0x821FBF48 :
//   BrnDirector::PostBox<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>::GetPackage() const
// (BrnPostBox.h:108). Asserts meState == E_STATE_GOT_PACKAGE, then returns &mPackage. Return this+0x10
// (asm `addi r3,r31,0x10`): committed CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult is
// `struct alignas(16)` (CgsSceneManagerModuleIO.h:33), so the 4-byte EState@+0 is followed by 12 bytes
// of pad, landing mPackage at +0x10. DWARF (BrnPostBox.h:100) return type is `const OutEventLineTestNearestResult&`.
// Callers (all hold LineTestNearestPostBox members): Camera::Utils::ResolveLineTestNearestUsingNormalStrict,
// Camera::Utils::ResolveLineTestNearestUsingDisplacementAndVector, BehaviourRoadRunner::Update,
// BehaviourRenderMetrics::Update.
template const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult&
    BrnDirector::PostBox<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>::GetPackage() const;
