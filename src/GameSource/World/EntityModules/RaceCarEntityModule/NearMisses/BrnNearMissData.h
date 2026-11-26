#pragma once

// Near-miss bookkeeping element type + the NearMissData<A,B> template that owns the
// per-category "remembered vehicle" lists. DWARF home
// GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h.
//
// BrnWorld::NearMissData<A, B> tracks recently near-missed / contacted / crashed /
// taken-down / checked traffic vehicles for a race car. Each "remembered" list is an
// inline Array<VehicleTimePair, N> of (entity-id, time) records that age out over time.
// VehicleTimePair is the per-record element type (DWARF BrnNearMissData.h:122-124): an
// entity id and the timestamp it was logged. The two instantiations the X360 emits are
// <4,7> (the race-car near-miss vein) and <4,8> (the sibling with an 8-slot long-list).
//
// LAYOUT (X360-authoritative, recovered store-for-store from the member accessors of both
// instantiations; A = the "near" capacity, B = the "long" capacity):
//   +0x00  mauCurrentFrameNearTraffic        Array<u32,A>              (count word @ +0x10)
//   +0x14  mauPreviousFrameNearTraffic  Array<u32,A>              (count word @ +0x24)
//   +0x28  maRecentTrafficNearMisses            Array<VehicleTimePair,A>  (count word @ +0x48)
//   +0x4C  maRecentTrafficContacted           Array<VehicleTimePair,A>  (count word @ +0x6C)
//   +0x70  maRecentlyCrashedTraffic             Array<VehicleTimePair,B>  (count word @ +0xA8 for B=7 / +0xB0 for B=8)
//   +0xAC  maRecentlyTakenDownTraffic           Array<VehicleTimePair,B>  (<4,7>) / +0xB4 (<4,8>)
//   +0xE8  maRecentlyCheckedTraffic             Array<VehicleTimePair,B>  (<4,7>) / +0xF8 (<4,8>)
// The two int arrays are proven by BackupNearArray (int_A_::GetItem/Append on +0x00/+0x14),
// while every adder/query calls VehicleTimePair_{A,B}_::Append/operator[] on the record
// lists. The <4,8> instantiation's last three arrays chain 0x70 -> 0xB4 -> 0xF8 exactly
// because Array<VehicleTimePair,8> is 8*8 + 4 = 0x44 wide (count @ buffer+0x40); the <4,7>
// instantiation chains 0x70 -> 0xAC -> 0xE8 because Array<VehicleTimePair,7> is 7*8 + 4 =
// 0x3C wide (count @ buffer+0x38). Both fall straight out of B.
//
// sizeof(VehicleTimePair) == 8 is X360-authoritative: Array<VehicleTimePair,4>::Append
// (@0x822AF0E0, count @ buffer+0x20 == 4*8) and Array<VehicleTimePair,7>::Append
// (@0x822AF2C0, count @ buffer+0x38 == 7*8) index the inline buffer with an 8-byte stride
// (slwi r,count,3) and write two 4-byte stores: id@0 + time@4.
#include "types.hpp"                                     // u32 / s32 / f32
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N> (inline generic)

namespace BrnWorld
{
    // Remember-timer seconds, namespace-scope per DWARF (BrnNearMissData.h:39/40): 0.5f for
    // near/contacted/crashed/checked (ARTIST lfs 0x820147FC), 10.0f for taken-down (0x820149B0).
    const f32 KF_INFORMATION_REMEMBER_TIME_GENERAL   = 0.5f;
    const f32 KF_INFORMATION_REMEMBER_TIME_TAKEDOWNS = 10.0f;

    // Per-race-car near-miss/contact/crash/takedown/check bookkeeping. The bodies below are
    // all inline template definitions (both the <4,7> and <4,8> instantiations force them via
    // the explicit `template class` instantiations in BrnNearMissData.cpp). Member names and
    // method const-ness follow DWARF (BrnNearMissData.h:132..138, cpp:44..545); the query
    // loops re-test GetLength() each pass as in LegacySrc (ARTIST re-loads the count word and
    // re-runs its Construct assert per iteration). The X360 addresses of each instantiation's
    // copy are noted per method.
    template <u32 A, u32 B>
    class NearMissData
    {
    public:
        // One remembered traffic vehicle and the time it was logged. DWARF nests it in each
        // instantiation (BrnNearMissData.h:122 `struct NearMissData<4u,8u>::VehicleTimePair`),
        // and ARTIST emits one Array instance per outer type: Array<NearMissData<4,8>::
        // VehicleTimePair,4>::Erase @0x822AEE48 beside Array<NearMissData<4,7>::VehicleTimePair,4>
        // ::Erase @0x822AF208. DWARF spells the members miEntityId (uint32_t) and mfTime.
        struct VehicleTimePair
        {
            u32 miEntityId;   // BrnNearMissData.h:123  (record id, elem+0)
            f32 mfTime;       // BrnNearMissData.h:124  (remaining timer, elem+4)
        };

