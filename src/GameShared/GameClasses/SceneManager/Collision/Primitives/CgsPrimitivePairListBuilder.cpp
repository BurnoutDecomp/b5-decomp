#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h"

#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc::Malloc / GetAlignment
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([DIAG] block only)
#include "SDKs/EATech/rwcollision/volume_debug_access.h"     // rw::collision::Volume + EVolumeType

#include <cmath>     // std::sqrt
#include <stdlib.h>  // [DIAG] getenv -- BRN_PROP_DIAG latch only

// CgsSceneManager::CgsCollision::PrimitivePairListBuilder — the writer bodies. Construct/Prepare
// bring up the packed-pair blob; AddCollisionHeader stamps the 16-byte record header; AddPrimitive /
// AddPrimitivePair bump-allocate the primitive payload(s), copy the caller's data in verbatim, and
// bump the record count. AllocateMemory is the bump allocator every append draws from.

namespace CgsSceneManager
{
namespace CgsCollision
{
    // -------------------------------------------------------------------------
    // PrimitivePairListBuilder::Construct @ 0x82814330
    //
    // Zero the four inherited PrimitivePairList bookkeeping members. The asm
    // materialises 0 once (li r11,0) and stores it as a word at +0x00 and as
    // halfwords at +0x04/+0x06/+0x08.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::Construct()
    {
        mpaDataStream   = nullptr;   // +0x00  (stw of 0)
        mu16UsedData    = 0;         // +0x04  (sth of 0)
        mu16NumTests    = 0;         // +0x06  (sth of 0)
        mu16MaxDataSize = 0;         // +0x08  (sth of 0)
    }

    // -------------------------------------------------------------------------
    // PrimitivePairListBuilder::Prepare @ 0x82814348
    //
    // Reserve a packed-pair blob big enough for lu16NumPrimitives pair records:
    // each record is a fixed 96 bytes (16-byte CollisionHeader + up to five
    // 16-byte primitive slots, the box/box worst case). The asm computes
    // 96 * count via ((count * 3) << 5) and bump-allocates it from lpMalloc.
    // -------------------------------------------------------------------------
    bool PrimitivePairListBuilder::Prepare(CgsMemory::LinearMalloc* lpMalloc, u16 lu16NumPrimitives)
    {
        const s32 liRequiredMemory = 96 * lu16NumPrimitives;

        u8* lpMemory = static_cast<u8*>(lpMalloc->Malloc(liRequiredMemory));
        CGS_ASSERT(lpMemory != nullptr, "lpMemory");
        CGS_ASSERT(lpMalloc->GetAlignment() >= 16, "lpMalloc->GetAlignment() >= 16");

        mpaDataStream   = lpMemory;
        mu16MaxDataSize = static_cast<u16>(liRequiredMemory);
        mu16UsedData    = 0;
        mu16NumTests    = 0;

        return true;
    }

    // -------------------------------------------------------------------------
    // AllocateMemory @ 0x82812098
    //
    // Bump allocator over the base list's data stream. Returns the current write
    // cursor (mpaDataStream + mu16UsedData) and advances mu16UsedData by liBytes.
    // The X360 asserts the stream pointer is non-null and that the post-bump used
    // count does not exceed the capacity (a signed 32-bit compare of the zero-
    // extended halfword used-count + liBytes against the halfword capacity).
    // -------------------------------------------------------------------------
    void* PrimitivePairListBuilder::AllocateMemory(s32 liBytes)
    {
        CGS_ASSERT(mpaDataStream != NULL, "mpaDataStream != NULL");
        CGS_ASSERT(static_cast<s32>(mu16UsedData) + liBytes <= static_cast<s32>(mu16MaxDataSize),
                   "Trying to use too much memory");

        void* lpMemory = mpaDataStream + mu16UsedData;
        mu16UsedData = static_cast<u16>(mu16UsedData + liBytes);
        return lpMemory;
    }

