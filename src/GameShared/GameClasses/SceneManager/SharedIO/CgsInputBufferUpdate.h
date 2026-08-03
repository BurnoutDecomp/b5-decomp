#pragma once

// ============================================================================
// GameShared/GameClasses/SceneManager/SharedIO/CgsInputBufferUpdate.h
//
// ⚠️⚠️ RETIRED AS A DEFINITION 2026-08-03 (task #123) -- this file is now a FORWARDER.
//
// It used to define `CgsSceneManager::SceneManagerIO::InputBuffer_Update` itself, describing
// itself as "a MINIMAL additive home for the type (it was only forward-declared previously)".
// That was not true: GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h already had a full
// definition of the SAME type in the SAME namespace, and the two definitions DISAGREED --
// this one placed the embedded InSceneUpdateInterface straight after the CgsModule::IOBuffer
// base, while the canonical one carries `u8 maStatusPad[15]` to put it on +16, which is the
// offset the X360 accessor @0x825BD8C0 actually uses.
//
// Nothing caught it because no TU had ever included both: this header had exactly ONE includer
// (BrnDeformationManager.h) and the canonical one arrives through the world/scene chain.
// BrnPhysicsModule.h embedding the deformation manager by value is what finally made them meet,
// as a hard C2011.
//
// The canonical (asm-attested) home wins. The DWARF-named typedef + GetSceneUpdateInterface()
// accessor pair this file carried moved there VERBATIM, so the two call sites in
// BrnDeformationManager_Contacts.cpp are unchanged. Folding the fork also retires the wrong
// interface offset the deformation path had been compiled against.
//
// Kept as a forwarder rather than deleted so the include in BrnDeformationManager.h (and any
// future one) keeps resolving to the one true home.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"   // the canonical InputBuffer_Update