        // Prepare -- declared at. Empties all seven lists: one `stw 0` into each count word
        // (+0x10, +0x24, +0x48, +0x6C and the three B-sized lists), emitted inline in
        // RaceCarEntityModule::Prepare through NearMissManager::Prepare. That call site
        // does not test the result.
        bool Prepare()
        {
            mauCurrentFrameNearTraffic.Clear();
            mauPreviousFrameNearTraffic.Clear();
            maRecentTrafficNearMisses.Clear();
            maRecentTrafficContacted.Clear();
            maRecentlyCrashedTraffic.Clear();
            maRecentlyTakenDownTraffic.Clear();
            maRecentlyCheckedTraffic.Clear();
            return true;
        }

        // ---- logging / ageing (mutating) ----

        // RememberNearMiss -- <4,7> @0x822E4608 / <4,8> @0x822E3CC8. Log luEntityId into the
        // near-miss list with a fresh 0.5s timer, de-duping any prior record and evicting the
        // oldest (front) when the list is full.
        void RememberNearMiss(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_INFORMATION_REMEMBER_TIME_GENERAL;

            for (u32 luIndex = 0; luIndex < maRecentTrafficNearMisses.GetLength(); ++luIndex)
            {
                if (maRecentTrafficNearMisses[luIndex].miEntityId == luEntityId)
                {
                    maRecentTrafficNearMisses.Erase(luIndex);
                    break;
                }
            }
            if (maRecentTrafficNearMisses.GetLength() == A)
            {
                maRecentTrafficNearMisses.Erase(0);
            }
            maRecentTrafficNearMisses.Append(loRecord);
        }

        // AddContacted -- <4,7> @0x822E4780 / <4,8> @0x822E3E40. (maRecentTrafficContacted, cap A.)
        void AddContacted(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_INFORMATION_REMEMBER_TIME_GENERAL;
            for (u32 luIndex = 0; luIndex < maRecentTrafficContacted.GetLength(); ++luIndex)
            {
                if (maRecentTrafficContacted[luIndex].miEntityId == luEntityId)
                {
                    maRecentTrafficContacted.Erase(luIndex);
                    break;
                }
            }
            if (maRecentTrafficContacted.GetLength() == maRecentTrafficContacted.GetSize())
            {
                maRecentTrafficContacted.Erase(0);
            }
            maRecentTrafficContacted.Append(loRecord);
        }

        // AddCrashed -- <4,7> @0x822E4868 / <4,8> @0x822E3F28. (maRecentlyCrashedTraffic, cap B.)
        void AddCrashed(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_INFORMATION_REMEMBER_TIME_GENERAL;
            for (u32 luIndex = 0; luIndex < maRecentlyCrashedTraffic.GetLength(); ++luIndex)
            {
                if (maRecentlyCrashedTraffic[luIndex].miEntityId == luEntityId)
                {
                    maRecentlyCrashedTraffic.Erase(luIndex);
                    break;
                }
            }
            if (maRecentlyCrashedTraffic.GetLength() == maRecentlyCrashedTraffic.GetSize())
            {
                maRecentlyCrashedTraffic.Erase(0);
            }
            maRecentlyCrashedTraffic.Append(loRecord);
        }

        // AddTakenDown -- <4,7> @0x822E4950. (maRecentlyTakenDownTraffic, cap B; the longer 10s timer.)
        void AddTakenDown(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_INFORMATION_REMEMBER_TIME_TAKEDOWNS;
            for (u32 luIndex = 0; luIndex < maRecentlyTakenDownTraffic.GetLength(); ++luIndex)
            {
                if (maRecentlyTakenDownTraffic[luIndex].miEntityId == luEntityId)
                {
                    maRecentlyTakenDownTraffic.Erase(luIndex);
                    break;
                }
            }
            if (maRecentlyTakenDownTraffic.GetLength() == maRecentlyTakenDownTraffic.GetSize())
            {
                maRecentlyTakenDownTraffic.Erase(0);
            }
            maRecentlyTakenDownTraffic.Append(loRecord);
        }

