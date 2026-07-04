// ---- GameSource/Network/Managers/BrnNetworkScoreboardManager.h ----
// BrnNetwork::ScoreboardManager -- the online-leaderboard browser/state machine. It drives the
// DirtySock rankings component (mpRankings) through a Category -> Index -> Variation -> Scoreboard
// drill-down, queues the GUI's "select scoreboard" events, downloads headings/rows, and builds the
// front-end-facing BrnNetwork::Scoreboard grid from the downloaded rankings rows.
//
// SHAPE (member names/types/order, the EState/ScoreboardRowSortData nested types, and the method
// set) is from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkScoreboardManager.{h,cpp}),
// gated against the X360 ARTIST binary. Member OFFSETS used by the recovered bodies (read as the
// _DWORD* indices a1[N]) line up with this declaration order:
//   miCurrentView +0 / miCurrentVariation +4 / miCurrentCategory +8 / miCurrentIndex +12 (a1[0..3])
//   mpNetworkModule +16 (a1[4]); mScoreboardEventQueue +20 (FifoQueue<...,1>: maData[1]==16B +
//     3 s32 cursors == 28B, cursors at +36/+40/+44 == a1[9..11], matching Construct's stores);
//   mpRankings +48 (a1[12]); mpServerInterface +52 (a1[13]); meCurrentState +56 (a1[14]);
//   miCurrentViewOffset +60 (a1[15]); mDebugComponent +100 (a1+0x64, the Construct/Prepare arg).
//
// NOTE: Construct (X360 0x8255A408) also zeroes a few words in the +64..+96 gap between
// miCurrentViewOffset and mDebugComponent whose DWARF member names are not attested in the recovered
// exports. They are NOT modelled as named members (none of the recovered bodies read them); the
// reconstructed Construct resets the named members by name and Constructs the embedded sub-objects.
// A fixed-byte sizeof/offset static_assert is intentionally omitted (the PC gate targets x64, where
// the pointers and the embedded sub-objects widen and shift the absolute offsets).
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"                              // FifoQueue<T,N>
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                                // NetworkInSelectScoreboardEvent (queue element)
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"                            // Scoreboard / ScoreboardColumn::EDataType
#include "GameSource/Network/Debug Components/BrnNetworkScoreboardDebugComponent.h"      // ScoreboardDebugComponent (by-value member)
#include "GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.h"           // CgsNetwork::UniquePlayerIDX360 (by-value member)

namespace CgsNetwork
{
    class ServerInterface;             // pointer-only (mpServerInterface)
    class ServerInterfaceRankings;     // pointer-only (mpRankings)
}

