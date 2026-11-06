#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h"
#include <stddef.h>
using namespace CgsSceneManager::SceneManagerIO;
CGS_STATIC_ASSERT(sizeof(EExclusionMode)==4,"enum");
CGS_STATIC_ASSERT(sizeof(InEventLineTestNearest)==0x40,"event size");
CGS_STATIC_ASSERT(__alignof(InEventLineTestNearest)==16,"event alignment");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,mLineStart)==0,"start");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,mLineEnd)==0x10,"end");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,mQueryId)==0x20,"query");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,mx32EntityTypeFlags)==0x24,"flags");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,mExcludeEntityId)==0x28,"entity");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,meExclusionMode)==0x2c,"mode");
CGS_STATIC_ASSERT(offsetof(InEventLineTestNearest,mxVolumeTypeFlags)==0x30,"volume");
