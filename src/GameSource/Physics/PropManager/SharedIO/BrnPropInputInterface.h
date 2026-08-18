#pragma once

// ============================================================================
// BrnPhysics::Props::PropInputInterface
//   b5-decomp/src/GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h
//   (DWARF home BrnPropInputInterface.h:37)
//
// The per-frame request bundle the world PropEntityModule pushes to the prop
// physics manager: four fixed-capacity event queues (add-prop / add-part /
// remove-prop / remove-part), a physics-data ResourceHandle, and a
// remove-all flag. Member names/types/order + queue capacities are DWARF-
// authoritative (BrnPropInputInterface.h:125-130, BrnPropQueues.h:43-48);
// the AddProp/AddPart/RemovePart enqueue bodies are reconstructed store-for-
// store from BURNOUT_X360_ARTIST.XEX (AddPropInstance @0x822CCB60,
// AddPartInstance @0x822CCCA0, RemovePartInstance @0x822CCE20).
//
// Queue-member byte offsets pinned by the X360 asm (all element strides
// attested by successive queue offsets):
//   mAddPropQueue    @   0     (EventQueue<AddPhysicalPropEvent,50>,  stride 80)
//   mAddPartQueue    @0xFB0   (=4016 = 16 + 50*80)
//   mRemovePropQueue @0x1F60   (=8032 = 4016 + 16 + 50*80)
//   mRemovePartQueue @0x28CC   (=10444 = 8032 + 12 + 300*8)
// The 16-byte padded EventQueue prefix on the first three queues comes from
// Matrix44Affine forcing alignas(16) on their element; the remove-* elements
// are pointer-free 8-byte records (align 4) so their queue prefix is the bare
// 12-byte BaseEventQueue header.
//
// The four event-payload structs (AddPhysicalPropEvent / AddPhysicalPartEvent /
// RemovePhysicalPropEvent / RemovePhysicalPartEvent) are DWARF-homed in
// BrnPropEvents.h and are ADDED there alongside the committed siblings
// UpdatePropEvent / PropUpdateNotification; this header only includes them.
// ============================================================================

#include "types.hpp"                                                              // s16/s32/bool
#include "BrnCommonTypes.h"                                                       // Matrix44Affine (rw::math::vpu::Matrix44Affine)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"            // CgsResource::ResourceHandle
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                          // BrnWorld::PropEntityID, EEntityType
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h" // BrnWorld::EPropState
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"                // AddPhysical*/RemovePhysical* event payloads

namespace BrnPhysics
{
namespace Props
{
    // KU_MAX_PHYSICAL_PROPS -- the physical-prop slot ceiling (asm bound == 0xF).
    // DWARF-authoritative home is BrnPropConstants.h (SharedClasses/Physics/Props,
    // namespace BrnPhysics::Props, `const uint32_t KU_MAX_PHYSICAL_PROPS = 15`),
    // which has NO committed .h yet. Declared here so the AddPropInstance slot assert
    // (whose message literally names BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS) compiles;
    // MOVE to BrnPropConstants.h when that file lands to avoid a duplicate definition.
    static const u32 KU_MAX_PHYSICAL_PROPS = 15;

    // ⭐ ADDED 2026-08-18 (breakable-props keystone). The part-slot ceiling, same DWARF home
    // (BrnPropConstants.h, still uncommitted) and the same MOVE-WHEN-IT-LANDS caveat as the
    // prop one above. Value is X360-attested twice over: PropManager::ReadUpdatedBodies
    // @0x82632E1C bounds-checks the part index with `cmplwi r11, 0x1E` under the assert
    // "liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )"
    // (BrnPropManager.cpp:1043 -- the message literally names this constant), and
    // PropManager::mUsedParts is a BitArray<30>.
    static const u32 KU_MAX_PHYSICAL_PROP_PARTS = 30;

    struct PropInputInterface
    {
        // ---- embedded-queue typedefs (DWARF BrnPropInputInterface.h:43/44/47/48,
        //      capacities BrnPropQueues.h:43/44/47/48) --------------------------------
        typedef CgsModule::EventQueue<AddPhysicalPropEvent,    50>  AddPhysicalPropEventQueue; // BrnPropQueues.h:43
        typedef CgsModule::EventQueue<AddPhysicalPartEvent,    50>  AddPhysicalPartEventQueue; // BrnPropQueues.h:44
        typedef CgsModule::EventQueue<RemovePhysicalPropEvent, 300> RemovePropEventQueue;      // BrnPropQueues.h:47
        typedef CgsModule::EventQueue<RemovePhysicalPartEvent, 100> RemovePartEventQueue;      // BrnPropQueues.h:48

