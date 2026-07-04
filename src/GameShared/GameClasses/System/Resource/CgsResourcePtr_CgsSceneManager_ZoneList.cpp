#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/SceneManager/Zones/ZoneList.h"   // complete CgsSceneManager::ZoneList

// Per-instantiation TU for CgsResource::ResourcePtr<CgsSceneManager::ZoneList>.
// Bodies are the generic inline accessors in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the two operator-> symbols the X360 ARTIST build
// attested. Both accessors only read/static_cast the base's mpResourceMemory (+0)
// to ZoneList* -- no ZoneList member is touched -- so the accessor bodies are the
// single-load generic (NOT a Font-style double-deref member specialization). The
// canonical ZoneList layout/home is ZoneList.h (included for the complete type, in
// line with the TriggerData / StreetData sibling TUs).
//
//   operator->() const  @ 0x827C2FA8  (single-deref of +0; baked assert line 563;
//                                      SIGNED cmpwi; msg 'Can not instance resource
//                                      pointer - it has no main memory resource\n').
//                                      Called by BrnWorld::PVSDebugComponent::RenderPVS.
//   operator->()        @ 0x822C9F68  (single-deref of +0; baked assert line 544;
//                                      non-const). Called by BrnWorld::PVSModule::
//                                      Update / Prepare.
// The X360-baked file/line args are discarded per project convention.

template CgsSceneManager::ZoneList*
    CgsResource::ResourcePtr<CgsSceneManager::ZoneList>::operator->();
template const CgsSceneManager::ZoneList*
    CgsResource::ResourcePtr<CgsSceneManager::ZoneList>::operator->() const;
