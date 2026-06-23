#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"   // complete BrnGui::PFXHookBundle

// Per-instantiation TU for CgsResource::ResourcePtr<BrnGui::PFXHookBundle>.
// The body is the generic inline operator-> in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   operator->()  @ 0x824FBF18  (non-const)
//
// X360 @ 0x824FBF18: load *this (the main-memory resource pointer at +0x00); if it is
// null, run the Begin/Fire/End assert sequence with the baked message
// "Can not instance resource pointer - it has no main memory resource\n" (CgsResourcePtr.h
// baked line 544); then return *this as the PFXHookBundle*. Matches the generic accessor
// 1:1 (single-dword read at offset 0, null guard, return). The EffectsArbitrator hook
// lookups (GetHookFromName / GetHookFromGUID / EventUpdate / Acquire3dTints) all reach the
// bundle through this instantiation.
template BrnGui::PFXHookBundle* CgsResource::ResourcePtr<BrnGui::PFXHookBundle>::operator->();
