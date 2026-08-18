// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_04.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q KEYSTONE, GROUP 4: **THE BREAK**.
// (spec: scratchpad/waveQ/PropEntityModule.spec.md, "GROUPS" row 4.)
//
// Ledger TUs covered (ONE class, TWO ledger ids -- the conductor closes them separately):
//   * GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   * class:BrnWorld::PropEntityModule
//
// Functions in this file (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::ChangePropState     @0x822EF550  (201 insns)
//   PropEntityModule::BreakPropIntoParts  @0x822FB000  (168 insns)
//
// The two share the physical-slot broker (PropZoneManager::GetPhysicalPropSlot /
// IncrementNumberOfPropsInSim) and the contact-generation forwarders the keystone added.
//
// ---- GROUND TRUTH ---------------------------------------------------------------------
// Both bodies are reconstructed from the RAW ASSEMBLY of their per-address ARTIST exports
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EF550.json and 0x822FB000.json), read
// instruction by instruction. The Hex-Rays pseudocode was used only to read the assert
// STRING LITERALS out in full; it is wrong about the shapes -- it renders ChangePropState
// as `_DWORD *(int,int,int,unsigned int,int,int)` and BreakPropIntoParts as `int()`, and it
// drops the second argument of PropZoneManager::GetProp entirely (`sub_822CDA28(a1 + 640)`).
// Declaration shapes are the DecFIGS DWARF ones already committed in
// BrnPropEntityModule.h (:1538 / :1920).
//
// ⚠️ The Feb-2007 leaked source DOES contain a BreakPropIntoParts
// (references/Feb-2007/BrnEntityModuleUnity/GameSource/World/EntityModules/
//  PropEntityModule/BrnPropEntityModule.cpp:835) but it is a DIFFERENT, EARLIER function:
// it takes a PropEntityInstance* rather than a PropEntityID, it walks the scene interface
// itself (RemoveEntity / RemoveForCollision / RemoveVolumeInstance per collision volume)
// instead of calling the cell-manager forwarders, and it builds part ids with
// `lPartId.Set(entityIndex + partIndex + 1, E_PART)` rather than SetPartIndex. NOTHING from
// that revision is used below; the shipped X360 body is the authority. Recorded so a later
// reader does not "restore" the old shape.
//
// ---- MEASURED vs INFERENCE ------------------------------------------------------------
// MEASURED (read off the asm listing):
//   * every assert site, its predicate, its rodata message and its console line number;
//   * the whole control flow of both bodies, including which leg calls what and in which
//     order (the two legs of ChangePropState order SetState differently -- see there);
//   * every member touched, by console offset, each of which maps to a NAMED member that
//     BrnPropEntityInstance.h / BrnPhysicsPropTypeData.h / BrnPropEntityModule.h already
//     pin with static_asserts (0x44 muTypeId, 0x40 muInstanceID, 0x48 mu16PartsIndex,
//     0x4A mu8Flags, 0x4C mu8PhysicsIndex, 0x4D mu8State; type 0x5D muNumberOfParts,
//     0x60 mu8ExtraTypeInfo, 0x40 maParts);
//   * the part-transform maths (0x822FB1E0..0x822FB230) and the 1-based inclusive loop
//     bound (`blt` at 0x822FB23C against a RE-READ `lbz 0x5D`);
//   * KVF_PROP_FLOOR == -1000.0f -- re-dumped for THIS file in round 1; the constant is
//     now consumed from its real home, SharedClasses/Physics/Props/BrnPropConstants.h.
// INFERENCE (each marked where it appears):
//   * that `stb 6, 0x4D(prop)` / `stb 5, 0x4D(prop)` are the compiler's fold of
//     PropEntityInstance::SetState(E_SMASHED) / SetState(E_MOVED). The asm stores the raw
//     bytes 6 and 5; the enumerator names come from the committed EPropState. The folded
//     store carries no range assert, the call does -- with a compile-time-true predicate,
//     so the behaviour is identical.
//   * that `lbz 0x60(type); cmplwi 1` is PropTypeData::IsTrafficLight(). The committed
//     accessor IS `mu8ExtraTypeInfo == 1` (BrnPhysicsPropTypeData.h:140), and the guarded
//     call is RequestTrafficLightKnockDown, so the reading is corroborated by the callee.
//
// ---- CONSOLE-LITERAL DISCIPLINE (gotcha 1) -------------------------------------------
// Not one console byte offset, stride or object size appears in the code below. The two
// traps this pair carries, both defused:
//   (1) BreakPropIntoParts walks the part descriptors with `r29 += 0x30` (0x822FB228).
//       48 is the CONSOLE sizeof(PropPartTypeData); on the host it is 64 (its embedded
//       volume pointer widened 4->8; pinned by PropPartTypeData::_AssertLayout). The loop
//       below subscripts `lpType->GetParts()[luPart - 1]` and lets the host compiler pick
//       the stride. Identical trap, identical fix, as PropCellManager::AddPropPartsToScene.
//   (2) the AddPropPartsToContactGeneration argument at 0x822FB25C..0x822FB288 is
//       `&mZoneManager.maParts[ lpProp->mu16PartsIndex ]`, computed with the CONSOLE
//       80-byte PropPartEntityInstance stride (`rotlwi 2; add; slwi 4`) and the console
//       maParts base (+435968). That whole computation belongs to the de-inlined
//       PropZoneManager::AddPropPartsToContactGeneration forwarder, which subscripts the
//       array itself; the four-argument call below does NOT reintroduce it.
//   The one numeric literal that survives is the `1` in IncrementNumberOfPropsInSim(1) --
//   a population delta, not an address.
//
// ---- NaN POLARITY (gotcha 4) ----------------------------------------------------------
// BreakPropIntoParts: NOT APPLICABLE -- it contains no floating-point COMPARE at all (the
// only FP work is the VMX part-transform multiply-add cascade).
// ChangePropState: exactly one float compare, the prop-floor tripwire at
// 0x822EF5F4 `vcmpgtfp. v13, v0` with v13 == splat(Pos().Y()) and v0 == splat(KVF_PROP_FLOOR).
// This is the VMX ORDERED greater-than (NaN in either operand yields a FALSE lane), and the
// branch tests CR6[0] "all lanes true", so the assert PASSES iff `Y > FLOOR` with NaN
// failing -- which is precisely what C++ `>` does. It is NOT the fcmpu+bge/ble form, so the
// negated-ordered-predicate rewrite does NOT apply here and writing `>` is the faithful
// spelling. (Writing `!(Y <= FLOOR)` would be the WRONG polarity: it would pass on NaN.)
// ============================================================================

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h"

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"
#include "SharedClasses/Physics/Props/BrnPropConstants.h"        // KVF_PROP_FLOOR