    // -------------------------------------------------------------------------
    // AddCollisionHeader(EVolumeType, f32, u16) @ 0x828143F0
    //
    // Bump-allocate a 16-byte CollisionHeader for a single-primitive record and
    // stamp it. The X360 sets both type bytes to lePrimType, header-type 0, the
    // checksum byte to 2*lePrimType (== A + B + HeaderType), primitive-A data size
    // to KAU16_VOLUME_SIZES[lePrimType], primitive-B data size 0, tag A to the
    // caller's tag and tag B to 0xFFFF.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddCollisionHeader(EVolumeType lePrimType,
                                                      f32 lfPadding,
                                                      u16 lu16PrimitiveTag)
    {
        CollisionHeader* lpHeader =
            static_cast<CollisionHeader*>(AllocateMemory(sizeof(CollisionHeader)));

        const u8 lu8Type = static_cast<u8>(lePrimType);

        lpHeader->mfPadding                 = lfPadding;
        lpHeader->mu8PrimTypeA              = lu8Type;
        lpHeader->mu8PrimTypeB              = lu8Type;
        lpHeader->mu8HeaderType             = 0;
        lpHeader->mu16PrimBDataSize         = 0;
        lpHeader->mu16PrimADataSize         = KAU16_VOLUME_SIZES[lu8Type];
        lpHeader->mu16PrimitiveATag         = lu16PrimitiveTag;
        lpHeader->mu16PrimitiveBTag         = 0xFFFF;
        // checksum byte == mu8PrimTypeA + mu8PrimTypeB + mu8HeaderType == 2*type
        lpHeader->mu8DataPaddingAndCheckSum = static_cast<u8>(lu8Type << 1);
    }

    // -------------------------------------------------------------------------
    // AddCollisionHeader(EVolumeType, EVolumeType, f32, u16, u16) @ 0x82814480
    //
    // ⭐ BODIED 2026-08-06 (big-five #2 wave; it was declared "defined in its own
    // reconstruction" and nothing defined it -- found by the LNK2019 when this TU
    // mounted). Bump-allocate a 16-byte CollisionHeader for a two-primitive PAIR
    // record and stamp it, asm-exact @0x82814480: both type bytes, header-type 1,
    // per-type packed sizes from KAU16_VOLUME_SIZES, both caller tags, checksum
    // byte == typeA + typeB + 1 (== A + B + HeaderType).
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddCollisionHeader(EVolumeType lePrimTypeA,
                                                      EVolumeType lePrimTypeB,
                                                      f32 lfPadding,
                                                      u16 lu16PrimitiveTagA,
                                                      u16 lu16PrimitiveTagB)
    {
        CollisionHeader* lpHeader =
            static_cast<CollisionHeader*>(AllocateMemory(sizeof(CollisionHeader)));

        const u8 lu8TypeA = static_cast<u8>(lePrimTypeA);
        const u8 lu8TypeB = static_cast<u8>(lePrimTypeB);

        lpHeader->mu8PrimTypeA              = lu8TypeA;
        lpHeader->mu8PrimTypeB              = lu8TypeB;
        lpHeader->mu8HeaderType             = 1;
        lpHeader->mu16PrimADataSize         = KAU16_VOLUME_SIZES[lu8TypeA];
        lpHeader->mu16PrimBDataSize         = KAU16_VOLUME_SIZES[lu8TypeB];
        lpHeader->mfPadding                 = lfPadding;
        lpHeader->mu16PrimitiveATag         = lu16PrimitiveTagA;
        lpHeader->mu16PrimitiveBTag         = lu16PrimitiveTagB;
        lpHeader->mu8DataPaddingAndCheckSum = static_cast<u8>(lu8TypeA + lu8TypeB + 1);
    }

