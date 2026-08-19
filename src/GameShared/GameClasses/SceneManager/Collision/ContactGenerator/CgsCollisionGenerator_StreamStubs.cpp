// ============================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/
// CgsCollisionGenerator_StreamStubs.cpp
//
// ⭐⭐⭐ THIS TU IS EMPTY AS OF 2026-08-19 (wave Q7, cluster `pairlist`). ALL SEVEN OF ITS
// STUBS ARE RETIRED. It defines NO symbol; it is kept as a tombstone so the history of
// where each stub went is readable, and it is REPORTED FOR UNMOUNT
// (tools/build/build_game_exe.bat:1001 -- the conductor owns that file and that deletion).
// Compiling it is harmless but pointless (MSVC emits LNK4221 "no public symbols found").
//
// WHERE EVERYTHING WENT
//   2026-08-14 (walls leg 1) -- SIX OF THE SEVEN BIRTH STUBS ->
//     CgsCollisionGenerator_CollideStreams.cpp: the three Create* factories and the three Run*
//     dispatchers. (The two Add* posters landed in the same change but were never stubs here --
//     the TU was born, at fbd9392e, holding exactly 3 Create* + 3 Run* + CollidePrimitivePairList.)
//     The three Create* proved byte-identical bar assert line numbers; the Run* wire
//     ContactGeneratorEntry over descriptor types 6/14/8, whose workers are all real bodies now
//     (ContactGeneratorJob.cpp and its wQ6/wQ7 partfiles, wave Q7).
//   2026-08-19 (wave Q6, cluster pstream) -- A GATE ADDED TO THIS FILE AFTER BIRTH, NOT ONE OF THE
//     SEVEN: AddPrimitiveListWithTriangleListToStream
//     @0x82811D40 (35) -> CgsCollisionGenerator.cpp, its DWARF home TU (dumpfile source
//     line 1996). Its whole blocker was ONE type + ONE enum member, both landed with it:
//     JobDescription/CgsPrimitiveListWithTriangleListStreamJobDesc.h and
//     E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12.
//   2026-08-19 (wave Q7, cluster pairlist) -- THE LAST ONE:
//     BaseCollisionGenerator::CollidePrimitivePairList @0x82814138 -> CgsCollisionGenerator.cpp,
//     beside its sibling synchronous leg CollidePrimitiveListAgainstTriangleList @0x828141D8.
//     Its closure was ALREADY COMPLETE and had been for waves -- PrepareNewPrimitiveTestResults-
//     List @0x82810798, CreateNewBatch @0x82810960, PrimitivePairListJobDesc::Prepare
//     @0x82810478 (which seats job type 10), CollisionBatch::SetupJob @0x82810508 and the two
//     PerfMonCpu monitors are all real bodies in the tree; the gate was outliving its reason.
//     ⚠️ TWO CORRECTIONS to the banner that stood here, both real defects (gotcha 9):
//       * "(92)" instructions -- WRONG, it is 40 (0x82814138..0x828141D4, and
//         (0x828141D8-0x82814138)/4 == 40).
//       * "Its closure is its own: the type-10 descriptor prepare + ExecutePrimitivePairList
//         @0x82925798 (92) + the pair-list walk" -- the descriptor prepare was already landed,
//         and only ExecutePrimitivePairList was ever outstanding. ⭐ It is no longer: the type-10
//         worker landed as a full body in ContactGeneratorJob.cpp (ExecutePrimitivePairList,
//         dispatched from Execute case 10) in the SAME wave Q7, so this leg is bodied end to end.
//     The "dead at runtime today" observation still holds and is still worth keeping: on the
//     junkyard path both simple-traffic builders report GetNumTests() == 0, so nothing posts
//     yet. It goes live with traffic, and with VehicleManager::StartPartContactGeneration's
//     gated tail (owner `carcar`).
//
// If a definition for ANY of the seven ever reappears in this file, the link will say so
// (LNK2005) -- which is exactly why each retirement had to happen in the same change as its
// body: both TUs are mounted and `cl /c` cannot see a duplicate definition.
// ============================================================================

// Deliberately no includes and no definitions: this TU contributes nothing to the link.