#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Vector3 operator* / operator+ -- the part-transform cascade in BreakPropIntoParts. Same
// include, for the same reason, as BrnPropCellManager.cpp:46 (which runs the identical maths).
#include "rw/math/vpu/vector3_operation.h"

namespace BrnWorld
{
    // ------------------------------------------------------------------
    // ROUND-2 UPDATE (2026-08-18): the file-local `KF_PROP_FLOOR` copy that lived here (in
    // an anonymous namespace) is GONE, along with the namespace. `BrnPhysics::Props::
    // KVF_PROP_FLOOR` now has its real DWARF home -- SharedClasses/Physics/Props/
    // BrnPropConstants.h:56 -- landed by the round-2 owner with the same measured value
    // (-1000.0f) and the same headless-idat derivation this file carried (unk_82FAD840 is
    // .bss, splat-filled at run time by the unnamed initialiser at 0x82C4B478 from
    // flt_8200D4F8 == 0xC47A0000 == -1000.0f, which is why a function-export scan finds no
    // writer). ChangePropState's third tripwire now reads the homed constant; nothing in
    // this file re-spells the value.
    // ------------------------------------------------------------------

    // ========================================================================
    // PropEntityModule::ChangePropState   @ 0x822EF550   (201 insns)
    // DWARF BrnPropEntityModule.cpp:1538.
    // ------------------------------------------------------------------------
    // Move ONE prop between simulation states, brokering a physical slot when it needs one.
    // Two legs, chosen on the OLD state (0x822EF6F4..0x822EF718):
    //
    //   * old == E_STATIC or E_LEANING  -> the prop is ALREADY in the sim on a slot it keeps.
    //     Re-issue it to the prop manager in place (Remove then Add on the SAME
    //     mu8PhysicsIndex) and store the new state. NO slot churn, no population change,
    //     no traffic-light event. (0x822EF814..0x822EF868.)
    //
    //   * old == E_NON_PHYSICAL or E_MOVED -> the prop needs a physical slot. Ask the broker;
    //     if it says no, the whole call is a no-op (the prop stays where it was). If the slot
    //     came from an EVICTION, demote the evicted prop to E_MOVED, pull its rigid body and
    //     remember it in maRecentlyRecycledProps (when there is room); otherwise the sim
    //     population grows by one. Then adopt the slot and add the body.
    //     (0x822EF71C..0x822EF80C.)
    //
    // REGISTER -> PARAMETER MAP (prologue 0x822EF554..0x822EF578, all measured):
    //   r3 -> r23 this | r4 -> r28 lpProp | r5 -> r22 lPropEntityId
    //   r6 -> r29 leOldState | r7 -> r25 leNewState | r8 -> r21 lpOutput
    // No float rides in, so gotcha 3's "a float skips its GPR slot" does not bite: the six
    // GPRs are six parameters, in order.
    //
    // E_PHYSICAL_WITH_EXTRA_COM_OFFSET never reaches the prop manager as a state: at
    // 0x822EF6A4 the body rewrites it to E_PHYSICAL and raises the separate
    // lbAddExtraComOffset flag AddPropInstance carries. Both legs pass the rewritten pair.
    void PropEntityModule::ChangePropState( PropEntityInstance* lpProp, PropEntityID lPropEntityId,
                                            EPropState leOldState, EPropState leNewState,
                                            PropEntityIO::OutputBuffer_PrePhysics* lpOutput )
    {
        // 0x822EF578. The write-locked prop-manager input interface this whole body feeds.
        BrnPhysics::Props::PropInputInterface* lpPropManagerInterface =
            lpOutput->GetPropInputInterface();
        CGS_ASSERT(lpPropManagerInterface != NULL, "lpPropManagerInterface");            // :1565

        // 0x822EF5AC. ⚠️ THIS ASSERT IS *NOT* CanChangeState -- corrected 2026-08-18.
        // Round 1 routed it through the member because the committed inline happened to be
        // the same two terms. The round-2 owner corrected CanChangeState to the DWARF's
        // three-term form (BrnPropEntityModule.h:531, with the extra
        // `leOldState == E_MOVED && leNextState == E_PHYSICAL` conjunct that both of ITS
        // folds carry), so routing through it here would assert STRICTER than the console
        // and fire on E_MOVED -> lower transitions the shipped build permits.
        // This assert's own fold is exactly two terms -- `cmpw r29,r25 ; blt ;
        // cmpwi r29,5 ; beq` -- and the full rodata message at 0x8201DFAC is literally the
        // expression spelled below. So it is spelled inline, verbatim.
        CGS_ASSERT((leOldState < leNewState) || (leOldState == E_MOVED),
                   "leOldState < leNewState || (leOldState == E_MOVED)");                // :1567

        // 0x822EF5D8. See the NaN-polarity note in this file's banner: `vcmpgtfp.` is the
        // ordered compare, so plain `>` is the faithful (and NaN-correct) spelling.
        CGS_ASSERT(lpProp->GetWorldTransform().Pos().y > BrnPhysics::Props::KVF_PROP_FLOOR,
                   "lpProp->GetWorldTransform().Pos().Y() > BrnPhysics::Props::KVF_PROP_FLOOR"); // :1569

        // 0x822EF628 / 0x822EF664 -- the legality pair. Each is a chain of `cmpwi/beq` over
        // exactly the enumerators its message names, in the asm's own order.
        CGS_ASSERT(leOldState == E_NON_PHYSICAL || leOldState == E_STATIC ||
                   leOldState == E_LEANING     || leOldState == E_MOVED,
                   "leOldState == E_NON_PHYSICAL || leOldState == E_STATIC || "
                   "leOldState == E_LEANING || leOldState == E_MOVED");                  // :1574
        CGS_ASSERT(leNewState == E_STATIC   || leNewState == E_LEANING ||
                   leNewState == E_PHYSICAL || leNewState == E_PHYSICAL_WITH_EXTRA_COM_OFFSET,
                   "leNewState == E_STATIC || leNewState == E_LEANING || "
                   "leNewState == E_PHYSICAL || leNewState == E_PHYSICAL_WITH_EXTRA_COM_OFFSET"); // :1579

        // 0x822EF6A0..0x822EF6B0. E_PHYSICAL_WITH_EXTRA_COM_OFFSET is folded away here.
        bool lbAddExtraComOffset = false;
        if (leNewState == E_PHYSICAL_WITH_EXTRA_COM_OFFSET)
        {
            lbAddExtraComOffset = true;
            leNewState          = E_PHYSICAL;
        }

        // 0x822EF6B4..0x822EF6C8. `addis r3,r23,0xD; addi r3,r3,-0x2278` == this + 0xCDD88 ==
        // mpPropPhysicsDataHeader; the truncated `BrnPhysics__Props__Prop` symbol is that
        // ResourcePtr's GetMemoryResource. The type id is `lhz 0x44(prop)` == muTypeId.
        const BrnPhysics::Props::PropTypeData* lpPropTypeData =
            mpPropPhysicsDataHeader.GetMemoryResource()->GetType(lpProp->muTypeId);
        CGS_ASSERT(lpPropTypeData != NULL, "lpPropTypeData");                            // :1590

        if (leOldState == E_STATIC || leOldState == E_LEANING)
        {
            // ---- leg A (0x822EF814): already in the sim, keep the slot ----------------
            // Both calls read mu8PhysicsIndex fresh off the prop (`lbz 0x4C` at 0x822EF818
            // and again at 0x822EF828) -- nothing between them can change it, so the two
            // reads are one value.
            lpPropManagerInterface->RemovePropInstance(lPropEntityId, lpProp->mu8PhysicsIndex);
            lpPropManagerInterface->AddPropInstance(lPropEntityId,
                                                    lpProp->muTypeId,
                                                    lpProp->mu8PhysicsIndex,
                                                    lpProp->GetWorldTransform(),
                                                    lbAddExtraComOffset,
                                                    leNewState);
            // 0x822EF840..0x822EF868 -- the compiler INLINED SetState here (its range assert
            // and its `stb 0x4D`), and it lands AFTER the add. The other leg calls SetState
            // BEFORE its add. That ordering difference is real; it is reproduced, not tidied.
            lpProp->SetState(leNewState);
            return;
        }

        // ---- leg B (0x822EF71C): broker a physical slot ------------------------------
        // The X360 hoists this local's zero-init all the way up to 0x822EF5F8 (`stw r11(0),
        // var_68`, immediately before the prop-floor assert). Behaviourally the broker
        // overwrites it on success and nothing reads it on failure, so it is initialised at
        // its declaration here.
        s32          liPhysicsSlot = 0;
        PropEntityID lEvictedId;
        bool         lbEvicted     = false;

        if (!mZoneManager.GetPhysicalPropSlot(lPropEntityId, liPhysicsSlot, lEvictedId, lbEvicted))
        {
            // 0x822EF73C -- no slot available: the prop keeps its old state entirely.
            return;
        }

        if (lbEvicted)
        {
            // 0x822EF750. The slot was taken from another prop. `sub_822CDA28` is
            // PropZoneManager::GetProp(PropEntityID) -- Hex-Rays drops its second argument;
            // the asm passes r4 = the evicted id.
            PropEntityInstance* lpEvictedProp = mZoneManager.GetProp(lEvictedId);
            // INFERENCE (named in the banner): `stb 5, 0x4D` == SetState(E_MOVED).
            lpEvictedProp->SetState(E_MOVED);

            // The evicted prop's rigid body is pulled off the slot WE are about to take.
            lpPropManagerInterface->RemovePropInstance(lEvictedId, liPhysicsSlot);

            // 0x822EF77C. `addis r30,r23,0xD; addi r30,r30,-0x2198` == this + 0xCDE68 ==
            // maRecentlyRecycledProps (::Array<PropEntityID,15>). A full list simply drops
            // the record -- the asm has no else arm.
            if (!maRecentlyRecycledProps.IsFull())
            {
                maRecentlyRecycledProps.Append(lEvictedId);
            }
        }
        else
        {
            // 0x822EF7A8. `lwz/addi/stw 0x904(this+0x280)` == a bare `++miNumPropsInSim` on
            // the embedded PropCellManager (0x280 + 0x904 == 2948). The de-inlined forwarder
            // takes the DWARF's int32_t delta, so the folded form is the delta == 1 case.
            mZoneManager.IncrementNumberOfPropsInSim(1);
        }

        // 0x822EF7B4. Here the state lands BEFORE the add (contrast leg A).
        lpProp->SetState(leNewState);
        lpProp->SetPhysicsIndex(liPhysicsSlot);

        lpPropManagerInterface->AddPropInstance(lPropEntityId,
                                                lpProp->muTypeId,
                                                liPhysicsSlot,
                                                lpProp->GetWorldTransform(),
                                                lbAddExtraComOffset,
                                                leNewState);

        // 0x822EF7EC. `lbz 0x60(type); cmplwi 1` == PropTypeData::IsTrafficLight(); the
        // knock-down request carries `lwz 0x40(prop)` == muInstanceID. Only the slot-brokering
        // leg raises it -- a prop that was already E_STATIC/E_LEANING has been knocked down
        // once already.
        if (lpPropTypeData->IsTrafficLight())
        {
            lpOutput->GetPropToTrafficInterface()
                ->RequestTrafficLightKnockDown(lpProp->muInstanceID);
        }
    }

