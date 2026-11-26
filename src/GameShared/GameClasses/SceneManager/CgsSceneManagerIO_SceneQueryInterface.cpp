#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"    // CgsSceneManager::SceneManagerIO::SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"          // InEventLineTestFine element (committed, 0x40 stride) + EExclusionMode
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h" // InEventVolumeTestDeepest


// =============================================================================
// CgsSceneManager::SceneManagerIO::SceneQueryInterface -- two of the producer methods
// (LineTestFine @ 0x82216EF0, VolumeTestDeepest @ 0x822170B0). The sibling LineTestNearest
// @ 0x82216FD0 lives in CgsSceneManagerIO_SceneQueryInterface_LineTestNearest.cpp; HasData
// @ 0x82204E48 lives in CgsSceneManagerModuleIO.cpp.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// =============================================================================

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestFine  @ 0x82216EF0
//
// DecFIGS CgsSceneManagerModuleIO.h:346. ARTIST takes the two endpoints in v1/v2.
void CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestFine(
    SceneQueryId lQueryId,
    u32 lx32EntityTypeFlags,
    u8 lxVolumeTypeFlags,
    Vector3 lLineStart,
    Vector3 lLineEnd,
    EntityId lExcludeEntityId,
    EExclusionMode leExclusionMode)
{
    static const char KAC_ARTIST_FILE[] =
        "..\\..\\..\\GameShared\\GameClasses\\SceneManager/CgsSceneManagerModuleIO.h";

    CGS_ASSERT_AT(leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY,
               "leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY",
                                     KAC_ARTIST_FILE, 995);
    CGS_ASSERT_AT(mpFineLineTestQueue,
               "mpFineLineTestQueue",
                                     KAC_ARTIST_FILE, 996);

    InEventLineTestFine lEvent;
    lEvent.mQueryId = lQueryId;
    lEvent.mx32EntityTypeFlags = lx32EntityTypeFlags;
    lEvent.mLineStart = lLineStart;
    lEvent.mLineEnd = lLineEnd;
    lEvent.mxVolumeTypeFlags = lxVolumeTypeFlags;
    lEvent.mExcludeEntityId = lExcludeEntityId;
    lEvent.meExclusionMode = leExclusionMode;
    mpFineLineTestQueue->AddEvent(lEvent);
}

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::VolumeTestDeepest  @ 0x822170B0
//
// DecFIGS CgsSceneManagerModuleIO.h:390 and the ARTIST caller at 0x82233128.
void CgsSceneManager::SceneManagerIO::SceneQueryInterface::VolumeTestDeepest(
    SceneQueryId lQueryId,
    u32 lx32EntityTypeFlags,
    u8 lxVolumeTypeFlags,
    const rw::collision::Volume* lpVolume,
    const Matrix44Affine& lrTransform,
    EntityId lExcludeEntityId,
    EExclusionMode leExclusionMode)
{
    static const char KAC_ARTIST_FILE[] =
        "..\\..\\..\\GameShared\\GameClasses\\SceneManager/CgsSceneManagerModuleIO.h";

    CGS_ASSERT_AT(leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY,
               "leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY",
                                     KAC_ARTIST_FILE, 1094);
    CGS_ASSERT_AT(mpFineVolumeTestDeepestQueue,
               "mpFineVolumeTestDeepestQueue", KAC_ARTIST_FILE, 1095);

    InEventVolumeTestDeepest lEvent;
    lEvent.mQueryId = lQueryId;
    lEvent.mx32EntityTypeFlags = lx32EntityTypeFlags;
    lEvent.mTransform = lrTransform;
    lEvent.mxVolumeTypeFlags = lxVolumeTypeFlags;
    lEvent.mExcludeEntityId = lExcludeEntityId;
    lEvent.meExclusionMode = leExclusionMode;
    // DecFIGS volume.h:39 aliases the SDK Volume type to VolRef::Volume.
    lEvent.mVolumeBuffer.SetVolume(reinterpret_cast<const VolRef::Volume*>(lpVolume));
    mpFineVolumeTestDeepestQueue->AddEvent(lEvent);
}

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::Append  @ 0x823C4FF8
// (DWARF CgsSceneManagerModuleIO.h:405; added 2026-09-25, FX-DIRECTOR2)
//
// r3 = this (-> r30, the DESTINATION table), r4 = &lOther (-> r29, the SOURCE table). Six blocks,
// one per fine query queue, in exactly this order (0x823C500C / 505C / 50A4 / 50EC / 5134 / 517C):
//     if (!dest[i] && src[i]) assert("mpX || !lOther.mpX", CgsSceneManagerModuleIO.h:<line>);
//     if (src[i])             BaseEventQueue<T>::Append(dest[i], *src[i]);
// where i walks the slots +0x00 (LineTestFine, :1138 = 0x472), +0x14 (VolumeTestFine, :1144),
// +0x08 (LineTestFastDoubleSided, :1150), +0x0C (SphereTestFast, :1156), +0x04 (LineTestNearest,
// :1162) and +0x10 (VolumeTestDeepest, :1168). The asserts are non-gating tripwires: when a source
// queue exists and the destination does not, the console still calls Append on the null
// destination, and so does this body. The triangle-collision slots (+0x18..+0x20) are not touched.
// Each Append is the inline generic BaseEventQueue<T>::Append (CgsBaseEventQueue.h); the six
// X360 instantiations are 0x823C1FA0 / 0x823C2410 / 0x823C2160 / 0x823C2240 / 0x823C2080 /
// 0x823C2330.
// -----------------------------------------------------------------------------
void CgsSceneManager::SceneManagerIO::SceneQueryInterface::Append(const SceneQueryInterface& lOther)
{
    const char* const lpcAssertFile =
        "..\\..\\..\\GameShared\\GameClasses\\SceneManager/CgsSceneManagerModuleIO.h";

    CGS_ASSERT_AT(mpFineLineTestQueue || !lOther.mpFineLineTestQueue,
        "mpFineLineTestQueue || !lOther.mpFineLineTestQueue",
            lpcAssertFile, 1138 /* ARTIST 0x823C5034 */);
    if (lOther.mpFineLineTestQueue)
        mpFineLineTestQueue->Append(*lOther.mpFineLineTestQueue);

    CGS_ASSERT_AT(mpFineVolumeTestQueue || !lOther.mpFineVolumeTestQueue,
        "mpFineVolumeTestQueue || !lOther.mpFineVolumeTestQueue",
            lpcAssertFile, 1144 /* ARTIST 0x823C507C */);
    if (lOther.mpFineVolumeTestQueue)
        mpFineVolumeTestQueue->Append(*lOther.mpFineVolumeTestQueue);

    CGS_ASSERT_AT(mpFineLineTestFastDoubleSidedQueue || !lOther.mpFineLineTestFastDoubleSidedQueue,
        "mpFineLineTestFastDoubleSidedQueue || !lOther.mpFineLineTestFastDoubleSidedQueue",
            lpcAssertFile, 1150 /* ARTIST 0x823C50C4 */);
    if (lOther.mpFineLineTestFastDoubleSidedQueue)
        mpFineLineTestFastDoubleSidedQueue->Append(*lOther.mpFineLineTestFastDoubleSidedQueue);

    CGS_ASSERT_AT(mpFineSphereTestFastQueue || !lOther.mpFineSphereTestFastQueue,
        "mpFineSphereTestFastQueue || !lOther.mpFineSphereTestFastQueue",
            lpcAssertFile, 1156 /* ARTIST 0x823C510C */);
    if (lOther.mpFineSphereTestFastQueue)
        mpFineSphereTestFastQueue->Append(*lOther.mpFineSphereTestFastQueue);

    CGS_ASSERT_AT(mpFineLineTestNearestQueue || !lOther.mpFineLineTestNearestQueue,
        "mpFineLineTestNearestQueue || !lOther.mpFineLineTestNearestQueue",
            lpcAssertFile, 1162 /* ARTIST 0x823C5154 */);
    if (lOther.mpFineLineTestNearestQueue)
        mpFineLineTestNearestQueue->Append(*lOther.mpFineLineTestNearestQueue);

    CGS_ASSERT_AT(mpFineVolumeTestDeepestQueue || !lOther.mpFineVolumeTestDeepestQueue,
        "mpFineVolumeTestDeepestQueue || !lOther.mpFineVolumeTestDeepestQueue",
            lpcAssertFile, 1168 /* ARTIST 0x823C519C */);
    if (lOther.mpFineVolumeTestDeepestQueue)
        mpFineVolumeTestDeepestQueue->Append(*lOther.mpFineVolumeTestDeepestQueue);
}
