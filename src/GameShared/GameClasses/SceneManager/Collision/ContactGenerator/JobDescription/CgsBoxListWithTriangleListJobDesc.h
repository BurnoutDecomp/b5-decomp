#pragma once

// CgsSceneManager::CgsCollision::BoxListWithTriangleListJobDesc — the collision
// contact-generator job descriptor for a BOX-list vs triangle-list test (job-type id 9).
//
// ⭐ ADDED 2026-08-19 (wave Q7, fixer round 1), purely so that
// ContactGeneratorJob::ExecuteBoxListWithTriangleList @0x829218B8 can be spelled store for store.
// It is a PURE-DECLARATION header: no new TU, no mount line, and (see the accessor note below)
// no new undefined external in any mounted object.
//
// ⚠️⚠️ NOTHING IN THE X360 IMAGE POSTS A TYPE-9 DESCRIPTOR, AND NOTHING CONSUMES ONE EITHER.
// Measured, not assumed: a scan of the NAME field of all 30,095 per-address exports finds exactly
// ONE function in the whole image whose name contains "BoxList" -- the arm itself. There is no
// BoxListWithTriangleListJobDesc::Prepare body, no BaseCollisionGenerator::Run* or Collide* that
// writes 9 into a batch's +0x4CF job-type byte, and `xrefs_to` on the arm is
// ContactGeneratorJob::Execute and nothing else. The arm's own body is the console's
// `CGS_ASSERT(false, "Not implemented")` at ContactGeneratorJob.cpp:841. So this type describes a
// job the shipped game never runs; it exists here to type ONE `addi r4, r30, 8`.
//
// SOURCE: the DecFIGS dwarfdump for this exact header,
// references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/
// JobDescription/CgsBoxListWithTriangleListJobDesc.h -- every declaration below is transcribed
// from it (h:37/39/40/43 BoxList, h:58 the descriptor, h:62/65/72/75 Construct/Destruct/Prepare/
// Release, h:78/82/86/90 the four accessors, h:95/97/98 the nested Data, h:102/106 the two
// private GetData). Nothing here is invented, and nothing the DWARF does not name is added.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"

// BoxList holds a Box POINTER only, so the layout home CgsBox.h is not needed here -- this is
// the same forward-declaration the other five headers that mention Box already use, and it keeps
// CgsBox.h (whose Box::Set now calls the real Box::IsValid) out of this include graph.
namespace CgsGeometric { struct Box; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    // DWARF h:37-43. The box analogue of SphereList/TriangleList: a base pointer to a packed
    // array of oriented boxes plus the count. GetBoxDataSize (h:43) is DECLARATION ONLY -- it has
    // no body in this tree and nothing calls it; giving it one would be invention, since the only
    // function in the image that could have shown its arithmetic does not exist.
    struct BoxList
    {
        CgsGeometric::Box* mpaBoxes;    // DWARF h:39  (console 4B pointer; host 8)
        s32                miNumBoxes;  // DWARF h:40

        s32 GetBoxDataSize() const;     // DWARF h:43  -- declared, never defined, never called
    };

    struct BoxListWithTriangleListJobDesc : public CollisionJobDescription
    {
        // The DWARF models the payload as a nested `Data { BoxList mBoxList; TriangleList
        // mTriangleList; }` (h:95-98) reached through the two private GetData() accessors
        // (h:102/106). It is flattened to named members here, EXACTLY as the committed sibling
        // CgsSphereListWithTriangleListJobDesc.h flattens its own Data -- same family, same
        // precedent, and it is what makes the arm's offset legible instead of a cast.
        //
        // ⚠️ THE +0x08 SEAT IS CONFIRMED FROM BOTH DIRECTIONS: the DWARF puts mTriangleList
        // second behind an 8-byte console BoxList, and the arm reads it as `addi r4, r30, 8`
        // (0x829218D8). On this host mpaBoxes widens 4 -> 8, so the member sits at +0x10 -- which
        // is why the arm below names the member instead of adding 8 to a pointer.
        BoxList      mBoxList;       // DWARF h:97  (console +0x00)
        TriangleList mTriangleList;  // DWARF h:98  (console +0x08)

        // DWARF h:78/82/86/90. Given INLINE bodies rather than left as bare declarations: they are
        // inlined on the console too (the arm's read of the triangle list is a bare `addi`, not a
        // `bl`), and a declared-only accessor called from the MOUNTED ContactGeneratorJob.cpp
        // would be an LNK2019 that the compile gate cannot see.
        const BoxList&      GetBoxList()      const { return mBoxList; }       // h:78
        const TriangleList& GetTriangleList() const { return mTriangleList; }  // h:82
        BoxList&            GetBoxList()            { return mBoxList; }       // h:86
        TriangleList&       GetTriangleList()       { return mTriangleList; }  // h:90

        // DWARF h:62/65/72/75. DECLARATIONS ONLY, deliberately: none of the four has a body
        // anywhere in the X360 image (see the type-9 note at the head of this file), nothing in
        // the tree calls them, and writing a body would be inventing the console's arithmetic.
        // Calling any of them is an LNK2019, which is the correct and loudest possible answer.
        void Construct();                                                      // h:62
        void Destruct();                                                       // h:65
        bool Prepare(const BoxList*          lpBoxList,                        // h:72
                     const TriangleList*     lpTriangleList,
                     CollisionResultList*    lpResultsList,
                     f32                     lfPadding);
        void Release();                                                        // h:75
    };
}
}