        // ---- wave-8b bodied ledger functions ------------------------------------------
        // @0x822CCB60: enqueue an add-physical-prop request.
        void AddPropInstance(BrnWorld::PropEntityID lEntityId, s32 liPropTypeId, s32 liSlot,
                             Matrix44Affine lTransform, bool lbAddExtraComOffset,
                             BrnWorld::EPropState leState);

        // @0x822CCCA0: enqueue an add-physical-part request.
        void AddPartInstance(BrnWorld::PropEntityID lEntityId, s32 liPropTypeId, s32 liPartId,
                             Matrix44Affine lTransform, s32 liSlot);

        // @0x822CCE20: enqueue a remove-physical-part request.
        void RemovePartInstance(BrnWorld::PropEntityID lEntityId, s32 liPhysicalIndex);

        // ADDITIVE GROW (prop-spawn wave, 2026-08-12). @0x822CCD88: enqueue a
        // remove-physical-PROP request -- the whole-prop twin of RemovePartInstance.
        // Called by BrnWorld::PropCellManager::RemovePropFromSim @0x822DFF48
        // (`RemovePropInstance(lVolumeInstanceID.GetPropEntityID(), lu8PhysicsIndex)`);
        // args are the r4/r5 pair in that call site's asm. Declaration only -- the body
        // belongs to this interface's own TU. No layout change.
        void RemovePropInstance(BrnWorld::PropEntityID lEntityId, s32 liPhysicalIndex);

        // DWARF :42. The console emits this INLINE inside
        // PhysicsModuleIO::InputBuffer::Construct @0x825ABA18 (r30 = buffer + 327216 == this):
        // the four queue Constructs in the order below, then mbRemoveAllPropsAndParts = false,
        // then Clear(). Bodied 2026-08-10 (root-cause wave) -- the destination interface of
        // both prop bridges was reached with its four queues un-Constructed.
        // ⚠️ AS SHIPPED: mpPhysicsData is NOT cleared here (the console zeroes no word at
        // +0x2BF8); left alone rather than invented.
        void Construct();

        // DWARF :60. The console's tail of Construct: the four queue length resets
        // (+8 / +0x1F68 / +0x28D4 / +0xFB8 -- each queue's miLength) and the flag byte.
        void Clear();

        // DWARF :57 -- `void Append(const PropInputInterface *)`.
        // ⚠️ SIGNATURE CORRECTED 2026-08-10: this was committed taking a REFERENCE. Two
        // independent witnesses say pointer -- the DecFIGS DWARF renders it `*` (and renders
        // real references as `&` elsewhere in the same dump, so it discriminates), and the PS3
        // mangled symbol is `_ZN10BrnPhysics5Props18PropInputInterface6AppendEPKS1_` whose
        // `PK` is pointer-to-const (a reference would mangle `RK`).
        // Reconstructed store-for-store from X360 0x827A9CA8 (33 instructions).
        // Driven by the world prop->physics bridges
        // (WorldModule::BridgePropModuleToPhysicsModule_Prepare @0x827AB410,
        //  BridgeEntityModulesToPhysicsModule_PreScene @0x827AADB8 and _PrePhysics @0x827AAEC0).
        void Append(const PropInputInterface* lpOther);

        // ADDITIVE GROW (prop-spawn wave, 2026-08-12) -- DWARF BrnPropInputInterface.h:72
        // `void RemoveAllPropsAndParts()` and :75 `bool ShouldRemoveAllPropsAndParts() const`.
        // Both are header inlines the X360 folds at every call site: the request setter is
        // the `stb 1, 0x2C00(interface)` tail of BrnWorld::PropZoneManager::
        // RemoveAllPropsAndParts @0x822DEF50 (console +0x2C00 == mbRemoveAllPropsAndParts),
        // and the query is the matching read on the physics side. Defined inline here (the
        // console emits no out-of-line body) so callers set the flag BY NAME. Pure addition:
        // no member/layout change.
        void RemoveAllPropsAndParts()            { mbRemoveAllPropsAndParts = true; }
        bool ShouldRemoveAllPropsAndParts() const { return mbRemoveAllPropsAndParts; }