    // ========================================================================
    // PropEntityModule::BreakPropIntoParts   @ 0x822FB000   (168 insns)
    // DWARF BrnPropEntityModule.cpp:1920.
    // ------------------------------------------------------------------------
    // THE BREAK. Shatter one whole prop into its part instances: mark it E_SMASHED, pull the
    // whole-prop body out of contact generation and out of the scene, seat every part at the
    // prop's pose offset by that part's design-time offset, then push the parts into the
    // scene and into contact generation.
    //
    // REGISTER -> PARAMETER MAP (prologue 0x822FB014..0x822FB020, measured):
    //   r3 -> r26 this | r4 -> r19 lPropEntityId | r5 -> r28 lpOutput
    // (r19 is fed straight to `sub_822CDA28` == PropZoneManager::GetProp(PropEntityID), and
    //  r28 is the object every `sub_822B9738` == OutputBuffer_PreScene::GetSceneInputInterface
    //  call is made on -- which is what fixes both types.)
    //
    // r27 == this + 0x280 == &mZoneManager throughout; PropCellManager is embedded at
    // offset 0 inside it, which is why the raw asm appears to call PropCellManager methods
    // on the zone manager's address (gotcha 2 -- three classes on one pointer). The calls
    // below go through the de-inlined PropZoneManager forwarders, per the keystone's
    // convention.
    void PropEntityModule::BreakPropIntoParts( PropEntityID lPropEntityId,
                                               PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        PropEntityInstance* lpProp = mZoneManager.GetProp(lPropEntityId);

        // 0x822FB030 `lbz 0x4A(prop); rlwinm r11,r11,0,29,29` -- mask 0x00000004 ==
        // KU_ADDED_TO_CONTACT_GEN_BIT. A prop that never entered contact generation cannot
        // be broken by one, and the whole body is skipped (branch to the epilogue).
        if ((lpProp->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT) == 0)
        {
            return;
        }

        // 0x822FB040..0x822FB0FC. The first test is GetState() INLINED (its
        // "mu8State < E_STATE_COUNT" assert at BrnPropEntityInstance.h:653, then `cmplwi 4`);
        // the other two are real out-of-line GetState calls. Three calls, three states.
        //
        // The failing arm builds its message with CgsDev::StrStream
        // (`"Can't break prop in state: " << GetState() << "\n"`); collapsed to a plain
        // CGS_ASSERT per the convention this TU already uses for StrStream asserts
        // (BrnPropZoneManager.cpp:1356). The streamed state value is dropped, the literal is
        // verbatim.
        CGS_ASSERT(lpProp->GetState() == E_PHYSICAL ||
                   lpProp->GetState() == E_STATIC   ||
                   lpProp->GetState() == E_LEANING,
                   "Can't break prop in state: ");                                       // :2127

        // 0x822FB100. INFERENCE (named in the banner): `stb 6, 0x4D` == SetState(E_SMASHED).
        lpProp->SetState(E_SMASHED);

        // 0x822FB10C -> r16. The scene-update interface is fetched THREE separate times in
        // the shipped body (0x822FB10C, 0x822FB14C, 0x822FB178); only the first result is
        // kept live across the part loop, for the two Add* calls at the end.
        // ⚠️ WHY THE REPEAT IS KEPT (round-1 wording corrected -- it said the getter "takes
        // the buffer's write lock, so the repeat is observable", which the asm contradicts):
        // sub_822B9738 == OutputBuffer_PreScene::GetSceneInputInterface ASSERTS the buffer is
        // already locked for writing -- `lbz r11,0(buf) ; extrwi r11,r11,1,28`, and on a
        // clear bit it fires the assert whose message is "Not locked for writing\n" (StrStream
        // 0x822B977C-0x822B97B4, FireAssert 0x822B97C8) -- then `addi r3,r28,0x420`. It takes
        // no lock and has no side effect beyond that assert. The three calls are reproduced
        // call for call because that is what the shipped body does, not because a lock is
        // acquired.
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput =
            lpOutput->GetSceneInputInterface();

        // 0x822FB118..0x822FB128. Same mpPropPhysicsDataHeader -> GetType(muTypeId) pair as
        // ChangePropState above.
        const BrnPhysics::Props::PropTypeData* lpType =
            mpPropPhysicsDataHeader.GetMemoryResource()->GetType(lpProp->muTypeId);

        // 0x822FB12C..0x822FB144. `li r11,3; sldi r11,r11,56; std r11, var_E0` is the
        // compiler's fold of PropVolumeInstanceID's default constructor, which seeds the
        // embedded entity word's OWNER byte with E_ENTITYTYPE_PROP (== 3) at bit 56. The
        // committed constructor already does exactly that, so the declaration alone
        // reproduces the store; then the prop's id is installed over it.
        PropVolumeInstanceID lVolumeInstanceID;
        lVolumeInstanceID.SetPropEntityId(lPropEntityId);

        // 0x822FB168 / 0x822FB194. Both take a FRESHLY fetched scene interface (r7 comes from
        // the 0x822FB14C and 0x822FB178 getter calls, not from r16).
        mZoneManager.RemovePropFromContactGeneration(lpProp, lpType, lVolumeInstanceID,
                                                     lpOutput->GetSceneInputInterface());
        mZoneManager.RemovePropFromScene(lpProp, lpType, lVolumeInstanceID,
                                         lpOutput->GetSceneInputInterface(),
                                         &mPropEntitySerialiser);

        // ---- seat every part at the prop's pose --------------------------------------
        // 0x822FB198..0x822FB23C. The asm guards the loop on `lbz 0x5D(type) != 0` and then
        // runs a do-while over 1-BASED part indices, RE-READING the part count at the bottom
        // of every iteration (0x822FB234). The guarded do-while with a re-read bound is
        // exactly the `for` below; the count cannot change inside the body (lpType is a
        // const serialised record), so the two are equivalent as well as same-shaped.
        //
        // ⚠️ CONSOLE STRIDE TRAP: the descriptor walk is `r29 += 0x30` -- see banner note (1).
        const Matrix44Affine& lrPropTransform = lpProp->mWorldTransform;

        for (u32 luPart = 1; luPart <= lpType->GetNumberOfParts(); ++luPart)
        {
            const BrnPhysics::Props::PropPartTypeData& lrPartType =
                lpType->GetParts()[luPart - 1];

            // 0x822FB1D0/0x822FB1DC. A local copy of the prop id with the part field set;
            // the asm stores the incoming word into the stack slot and calls SetPartIndex on
            // it. (The slot is the same one the volume id used -- the compiler reused it
            // after r18 had already taken the volume id's value at 0x822FB150. Two distinct
            // locals here, one stack slot there.)
            PropEntityID lPartEntityId = lPropEntityId;
            lPartEntityId.SetPartIndex(luPart);

            // 0x822FB1E0..0x822FB214. The textbook affine point transform.
            //
            // ⚠️ IDA OPERAND-ORDER TRAP, AND THE TWO FORMS PRINT DIFFERENTLY -- MEASURED,
            // corrected 2026-08-18 (the round-1 wording quoted the base-VMX rule while
            // annotating VMX128 instructions, which would have inverted this maths):
            //   * VMX128 -- `vmaddfp128 vD,vA,vC,vB` prints in ISA MNEMONIC order, so the
            //     ADDEND IS THE LAST OPERAND: vD = vA*vC + vB. That is the form HERE:
            //     0x822FB204 `vmulfp128 v12, v127, v12` then 0x822FB20C
            //     `vmaddfp128 v12, v126, v11, v12` with v11 = `vspltw v0,v0,1` (off.y,
            //     0x822FB1EC) and v12 the running accumulator -- accumulator LAST.
            //   * BASE VMX -- `vmaddfp vD,vA,vB,vC` prints in RAW FIELD order, so the
            //     addend is the THIRD operand: vD = vA*vC + vB. Contrast site in the same
            //     export: 0x822B8188 `vmaddfp v13, v12, v13, v8` inside
            //     PropEntityInstance::InitialiseFromData, where the accumulator v13 is
            //     third and the fresh splat v8 is fourth.
            // Applying the base rule to `vmaddfp128 v12, v126, v11, v12` would read
            // v12 = v126*v12 + v11, i.e. accumulating into a splat -- nonsense, and not
            // what the code below (correctly) does.
            //     partPos = M.Right()*offset.x + M.Up()*offset.y + M.At()*offset.z + M.Pos()
            // Kept as 4-lane Vector3 expressions rather than a TransformPoint call because
            // the console carries the w lane through -- identical to
            // PropCellManager::AddPropPartsToScene, which is the same maths on the same data.
            const Vector3& lrOffset = lrPartType.GetOffset();
            Vector3 lPartPosition = lrPropTransform.xAxis * lrOffset.x;
            lPartPosition = lrPropTransform.yAxis * lrOffset.y + lPartPosition;
            lPartPosition = lrPropTransform.zAxis * lrOffset.z + lPartPosition;
            lPartPosition = lrPropTransform.wAxis + lPartPosition;

            // 0x822FB218 `sub_822CDB90` == PropZoneManager::GetPart(PropEntityID).
            // 0x822FB21C..0x822FB230: the part inherits the prop's ORIENTATION verbatim --
            // the three rotation rows are block-copied (stvx128 into part+0/+0x10/+0x20) and
            // only the translation row differs.
            PropPartEntityInstance* lpPart = mZoneManager.GetPart(lPartEntityId);
            lpPart->mWorldTransform.xAxis = lrPropTransform.xAxis;
            lpPart->mWorldTransform.yAxis = lrPropTransform.yAxis;
            lpPart->mWorldTransform.zAxis = lrPropTransform.zAxis;
            lpPart->mWorldTransform.wAxis = lPartPosition;
        }

        // 0x822FB258 / 0x822FB28C. Both use the FIRST scene interface (r16), and both are
        // handed the same volume id (r18) the two removals used.
        //
        // ⚠️ The contact-generation call is FOUR arguments here; PropCellManager's takes
        // FIVE. The console's EXTRA parameter -- positionally the SECOND, r5 =
        // &maParts[lpProp->mu16PartsIndex], built at 0x822FB264-0x822FB288 with the 80-byte
        // console stride -- is resolved inside the de-inlined forwarder (see banner note
        // (2), and BrnPropCellManager.h:236 for the callee's own parameter order:
        // (lpProp, lpFirstPart, ...)). It is NOT the fifth ARGUMENT of this call.
        mZoneManager.AddPropPartsToScene(lpProp, lpType, lVolumeInstanceID, lpSceneInput,
                                         &mPropEntitySerialiser);
        mZoneManager.AddPropPartsToContactGeneration(lpProp, lpType, lVolumeInstanceID,
                                                     lpSceneInput);
    }
}
