#pragma once

// rw::collision::VolumeBBoxQuery — the bounding-box overlap query object: it
// consumes a list of input volumes (walking aggregate volumes through a
// VolRef traversal stack) and stages every primitive whose AABB overlaps the
// query box into a VolRef result buffer. VolumeVolumeQuery embeds two of
// them (one per query side); the CgsSceneManager debug component owns a
// standalone one.
//
// Ground truth (X360 BURNOUT_X360_ARTIST.XEX):
//   rw::collision::VolumeBBoxQuery::AddVolumeRef           @ 0x82BBBC20
//   rw::collision::VolumeBBoxQuery::GetResourceDescriptor  @ 0x82BBBD38
//   rw::collision::VolumeBBoxQuery::Initialize             @ 0x82BBBD90
//   rw::collision::VolumeBBoxQuery::GetOverlaps            @ 0x82BBBDE8
//
// MEMBER BLOCK: canonical, from the DecFIGS DWARF (references/DecFIGS/
// dwarfdump/SDKs/EATech/include/cmn/rw/collision/volumebboxquery.h:287-318).
// Every console offset in the comments is attested by the wave-2 bodies
// (field-by-field priming by VolumeVolumeQuery::GetPrimitiveBBoxOverlaps
// @ 0x82BB3AB0 + the loads/stores of GetOverlaps/AddVolumeRef/Initialize).
// Pointers widen on x64 exactly as the committed GPInstance precedent;
// access is by name.
//
// This header lives apart from VolumeQuery.hpp on purpose: the by-value
// AABBox / VolRef embeds pull the SDKs/EATech vpu math headers, which must
// not reach the CgsSceneManager consumers of VolumeQuery.hpp (they carry the
// BrnCommonTypes vpu vocabulary).

#include "types.hpp"
#include "vendor/renderware/collision/AABBox.hpp"        // AABBox (embedded m_aabb)
#include "vendor/renderware/collision/FeatureEdge.hpp"   // RwBool
#include "vendor/renderware/collision/VolRef.hpp"        // VolRef (embedded m_currVRef)

namespace rw
{
namespace collision
{
    struct Volume;   // CollisionVolume.hpp

    class VolumeBBoxQuery
    {
    public:
        // --- static factory entry points (called with no implicit `this`) ---

        // @ 0x82BBBD38 -- fills the 5-entry rw::ResourceDescriptor block at
        // lpOut (lpOut[0] = total backing size, lpOut[1] = 16). Canonical
        // param meaning per DWARF volumebboxquery.h:179 / rwccore.h:2866:
        // (stackMax, resBufferSize).
        static void* GetResourceDescriptor(void* lpOut, int liStackMax, int liResBufferSize);

        // @ 0x82BBBD90 -- partitions the backing buffer (*lppBuffer at [0])
        // into the query's working regions and returns the query handle (the
        // buffer base interpreted as the query object). Null base -> 0.
        static void* Initialize(void** lppBuffer, int liStackMax, int liResBufferSize);

        // @ 0x82BBBDE8 -- the driver loop: consume the remaining input
        // volumes and the aggregate-traversal stack until both are empty or
        // a result buffer fills up (the query is resumable). Returns
        // m_primNext, the number of primitive refs staged this call.
        // (Console instance method, `this` in r3; kept in the committed
        // static-with-explicit-this shape its call sites already use.)
        static int GetOverlaps(VolumeBBoxQuery* lpThis);

        // --- instance entry points (this in r3) -----------------------------

        // @ 0x82BBBC20 -- stage a volume reference; aggregates go on the
        // traversal stack (a 0x80-stride VolRef record with the transform
        // rows cached inline), primitives divert to AddPrimitiveRef. Returns
        // 0 when the stack is full.
        RwBool AddVolumeRef(const Volume* lpVol,
                            const math::vpu::Matrix44Affine* lpTm,
                            const AABBox& arBBox, u32 auTag, u8 auNumTagBits);

        // @ 0x82BB0478 -- stage a primitive reference in m_primVRefBuffer;
        // returns 0 when the result buffer is full. Canonical inline body
        // rwccore.h:2796-2818. HOMED 2026-08-18 (waveQ5 C1) in
        // VolumeBBoxQuery.cpp: the address has no per-address IDA export
        // (an export hole), which is why it read as "cross-TU, un-homed".
        RwBool AddPrimitiveRef(const Volume* lpVol,
                               const math::vpu::Matrix44Affine* lpTm,
                               const AABBox& arBBox, u32 auTag, u8 auNumTagBits);

        // --- members (DWARF volumebboxquery.h:287-318; console offsets) -----
        const Volume**                    m_inputVols;   // +0x00  input volume array
        const math::vpu::Matrix44Affine** m_inputMats;   // +0x04  parallel matrix array (may be null)
        u32            m_numInputs;             // +0x08
        u32            m_currInput;             // +0x0C
        AABBox         m_aabb;                  // +0x10  the query box (0x20 bytes)
        VolRef*        m_stackVRefBuffer;       // +0x30  aggregate traversal stack
        VolRef         m_currVRef;              // +0x40  current ref (16-aligned, 0x80)
        u32            m_stackNext;             // +0xC0
        u32            m_stackMax;              // +0xC4
        VolRef*        m_primVRefBuffer;        // +0xC8  primitive result records
        u32            m_primNext;              // +0xCC
        u32            m_primBufferSize;        // +0xD0
        Volume*        m_instVolPool;           // +0xD4  instanced-volume pool
        u32            m_instVolCount;          // +0xD8
        u32            m_instVolMax;            // +0xDC
        u32            m_aggIndex;              // +0xE0  resumable aggregate cursor
        void*          m_spatialMapQueryMem;    // +0xE4  spatial-map query workspace
        void*          m_curSpatialMapQuery;    // +0xE8  live spatial-map query (resumable)
        u32            m_tag;                   // +0xEC  current tag context
        u8             m_numTagBits;            // +0xF0
    };
}
}
