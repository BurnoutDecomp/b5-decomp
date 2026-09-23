#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (streamed asserts)

// @ 0x82355540  int __fastcall BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns(_DWORD *a1)
// Sum of the local player's and the rival's lifetime takedowns across the whole
// relationship. X360: return *a1 + a1[9]  ->  mOverallStats.mPlayerStats.miTakedowns
// (+0) + mOverallStats.mRivalStats.miTakedowns (+36). The X360 build asserts the
// total is non-negative before returning it.
// Original assert site: GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h:389
s32 BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns() const
{
    const s32 liTotal = mOverallStats.mPlayerStats.miTakedowns +
                        mOverallStats.mRivalStats.miTakedowns;

    CGS_ASSERT(liTotal >= 0,
               "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");

    return liTotal;
}

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------
    // Lifecycle. Release, Destruct and the debug clear run the header-inline Clear(); Prepare
    // clears, stamps the change time and adopts the rival's identity.
    // ----------------------------------------------------------------------------------

    bool LiveRevengeRelationship::Prepare(const UniquePlayerID* lpUniquePlayerID)
    {
        CGS_ASSERT(lpUniquePlayerID, "lpUniquePlayerID");
        // PlayerName::IsEmpty (inline): the first name character is the terminator.
        CGS_ASSERT(lpUniquePlayerID->GetPlayerName()[0] != '\0',
                   "!lpUniquePlayerID->GetPlayerName()->IsEmpty()");

        Clear();
        mLastTimeChanged.SetLocal(false);
        mLastTimeChanged.Update();

        // UniquePlayerID::IsValid (inline): a non-empty name and a non-zero XUID.
        CGS_ASSERT(lpUniquePlayerID->GetPlayerName()[0] != '\0' && lpUniquePlayerID->mqXuid != 0,
                   "lpUniquePlayerID->IsValid()");

        mUniqueID = *lpUniquePlayerID;
        return true;
    }

    // Reset the relationship in place; always succeeds.
    bool LiveRevengeRelationship::Release()
    {
        Clear();
        return true;
    }

    // Reset the relationship in place.
    void LiveRevengeRelationship::Destruct()
    {
        Clear();
    }

    // ----------------------------------------------------------------------------------
    // Takedowns. A takedown against the side that was ahead settles the score (the running
    // score returns to zero and that side's settled count rises); otherwise it extends the
    // taker's streak. The streak high-water mark and the marked-man count follow, then the
    // change time is stamped.
    // ----------------------------------------------------------------------------------

    void LiveRevengeRelationship::AddTakedownByLocalPlayer(bool lbMarkedMan)
    {
        ++mOverallStats.mPlayerStats.miTakedowns;

        if (miCurrentScoreForPlayersPointOfView < 0)
        {
            ++mOverallStats.mPlayerStats.miScoresSettled;
            mOverallStats.mPlayerStats.miEventsSinceLastTakedown = 0;
            miCurrentScoreForPlayersPointOfView = 0;
        }
        else
        {
            ++miCurrentScoreForPlayersPointOfView;
        }

        if (miCurrentScoreForPlayersPointOfView > mOverallStats.mPlayerStats.miLongestStreak)
        {
            mOverallStats.mPlayerStats.miLongestStreak = miCurrentScoreForPlayersPointOfView;
        }

        if (lbMarkedMan)
        {
            ++mOverallStats.mPlayerStats.miScalps;
        }

        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddTakedownByRival(bool lbMarkedMan)
    {
        ++mOverallStats.mRivalStats.miTakedowns;

        if (miCurrentScoreForPlayersPointOfView > 0)
        {
            ++mOverallStats.mRivalStats.miScoresSettled;
            mOverallStats.mRivalStats.miEventsSinceLastTakedown = 0;
            miCurrentScoreForPlayersPointOfView = 0;
        }
        else
        {
            --miCurrentScoreForPlayersPointOfView;
        }

        if (-miCurrentScoreForPlayersPointOfView > mOverallStats.mRivalStats.miLongestStreak)
        {
            mOverallStats.mRivalStats.miLongestStreak = -miCurrentScoreForPlayersPointOfView;
        }

        if (lbMarkedMan)
        {
            ++mOverallStats.mRivalStats.miScalps;
        }

        mLastTimeChanged.Update();
    }

    // ----------------------------------------------------------------------------------
    // Single-counter events (no out-of-line console bodies; each is inlined at its one
    // manager call site as the increment followed by the change-time stamp).
    // ----------------------------------------------------------------------------------

    void LiveRevengeRelationship::AddPaybackDealtByLocalPlayer()
    {
        ++mOverallStats.mPlayerStats.miPaybacksDealt;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddPaybackDealtByRival()
    {
        ++mOverallStats.mRivalStats.miPaybacksDealt;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddPaybackScoredByLocalPlayer()
    {
        ++mOverallStats.mPlayerStats.miPaybacksScored;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddPaybackScoredByRival()
    {
        ++mOverallStats.mRivalStats.miPaybacksScored;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddWinByLocalPlayer()
    {
        ++mOverallStats.mPlayerStats.miWins;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddWinByRival()
    {
        ++mOverallStats.mRivalStats.miWins;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddMarkByPlayer()
    {
        ++mOverallStats.mPlayerStats.miMarks;
        mLastTimeChanged.Update();
    }

    void LiveRevengeRelationship::AddMarkByRival()
    {
        ++mOverallStats.mRivalStats.miMarks;
        mLastTimeChanged.Update();
    }

    // One more shared event with this rival.
    void LiveRevengeRelationship::OnRoundFinish()
    {
        ++miTotalEvents;
        mLastTimeChanged.Update();
    }

    // ----------------------------------------------------------------------------------
    // Point of view. A relationship received from the rival is expressed from the rival's
    // side; flipping negates the running score and swaps the two stat blocks.
    // ----------------------------------------------------------------------------------

    void LiveRevengeRelationship::FlipCommonRelationship(CommonRelationship* lpRelationship)
    {
        CommonRelationshipStats lStats = lpRelationship->mRivalStats;
        lpRelationship->mRivalStats  = lpRelationship->mPlayerStats;
        lpRelationship->mPlayerStats = lStats;
    }

    void LiveRevengeRelationship::FlipPointOfView()
    {
        miCurrentScoreForPlayersPointOfView = -miCurrentScoreForPlayersPointOfView;
        FlipCommonRelationship(&mOverallStats);
    }

    // ----------------------------------------------------------------------------------
    // Validation.
    // ----------------------------------------------------------------------------------

    bool LiveRevengeRelationship::Validate() const
    {
        // UniquePlayerID::IsValid (inline): a non-empty name and a non-zero XUID.
        if (!(mUniqueID.GetPlayerName()[0] != '\0' && mUniqueID.mqXuid != 0))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "mUniqueID is not valid: " << mUniqueID.GetPlayerName()
                       << ". If you delete your profile you will stop seeing this.\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return false;
        }

        if (mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns < 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "number of takedowns are invalid player: " << mOverallStats.mPlayerStats.miTakedowns
                       << " rival: " << mOverallStats.mRivalStats.miTakedowns
                       << ". If you delete your profile you will stop seeing this.\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return false;
        }

        CGS_ASSERT(!mLastTimeChanged.IsZero(), "!mLastTimeChanged.IsZero()");
        return true;
    }

    // A local/remote stat pair is inconsistent when one side claims more for the player while
    // the other claims more for the rival. With lbShouldWeAssert the mismatch asserts; without
    // it the build only writes a warning line to the network log stream.
    void LiveRevengeRelationship::ValidateStat(s32 liLocalPlayerStat, s32 liRemotePlayerStat,
                                               s32 liLocalRivalStat, s32 liRemoteRivalStat,
                                               const char* lpcName, bool lbShouldWeAssert)
    {
        bool lbDetectedWrong = false;
        if (liLocalPlayerStat > liRemotePlayerStat && liLocalRivalStat < liRemoteRivalStat)
        {
            lbDetectedWrong = true;
        }
        if (liLocalRivalStat > liRemoteRivalStat && liLocalPlayerStat < liRemotePlayerStat)
        {
            lbDetectedWrong = true;
        }

        if (lbDetectedWrong && lbShouldWeAssert)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "We are detecting " << lpcName << " wrong, pl: " << liLocalPlayerStat
                       << " pr: " << liRemotePlayerStat << " rl: " << liLocalRivalStat
                       << " rr: " << liRemoteRivalStat
                       << ".  Step over, run an external or delete your profile to stop getting this every time you join a game with "
                       << mUniqueID.GetPlayerName() << ".\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        // FLAG: the non-asserting branch writes "WARNING: We are detecting <name> wrong, pl: ..
        // pr: .. rl: .. rr: ..\n" to the network log stream, which has no home in this tree;
        // the line is dropped.
    }

    // Merge the rival's copy (already flipped to this side's point of view) into this one:
    // cross-check five stats, reconcile a disagreeing running score, then keep the larger
    // value of every merged stat.
    void LiveRevengeRelationship::Merge(LiveRevengeRelationship* lpRemoteRelationship)
    {
        CommonRelationshipStats& lrLocalPlayer  = mOverallStats.mPlayerStats;
        CommonRelationshipStats& lrLocalRival   = mOverallStats.mRivalStats;
        const CommonRelationshipStats& lrRemotePlayer = lpRemoteRelationship->mOverallStats.mPlayerStats;
        const CommonRelationshipStats& lrRemoteRival  = lpRemoteRelationship->mOverallStats.mRivalStats;

        ValidateStat(lrLocalPlayer.miTakedowns, lrRemotePlayer.miTakedowns,
                     lrLocalRival.miTakedowns, lrRemoteRival.miTakedowns, "Takedowns", false);
        ValidateStat(lrLocalPlayer.miWins, lrRemotePlayer.miWins,
                     lrLocalRival.miWins, lrRemoteRival.miWins, "Wins", false);
        ValidateStat(lrLocalPlayer.miLongestStreak, lrRemotePlayer.miLongestStreak,
                     lrLocalRival.miLongestStreak, lrRemoteRival.miLongestStreak, "Longest Streak", false);
        ValidateStat(lrLocalPlayer.miEventsSinceLastTakedown, lrRemotePlayer.miEventsSinceLastTakedown,
                     lrLocalRival.miEventsSinceLastTakedown, lrRemoteRival.miEventsSinceLastTakedown,
                     "Events since takedown", false);
        ValidateStat(lrLocalPlayer.miScoresSettled, lrRemotePlayer.miScoresSettled,
                     lrLocalRival.miScoresSettled, lrRemoteRival.miScoresSettled, "Scores Settled", false);

        if (miCurrentScoreForPlayersPointOfView != lpRemoteRelationship->miCurrentScoreForPlayersPointOfView)
        {
            // The local takedown total is weighed against twice the remote player-side takedown
            // count (the build doubles that one field, not the remote total).
            const s32 liLocalTakedowns  = lrLocalPlayer.miTakedowns + lrLocalRival.miTakedowns;
            const s32 liRemoteTakedowns = lrRemotePlayer.miTakedowns * 2;
            if (liLocalTakedowns < liRemoteTakedowns)
            {
                miCurrentScoreForPlayersPointOfView = lpRemoteRelationship->miCurrentScoreForPlayersPointOfView;
            }
            else if (liLocalTakedowns == liRemoteTakedowns)
            {
                miCurrentScoreForPlayersPointOfView = 0;
            }
        }

        // rw::core::stdc::Max over each merged stat (the branchy max, spelled in place).
        lrLocalPlayer.miTakedowns               = (lrLocalPlayer.miTakedowns > lrRemotePlayer.miTakedowns) ? lrLocalPlayer.miTakedowns : lrRemotePlayer.miTakedowns;
        lrLocalPlayer.miWins                    = (lrLocalPlayer.miWins > lrRemotePlayer.miWins) ? lrLocalPlayer.miWins : lrRemotePlayer.miWins;
        lrLocalPlayer.miLongestStreak           = (lrLocalPlayer.miLongestStreak > lrRemotePlayer.miLongestStreak) ? lrLocalPlayer.miLongestStreak : lrRemotePlayer.miLongestStreak;
        lrLocalPlayer.miEventsSinceLastTakedown = (lrLocalPlayer.miEventsSinceLastTakedown > lrRemotePlayer.miEventsSinceLastTakedown) ? lrLocalPlayer.miEventsSinceLastTakedown : lrRemotePlayer.miEventsSinceLastTakedown;
        lrLocalPlayer.miScoresSettled           = (lrLocalPlayer.miScoresSettled > lrRemotePlayer.miScoresSettled) ? lrLocalPlayer.miScoresSettled : lrRemotePlayer.miScoresSettled;
        lrLocalRival.miTakedowns                = (lrLocalRival.miTakedowns > lrRemoteRival.miTakedowns) ? lrLocalRival.miTakedowns : lrRemoteRival.miTakedowns;
        lrLocalRival.miWins                     = (lrLocalRival.miWins > lrRemoteRival.miWins) ? lrLocalRival.miWins : lrRemoteRival.miWins;
        lrLocalRival.miLongestStreak            = (lrLocalRival.miLongestStreak > lrRemoteRival.miLongestStreak) ? lrLocalRival.miLongestStreak : lrRemoteRival.miLongestStreak;
        lrLocalRival.miEventsSinceLastTakedown  = (lrLocalRival.miEventsSinceLastTakedown > lrRemoteRival.miEventsSinceLastTakedown) ? lrLocalRival.miEventsSinceLastTakedown : lrRemoteRival.miEventsSinceLastTakedown;
        lrLocalRival.miScoresSettled            = (lrLocalRival.miScoresSettled > lrRemoteRival.miScoresSettled) ? lrLocalRival.miScoresSettled : lrRemoteRival.miScoresSettled;
    }

    // ----------------------------------------------------------------------------------
    // Debug-menu callbacks (the void* is the relationship).
    // ----------------------------------------------------------------------------------

    void LiveRevengeRelationship::DEBUGResetTimeStamp(void* lpParameter)
    {
        LiveRevengeRelationship* lpRelationship = static_cast<LiveRevengeRelationship*>(lpParameter);
        lpRelationship->mLastTimeChanged.Update();
    }

    // A default-constructed (zero) time makes the relationship the oldest in the table.
    void LiveRevengeRelationship::DEBUGSetTimeStampOld(void* lpParameter)
    {
        LiveRevengeRelationship* lpRelationship = static_cast<LiveRevengeRelationship*>(lpParameter);
        CgsSystem::DateAndTime lDateAndTime;
        lpRelationship->SetLastTimeChanged(lDateAndTime);
    }

    // Clear the relationship, then stamp the change time.
    void LiveRevengeRelationship::DEBUGClearRelationship(void* lpParameter)
    {
        LiveRevengeRelationship* lpRelationship = static_cast<LiveRevengeRelationship*>(lpParameter);
        lpRelationship->Clear();
        lpRelationship->mLastTimeChanged.Update();
    }
}