namespace BrnNetwork
{
    class BrnNetworkModule;            // pointer-only (mpNetworkModule)

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;           // pointer-only (ProcessBeforeSimulation param)
    }

    // Forward decl for the score-target-challenge handler (payload defined in the .cpp).
    struct EvScoreTargetEvent;

    struct ScoreboardManager
    {
    public:
        // DWARF BrnNetworkScoreboardManager.h:55 -- the download state machine.
        enum EState
        {
            E_STATE_IDLE              = 0,
            E_STATE_GETTING_HEADINGS  = 1,
            E_STATE_GETTING_SCOREBOARD = 2,
            E_STATE_COUNT             = 3,
        };

        void Construct();                                                                // @ 0x8255A408
        bool Prepare(CgsNetwork::ServerInterface* lpServerInterface,
                     BrnNetwork::BrnNetworkModule* lpModule);                             // @ 0x825471B0
        bool Release();                                                                  // @ 0x82552EF0
        void Destruct();                                                                 // DWARF :81 (declared-only; body folds onto queue Clear)

        void ProcessBeforeSimulation(
            BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);               // DWARF :86 (declared-only)
        void HandleScoreboardEvent(
            const BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent*
                lpSelectScoreboardEvent);                                                // DWARF :91 (declared-only)
        void SetViewAndDownloadHeaders();                                                // @ 0x82547280
        void Disconnected();                                                             // @ 0x82553270

        // @ 0x82568960 -- handle a GUI "challenge this event score" request.
        void HandleEvScoreTargetEvent(const EvScoreTargetEvent* lpScoreTargetEvent);

    private:
        void BuildDownloadedScoreboard(Scoreboard* lpScoreboard);                        // @ 0x8255F948
        void AddColumnInfoToScoreboard(Scoreboard* lpScoreboard);                        // @ 0x82553010
        void AddRowDataToScoreboard(Scoreboard* lpScoreboard);                           // @ 0x8255A448
        void AddSortedRowDataToScoreboard(Scoreboard* lpScoreboard);                     // @ 0x8255A5D0
        void ProcessEventQueue();                                                        // DWARF :161 (declared-only)
        void CopyCategories();                                                           // DWARF :165 (declared-only)
        void CopyVariations(s32 liCategory, s32 liIndex);                                // DWARF :171 (declared-only)
        void CopyIndexes(s32 liCategory);                                                // DWARF :176 (declared-only)
        void PageUp();                                                                   // DWARF :180 (declared-only)
        void PageDown();                                                                 // DWARF :184 (declared-only)
        void OffsetScoreboard(s32 liNumberOfRows);                                       // @ 0x82547348
        s32  GetColumnIndexOfType(s32 liType);                                           // @ 0x82547490
        void PostProcessColumnData(char* lpcData, s32 liColumnIndex);                    // @ 0x825531F8
        ScoreboardColumn::EDataType DirtySockColumnTypeToEDataType(s32 liDSType);        // DWARF :205 (declared-only; the AddColumnInfo/PostProcess callee)
        void AddNumberBeforeAndAfter(Scoreboard* lpScoreboard, s8 liCurrentRows);        // DWARF :217 (declared-only)

        // DWARF BrnNetworkScoreboardManager.h:104 -- one row's sort key (the qsort element).
        struct ScoreboardRowSortData
        {
            s32  miRowIndex;     // +0x00  DWARF :106
            s32  miScore;        // +0x04  DWARF :107
            bool mbAscending;    // +0x08  DWARF :108
        };

        // DWARF :211 -- the qsort comparator. STATIC: the X360 body (0x82547640) takes no `this`
        // (it is handed straight to qsort as the 4th arg), comparing two ScoreboardRowSortData by
        // miScore, negated when mbAscending. Returns the C-style {-1,0,1} ordering int.
        static int _ScoreboardSortData(const void* lpData1, const void* lpData2);        // @ 0x82547640

        // @ 0x82562E50 -- static gamer-card-manager completion callback (matches the
        // NetworkGamerCardManagerX360::CompleteCallback typedef: s32(*)(s32,u64,s32)). Stamps the
        // resolved XUID onto mScoreTargetPlayerID and queues the score-target challenge event.
        static s32 RequestXUIDForPlayerCallback(s32 lbSuccess, u64 lqRequestedXuid, s32 liUserData);

        // KI_INVALID_HEADING (DWARF :111) -- the -1 "no heading selected" sentinel.
        static const s32 KI_INVALID_HEADING = -1;
        // KI_EVENT_QUEUE_SIZE (DWARF :122) -- the select-event FIFO capacity.
        static const s32 KI_EVENT_QUEUE_SIZE = 1;

        // ---- member layout (see header comment) ----
        s32                           miCurrentView;          // +0x00  (a1[0])
        s32                           miCurrentVariation;     // +0x04  (a1[1])
        s32                           miCurrentCategory;      // +0x08  (a1[2])
        s32                           miCurrentIndex;         // +0x0C  (a1[3])
        BrnNetwork::BrnNetworkModule* mpNetworkModule;        // +0x10  (a1[4])
        FifoQueue<BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent,
                  KI_EVENT_QUEUE_SIZE> mScoreboardEventQueue; // +0x14  (cursors a1[9..11])
        CgsNetwork::ServerInterfaceRankings* mpRankings;      // +0x30  (a1[12])
        CgsNetwork::ServerInterface*  mpServerInterface;      // +0x34  (a1[13])
        EState                        meCurrentState;         // +0x38  (a1[14])
        s32                           miCurrentViewOffset;    // +0x3C  (a1[15])
        // ---- score-target-challenge working state (+0x40..+0x64 gap; filled by
        //      HandleEvScoreTargetEvent @ 0x82568960 / RequestXUIDForPlayerCallback @ 0x82562E50) ----
        u64                           mScoreTargetScoreboard; // +0x40  (a1+64)
        CgsNetwork::UniquePlayerIDX360 mScoreTargetPlayerID;  // +0x48  (24B: macName[16]+mqXuid; a1+72, xuid a1+88)
        s32                           miScoreTargetValue;     // +0x60  (a1+96)
        ScoreboardDebugComponent      mDebugComponent;        // +0x64  (Construct/Prepare arg)
    };
}