    // =========================================================================
    // AddPrimitive(const rw::collision::Volume*, Matrix44Affine, f32, u16) @0x82814AB8
    // (140 insns, 0x82814AB8..0x82814CE4)
    //
    // ⭐⭐ BODIED 2026-08-19 (wave Q6 round 2, cluster `addprim`). This REPLACES the loud
    // conductor gate that stood here from 2026-08-19 round 1 -- the gate was the sole
    // definition, so there is no second definition to retire and no LNK2005 risk.
    //
    // WHY IT MATTERS AT RUNTIME: this is the function that turns a prop's (or a part's)
    // rwcollision volumes into packed CgsGeometric collision primitives. While it was a
    // gate the pair list every prop handed the contact generator was EMPTY, so a smashed
    // prop's parts had nothing to collide the world with and fell forever
    // ("Warning!! prop fell out of the world", PropManager_wQ2_01.cpp:930).
    //
    // ITS ONLY TWO CALLERS in the whole image are PropManager::DoPartWorldContactGeneration
    // (bl @0x82611F44) and ::DoPropInstanceWorldContactGeneration (bl @0x826124A0) --
    // measured xrefs; both were landed in round 1 in PropManager_wQ2_03.cpp.
    //
    // SIGNATURE, off the prologue (raw `assembly`, 0x82814AB8.json):
    //     r3 = this        (`mr r31, r3` @0x82814AD8)
    //     r4 = the volume  (`mr r11, r4` @0x82814AD0)
    //     r5 = the transform -- a by-value Matrix44Affine riding a hidden pointer; every
    //          arm reads it as four 16-byte rows at +0x00/+0x10/+0x20/+0x30
    //     f1 = the padding (`fmr f31, f1` @0x82814AD4, saved because the arms clobber f1)
    //     r7 = the tag     (`mr r30, r7` @0x82814ADC, forwarded as r6 to every leaf)
    // ⚠️ GOTCHA 3: the f32 rides f1 AND consumes r6's GPR slot, so the u16 tag lands in
    // r7. FOUR parameters, not five -- a signature read off the GPRs alone would invent a
    // dead fifth.
    //
    // THE DISPATCH (`lwz r10,0x40(volume)` -> the per-TYPE descriptor, `lwz r10,0(r10)`
    // -> its typeID, `addi r10,r10,-1`, `cmplwi r10,4`, `bgt default`, then the 5-entry
    // jump table jpt_82814B08). The table was READ OUT OF THE IMAGE on a private .i64 copy
    // twice (round 1 and again this wave, scratchpad/waveQ6/ida_addprim/out.json):
    //     jpt_82814B08 = { 0x82814C04, 0x82814B88, 0x82814CAC, 0x82814B20, 0x82814C48 }
    // indexed by typeID-1, i.e.
    //     1 SPHERE   -> 0x82814C04 -> AddPrimitive(Sphere*)   @0x82814508
    //     2 CAPSULE  -> 0x82814B88 -> AddPrimitive(Capsule*)  sub_82814600
    //     3 TRIANGLE -> 0x82814CAC == THE DEFAULT LABEL: the console REFUSES it
    //     4 BOX      -> 0x82814B20 -> Box::Set @0x825E6918 then AddPrimitive(Box*)
    //                                 sub_82814570
    //     5 CYLINDER -> 0x82814C48 -> AddPrimitive(Cylinder*) sub_82814678
    // typeID 0 (NULL) and 6 (AGGREGATE) take the `bgt` out to the same default label --
    // 0-1 wraps to 0xFFFFFFFF and 6-1 == 5, both > 4 unsigned. So EVERY volume kind the
    // console has is covered here: four build a primitive, everything else asserts. No arm
    // is silently omitted.
    //
    // THE VOLUME FIELDS EACH ARM READS (console offsets; the names are
    // SDKs/EATech/rwcollision/volume_debug_access.h's, whose layout is the DWARF's):
    //     +0x00..+0x3F  the volume's own relative transform -- NOT used here; every arm
    //                   uses the CALLER's lTransform (the volume's frame times the prop's
    //                   pose, already composed by the two callers)
    //     +0x44         the 12-byte type-specific union: BoxSpecificData{hx,hy,hz} for
    //                   BOX; its x lane doubles as the capsule/cylinder half-height
    //     +0x50         float32_t radius (sphere / capsule / cylinder)
    // =========================================================================
    void PrimitivePairListBuilder::AddPrimitive(const ::rw::collision::Volume* lpVolume,
                                                Matrix44Affine lTransform,
                                                f32 lfPadding,
                                                u16 lu16PrimitiveTag)
    {
        // `lwz r10,0x40(volume) ; lwz r10,0(r10)` -- the per-TYPE descriptor's leading
        // word IS its typeID. On this host the +0x40 slot holds the 4-byte type enum and
        // Volume::GetVTable() recovers the console's descriptor as gVolumeVTable[enum]
        // (volume_debug_access.h's documented host representation), so GetType() is the
        // exact same two loads.
        const u32 luTypeID = lpVolume->GetType();

        // [DIAG] NOT IN THE X360 BINARY. Opt-in one-shot (set BRN_PROP_DIAG): the first
        // volume this builder is ever asked to convert, and which arm it took.
        // HOW TO READ IT (re-measured 2026-08-19, wave Q6 round 2 -- the earlier text here
        // named the wrong TUs and the wrong bat lines):
        //   * The two prop-vs-world legs that call this -- DoPartWorldContactGeneration
        //     (bl @0x82611F44) and DoPropInstanceWorldContactGeneration (bl @0x826124A0) --
        //     are BODIED in PropManager_wQ2_03.cpp (MOUNTED, bat:1810), and their
        //     dispatcher PropManager_wQ2_02.cpp is MOUNTED too as of this integration
        //     (bat:1816, paired with PropManager_wQ_03.cpp at bat:1817). So the whole chain
        //     into this function is live and the line IS EXPECTED TO FIRE on the first boot
        //     that reaches a prop.
        //   * A SILENT log therefore means the callers never ran (no prop/part event
        //     reached BeginPropWorldContactGeneration), NOT that this switch is broken --
        //     a different failure, and the two are easy to confuse.
        //   * ⭐ THE DOWNSTREAM GATE IS GONE (this block used to say it survived).
        //     ContactGeneratorJob::ExecutePrimitiveListWithTriangleList @0x82925908 (849) has
        //     been REAL since wave Q6 round 3 (2026-08-19) -- the body lives in
        //     ContactGeneratorJob.cpp, and the two workers it delegates to (::BuildGPInstance
        //     @0x829222A0 and ::CollideGPInstances @0x829253C8, in the sibling partfile
        //     ContactGeneratorJob_wQ6_01.cpp) landed with it. A correctly
        //     built pair list now yields real contacts, so "prop fell out of the world" after
        //     this line means something else.
        // The getenv latch is evaluated once; a per-call getenv would be a syscall on the
        // contact-generation path.
        {
            static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
            static bool       sbLoggedFirstVolume = false;
            if (sbPropDiag && !sbLoggedFirstVolume && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedFirstVolume = true;
                *CgsDev::Log::gpDebugPrint
                    << "[Q6-addprim] first rw::collision::Volume converted: typeID "
                    << luTypeID << " (1=sphere 2=capsule 3=triangle/UNSUPPORTED 4=box "
                       "5=cylinder), tag " << static_cast<u32>(lu16PrimitiveTag) << "\n";
            }
        }

        switch (luTypeID)
        {
        // -----------------------------------------------------------------------------
        // case 1 SPHERE -- loc_82814C04. Build a packed 16-byte sphere from the caller's
        // transform position and the volume's radius, then append it.
        //     0x82814C1C  lvx128 v13,[sphere]            read the (uninitialised) local
        //     0x82814C24  lvlx   v0,[volume+0x50]        the radius
        //     0x82814C2C  vspltw v0,v0,0                 broadcast it  (a VecFloat)
        //     0x82814C30  lvx128 v12,[transform+0x30]    the centre row
        //     0x82814C34  vrlimi128 v12,v13,1,0          keep the old w  -> SetPosition
        //     0x82814C38  vrlimi128 v12,v0,1,0           w := radius     -> SetRadius
        //     0x82814C3C  stvx128 v12,[sphere]
        //     0x82814C40  bl     AddPrimitive(Sphere*)
        // ⚠️ The two vrlimi pairs are CgsGeometric::Sphere::SetPosition(Vector3) and
        // ::SetRadius(VecFloat) inlined -- both are DWARF-declared (CgsSphere.h:47/:51)
        // header inlines with no standalone X360 body. Neither is DECLARED in this tree's
        // CgsSphere.h yet (it carries only GetPosition/GetRadius, which do have X360
        // bodies), and that header is outside this cluster's ownership, so the two lane
        // writes are spelled directly on the named member here. REPORTED as a follow-up
        // for CgsSphere.h's owner: adding the two DWARF setters would let this arm read
        // like the console does.
        // -----------------------------------------------------------------------------
        case ::rw::collision::E_VOLUMETYPE_SPHERE:
        {
            CgsGeometric::Sphere lSphere;
            lSphere.mPositionRadius.x = lTransform.Pos().x;   // SetPosition(transform.wAxis)
            lSphere.mPositionRadius.y = lTransform.Pos().y;
            lSphere.mPositionRadius.z = lTransform.Pos().z;
            lSphere.mPositionRadius.w = lpVolume->GetRadius();// SetRadius(volume->radius)

            AddPrimitive(&lSphere, lfPadding, lu16PrimitiveTag);
            break;
        }

        // -----------------------------------------------------------------------------
        // case 2 CAPSULE -- loc_82814B88. Build a packed 32-byte capsule: start position
        // from the transform's centre row, axis from its "at" row, radius from the
        // volume's +0x50 and length from the volume's +0x44 half-height lane. The exact
        // store sequence is transcribed in CgsCapsule.h's Set banner.
        // -----------------------------------------------------------------------------
        case ::rw::collision::E_VOLUMETYPE_CAPSULE:
        {
            // `vspltw v12,<volume+0x50>,0` and `vspltw v13,<volume+0x44>,0` -- both
            // scalars are broadcast to all four lanes before the w-lane insert, which is
            // what makes Capsule::Set's two scalars VecFloat and not f32.
            VecFloat lvfRadius;
            lvfRadius.x = lvfRadius.y = lvfRadius.z = lvfRadius.w = lpVolume->GetRadius();

            // The +0x44 union lane: CapsuleSpecificData::mfHalfHeight (volume_debug_access.h
            // names the 12-byte arm GetExtent(); its x lane IS the capsule half-height).
            const f32 lfHalfHeight = lpVolume->GetExtent().x;
            VecFloat lvfLength;
            lvfLength.x = lvfLength.y = lvfLength.z = lvfLength.w = lfHalfHeight;

            CgsGeometric::Capsule lCapsule;
            lCapsule.Set(lTransform.Pos(),   // 0x82814BBC lvx128 v9,[transform+0x30]
                         lTransform.At(),    // 0x82814BB0 lvx128 v10,[transform+0x20]
                         lvfRadius,
                         lvfLength);

            AddPrimitive(&lCapsule, lfPadding, lu16PrimitiveTag);
            break;
        }

        // -----------------------------------------------------------------------------
        // case 4 BOX -- loc_82814B20. Compose the packed 80-byte box through
        // CgsGeometric::Box::Set @0x825E6918 (an out-of-line `bl`, unlike the capsule and
        // cylinder arms which inline their Set), then append it.
        //     0x82814B34  lfs f0,flt_82001CC0        the fatness literal
        //     0x82814B3C  stfs f0,[var_A0]           -> broadcast into v2 at 0x82814B64
        //     0x82814B40..0x82814B54  volume +0x44/+0x48/+0x4C -> three contiguous f32
        //     0x82814B58  stw r8(=0),[var_84]        the Vector3's 4th slot, zeroed
        //     0x82814B68  lvx128 v1,[var_90]         -> the Vector3 dimensions
        //     0x82814B6C  bl CgsGeometric::Box::Set  (r3=&box, r4=&transform, v1, v2)
        //     0x82814B80  bl AddPrimitive(Box*)
        // flt_82001CC0 is 0.0f -- image bytes 00000000, re-dumped from a private .i64 copy
        // this wave (scratchpad/waveQ6/ida_addprim/out.json) and already recorded as 0.0f
        // by a dozen committed TUs. So a collision box built from an rwcollision volume
        // carries NO fatness / collision skin; the skin comes from the pair record's own
        // `lfPadding` field instead.
        // -----------------------------------------------------------------------------
        case ::rw::collision::E_VOLUMETYPE_BBOX:
        {
            // BoxSpecificData{mfHx, mfHy, mfHz} at volume +0x44/+0x48/+0x4C. The asm
            // stages the three floats individually and zeroes the 4th slot, so the
            // dimensions are built lane by lane rather than copied as a vector.
            const Vector3& lrExtent = lpVolume->GetExtent();
            Vector3 lvDimensions;
            lvDimensions.x = lrExtent.x;
            lvDimensions.y = lrExtent.y;
            lvDimensions.z = lrExtent.z;
            lvDimensions.w = 0.0f;              // `stw r8,[var_84]`, r8 == 0

            VecFloat lvfFatness;
            lvfFatness.SetZero();               // flt_82001CC0 == 0.0f, broadcast

            CgsGeometric::Box lBox;
            lBox.Set(lTransform, lvDimensions, lvfFatness);

            AddPrimitive(&lBox, lfPadding, lu16PrimitiveTag);
            break;
        }

        // -----------------------------------------------------------------------------
        // case 5 CYLINDER -- loc_82814C48. Copy the caller's whole transform into the
        // packed 80-byte cylinder and take the two scalars from the volume; the store
        // sequence is transcribed in CgsCylinder.h's Set banner.
        // ⚠️ The volume's +0x44 lane is CylinderSpecificData::mfHalfHeight and it is
        // stored VERBATIM into Cylinder::mfLength -- the console does not double it. The
        // name difference is the console's, not a conversion.
        // -----------------------------------------------------------------------------
        case ::rw::collision::E_VOLUMETYPE_CYLINDER:
        {
            CgsGeometric::Cylinder lCylinder;
            lCylinder.Set(lTransform,
                          lpVolume->GetRadius(),        // 0x82814C48 lfs [volume+0x50]
                          lpVolume->GetExtent().x);     // 0x82814C58 lfs [volume+0x44]

            AddPrimitive(&lCylinder, lfPadding, lu16PrimitiveTag);
            break;
        }

        // -----------------------------------------------------------------------------
        // case 3 TRIANGLE, plus NULL / AGGREGATE / anything out of range -- loc_82814CAC,
        // which the jump table's third entry points at DIRECTLY: the console does not
        // support a triangle volume here and says so. The message string was dumped from
        // the image this wave (aTriedToAddARwV @0x820DB268) rather than left truncated,
        // and the file/line the console passes are
        // "..\\..\\..\\GameShared\\GameClasses\\SceneManager/Collision/Primitives/
        // CgsPrimitivePairListBuilder.cpp" : 0x13F == 319 (aGamesharedGame_287
        // @0x820DAFF0). CGS_ASSERT substitutes the host __FILE__/__LINE__, which is the
        // established house behaviour for every assert in this tree.
        // -----------------------------------------------------------------------------
        case ::rw::collision::E_VOLUMETYPE_TRIANGLE:
        default:
            CGS_ASSERT(false,
                       "Tried to add a RW volume that wasn't a box, capsule, sphere or "
                       "cylinder (only these are supported)");
            break;
        }
    }

