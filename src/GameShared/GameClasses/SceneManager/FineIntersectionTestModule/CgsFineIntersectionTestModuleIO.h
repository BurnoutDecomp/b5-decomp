#pragma once

// CgsSceneManager::FineIntersectionTestIO -- the FineIntersectionTestModule's IO vocabulary:
// the four fine-query INPUT records the SceneManagerModule's fine-query dispatchers build on
// the stack for the module's Compute* entry points, the four OUTPUT records those entry points
// fill, and the per-pass OutputBuffer ProcessFineQueries stacks ("FineTest").
//
// DWARF home: CgsFineIntersectionTestModuleIO.h (:66..:197). Every struct below is the DWARF's
// member list verbatim; the console offsets quoted are pinned by the X360 producers/consumers:
//
//   InEventLineTestNearest (:80) -- built by SceneManagerModule::ProcessLineTestNearest
//     @0x828D3AD8..0x828D3B1C and READ by FineIntersectionTestModule::ComputeLineTestNearest
//     @0x828C8CC8: +0x00 mLineStart (lvx128), +0x10 mLineEnd, +0x20 mQueryId (lwz 0x20),
//     +0x24 mpau16EntityIndices (lwz 0x24), +0x28 mu16NumEntities (lhz 0x28),
//     +0x2A mu16ExcludeEntityIndex (lhz 0x2A), +0x2C mxVolumeTypeFlags (lbz 0x2C),
//     +0x2D mbExcludeParts (lbz 0x2D). InEventLineTestFine (:66) is the identical field set
//     (ProcessLineTestFine / ComputeLineTestFine @0x828C7D70).
//   OutEventLineTestNearestResult (:135) -- WRITTEN by ComputeLineTestNearest: +0x00 mQueryId
//     (stw 0(r30)), +0x04 muVolumeInstanceIndex (stw 4(r30)), +0x10 mPosition (stvx128 +0x10),
//     +0x20 mNormal (stvx128 +0x20), +0x30 mfLineParam (stfs 0x30), +0x34 mu16EntityIndex
//     (sth 0x34), +0x36 mu16MaterialTag (sth 0x36), +0x38 mu16GroupTag (sth 0x38),
//     +0x3A mbIntersection (stb 0x3A) -- and READ back by ProcessLineTestNearest
//     (lbz 0x3A / lwz 4 / lfs 0x30 / lhz 0x34,0x36,0x38 / lvx 0x10,0x20 @0x828D3B24..0x828D3D9C).
//   OutputBuffer (:176) -- CreateIOBuffer<OutputBuffer> @0x828C56B8 allocates 49200 bytes and
//     writes `*p = 1` (IOBuffer status), `*(p+16400) = 0` and `*(p+49184) = 0`: the two
//     CgsContainers::Array counts, which sit AFTER their element arrays (Array<T,N> is
//     {T maElements[N]; s32 miCount;} -- CgsArray.h): mLineTestIntersectionArray at +16
//     (256 x 64-byte LineTestIntersection -> count at 16+16384 == 16400) and mEntityBuffer at
//     +16416 (16384 x u16 -> count at 16416+32768 == 49184). The write-locked GetEntityBuffer()
//     @0x828B0A88 returns +0x4020 == 16416, i.e. exactly that second array.
//     (The previous slice here called that getter "GetResults()" over an opaque store; the DWARF
//     names both arrays, so the slice is RETIRED -- scene-query wave 1, 2026-09-02.)

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Containers/CgsArray.h"                 // Array<T,N>
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                  // CgsModule::IOBuffer
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"        // SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerTypes.h"   // LineTestIntersection
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"       // VolumeManagerVolume::VolumeTypeFlags
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"         // VolumeSlot (the 128-byte volume image)

namespace CgsSceneManager
{
namespace FineIntersectionTestIO
{
    // Empty per-module event base (CgsModule event-queue convention). Distinctly named so it
    // never ODR-clashes with the SceneManagerIO::Event family.
    struct EventBaseFineIntersectionTest {};

    // ---- INPUT records (:66 / :80 / :94 / :107) --------------------------------------
    // The line-test pair carry two 16-byte lanes, so alignas(16) and sizeof 0x40 (the console
    // builds them at 16-aligned stack slots: `addi r4, r1, 0x60` in ProcessLineTestNearest).
    struct alignas(16) InEventLineTestFine : public EventBaseFineIntersectionTest
    {
        Vector3                              mLineStart;              // :69  +0x00
        Vector3                              mLineEnd;                // :70  +0x10
        SceneQueryId                         mQueryId;                // :71  +0x20
        const u16*                           mpau16EntityIndices;     // :72  +0x24 (console 4-byte ptr)
        u16                                  mu16NumEntities;         // :73  +0x28
        u16                                  mu16ExcludeEntityIndex;  // :74  +0x2A
        VolumeManagerVolume::VolumeTypeFlags mxVolumeTypeFlags;       // :75  +0x2C
        bool                                 mbExcludeParts;          // :76  +0x2D
    };