        // AddChecked -- <4,8> @0x822E4010. (maRecentlyCheckedTraffic, cap B.)
        void AddChecked(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_INFORMATION_REMEMBER_TIME_GENERAL;
            for (u32 luIndex = 0; luIndex < maRecentlyCheckedTraffic.GetLength(); ++luIndex)
            {
                if (maRecentlyCheckedTraffic[luIndex].miEntityId == luEntityId)
                {
                    maRecentlyCheckedTraffic.Erase(luIndex);
                    break;
                }
            }
            if (maRecentlyCheckedTraffic.GetLength() == maRecentlyCheckedTraffic.GetSize())
            {
                maRecentlyCheckedTraffic.Erase(0);
            }
            maRecentlyCheckedTraffic.Append(loRecord);
        }

        // UpdateTimers -- <4,7> @0x822E4330 / <4,8> @0x822E39F0. Age every remembered record
        // by lfDeltaTime across all five VehicleTimePair lists (in declared order) and drop any
        // whose timer has fallen below zero (Erase + re-visit the shifted-down successor).
        void UpdateTimers(f32 lfDeltaTime)
        {
            for (u32 luIndex = 0; luIndex < maRecentTrafficNearMisses.GetLength(); ++luIndex)
            {
                maRecentTrafficNearMisses[luIndex].mfTime -= lfDeltaTime;
                if (maRecentTrafficNearMisses[luIndex].mfTime < 0.0f) { maRecentTrafficNearMisses.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maRecentTrafficContacted.GetLength(); ++luIndex)
            {
                maRecentTrafficContacted[luIndex].mfTime -= lfDeltaTime;
                if (maRecentTrafficContacted[luIndex].mfTime < 0.0f) { maRecentTrafficContacted.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maRecentlyCrashedTraffic.GetLength(); ++luIndex)
            {
                maRecentlyCrashedTraffic[luIndex].mfTime -= lfDeltaTime;
                if (maRecentlyCrashedTraffic[luIndex].mfTime < 0.0f) { maRecentlyCrashedTraffic.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maRecentlyTakenDownTraffic.GetLength(); ++luIndex)
            {
                maRecentlyTakenDownTraffic[luIndex].mfTime -= lfDeltaTime;
                if (maRecentlyTakenDownTraffic[luIndex].mfTime < 0.0f) { maRecentlyTakenDownTraffic.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maRecentlyCheckedTraffic.GetLength(); ++luIndex)
            {
                maRecentlyCheckedTraffic[luIndex].mfTime -= lfDeltaTime;
                if (maRecentlyCheckedTraffic[luIndex].mfTime < 0.0f) { maRecentlyCheckedTraffic.Erase(luIndex); --luIndex; }
            }
        }

        // BackupNearArray -- <4,7> @0x822E4BE8 / <4,8> @0x822E42A8. Snapshot the live
        // near-section list into the backup (clear backup first), then clear the live list.
        // Both are Array<u32,A> (AISectionId lists), distinct from the VehicleTimePair lists.
        void BackupNearArray()
        {
            mauPreviousFrameNearTraffic.Clear();
            for (u32 luIndex = 0; luIndex < mauCurrentFrameNearTraffic.GetLength(); ++luIndex)
            {
                mauPreviousFrameNearTraffic.Append(mauCurrentFrameNearTraffic.GetItem(luIndex));
            }
            mauCurrentFrameNearTraffic.Clear();
        }

        // AddNear -- append luEntityId to the current-frame near-section working list (cap A).
        // The length<A guard silently drops the id when the working list is full. The LegacySrc
        // AddNear body confirms there is NO assert here; the not-contained assert lives in the
        // NearMissManager caller (AddNearRaceCar/AddNearTraffic). (DWARF cpp:212)
        void AddNear(u32 luEntityId)
        {
            if (mauCurrentFrameNearTraffic.GetLength() < A) { mauCurrentFrameNearTraffic.Append(luEntityId); }
        }

        // IsContainedInNearArray -- is luEntityId already in the current-frame near-section list?
        // Array<u32,A>::Contains on mauCurrentFrameNearTraffic. Non-const per DWARF (cpp:516).
        bool IsContainedInNearArray(u32 luEntityId)
        {
            return mauCurrentFrameNearTraffic.Contains(luEntityId);
        }

        // ---- queries (const) ----

        // GetNumPreviousFrameNearElements / GetPreviousFrameNearElement -- read back the
        // previous frame's near-section working list (the mauPreviousFrameNearTraffic snapshot that
        // BackupNearArray populated). NearMissManager::Update iterates these to replay each
        // remembered vehicle. The X360 inlines both as mauPreviousFrameNearTraffic.GetLength()/GetItem
        // (count @ +0x24, GetItem on the +0x14 array) -- see NearMissManager::Update.
        // Non-const per DWARF (cpp:531/545): ARTIST's NearMissManager::Update calls the
        // non-const Array<u32,4>::GetItem (0x822B02F0, asserts CgsArray.h:556/557).
        u32 GetNumPreviousFrameNearElements()
        {
            return mauPreviousFrameNearTraffic.GetLength();
        }
        u32 GetPreviousFrameNearElement(u32 luIndex)
        {
            return mauPreviousFrameNearTraffic.GetItem(luIndex);
        }

        // HasRecentlyCrashed -- <4,7> @0x822E46F0 / <4,8> @0x822E3DB0.
        // Non-const (DWARF cpp:101): ARTIST calls the non-const operator[] instance
        // (<4,8> sub_822B0608, <4,7> sub_822B0A28, shared with UpdateTimers).
        bool HasRecentlyCrashed(u32 luEntityId)
        {
            for (u32 luIndex = 0; luIndex < maRecentlyCrashedTraffic.GetLength(); ++luIndex)
            {
                if (luEntityId == maRecentlyCrashedTraffic[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // HasThereBeenARecentNearMiss -- <4,7> @0x822C8988. True iff the near-miss list is non-empty.
        bool HasThereBeenARecentNearMiss() const
        {
            return maRecentTrafficNearMisses.GetLength() > 0;   // LegacySrc; subfic/subfe at 0x822C89CC
        }

        // IsRecentNearMiss -- <4,7> @0x822E4AC8 / <4,8> @0x822E40F8.
        bool IsRecentNearMiss(u32 luEntityId) const
        {
            for (u32 luIndex = 0; luIndex < maRecentTrafficNearMisses.GetLength(); ++luIndex)
            {
                if (luEntityId == maRecentTrafficNearMisses[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // IsRecentContacted -- <4,7> @0x822E4B58 / <4,8> @0x822E4188.
        bool IsRecentContacted(u32 luEntityId) const
        {
            for (u32 luIndex = 0; luIndex < maRecentTrafficContacted.GetLength(); ++luIndex)
            {
                if (luEntityId == maRecentTrafficContacted[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // IsRecentlyTakeDown -- <4,7> @0x822E4A38. (maRecentlyTakenDownTraffic, cap B.)
        // Non-const (DWARF cpp:299): calls the non-const operator[] sub_822B0A28.
        bool IsRecentlyTakeDown(u32 luEntityId)
        {
            for (u32 luIndex = 0; luIndex < maRecentlyTakenDownTraffic.GetLength(); ++luIndex)
            {
                if (luEntityId == maRecentlyTakenDownTraffic[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // IsRecentTrafficCheck -- <4,8> @0x822E4218. (maRecentlyCheckedTraffic, cap B.)
        bool IsRecentTrafficCheck(u32 luEntityId) const
        {
            for (u32 luIndex = 0; luIndex < maRecentlyCheckedTraffic.GetLength(); ++luIndex)
            {
                if (luEntityId == maRecentlyCheckedTraffic[luIndex].miEntityId) { return true; }
            }
            return false;
        }

    private:
        // Head: section-index working list (count @ +0x10) and its backup (count @ +0x24).
        // DWARF h:132/133: Array<BrnAI::AISectionId,4u>, AISectionId = uint32_t
        // (SharedClasses/AI/AISectionsData.h:214); ARTIST compares the elements unsigned
        // (FindFirstInstanceOf 0x822B0258: cmplw).
        CgsContainers::Array<u32, A>             mauCurrentFrameNearTraffic;        // +0x00
        CgsContainers::Array<u32, A>             mauPreviousFrameNearTraffic;  // +0x14
        // Remembered-vehicle record lists.
        CgsContainers::Array<VehicleTimePair, A> maRecentTrafficNearMisses;            // +0x28
        CgsContainers::Array<VehicleTimePair, A> maRecentTrafficContacted;           // +0x4C
        CgsContainers::Array<VehicleTimePair, B> maRecentlyCrashedTraffic;             // +0x70
        CgsContainers::Array<VehicleTimePair, B> maRecentlyTakenDownTraffic;           // +0xAC (<4,7>) / +0xB4 (<4,8>)
        CgsContainers::Array<VehicleTimePair, B> maRecentlyCheckedTraffic;             // +0xE8 (<4,7>) / +0xF8 (<4,8>)
    };

}
