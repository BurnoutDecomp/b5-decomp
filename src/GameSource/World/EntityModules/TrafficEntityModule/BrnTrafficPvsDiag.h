#pragma once

#include "types.hpp"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"

#include <atomic>

namespace BrnTraffic
{
    class Pvs;
    class TrafficEntityModule;

    namespace PvsDiag
    {
        enum Phase
        {
            PHASE_BASELINE = 0,
            PHASE_ON = 1,
            PHASE_OFF = 2,
            PHASE_DONE = 3,
        };

        struct Snapshot
        {
            u64 muEpoch;
            u64 muSetCall;
            Phase mePhase;
            const TrafficEntityModule* mpModule;
            const Pvs* mpPvs;
            bool mbRunningWorstCase;
            bool mbDecisionFrame;
            bool mbDataReady;
            bool mbPvsPresent;
            u32 muFramesSinceDecision;
            u32 muActiveCount;
            u32 muAddedCount;
            u32 muRemovedCount;
            u16 mauActiveIds[KU_MAX_ACTIVE_HULLS];
            u16 mauAddedIds[KU_MAX_ACTIVE_HULLS];
            u16 mauRemovedIds[KU_MAX_ACTIVE_HULLS];

            Snapshot()
                : muEpoch(0), muSetCall(0), mePhase(PHASE_BASELINE), mpModule(nullptr),
                  mpPvs(nullptr), mbRunningWorstCase(false), mbDecisionFrame(false),
                  mbDataReady(false), mbPvsPresent(false), muFramesSinceDecision(0),
                  muActiveCount(0), muAddedCount(0), muRemovedCount(0)
            {
                for (u32 lu = 0; lu < KU_MAX_ACTIVE_HULLS; ++lu)
                {
                    mauActiveIds[lu] = 0;
                    mauAddedIds[lu] = 0;
                    mauRemovedIds[lu] = 0;
                }
            }
        };

        struct DrawBoxRecord
        {
            u16 muHull;
            u32 muColumns;
            u32 muColumn;
            u32 muRow;
        };

        struct DrawSample
        {
            const TrafficEntityModule* mpModule;
            const Pvs* mpPvs;
            bool mbRunningWorstCase;
            bool mbDrawToggle;
            bool mbDecisionFrame;
            bool mbDataReady;
            bool mbPvsPresent;
            bool mbMappingTruncated;
            u32 muFramesSinceDecision;
            u32 muCellsX;
            u32 muCellsZ;
            u32 muTotalCells;
            u32 muDrawRows;
            u32 muBoxCalls;
            u32 muMappingCount;
            f32 mfBoxStepX;
            f32 mfBoxStepY;
            DrawBoxRecord maBoxes[KU_MAX_ACTIVE_HULLS];

            DrawSample()
                : mpModule(nullptr), mpPvs(nullptr), mbRunningWorstCase(false),
                  mbDrawToggle(false), mbDecisionFrame(false), mbDataReady(false),
                  mbPvsPresent(false), mbMappingTruncated(false), muFramesSinceDecision(0),
                  muCellsX(0), muCellsZ(0), muTotalCells(0), muDrawRows(0),
                  muBoxCalls(0), muMappingCount(0),
                  mfBoxStepX(0.0f), mfBoxStepY(0.0f)
            {
                for (u32 lu = 0; lu < KU_MAX_ACTIVE_HULLS; ++lu)
                {
                    maBoxes[lu].muHull = 0;
                    maBoxes[lu].muColumns = 0;
                    maBoxes[lu].muColumn = 0;
                    maBoxes[lu].muRow = 0;
                }
            }
        };

        struct Mailbox
        {
            std::atomic_flag mLock = ATOMIC_FLAG_INIT;
            u64 muNextEpoch = 0;
            u64 muAcceptedSetCalls = 0;
            u64 muDrawSequence = 0;
            Phase meNextPhase = PHASE_BASELINE;
            bool mbPending = false;
            Snapshot mPending;
        };

        class TryLock
        {
        public:
            explicit TryLock(Mailbox& lrMailbox)
                : mrMailbox(lrMailbox),
                  mbAcquired(!mrMailbox.mLock.test_and_set(std::memory_order_acquire))
            {
            }

            ~TryLock()
            {
                if (mbAcquired)
                    mrMailbox.mLock.clear(std::memory_order_release);
            }

            bool Acquired() const { return mbAcquired; }
            TryLock(const TryLock&) = delete;
            TryLock& operator=(const TryLock&) = delete;

        private:
            Mailbox& mrMailbox;
            bool mbAcquired;
        };

        extern Mailbox gMailbox;

        bool Enabled();
        void PublishAfterSet(const Snapshot& lrSource);
        bool CopyPending(Snapshot& lrOut);
        void InvalidateIfToggleMismatch(Phase lePhase, bool lbCurrentRunningWorstCase);
        bool ConsumeIfMatching(const Snapshot& lrPublished, const DrawSample& lrDraw);
    }
}