        // ADDITIVE GROW (prop-spawn wave, 2026-08-12, conductor) -- the named form of the
        // tail of BrnWorld::PropEntityModule::InitializePropPhysicsData @0x822DA840
        // (0x822DB00C..0x822DB024): `ldx r31, module, 0xCDD9C` loads mpPropPhysicsDataHeader
        // + 0x14 -- the ResourcePtr's {mpThis, muThreadId} pair, which IS a ResourceHandle --
        // and `std r31, 0x2BF8(interface)` stores it into mpPhysicsData.
        //
        // This is how the PHYSICS side receives the prop TYPE TABLE (the 219 PropTypeData
        // records out of PROPPHYSICS.BUNDLE). Without it the field stays whatever Construct
        // left it -- and Construct deliberately does not even clear it, faithfully to the
        // console -- so every physics-side prop type lookup reads a stale handle.
        //
        // Defined inline here rather than poked at +0x2BF8 from the caller: the member is
        // private, and a raw-offset write is exactly the console-offset-on-x64 pattern that
        // AGENTS.md bans (the ResourceHandle widens on the host, so +0x2BF8 is wrong here
        // anyway). Pure addition -- no member added, no layout change.
        void SetPhysicsData(const CgsResource::ResourceHandle& lrHandle) { mpPhysicsData = lrHandle; }

        // ==================================================================================
        // ⭐ ADDED 2026-08-18 (breakable-props keystone wave). THE SEVEN DWARF-DECLARED
        // QUEUE / DATA ACCESSORS (BrnPropInputInterface.h:102..:120). Without them the four
        // PropManager::Process{Add,Remove}{Prop,Part}InstanceEvents drains have no legal way
        // to reach the queues at all -- the members are private, and poking `interface +
        // 0xFB0` from the caller is exactly the console-offset-on-x64 pattern AGENTS.md bans
        // (each EventQueue's element carries a Matrix44Affine, so the console offsets do not
        // survive the host build even though these particular payloads are pointer-free).
        //
        // X360 ATTESTATION (they are folded at every call site, so there is no out-of-line
        // emission to cite -- what is cited instead is the offset each fold lands on, which
        // is the member map below, stated twice):
        //   ProcessAddPartInstanceEvents    @0x826280F8  `addi r30, r4, 0xFB0`  == mAddPartQueue
        //   ProcessRemovePartInstanceEvents @0x82627818  `addi r24, r4, 0x28CC` == mRemovePartQueue
        // and each then reads `lwz r,8(queue)` == BaseEventQueue::miLength (GetLength()) and
        // calls BaseEventQueue<T>::GetEvent(i) on it.
        //
        // Const/non-const split is the DWARF's own (:108 vs :111, :114 vs :117): the drains
        // hold `const PropInputInterface*` and bind the const overloads.
        // ==================================================================================
        const AddPhysicalPropEventQueue& GetAddPhysicalPropQueue() const { return mAddPropQueue; }      // :102
        const AddPhysicalPartEventQueue& GetAddPhysicalPartQueue() const { return mAddPartQueue; }      // :105
        const RemovePropEventQueue&      GetRemovePhysicalPropQueue() const { return mRemovePropQueue; } // :108
        RemovePropEventQueue&            GetRemovePhysicalPropQueue()       { return mRemovePropQueue; } // :111
        const RemovePartEventQueue&      GetRemovePhysicalPartQueue() const { return mRemovePartQueue; } // :114
        RemovePartEventQueue&            GetRemovePhysicalPartQueue()       { return mRemovePartQueue; } // :117

        // :120. The physics-data handle the world side seated via SetPropPhysicsData.
        // PropManager::ProcessInputs_Prepare's DWARF body hint is a single
        // `ResourcePtr<PropPhysicsDataHeader>::operator=` -- i.e. mpPhysicsData = this.
        const CgsResource::ResourceHandle& GetPropPhysicsData() const { return mpPhysicsData; }

        // :65 -- the DWARF's own spelling of the setter. The committed SetPhysicsData(const&)
        // above predates this wave and keeps its callers compiling; this is the DWARF name,
        // by value as the DWARF has it. Both write the same member; neither is a new member.
        void SetPropPhysicsData(CgsResource::ResourceHandle lHandle) { mpPhysicsData = lHandle; }

    private:
        AddPhysicalPropEventQueue mAddPropQueue;             // +0x0000   (:125)
        AddPhysicalPartEventQueue mAddPartQueue;             // +0x0FB0   (:126)
        RemovePropEventQueue      mRemovePropQueue;          // +0x1F60   (:127)
        RemovePartEventQueue      mRemovePartQueue;          // +0x28CC   (:128)
        CgsResource::ResourceHandle mpPhysicsData;           // (:129)
        bool                      mbRemoveAllPropsAndParts;  // (:130)
    };
}
}
