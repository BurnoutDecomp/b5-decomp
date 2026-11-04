#include "GameShared/GameClasses/System/Resource/CgsTextFileResource.h"
#include "SharedClasses/Progression/BrnProfileUpgradeResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsRwMaterialCRC32ResourceType.h"

// ARTIST vtable 820A0C30 slot 0 -> 826D7F68.
uint32_t CgsResource::TextFileResourceType::GetTypeID() const { return 3; }
// ARTIST vtable 820A1B78 slot 0 -> 82676B98.
uint32_t BrnProgression::ProfileUpgradeResourceType::GetTypeID() const { return 0x10021; }
// ARTIST vtable 820A0CF0 slot 0 -> 828A80E8.
uint32_t CgsResource::RwMaterialCRC32ResourceType::GetTypeID() const { return 11; }
// ARTIST vtable 820A0C30 slot 11 -> 826658F0.
CgsResource::EDebugResourceCategory CgsResource::TextFileResourceType::GetDebugResourceCategory() const { return CgsResource::E_DEBUGRESOURCECATEGORY_DEBUG; }
