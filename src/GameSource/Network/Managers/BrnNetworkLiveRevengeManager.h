#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"                                 // Array<T, N>
#include "GameSource/Network/Debug Components/BrnNetworkLiveRevengeDebugComponent.h"    // BrnNetwork::LiveRevengeDebugComponent (mDebugComponent, by value)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"              // BrnNetwork::LiveRevengeRelationship (250-entry table element)

// BrnNetwork::LiveRevengeManager + LiveRevengeProfile -- the owning home for the live-revenge
// rival-history subsystem. Recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h), gated
// against the X360 ARTIST binary.
//
// SCOPE OF THIS RECONSTRUCTION: this header is currently consumed only by the Live Revenge debug
// component TU, which reaches exactly two things on the manager -- the loaded profile pointer
// (mpLiveRevengeProfile) and SendLiveRevengeRivalsToServer() -- plus the profile's relationship
// table. To keep the gate compile from cascading into the manager's many heavy, un-homed members
// (the by-value mapping-entry array, the takedown event queue, the network manager/module
// back-pointers, the dirty-rival bit array, ...), every manager member that PRECEDES
// mpLiveRevengeProfile other than the by-value debug component is modelled as a single documented
// opaque byte span (mPad_PreProfile). The DWARF member order is preserved (debug component,
// then the padded region, then the profile pointer); only the absolute X360 byte offset is not
// reproduced -- the PC gate targets x64 (8-byte pointers) where those offsets shift anyway, so no
// fixed-byte sizeof/offset assert is used. The manager's full member set + method bodies land with
// the BrnNetworkLiveRevengeManager.cpp reconstruction (its own TU); this is the declaration surface
// the debug component compiles against.

namespace BrnNetwork
{
    // BrnNetworkLiveRevengeManager.h:89 (DWARF). The saved/loaded live-revenge profile: a version
    // word followed by the fixed 250-entry relationship-history table. X360-AUTHORITATIVE: the
    // RegisterAll / UnregisterAll walkers read the table at profile+8 (lwz 0x934(manager) -> profile;
    // addi profile, 8 -> &maRelationshipTable), so the table begins at +0x08 -- the version word at
    // +0x00 plus a 4-byte alignment gap. (DecFIGS lists KI_MAX_REVENGE_HISTORY == 250 and
    // KI_VERSION_NUMBER == 6 as compile-time constants, and miVersionNumber + maRelationshipTable as
    // the two members.)
    struct LiveRevengeProfile
    {
        static const s32 KI_MAX_REVENGE_HISTORY = 250;
        static const s32 KI_VERSION_NUMBER      = 6;

        s32 miVersionNumber;                                            // +0x00
        u8  mPad_Version[4];                                            // +0x04  (align table to +0x08)
        Array<LiveRevengeRelationship, KI_MAX_REVENGE_HISTORY>
            maRelationshipTable;                                        // +0x08  (250 * 120 + count)

        void Clear();                       // own TU (BrnNetworkLiveRevengeManager.cpp)
        bool IsIncorrectVersion() const;    // own TU
        bool IsUpgradable() const;          // own TU
    };

    // BrnNetworkLiveRevengeManager.h:133 (DWARF). Owns the debug component by value (mDebugComponent
    // is the first member -- the dependency is one-directional: the debug component only forward-
    // declares this manager for its back-pointer).
    struct LiveRevengeManager
    {
        // BrnNetworkLiveRevengeManager.h:281 (DWARF).
        enum ELiveRevengeUploadStatus
        {
            E_LIVE_REVENGE_UPLOAD_STATUS_PENDING     = 0,
            E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS = 1,
            E_LIVE_REVENGE_UPLOAD_STATUS_IDLE        = 2,
            E_LIVE_REVENGE_UPLOAD_STATUS_COUNT       = 3,
        };

        // Force a server upload of the current top-rival list. Declared-only here (body in the
        // manager's own TU @ 0x82560358); the debug component's UploadToServer action tail-calls it.
        void SendLiveRevengeRivalsToServer();

        // The loaded profile (debug walkers read mpLiveRevengeProfile->maRelationshipTable).
        LiveRevengeProfile* GetProfile() const { return mpLiveRevengeProfile; }

    private:
        // DWARF member order, X360-gated. Only the first member and the profile pointer are needed by
        // the current consumer; the intervening heavy members are an opaque documented span (see the
        // header comment). The DWARF order is: mDebugComponent, maPlayerToTableIndexData[7],
        // maTopIndexes[10], miNumberOfTopRivals, mpLiveRevengeProfile, mTakedownEventQueue,
        // mpNetworkManager, mpNetworkModule, mpAllocator, meLiveRevengeUploadStatus, maDirtyTopRivals,
        // mbAreWeInOnlineGame, mbProfileIsDirty, mbNeedToUpdateMarksForCurrentRound.
        LiveRevengeDebugComponent mDebugComponent;   // DWARF :291 (by-value, first member)
        u8                        mPad_PreProfile[1]; // DWARF :302..306 (mapping array + top-rival
                                                      // indexes + count) -- opaque; size NOT
                                                      // X360-faithful (irrelevant to this TU; see note)
        LiveRevengeProfile*       mpLiveRevengeProfile; // DWARF :308 (X360 manager +0x934)
    };
}