    struct alignas(16) InEventLineTestNearest : public EventBaseFineIntersectionTest
    {
        Vector3                              mLineStart;              // :83  +0x00
        Vector3                              mLineEnd;                // :84  +0x10
        SceneQueryId                         mQueryId;                // :85  +0x20
        const u16*                           mpau16EntityIndices;     // :86  +0x24 (console 4-byte ptr)
        u16                                  mu16NumEntities;         // :87  +0x28
        u16                                  mu16ExcludeEntityIndex;  // :88  +0x2A
        VolumeManagerVolume::VolumeTypeFlags mxVolumeTypeFlags;       // :89  +0x2C
        bool                                 mbExcludeParts;          // :90  +0x2D
    };

    struct alignas(16) InEventVolumeTestDeepest : public EventBaseFineIntersectionTest
    {
        Matrix44Affine                       mTransform;              // :96  +0x00 (4 lanes)
        VolumeSlot                           mVolumeBuffer;           // :97  +0x40 (128 bytes)
        SceneQueryId                         mQueryId;                // :98  +0xC0
        const u16*                           mpau16EntityIndices;     // :99  +0xC4
        u16                                  mu16NumEntities;         // :100 +0xC8
        u16                                  mu16ExcludeEntityIndex;  // :101 +0xCA
        VolumeManagerVolume::VolumeTypeFlags mxVolumeTypeFlags;       // :102 +0xCC
        bool                                 mbExcludeParts;          // :103 +0xCD
    };

    struct alignas(16) InEventVolumeTestFine : public EventBaseFineIntersectionTest
    {
        Matrix44Affine                       mTransform;              // :109 +0x00
        VolumeSlot                           mVolumeBuffer;           // :110 +0x40
        SceneQueryId                         mQueryId;                // :111 +0xC0
        const u16*                           mpau16EntityIndices;     // :112 +0xC4
        u16                                  mu16NumEntities;         // :113 +0xC8
        u16                                  mu16ExcludeEntityIndex;  // :114 +0xCA
        VolumeManagerVolume::VolumeTypeFlags mxVolumeTypeFlags;       // :115 +0xCC
        bool                                 mbExcludeParts;          // :116 +0xCD
    };

    // ---- OUTPUT records (:127 / :135 / :149 / :157) ----------------------------------
    struct OutEventLineTestFineResult : public EventBaseFineIntersectionTest
    {
        SceneQueryId                mQueryId;      // :129 +0x00
        s32                         miNumResults;  // :130 +0x04
        const LineTestIntersection* mpaResults;    // :131 +0x08
    };

    struct alignas(16) OutEventLineTestNearestResult : public EventBaseFineIntersectionTest
    {
        SceneQueryId mQueryId;                // :137 +0x00
        u32          muVolumeInstanceIndex;   // :138 +0x04
        Vector3      mPosition;               // :139 +0x10
        Vector3      mNormal;                 // :140 +0x20
        f32          mfLineParam;             // :141 +0x30
        u16          mu16EntityIndex;         // :142 +0x34
        u16          mu16MaterialTag;         // :143 +0x36
        u16          mu16GroupTag;            // :144 +0x38
        bool         mbIntersection;          // :145 +0x3A
    };

    struct OutEventVolumeTestDeepestResult : public EventBaseFineIntersectionTest
    {
        SceneQueryId mQueryId;        // :151 +0x00
        f32          mfDepth;         // :152 +0x04
        bool         mbIntersection;  // :153 +0x08
    };

    struct OutEventVolumeTestFineResult : public EventBaseFineIntersectionTest
    {
        SceneQueryId mQueryId;       // :159 +0x00
        s32          miNumEntities;  // :160 +0x04
        const u16*   mpuResults;     // :161 +0x08
    };

    // ---- the per-pass output buffer (:176) ----------------------------------------------
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef Array<LineTestIntersection, 256u> LineTestIntersectionArray;  // :164
        typedef Array<u16, 16384u>                EntityBuffer;               // :165

        // :181 -- inlined into CreateIOBuffer<OutputBuffer> @0x828C56B8: the status byte and the
        // two Array counts (`*(p+16400) = 0 ; *(p+49184) = 0`), i.e. the two Array::Constructs.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mLineTestIntersectionArray.Construct();
            mEntityBuffer.Construct();
        }
        // :185 -- DestroyIOBuffer<OutputBuffer> @0x828C5880 bl's CgsModule::IOBuffer::Destruct.
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // :188 / :189 -- read-locked; :191 / :192 -- write-locked. Only :192 is emitted out of
        // line on the console (@0x828B0A88, returns +0x4020, asserts "Not locked for writing");
        // the other three follow the identical IOBuffer-getter shape. Bodied in the .cpp.
        const LineTestIntersectionArray* GetLineTestIntersectionArray() const;  // :188
        const EntityBuffer*              GetEntityBuffer() const;               // :189
        LineTestIntersectionArray*       GetLineTestIntersectionArray();        // :191
        EntityBuffer*                    GetEntityBuffer();                     // :192  @0x828B0A88

    private:
        LineTestIntersectionArray mLineTestIntersectionArray;  // :196  console +16
        EntityBuffer              mEntityBuffer;               // :197  console +16416 (0x4020)
    };
}
}
