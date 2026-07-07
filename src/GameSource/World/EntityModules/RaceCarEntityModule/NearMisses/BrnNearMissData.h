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
//   +0x00  maNearSections        Array<s32,A>              (count word @ +0x10)
//   +0x14  maNearSectionsBackup  Array<s32,A>              (count word @ +0x24)
//   +0x28  maNearMiss            Array<VehicleTimePair,A>  (count word @ +0x48)
//   +0x4C  maContacted           Array<VehicleTimePair,A>  (count word @ +0x6C)
//   +0x70  maCrashed             Array<VehicleTimePair,B>  (count word @ +0xA8 for B=7 / +0xB0 for B=8)
//   +0xAC  maTakenDown           Array<VehicleTimePair,B>  (<4,7>) / +0xB4 (<4,8>)
//   +0xE8  maChecked             Array<VehicleTimePair,B>  (<4,7>) / +0xF8 (<4,8>)
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
    // One remembered traffic vehicle and the time it was logged. DWARF spells the members
    // miEntityId (uint32_t) and mfTime (the project f32; DWARF float32_t == 32-bit IEEE).
    // Namespace-scoped (NOT nested) so the committed Array_VehicleTimePair_{4,7,8}.cpp
    // explicit instantiations that name BrnWorld::VehicleTimePair bind the same symbol.
    struct VehicleTimePair
    {
        u32 miEntityId;   // BrnNearMissData.h:123  (record id, elem+0)
        f32 mfTime;       // BrnNearMissData.h:124  (remaining timer, elem+4)
    };

    // Per-race-car near-miss/contact/crash/takedown/check bookkeeping. The bodies below are
    // all inline template definitions (both the <4,7> and <4,8> instantiations force them via
    // the explicit `template class` instantiations in BrnNearMissData.cpp). Method const-ness
    // is normalised: every pure-query method is const (they only read), every logging/ageing
    // method is non-const. The X360 addresses of each instantiation's copy are noted per method.
    template <u32 A, u32 B>
    class NearMissData
    {
    public:
        // Remember-timer seconds (X360 rodata): 0.5f for near/contacted/crashed/checked
        // (flt_820147FC), 10.0f for taken-down (flt_820149B0). constexpr so no out-of-line
        // template-static definition is required to satisfy the odr-uses below.
        static constexpr f32 KF_NEAR_MISS_REMEMBER_TIME  = 0.5f;
        static constexpr f32 KF_TAKEN_DOWN_REMEMBER_TIME = 10.0f;

        // ---- logging / ageing (mutating) ----

        // RememberNearMiss -- <4,7> @0x822E4608 / <4,8> @0x822E3CC8. Log luEntityId into the
        // near-miss list with a fresh 0.5s timer, de-duping any prior record and evicting the
        // oldest (front) when the list is full.
        void RememberNearMiss(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_NEAR_MISS_REMEMBER_TIME;

            const u32 luCount = maNearMiss.GetLength();
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                if (maNearMiss[luIndex].miEntityId == luEntityId)
                {
                    maNearMiss.Erase(luIndex);
                    break;
                }
            }
            if (maNearMiss.GetLength() == A)
            {
                maNearMiss.Erase(0);
            }
            maNearMiss.Append(loRecord);
        }

        // AddContacted -- <4,7> @0x822E4780 / <4,8> @0x822E3E40. (maContacted, cap A.)
        void AddContacted(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_NEAR_MISS_REMEMBER_TIME;
            for (u32 luIndex = 0; luIndex < maContacted.GetLength(); ++luIndex)
            {
                if (maContacted[luIndex].miEntityId == luEntityId)
                {
                    maContacted.Erase(luIndex);
                    break;
                }
            }
            if (maContacted.GetLength() == maContacted.GetSize())
            {
                maContacted.Erase(0);
            }
            maContacted.Append(loRecord);
        }

        // AddCrashed -- <4,7> @0x822E4868 / <4,8> @0x822E3F28. (maCrashed, cap B.)
        void AddCrashed(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_NEAR_MISS_REMEMBER_TIME;
            for (u32 luIndex = 0; luIndex < maCrashed.GetLength(); ++luIndex)
            {
                if (maCrashed[luIndex].miEntityId == luEntityId)
                {
                    maCrashed.Erase(luIndex);
                    break;
                }
            }
            if (maCrashed.GetLength() == maCrashed.GetSize())
            {
                maCrashed.Erase(0);
            }
            maCrashed.Append(loRecord);
        }

        // AddTakenDown -- <4,7> @0x822E4950. (maTakenDown, cap B; the longer 10s timer.)
        void AddTakenDown(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_TAKEN_DOWN_REMEMBER_TIME;
            for (u32 luIndex = 0; luIndex < maTakenDown.GetLength(); ++luIndex)
            {
                if (maTakenDown[luIndex].miEntityId == luEntityId)
                {
                    maTakenDown.Erase(luIndex);
                    break;
                }
            }
            if (maTakenDown.GetLength() == maTakenDown.GetSize())
            {
                maTakenDown.Erase(0);
            }
            maTakenDown.Append(loRecord);
        }

        // AddChecked -- <4,8> @0x822E4010. (maChecked, cap B.)
        void AddChecked(u32 luEntityId)
        {
            VehicleTimePair loRecord;
            loRecord.miEntityId = luEntityId;
            loRecord.mfTime     = KF_NEAR_MISS_REMEMBER_TIME;
            for (u32 luIndex = 0; luIndex < maChecked.GetLength(); ++luIndex)
            {
                if (maChecked[luIndex].miEntityId == luEntityId)
                {
                    maChecked.Erase(luIndex);
                    break;
                }
            }
            if (maChecked.GetLength() == maChecked.GetSize())
            {
                maChecked.Erase(0);
            }
            maChecked.Append(loRecord);
        }

        // UpdateTimers -- <4,7> @0x822E4330 / <4,8> @0x822E39F0. Age every remembered record
        // by lfDeltaTime across all five VehicleTimePair lists (in declared order) and drop any
        // whose timer has fallen below zero (Erase + re-visit the shifted-down successor).
        void UpdateTimers(f32 lfDeltaTime)
        {
            for (u32 luIndex = 0; luIndex < maNearMiss.GetLength(); ++luIndex)
            {
                maNearMiss[luIndex].mfTime -= lfDeltaTime;
                if (maNearMiss[luIndex].mfTime < 0.0f) { maNearMiss.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maContacted.GetLength(); ++luIndex)
            {
                maContacted[luIndex].mfTime -= lfDeltaTime;
                if (maContacted[luIndex].mfTime < 0.0f) { maContacted.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maCrashed.GetLength(); ++luIndex)
            {
                maCrashed[luIndex].mfTime -= lfDeltaTime;
                if (maCrashed[luIndex].mfTime < 0.0f) { maCrashed.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maTakenDown.GetLength(); ++luIndex)
            {
                maTakenDown[luIndex].mfTime -= lfDeltaTime;
                if (maTakenDown[luIndex].mfTime < 0.0f) { maTakenDown.Erase(luIndex); --luIndex; }
            }
            for (u32 luIndex = 0; luIndex < maChecked.GetLength(); ++luIndex)
            {
                maChecked[luIndex].mfTime -= lfDeltaTime;
                if (maChecked[luIndex].mfTime < 0.0f) { maChecked.Erase(luIndex); --luIndex; }
            }
        }

        // BackupNearArray -- <4,7> @0x822E4BE8 / <4,8> @0x822E42A8. Snapshot the live
        // near-section list into the backup (clear backup first), then clear the live list.
        // Both are Array<s32,A> (section-index ints), distinct from the VehicleTimePair lists.
        void BackupNearArray()
        {
            maNearSectionsBackup.Clear();
            for (u32 luIndex = 0; luIndex < maNearSections.GetLength(); ++luIndex)
            {
                maNearSectionsBackup.Append(maNearSections.GetItem(luIndex));
            }
            maNearSections.Clear();
        }

        // AddNear -- append luEntityId to the current-frame near-section working list (cap A).
        // The length<A guard silently drops the id when the working list is full. The Feb-2007
        // AddNear body confirms there is NO assert here; the not-contained assert lives in the
        // NearMissManager caller (AddNearRaceCar/AddNearTraffic). (DWARF cpp:212)
        void AddNear(u32 luEntityId)
        {
            if (maNearSections.GetLength() < A) { maNearSections.Append(static_cast<s32>(luEntityId)); }
        }

        // IsContainedInNearArray -- is luEntityId already in the current-frame near-section list?
        // Array<s32,A>::Contains on maNearSections. Non-const per DWARF (cpp:516).
        bool IsContainedInNearArray(u32 luEntityId)
        {
            return maNearSections.Contains(static_cast<s32>(luEntityId));
        }

        // ---- queries (const) ----

        // GetNumPreviousFrameNearElements / GetPreviousFrameNearElement -- read back the
        // previous frame's near-section working list (the maNearSectionsBackup snapshot that
        // BackupNearArray populated). NearMissManager::Update iterates these to replay each
        // remembered vehicle. The X360 inlines both as maNearSectionsBackup.GetLength()/GetItem
        // (count @ +0x24, GetItem on the +0x14 array) -- see NearMissManager::Update.
        u32 GetNumPreviousFrameNearElements() const
        {
            return maNearSectionsBackup.GetLength();
        }
        u32 GetPreviousFrameNearElement(u32 luIndex) const
        {
            return static_cast<u32>(maNearSectionsBackup.GetItem(luIndex));
        }

        // HasRecentlyCrashed -- <4,7> @0x822E46F0 / <4,8> @0x822E3DB0.
        bool HasRecentlyCrashed(u32 luEntityId) const
        {
            for (u32 luIndex = 0; luIndex < maCrashed.GetLength(); ++luIndex)
            {
                if (luEntityId == maCrashed[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // HasThereBeenARecentNearMiss -- <4,7> @0x822C8988. True iff the near-miss list is non-empty.
        bool HasThereBeenARecentNearMiss() const
        {
            return maNearMiss.GetLength() != 0;
        }

        // IsRecentNearMiss -- <4,7> @0x822E4AC8 / <4,8> @0x822E40F8.
        bool IsRecentNearMiss(u32 luEntityId) const
        {
            const u32 luCount = maNearMiss.GetLength();
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                if (luEntityId == maNearMiss[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // IsRecentContacted -- <4,7> @0x822E4B58 / <4,8> @0x822E4188.
        bool IsRecentContacted(u32 luEntityId) const
        {
            const u32 luCount = maContacted.GetLength();
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                if (luEntityId == maContacted[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // IsRecentlyTakeDown -- <4,7> @0x822E4A38. (maTakenDown, cap B.)
        bool IsRecentlyTakeDown(u32 luEntityId) const
        {
            const u32 luCount = maTakenDown.GetLength();
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                if (luEntityId == maTakenDown[luIndex].miEntityId) { return true; }
            }
            return false;
        }

        // IsRecentTrafficCheck -- <4,8> @0x822E4218. (maChecked, cap B.)
        bool IsRecentTrafficCheck(u32 luEntityId) const
        {
            const u32 luCount = maChecked.GetLength();
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                if (luEntityId == maChecked[luIndex].miEntityId) { return true; }
            }
            return false;
        }

    private:
        // Head: section-index working list (count @ +0x10) and its backup (count @ +0x24).
        Array<s32, A>             maNearSections;        // +0x00
        Array<s32, A>             maNearSectionsBackup;  // +0x14
        // Remembered-vehicle record lists.
        Array<VehicleTimePair, A> maNearMiss;            // +0x28
        Array<VehicleTimePair, A> maContacted;           // +0x4C
        Array<VehicleTimePair, B> maCrashed;             // +0x70
        Array<VehicleTimePair, B> maTakenDown;           // +0xAC (<4,7>) / +0xB4 (<4,8>)
        Array<VehicleTimePair, B> maChecked;             // +0xE8 (<4,7>) / +0xF8 (<4,8>)
    };
}