    // -------------------------------------------------------------------------
    // AddPrimitive(Sphere*) @ 0x82814508
    //
    // Append a single-sphere record: stamp a sphere CollisionHeader (padding/tag
    // forwarded), bump-allocate 16 bytes for the sphere payload, copy the caller's
    // sphere verbatim (one 16-byte vector; the asm does a two-dword ld/std copy),
    // and bump the record count.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitive(CgsGeometric::Sphere* lpSphere,
                                                f32 lfPadding,
                                                u16 lu16PrimitiveTag)
    {
        AddCollisionHeader(E_VOLUME_TYPE_SPHERE, lfPadding, lu16PrimitiveTag);

        CgsGeometric::Sphere* lpPrimitive =
            static_cast<CgsGeometric::Sphere*>(AllocateMemory(sizeof(CgsGeometric::Sphere)));
        *lpPrimitive = *lpSphere;

        ++mu16NumTests;
    }

    // -------------------------------------------------------------------------
    // AddPrimitive(Box*) @ 0x82814570 (35 insns, 0x82814570..0x828145F8)
    //
    // ⭐ BODIED 2026-08-19 (wave Q6, cluster `addprim`). IDA leaves it `sub_82814570` and
    // it has NO per-address export JSON at all -- an export-run gap, not a missing
    // function (AGENTS gotcha 6). Boundaries and disassembly came from a targeted headless
    // idat run on a PRIVATE .i64 copy (scratchpad/waveQ6/ida_worldc/out.json
    // `asm_82814570`, re-confirmed this wave in ida_addprim/out.json `leaves`).
    //
    // Append a single-box record, asm line for line:
    //     0x82814588  li r4,4                       E_VOLUME_TYPE_BOX
    //     0x82814590  bl AddCollisionHeader         (f1 padding + r6 tag pass straight
    //                                                through -- neither is touched)
    //     0x82814594  li r4,0x50                    == sizeof(CgsGeometric::Box)
    //     0x8281459C  bl AllocateMemory
    //     0x828145A8..0x828145D4  five lvx128/stvx128 pairs at 0/0x10/0x20/0x30/0x40
    //     0x828145D8  lhz r11,6(this) ; addi r11,r11,1 ; sth r11,6(this)   ++mu16NumTests
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitive(CgsGeometric::Box* lpBox,
                                                f32 lfPadding,
                                                u16 lu16PrimitiveTag)
    {
        AddCollisionHeader(E_VOLUME_TYPE_BOX, lfPadding, lu16PrimitiveTag);

        CgsGeometric::Box* lpPrimitive =
            static_cast<CgsGeometric::Box*>(AllocateMemory(sizeof(CgsGeometric::Box)));
        *lpPrimitive = *lpBox;   // the five 16-byte rows

        ++mu16NumTests;
    }

    // -------------------------------------------------------------------------
    // AddPrimitive(Capsule*) @ 0x82814600 (29 insns, 0x82814600..0x82814670)
    //
    // ⭐ BODIED 2026-08-19 (wave Q6, cluster `addprim`); IDA leaves it `sub_82814600`.
    // Identical shape to the Box overload with the capsule's type and size:
    //     0x82814618  li r4,2                       E_VOLUME_TYPE_CAPSULE
    //     0x82814620  bl AddCollisionHeader
    //     0x82814624  li r4,0x20                    == sizeof(CgsGeometric::Capsule)
    //     0x8281462C  bl AllocateMemory
    //     0x82814630..0x8281464C  four ld/std doubleword pairs at 0/8/0x10/0x18 -- the
    //                             whole 32-byte record, moved as GPR doublewords rather
    //                             than VMX rows because 32 bytes is only two vectors and
    //                             the payload is not required to be 16-aligned here
    //     0x82814650  ++mu16NumTests
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitive(CgsGeometric::Capsule* lpCapsule,
                                                f32 lfPadding,
                                                u16 lu16PrimitiveTag)
    {
        AddCollisionHeader(E_VOLUME_TYPE_CAPSULE, lfPadding, lu16PrimitiveTag);

        CgsGeometric::Capsule* lpPrimitive =
            static_cast<CgsGeometric::Capsule*>(AllocateMemory(sizeof(CgsGeometric::Capsule)));
        *lpPrimitive = *lpCapsule;   // the two packed 16-byte lanes

        ++mu16NumTests;
    }

    // -------------------------------------------------------------------------
    // AddPrimitive(Cylinder*) @ 0x82814678 (36 insns, 0x82814678..0x82814704)
    //
    // ⭐ BODIED 2026-08-19 (wave Q6, cluster `addprim`); IDA leaves it `sub_82814678`.
    //     0x82814690  li r4,5                       E_VOLUME_TYPE_CYLINDER
    //     0x82814698  bl AddCollisionHeader
    //     0x8281469C  li r4,0x50                    == sizeof(CgsGeometric::Cylinder)
    //     0x828146A4  bl AllocateMemory
    //     0x828146B0..0x828146D0  FOUR lvx128/stvx128 pairs at 0/0x10/0x20/0x30
    //     0x828146D4..0x828146E0  two lfs/stfs pairs at 0x40 and 0x44
    //     0x828146E4  ++mu16NumTests
    // ⚠️ The console copies 72 of the 80 allocated bytes and leaves +0x48..+0x4F (the
    // record's tail padding) as whatever the bump allocator last held. The C++ assignment
    // below copies the three named members, which is the same live data; the padding is
    // not part of the value either way, and KAU16_VOLUME_SIZES[CYLINDER] == 80 is what
    // the iterator strides by. Documented so nobody "fixes" the eight bytes.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitive(CgsGeometric::Cylinder* lpCylinder,
                                                f32 lfPadding,
                                                u16 lu16PrimitiveTag)
    {
        AddCollisionHeader(E_VOLUME_TYPE_CYLINDER, lfPadding, lu16PrimitiveTag);

        CgsGeometric::Cylinder* lpPrimitive =
            static_cast<CgsGeometric::Cylinder*>(AllocateMemory(sizeof(CgsGeometric::Cylinder)));
        *lpPrimitive = *lpCylinder;   // four rows + the two scalars

        ++mu16NumTests;
    }

    // -------------------------------------------------------------------------
    // AddPrimitivePair(Box*, Box*) @ 0x82814708
    //
    // Append a box-vs-box pair record. First (debug) the two boxes are tested for
    // near-identity across all five 16-byte rows of the packed box: the three
    // orientation-basis rows (0/16/32) are near-identical when dot(rowA,rowB)
    // exceeds a near-1 threshold (flt_82005450), and the position row (48) and
    // half-dimensions row (64) are near-identical when |rowA - rowB| < 0.1
    // (flt_82004014). If all five agree the asm fires an assert. Then it stamps a
    // two-type (BOX/BOX) CollisionHeader, bump-allocates two 80-byte box payloads,
    // copies box A and box B verbatim, and bumps the record count.
    //
    // ⭐ CORRECTED 2026-08-19 (wave Q6, cluster `addprim`) -- TWO REAL DEFECTS, both from
    // the same cause: the two thresholds were GUESSED when this body landed, and both
    // guesses were wrong. They are now IMAGE BYTES, dumped from a private .i64 copy
    // (scratchpad/waveQ6/ida_addprim/out.json):
    //     flt_82005450 = 3F666666 = 0.9f   (was written as 0.999f, with a note saying it
    //                                       was "not recoverable from the dossier rodata")
    //     flt_82004014 = 3DCCCCCD = 0.1f   (guessed 0.1f -- this one happened to be right,
    //                                       and is now measured rather than assumed; the
    //                                       same constant is the tolerance CgsGeometric::
    //                                       Box::IsValid @0x825BEB80 uses)
    // 0.999f vs 0.9f is a ~26x difference in the admitted angle (cos-1 0.999 = 2.6 deg,
    // cos-1 0.9 = 25.8 deg), so the old value made this debug assert far too strict about
    // what counts as "the same box" -- it would have stayed silent on genuinely duplicated
    // near-aligned box pairs. Debug-only, but a wrong constant is a wrong constant.
    //
    // ⭐ The Box member names below moved with the type to its real home
    // (GameShared/GameClasses/Geometric/Primitives/CgsBox.h): the five 16-byte rows the
    // old local fork spelled `maRows[0..4]` are `mTransform.xAxis/.yAxis/.zAxis/.wAxis`
    // and `mDimensionsAndFatness`. Members are private there (as the DWARF has them), so
    // the rows are read through the DWARF's own GetTransform()/GetDimensions() accessors.
    // Same five rows, same order, same test -- a rename, not a behaviour change.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitivePair(CgsGeometric::Box* lpBoxA,
                                                    CgsGeometric::Box* lpBoxB,
                                                    f32 lfPadding,
                                                    u16 lu16PrimitiveTagA,
                                                    u16 lu16PrimitiveTagB)
    {
        // Axis-alignment threshold for the three basis rows (dot > threshold).
        // MEASURED image bytes, not a guess: flt_82005450 == 3F666666 == 0.9f, loaded at
        // X360 0x82814740 and broadcast (vspltw) before `vcmpgtfp. v0, dot, threshold`.
        static const f32 KF_BOX_AXIS_ALIGNED_THRESHOLD = 0.9f;   // flt_82005450
        // MEASURED: flt_82004014 == 3DCCCCCD == 0.1f, loaded at 0x82814824, compared the
        // other way round (`vcmpgtfp. v0, threshold, length`) -- hence the reversed reads
        // below, which are the asm's own operand order, not a stylistic choice.
        static const f32 KF_BOX_NEAR_IDENTICAL_DISTANCE = 0.1f;  // flt_82004014

        const auto Dot3 = [](const Vector3& lvA, const Vector3& lvB) -> f32
        {
            return lvA.x * lvB.x + lvA.y * lvB.y + lvA.z * lvB.z;   // vmsum3fp128
        };
        const auto Length3 = [](const Vector3& lvA, const Vector3& lvB) -> f32
        {
            const f32 lfDx = lvA.x - lvB.x;
            const f32 lfDy = lvA.y - lvB.y;
            const f32 lfDz = lvA.z - lvB.z;
            const f32 lfLenSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
            return std::sqrt(lfLenSq); // vrsqrtefp + Newton refine -> length; 0 -> 0
        };

        // The five rows, read through the DWARF accessors now that Box lives in CgsBox.h.
        const Matrix44Affine lFrameA = lpBoxA->GetTransform();
        const Matrix44Affine lFrameB = lpBoxB->GetTransform();
        const Vector3        lvDimsA = lpBoxA->GetDimensions();
        const Vector3        lvDimsB = lpBoxB->GetDimensions();

        // Rows 0/16/32 (the right/up/at basis): aligned when dot(rowA,rowB) > threshold.
        const bool lbAxis0 = Dot3(lFrameA.Right(), lFrameB.Right()) > KF_BOX_AXIS_ALIGNED_THRESHOLD;
        const bool lbAxis1 = Dot3(lFrameA.Up(),    lFrameB.Up())    > KF_BOX_AXIS_ALIGNED_THRESHOLD;
        const bool lbAxis2 = Dot3(lFrameA.At(),    lFrameB.At())    > KF_BOX_AXIS_ALIGNED_THRESHOLD;
        // Row 64 (half-dimensions, X360 boxA/boxB +0x40) and row 48 (the centre row,
        // +0x30): near when |A - B| < 0.1.
        const bool lbDims  = KF_BOX_NEAR_IDENTICAL_DISTANCE > Length3(lvDimsA, lvDimsB);
        const bool lbPos   = KF_BOX_NEAR_IDENTICAL_DISTANCE > Length3(lFrameA.Pos(), lFrameB.Pos());

        CGS_ASSERT(!(lbAxis0 && lbAxis1 && lbAxis2 && lbDims && lbPos),
                   "Attempting to collide two near-identical boxes\n");

        // Two-type (BOX/BOX) header for the pair record.
        AddCollisionHeader(E_VOLUME_TYPE_BOX, E_VOLUME_TYPE_BOX,
                           lfPadding, lu16PrimitiveTagA, lu16PrimitiveTagB);

        CgsGeometric::Box* lpPrimitiveA =
            static_cast<CgsGeometric::Box*>(AllocateMemory(sizeof(CgsGeometric::Box)));
        *lpPrimitiveA = *lpBoxA;

        CgsGeometric::Box* lpPrimitiveB =
            static_cast<CgsGeometric::Box*>(AllocateMemory(sizeof(CgsGeometric::Box)));
        *lpPrimitiveB = *lpBoxB;

        ++mu16NumTests;
    }
}
}
