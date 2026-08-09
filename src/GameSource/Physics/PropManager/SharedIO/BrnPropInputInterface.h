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
