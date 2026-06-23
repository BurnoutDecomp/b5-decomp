#pragma once

#include <cstddef>                                              // offsetof (CarScoreData / OnlineScoringOutputInterface layout asserts)
#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameStateTypes.h"             // BrnGameState::LandmarkIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"     // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/Containers/CgsBitArray.h"      // CgsContainers::BitArray<N> (CarCheckpointData)
#include "GameSource/BurnoutConstants.h"                        // EActiveRaceCarIndex (FlybyRivalData)
#include "GameSource/GameState/BrnCgsPlayerName.h"              // CgsNetwork::PlayerName (shared 16-byte home)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"        // CgsSystem::Time (CarScoreData)
#include "GameShared/GameClasses/Containers/CgsArray.h"         // Array<T,N>::AddNew (CarScoreData::GetChainableStuntMultipliers)
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // BrnGameState::StuntToDisplay (ScoringOutputInterface::maStunts[1] by value); clean header (types.hpp + BrnCommonTypes.h + BrnGameStateTypes.h only, no cycle back here)

namespace BrnGameState
{
    // Opaque-enum forward declaration with the committed underlying type
    // (BrnScoringSystem.h:132 `enum ECurrentMedalTargetTime : s32`). Cannot #include
    // BrnScoringSystem.h here -- it includes THIS header (cycle). A fixed-underlying-type
    // enum is complete for the by-value ScoringOutputInterface members below.
    enum ECurrentMedalTargetTime : s32;
}

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        // Identifies which game mode is active. Recovered verbatim from the DecFIGS DWARF
        // (BrnGameStateSharedIO.h:51); the X360 build attests these values via the mode
        // classes and the ModeManager's current-mode-type field. The duplicate values are
        // intentional (the online block re-uses the offline-count slot as its start, and
        // E_MODE_ONLINE_RACE aliases E_MODE_ONLINE_MODE_START == 10).
        enum EGameModeType
        {
            E_MODE_NONE                    = -1,
            E_MODE_OFFLINE_RACE            = 0,
            E_MODE_FACE_OFF                = 1,
            E_MODE_OFFLINE_SHOWTIME        = 2,
            E_MODE_ROAD_RAGE               = 3,
            E_MODE_PURSUIT                 = 4,
            E_MODE_BURNING_ROUTE           = 5,
            E_MODE_ELIMINATOR              = 6,
            E_MODE_STUNT_ATTACK            = 7,
            E_MODE_MARKED_MAN              = 8,
            E_MODE_TRAFFIC_ATTACK          = 9,
            E_MODE_OFFLINE_COUNT           = 10,
            E_MODE_ONLINE_MODE_START       = 10,
            E_MODE_ONLINE_RACE             = 10,
            E_MODE_ONLINE_ROAD_RAGE        = 11,
            E_MODE_ONLINE_FUGITIVE         = 12,
            E_MODE_ONLINE_BURNING_HOME_RUN = 13,
            E_MODE_ONLINE_FREE_BURN        = 14,
            E_MODE_ONLINE_FREE_BURN_LOBBY  = 15,
            E_MODE_ONLINE_SHOWTIME         = 16,
            E_MODE_ONLINE_MODE_END         = 17,
            E_MODE_COUNT                   = 17,
        };

        // ADDITIVE GROW (BrnMugshotManager TU): the photo/"mugshot" image-type enum. DWARF
        // BrnGameStateSharedIO.h:529 (== BrnGameStateImageManagerBase.h:1153). The MugshotManager
        // stores E_IMAGE_TYPE_COUNT (6) as the cleared meCaptureMugshotType / meShowMugshotType and
        // uses the lower values to tag each kind of capture it requests. FLAG: declared here (the
        // SharedIO enum home) for the MugshotManager TU; the ImageManager TU shares the same values.
        enum EImageType
        {
            E_IMAGE_TYPE_START                  = 0,
            E_IMAGE_TYPE_FREEBURN_MUGSHOT       = 0,
            E_IMAGE_TYPE_MUGSHOT                = 1,
            E_IMAGE_TYPE_PAYBACK_MUGSHOT        = 2,
            E_IMAGE_TYPE_ROAD_RULE_TIME_MUGSHOT = 3,
            E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT = 4,
            E_IMAGE_TYPE_VICTORY_MUGSHOT        = 5,
            E_IMAGE_TYPE_COUNT                  = 6,
        };

        // Drives the per-mode state machine (GameMode's nested GameModeState set).
        // DWARF: BrnGameStateSharedIO.h:93. Slot 1 (E_GMS_INTRO) is IntroState's state.
        enum EGameModeState
        {
            E_GMS_INVALID        = -1,
            E_GMS_COUNTDOWN      = 0,
            E_GMS_INTRO          = 1,
            E_GMS_IN_PROGRESS    = 2,
            E_GMS_OUTRO          = 3,
            E_GMS_RESULTS        = 4,
            E_GMS_QUIT           = 5,
            E_GMS_ONLINE_LOADING = 6,
            E_GMS_ONLINE_SPLASH  = 7,
            E_GMS_COUNT          = 8,
        };

        // Per-player freeburn-challenge completion bit storage == CgsContainers::FastBitArray<2000>
        // (DWARF CgsFastBitArray.h:51/155: uint64_t maxBits[32] == 256 bytes, KU_NUMBEROFBITFIELDS=32).
        // Relocated above CompletedFburnChallengesData so the element struct can embed it; this is the
        // SAME struct previously defined further down (do not double-define -- delete the later copy).
        struct CompletedFburnChallenges
        {
            static const u32 KU_NUM_BIT_WORDS = 32;
            u64 maxBits[KU_NUM_BIT_WORDS]; // 256 bytes (FastBitArray<2000>)
        };

        // EventQueue element type. DWARF BrnGameStateSharedIO.h:288-291: an s32 network player id
        // followed by a FastBitArray<2000> bit store. The X360 build bakes a 264-byte element stride
        // into BaseEventQueue<...>::AddEvent (0x8258C160) and ::Append (0x823C46A8): s32@0x00 (4B) +
        // 4B implicit pad (u64 alignment) + 256B bit array == 264B, align 8. alignas(16) is DROPPED:
        // with it sizeof would round up to 272 != 264.
        struct CompletedFburnChallengesData
        {
            s32                      mNetworkPlayerID;             // 0x00 (BrnNetwork::NetworkPlayerID == s32)
            CompletedFburnChallenges mCompletedFreeburnChallenges; // 0x08 (FastBitArray<2000>, 256B)
        };

        // ===== Added by the GameMode/ModeManager leaf batch (Group B IO family) =====

        // Per-player scoring slot (online). Minimal home; enumerators mirror the
        // EActiveRaceCarIndex pattern (8 players + invalid). DWARF: BrnGameStateSharedIO.h.
        enum EPlayerScoringIndex : s32
        {
            E_PLAYER_SCORING_INDEX_INVALID = -1,
            E_PLAYER_SCORING_INDEX_0       = 0,
            E_PLAYER_SCORING_INDEX_1       = 1,
            E_PLAYER_SCORING_INDEX_2       = 2,
            E_PLAYER_SCORING_INDEX_3       = 3,
            E_PLAYER_SCORING_INDEX_4       = 4,
            E_PLAYER_SCORING_INDEX_5       = 5,
            E_PLAYER_SCORING_INDEX_6       = 6,
            E_PLAYER_SCORING_INDEX_7       = 7,
            E_PLAYER_SCORING_INDEX_COUNT   = 8
        };

        // Online team selector. DWARF: BrnGameStateSharedIO.h:32.
        enum EPlayerTeam : s32
        {
            E_PLAYER_TEAM_NONE      = 0,
            E_PLAYER_TEAM_RED_TEAM  = 1,
            E_PLAYER_TEAM_BLUE_TEAM = 2,
            E_PLAYER_TEAM_COUNT     = 3
        };

        // How the blue team finished an online team round. DWARF: BrnGameStateSharedIO.h:478.
        // Stored per-slot in OnlineScoringOutputInterface::maeBlueTeamFinishTypes[].
        enum EBlueTeamFinishType : s32
        {
            E_BLUE_TEAM_FINISH_TYPE_ELIMINATED = 0,
            E_BLUE_TEAM_FINISH_TYPE_SURVIVED   = 1,
            E_BLUE_TEAM_FINISH_TYPE_ESCAPED    = 2,
            E_BLUE_TEAM_FINISH_TYPE_COUNT      = 3,
        };

        // How the player crashed during an online Road-Rage round (DWARF home
        // BrnGameStateSharedIO.h:447). Passed by value to ScoringSystem::OnRoadRagePlayerCrashed;
        // grown here in its committed home so the keystone (BrnScoringSystem.h) can name it.
        enum ERoadRageCrashType : s32
        {
            E_ROADRAGE_CRASHTYPE_NONE      = -1,
            E_ROADRAGE_CRASHTYPE_TAKENDOWN = 0,
            E_ROADRAGE_CRASHTYPE_WRECKED   = 1,
            E_ROADRAGE_CRASHTYPE_COUNT     = 2,
        };

        // Per-event assert ceiling on landmarks (DWARF BrnGameStateSharedIO.h:1850).
        const s32 KI_MAX_LANDMARKS_IN_MODE = 16;

        // X360-only stunt-score scratch record. No DWARF/leak shape exists; the layout is
        // recovered purely from StuntScoreInfo::Clear (0x82356EA0), which zeroes a fixed set of
        // u32 words and leaves the muReservedNN gaps intact. Field semantics are unknown -- words
        // are named storage slots at their exact offsets (rename when a populating TU reveals
        // them). Single owner: grow in place, do not fork.
        struct StuntScoreInfo
        {
            u32 muWord00, muWord01, muWord02, muWord03, muWord04, muWord05, muWord06;
            u32 muReserved07;
            u32 muWord08;
            u32 muReserved09;
            u32 muWord10;
            u32 muReserved11;
            u32 muWord12;
            u32 muReserved13;
            u32 muWord14, muWord15, muWord16, muWord17, muWord18;
            u32 muReserved19;
            u32 muWord20;
            u32 muReserved21, muReserved22, muReserved23;
            u32 muWord24;

            void Clear();   // X360 0x82356EA0
        };

        // DWARF BrnGameStateSharedIO.h:872. The per-mode "specific game-mode event" interface: a
        // fixed Array<Event,175> of per-event records keyed by event id. The nested Event record
        // holds the event's landmark set + traffic-light trigger id + the event id. Grown by the
        // SpecificGameModeEventInterface TU (AddEvent @ 0x82361500, Event::GetLandmark @ 0x8240E7E0;
        // the X360 emits the per-element push as the generic Array<Event,175>::Append @ 0x8235BA28).
        class SpecificGameModeEventInterface
        {
        public:
            // maEvents capacity. The X360 over-capacity asserts compare the live length against
            // 0xAF == 175 (BrnGameStateSharedIO.h:1976 + the inlined CgsArray.h:226 capacity).
            static const u32 KU_MAX_EVENTS = 175;

            class Event
            {
            public:
                // luTrafficLightTriggerId is a LightTriggerId (== u32); spelled u32 here to avoid
                // a BrnGameModeParams.h include cycle (that header depends on this one).
                void Construct(s32 liEventID, u32 luTrafficLightTriggerId,
                               LandmarkIndex* lpaLandmarkIndices, s32 liNumLandmarks); // X360 0x82354340

                // X360 0x8240E7E0. Indexed accessor for one of this event's landmark indices:
                // returns maLandmarkIndices[liIndex] by value (sizeof(LandmarkIndex)==2). Bounds-
                // asserts liIndex in [0, miNumLandmarks) (BrnGameStateSharedIO.h:1929/1930); the
                // X360 assert text streams the count as "GetNumLandmarks()" (== miNumLandmarks).
                LandmarkIndex GetLandmark(s32 liIndex) const; // X360 0x8240E7E0

                // Inline read of the event id (matched by AddEvent's linear scan) and the landmark
                // count (no standalone X360 symbol; inlined at the call/assert sites).
                s32 GetEventID() const      { return miEventID; }
                s32 GetNumLandmarks() const { return miNumLandmarks; }

                // Inline read of this event's traffic-light trigger id (the X360 reads the
                // +0x20 field directly; e.g. GuiEventNetworkCreateGame::SetFromGameParams
                // @0x82481FC8 forwards it into a destination Event::Construct).
                u32 GetTrafficLightTriggerId() const { return mTrafficLightTriggerId; }

            private:
                LandmarkIndex maLandmarkIndices[KI_MAX_LANDMARKS_IN_MODE]; // 0x00
                u32           mTrafficLightTriggerId;                      // 0x20 (LightTriggerId)
                s32           miNumLandmarks;                              // 0x24
                s32           miEventID;                                   // 0x28
            };

            // X360 0x82361500. Find the event whose id == liEventID (linear scan of maEvents by
            // Event::GetEventID); if found, return that record. Otherwise Construct a fresh Event
            // (from the supplied trigger id + landmark array) and Append it, returning the new
            // record. The X360 asserts the over-capacity guard
            // (maEvents.GetLength() < maEvents.GetCapacity(), BrnGameStateSharedIO.h:1976) before
            // the Append; the push itself is the generic Array<Event,175>::Append (0x8235BA28).
            Event* AddEvent(s32 liEventID, u32 luTrafficLightTriggerId,
                            LandmarkIndex* lpaLandmarkIndices, s32 liNumLandmarks);

        private:
            Array<Event, KU_MAX_EVENTS> maEvents; // +0x00 (Event stride 44; miCount @ +0x1E14 == 175*44)
        };

        // DWARF/X360 BrnGameStateSharedIO.h:1700-1728. The per-mode "set up all event starts"
        // interface: a fixed Array<EventStart,175> of per-event start records keyed by an event
        // index. Direct twin of SpecificGameModeEventInterface above (same 175 capacity, same
        // linear-scan-then-Append AddXxx shape) but the EventStart element is 48 bytes wide.
        // Grown by the SetUpAllEventStartsInterface TU:
        //   AddEventStart        X360 0x82361398 (BrnGameStateSharedIO.h:1700)
        //   the per-element push X360 0x8235B7D8 (the inlined generic Array<EventStart,175>::Append;
        //                        Hex-Rays truncated the symbol to "...::EventSta")
        // The X360 over-capacity guard compares the live length against 0xAF == 175
        // (BrnGameStateSharedIO.h:1728, inlined CgsArray.h:226).
        class SetUpAllEventStartsInterface
        {
        public:
            // maEventStarts capacity. The X360 over-capacity asserts compare the live length
            // against 0xAF == 175 (BrnGameStateSharedIO.h:1728 + the inlined CgsArray.h:226).
            static const u32 KU_MAX_EVENT_STARTS = 175;

            // One per-event start record. 48-byte stride (X360-authoritative: AddEventStart builds
            // the record then the append copies 6 QWORDs == 48 bytes into &maElements[miCount], and
            // 175 * 48 == 0x20D0 == the miCount offset). Field roles recovered from the X360 store
            // map at the EventSta call setup (0x823614C4..): a leading 16-byte block, then five
            // scalar words the caller passes. The two words the AddEventStart scan/assert touch are
            // named by role (the +0x14 word is the scanned event index, the +0x18 word is the event
            // id matched by "luEventID == maEventStarts.GetItem(...).GetEventID()"); the remaining
            // caller-supplied words have no attested semantics and are spelled by their source arg.
            class EventStart
            {
            public:
                // Inline read of the event id (matched by AddEventStart's assert) and the event
                // index (matched by AddEventStart's linear scan). No standalone X360 symbols --
                // both are inlined at the AddEventStart compare/assert sites.
                s32 GetEventIndex() const { return miEventIndex; }
                s32 GetEventID() const    { return miEventID; }

            private:
                // AddEventStart builds a record field-by-field from the caller's args.
                friend class SetUpAllEventStartsInterface;

                u8  maLeadingBlock[16]; // +0x00  16-byte block from the caller's vector arg (v1)
                s32 miWord10;           // +0x10  caller arg a4 (r6)
                s32 miEventIndex;       // +0x14  caller arg a2 (r4); AddEventStart's scan key
                s32 miEventID;          // +0x18  caller arg a3 (r5); GetEventID()
                s32 miWord1C;           // +0x1C  caller arg a5 (r7)
                s16 miWord20;           // +0x20  caller arg a6 (r8, __int16)
                u8  maPad0x22[0x30 - 0x22]; // pad the record to its 0x30 (48-byte) stride
            };

            // X360 0x82361398. Find the EventStart whose event index == liEventIndex (linear scan
            // of maEventStarts by EventStart::GetEventIndex); if found, assert its event id matches
            // liEventID and return that record. Otherwise assert the over-capacity guard
            // (maEventStarts.GetLength() < maEventStarts.GetCapacity(), :1728), build a fresh
            // EventStart from the supplied fields and Append it, returning the new record.
            EventStart* AddEventStart(const u8* lpLeadingBlock, s32 liEventIndex, s32 liEventID,
                                      s32 liWord10, s32 liWord1C, s16 liWord20);

        private:
            // X360 0x8235B7D8 (Hex-Rays "...::EventSta"): the inlined generic
            // Array<EventStart,175>::Append. Asserts the array was Construct/Clear'd and has room
            // (the streamed "Array container out of space" message, CgsArray.h:225/226), copies the
            // 48-byte record into the next slot and bumps the count. Returns the newly added record.
            EventStart* AppendEventStart(const EventStart& lrEventStart);

            Array<EventStart, KU_MAX_EVENT_STARTS> maEventStarts; // +0x00 (EventStart stride 48; miCount @ +0x20D0 == 175*48)
        };

        // DWARF BrnGameStateSharedIO.h:936. The "every player" freeburn completion-status block:
        // 7 per-player slots {bit-array, player id} + the local player's bit array. Minimal slice:
        // only Construct (0x82326360) + AddCompletionStatus (0x823263C8) are owned here.
        class FburnChallengeEveryPlayerStatusData
        {
        public:
            void Construct();
            void AddCompletionStatus(const CompletedFburnChallenges* lpCompletedChallenges,
                                     BrnNetwork::NetworkPlayerID lPlayerID);

        private:
            static const s32 KI_NUM_PLAYER_SLOTS = 7;
            struct CompletedChallenges
            {
                CompletedFburnChallenges    mCompletedChallenges; // slot+0   (256 bytes)
                BrnNetwork::NetworkPlayerID mPlayerID;            // slot+256 (sentinel -1)
            };
            CompletedChallenges      maCompletedChallenges[KI_NUM_PLAYER_SLOTS];
            CompletedFburnChallenges mLocalChallengeCompletionData;
        };

        // ===== Added by the CarCheckpointData TU (checkpoint bit-set IO) =====
        //
        // X360-only per-car checkpoint tracker. Absent from the PS3 DecFIGS DWARF; the layout
        // is recovered from the X360 Hex-Rays of its four methods (0x8231BDF8 / 0x8231BF90 /
        // 0x823261D0 / 0x823C4D50). Every method's inlined bit math is byte-for-byte the
        // CgsContainers::BitArray<16> template instantiation (one u64 field, 16 valid bits). So
        // the class is a thin semantic wrapper: a "checkpoints still remaining" bit set where a
        // set bit == checkpoint not yet reached. KI_MAX_LANDMARKS_IN_MODE (==16) is the committed
        // capacity and equals BitArray<16>::GetCapacity(), so it is the NUMBITS.
        //
        // MINIMAL SLICE: the only data member the four bodies touch is the embedded BitArray<16>
        // (its single u64 sits at offset 0).
        class CarCheckpointData
        {
        public:
            // X360 0x8231BDF8. Initialise the tracker for an event with liNumCheckpoints checkpoints:
            // clear all bits, then mark checkpoints [0, liNumCheckpoints) as remaining (bit set).
            void SetupCheckpoints(s32 liNumCheckpoints);

            // X360 0x8231BF90. Mark checkpoint liCheckpointIndex as reached by clearing its remaining
            // bit. Asserts the index is in range and the bit was still set (not already hit).
            void MarkCheckpointAsHit(s32 liCheckpointIndex);

            // X360 0x823261D0. Index of the next (lowest-indexed) checkpoint still to be reached,
            // or -1 if none. Asserts at least one checkpoint is set.
            s32 GetNextCheckpointIndex() const;

            // X360 0x823C4D50. Write the indices of every remaining checkpoint, in ascending order,
            // into lpaiCheckpointIndexes; returns how many were written.
            s32 GetAllRemainingCheckpointIndexes(s32* lpaiCheckpointIndexes) const;

        private:
            // Sole data member (offset 0x00, 8 bytes). NUMBITS == KI_MAX_LANDMARKS_IN_MODE (16).
            CgsContainers::BitArray<KI_MAX_LANDMARKS_IN_MODE> mCheckpointsRemaining;
        };

        // Per-car race-distance interface (DWARF BrnGameStateSharedIO.h:706-708, 40 bytes): the
        // eight cars' distance-to-finish + total race distance + active-car count. RaceCarRaceDistanceInterface::Clear
        // (X360 0x82357470) zeroes them all.
        class RaceCarRaceDistanceInterface
        {
        public:
            void Clear();

        private:
            f32 mafRaceCarDistanceToFinish[8]; // +0x00 (8 floats)
            f32 mfTotalRaceDistance;           // +0x20
            s32 miNumActiveRaceCars;           // +0x24
        }; // sizeof == 40

        // ===== FlybyData / FlybyRivalData (full reconstruction; OnlineGameResults still a slice) =====
        //
        // X360-authoritative layout (proven from AddCar/AddMessage/Prepare displacements + the
        // 196-byte per-rival stride 0xC4): the PS3 DecFIGS DWARF's KI_MAX_PARAMTER_LENGTH==20 /
        // maacMessageParameter[2][20] is drift -- the X360 strides/copies 16 (slwi r22,4; strncpy
        // count 0x10), which is the only width that closes the record at 196 bytes. So
        // KI_MAX_PARAMTER_LENGTH==16 and maacMessageParameter is char[2][16]. (CgsNetwork::PlayerName
        // is the shared 16-byte home; not the 20-byte BrnNetwork::PlayerName.)
        struct FlybyRivalData
        {
            static const s32 KI_MAX_CARS_IN_FLYBY     = 3;   // flyby capacity (lives on the rival type, X360 spelling)
            static const s32 KI_MAX_MESSAGES          = 2;   // per-rival message slots
            static const s32 KI_MAX_MESSAGE_ID_BUFFER = 64;  // string-id buffer width (no drift)
            static const s32 KI_MAX_PARAMTER_LENGTH   = 16;  // X360-authoritative (NOT the DWARF 20)
            static const s32 KI_MAX_PARAMETERS        = 1;   // per-message parameter ceiling

            // Style applied to the flyby message (X360 Prepare defaults to NEUTRAL == 1).
            enum EMessageStyle
            {
                E_MESSAGE_STYLE_INVALID = 0,
                E_MESSAGE_STYLE_NEUTRAL = 1,
            };

            void Prepare();   // X360 cpp:267 (inlined into FlybyData::Prepare): per-slot reset.

            EActiveRaceCarIndex    meRaceCarIndex;                                   // +0x00 (4)
            char                   maacMessageIDs[KI_MAX_MESSAGES][KI_MAX_MESSAGE_ID_BUFFER]; // +0x04 (128)
            char                   maacMessageParameter[KI_MAX_MESSAGES][KI_MAX_PARAMTER_LENGTH]; // +0x84 (132) (32)
            s32                    maiNumberOfParameters[KI_MAX_MESSAGES];           // +0xA4 (164) (8)
            EMessageStyle          meMessageStyle;                                   // +0xAC (172) (4)
            s32                    miNumberOfMessages;                               // +0xB0 (176) (4)
            CgsNetwork::PlayerName mPlayerName;                                      // +0xB4 (180) (16)
        }; // sizeof == 196

        // X360 GameStateModuleIO::FlybyData. miNumberOfCars header + KI_MAX_CARS_IN_FLYBY rival slots.
        // sizeof == 592 (4 + 3*196), matching FlybyManager+0x10..+0x260.
        struct FlybyData
        {
            bool Prepare();                                              // X360 0x82363A08
            void AddCar(EActiveRaceCarIndex leRaceCarIndex,
                        const CgsNetwork::PlayerName* lpPlayerName);     // X360 0x82356EF0
            void AddMessage(const char* lpcMessage, const char* lpcParameter); // X360 0x82357070

            // X360 0x821F2A90. Indexed accessor for one of the rival records. Bounds asserts at
            // BrnGameStateSharedIO.h:1110/1111.
            FlybyRivalData* GetFlybyRivalData(s32 liRivalIndex);

            s32            miNumberOfCars;                               // +0x00
            FlybyRivalData mRivalsToShow[FlybyRivalData::KI_MAX_CARS_IN_FLYBY]; // +0x04, 196-byte stride
        }; // sizeof == 592

        static_assert(sizeof(FlybyRivalData) == 196, "FlybyRivalData layout drift");
        static_assert(sizeof(FlybyData)      == 592, "FlybyData layout drift");

        // ===== CarScoreData (per-car scoring record) =====
        //
        // X360-AUTHORITATIVE 296-byte (0x128) layout, recovered from the four owned methods
        // (ctor 0x822A45A8, ClearData 0x821F2B28, operator= 0x821F4740 -- a full field-by-field
        // copy through +292, and GetChainableStuntMultipliers 0x8231C170). The PS3 DecFIGS DWARF +
        // Feb-2007 partial source describe an EARLIER CarScoreData (with LandmarkIndex[16] arrays) that does
        // NOT match the packed X360 record. Field semantics are mostly unknown, so only the fields
        // the methods meaningfully name are declared; runs of untouched scalar/Time words (incl. the
        // ten chainable-stunt CgsSystem::Time pairs the ctor zeroes at +0x6C..+0xBC) are collapsed
        // into byte-exact storage members (ClearData zeroes them via memset, operator= via memcpy).
        // Single owner: grow this slice, do not fork. Used by ScoringSystem / the online-mode scorers.
        class CarScoreData
        {
        public:
            // Element produced by GetChainableStuntMultipliers (X360 stride 16, sub_823177E8).
            struct ChainableMultiplierInfo
            {
                s32 miChainableScore;   // elem +0  (CarScoreData +0xC8)
                s32 miChainableField;   // elem +4  (CarScoreData +0xCC)
                s32 miSuppliedValue;    // elem +8  (the caller-supplied a3)
                s32 miMultiplier;       // elem +12 (CarScoreData +0xD0)
            };
            static_assert(sizeof(ChainableMultiplierInfo) == 16, "ChainableMultiplierInfo stride");

            CarScoreData();                                       // 0x822A45A8
            void ClearData();                                     // 0x821F2B28
            CarScoreData& operator=(const CarScoreData& lOther);  // 0x821F4740 (ledger "op")
            void GetChainableStuntMultipliers(s32 liMaxMultiplier, s32 liSuppliedValue,
                                              Array<ChainableMultiplierInfo, 8>* lpaMultiplierInfo) const; // 0x8231C170

            // ===== Online-mode scoring accessors (grown for the online-mode scorers) =====
            //
            // The two online scorers (OnlineRaceModeScoring / OnlineBurningHomeRunModeScoring)
            // read these per-car fields when building their finishing-order sort rows, and write
            // back the awarded points + finishing rank. Every offset below is PROVEN from the X360
            // scorer pseudocode (UpdatePlayerPoints @ 0x8232EB70 / 0x8232F6C0, which read/write the
            // CarData record returned by ScoringSystem::GetCarData; CarData embeds CarScoreData at
            // offset 0, so these reads/writes land inside CarScoreData) cross-checked against this
            // record's ctor (0x822A45A8) and ClearData (0x821F2B28). Inline so the scorer TUs need
            // no out-of-line body. NOTE: GetRaceCarIndex / GetNetworkPlayerID are NOT here -- in the
            // X360 binary they read CarData fields at +0x144 / +0x148 (past sizeof(CarScoreData)),
            // so they live on CarData (see BrnScoringSystem.h), not on this record.
            CgsSystem::Time GetFinishTime() const          { return mTotalTime; }              // +0x00
            void            SetFinishTime(CgsSystem::Time lTime) { mTotalTime = lTime; }         // +0x00 (RegisterFinishForCar writes the accumulated total race time)
            f32             GetDistanceToFinish() const     { return mfDistanceToFinish; }      // +0x48
            s32             GetTakedowns() const            { return miTakedowns; }             // +0x4C
            void            SetTakedowns(s32 liTakedowns)   { miTakedowns = liTakedowns; }      // +0x4C (UpdateTakedowns/UpdatePaybackTakedowns @0x8232AC88/0x82338320 increment the aggressor's takedown count)
            bool            GetTimedOut() const             { return mbTimedOut; }              // +0x68
            void            SetTimedOut(bool lbTimedOut)    { mbTimedOut = lbTimedOut; }        // +0x68 (UpdateNetworkPlayerResults stb 0x68)
            bool            GetDisconnected() const         { return mbDisconnected; }          // +0x69
            void            SetDisconnected(bool lbDisconnected) { mbDisconnected = lbDisconnected; } // +0x69 (Set/Clear-PlayerDisconnected)
            CgsSystem::Time GetTimeAsRunner() const         { return mTimeAsRunner; }           // +0x84
            bool            GetCompletedBurningHomeRun() const { return mbCompletedBurningHomeRun; } // +0xBC
            s32             GetCumulativeCheckpoints() const { return miCumulativeCheckpoints; } // +0xC4

            // Writeback. Both scorers write the same two physical slots in the same order: first the
            // descending points/finish-position countdown value at +0x58 (X360 stw ...,0x58), then
            // the sorted standings rank at +0x5C (X360 stw ...,0x5C). The Race scorer spells the
            // first call SetOnlineRacePoints, the Burning-Home-Run scorer spells it
            // SetOnlineFinishPosition; both target +0x58, so they are unified here. Likewise the
            // second slot (+0x5C, ClearData = -1) is the standings rank.
            void SetOnlineRacePoints(s32 liPoints)          { miOnlineFinishPositionScore = liPoints; } // +0x58
            void SetOnlineFinishPosition(s32 liPosition)    { miOnlineStandingsPosition = liPosition; } // +0x5C

            // ===== Race / team-stat scoring accessors (grown for ScoringSystem query/update) =====
            //
            // The ScoringSystem query/update methods read and write these named per-car fields.
            // Inline so the ScoringSystem TU needs no out-of-line body. Offsets are the X360-proven
            // field offsets documented on each member in the private section below.
            s32             GetRacePosition() const                { return miRacePosition; }            // +0x08
            void            SetRacePosition(s32 liPosition)        { miRacePosition = liPosition; }       // +0x08

            s32             GetHighestRacePosition() const         { return miHighestRacePosition; }      // +0x0C
            void            SetHighestRacePosition(s32 liPosition) { miHighestRacePosition = liPosition; } // +0x0C

            s32             GetCompletedLaps() const               { return miCompletedLaps; }            // +0x10
            void            SetCompletedLaps(s32 liLaps)           { miCompletedLaps = liLaps; }          // +0x10

            s32             GetFinishPosition() const              { return miFinishPosition; }           // +0x14
            void            SetFinishPosition(s32 liPosition)      { miFinishPosition = liPosition; }     // +0x14

            f32             GetDistanceToFinishLive() const        { return mfDistanceToFinishLive; }     // +0x18
            void            SetDistanceToFinishLive(f32 lfDistance) { mfDistanceToFinishLive = lfDistance; } // +0x18

            // Per-lap times, KU_MAX_LAPS == 4 (8-byte Time stride at +0x24).
            CgsSystem::Time GetLapTime(u32 luLap) const
            {
                CGS_ASSERT(luLap < 4, "luLap < 4");
                return maaLapTimes[luLap];
            }
            void SetLapTime(u32 luLap, CgsSystem::Time lTime)
            {
                CGS_ASSERT(luLap < 4, "luLap < 4");
                maaLapTimes[luLap] = lTime;
            }

            bool            GetHasFinished() const                 { return mbHasFinished; }              // +0x45
            void            SetHasFinished(bool lbHasFinished)     { mbHasFinished = lbHasFinished; }     // +0x45

            // Getter for mfDistanceToFinish already exists above (GetDistanceToFinish, +0x48);
            // add the matching setter (SetPlayerEliminated captures the live distance into this slot).
            void            SetDistanceToFinish(f32 lfDistance)    { mfDistanceToFinish = lfDistance; }   // +0x48

            s32             GetTakedownsAgainst() const            { return miTakedownsAgainst; }         // +0x50
            void            SetTakedownsAgainst(s32 liTakedowns)   { miTakedownsAgainst = liTakedowns; }  // +0x50
            s32             GetMarkedManTakedownsFor() const       { return miMarkedManTakedownsFor; }       // +0x54
            void            SetMarkedManTakedownsFor(s32 li)       { miMarkedManTakedownsFor = li; }         // +0x54 (UpdateCrashes @0x8231F9B8 increments this per primary crash -- see crash-tally FLAG below)
            s32             GetTraitorousTakedownsFor() const      { return miTraitorousTakedownsFor; }      // +0x120
            void            SetTraitorousTakedownsFor(s32 li)      { miTraitorousTakedownsFor = li; }        // +0x120 (UpdateTakedowns @0x8232AC88 increments on a same-team "traitorous" takedown)
            s32             GetTraitorousTakedownsAgainst() const  { return miTraitorousTakedownsAgainst; }  // +0x124
            void            SetTraitorousTakedownsAgainst(s32 li)  { miTraitorousTakedownsAgainst = li; }    // +0x124 (UpdateTakedowns @0x8232AC88 increments on a same-team "traitorous" takedown)

            // Marked-man takedown-event tallies (UpdateTakedowns @0x8232AC88 increments the +0x108
            // aggressor / +0x10C victim slot when TakedownEvent::mbMarkedManTakeDown is set).
            s32             GetMarkedManTakedownEventsFor() const         { return miMarkedManTakedownEventsFor; }     // +0x108
            void            SetMarkedManTakedownEventsFor(s32 li)         { miMarkedManTakedownEventsFor = li; }       // +0x108
            s32             GetMarkedManTakedownEventsAgainst() const     { return miMarkedManTakedownEventsAgainst; } // +0x10C
            void            SetMarkedManTakedownEventsAgainst(s32 li)     { miMarkedManTakedownEventsAgainst = li; }   // +0x10C

            // Payback / dirty-trick takedown tallies (UpdatePaybackTakedowns @0x82338320 increments
            // the for/against pair selected by the DirtyTrickEvent status word: status 2 -> +0x110/+0x114,
            // status 4 -> +0x118/+0x11C).
            s32             GetPaybackTakedownsType2For() const           { return miPaybackTakedownsType2For; }       // +0x110
            void            SetPaybackTakedownsType2For(s32 li)           { miPaybackTakedownsType2For = li; }         // +0x110
            s32             GetPaybackTakedownsType2Against() const       { return miPaybackTakedownsType2Against; }   // +0x114
            void            SetPaybackTakedownsType2Against(s32 li)       { miPaybackTakedownsType2Against = li; }     // +0x114
            s32             GetPaybackTakedownsType4For() const           { return miPaybackTakedownsType4For; }       // +0x118
            void            SetPaybackTakedownsType4For(s32 li)           { miPaybackTakedownsType4For = li; }         // +0x118
            s32             GetPaybackTakedownsType4Against() const       { return miPaybackTakedownsType4Against; }   // +0x11C
            void            SetPaybackTakedownsType4Against(s32 li)       { miPaybackTakedownsType4Against = li; }     // +0x11C

            // Setter for +0x58 already exists above (SetOnlineRacePoints); add the matching getter
            // (UpdateCumulativeResults reads this slot).
            s32             GetOnlineFinishPositionScore() const   { return miOnlineFinishPositionScore; } // +0x58

            EActiveRaceCarIndex GetEliminatorRaceCarIndex() const  { return meEliminatorRaceCarIndex; }   // +0x60
            void            SetEliminatorRaceCarIndex(EActiveRaceCarIndex leIndex) { meEliminatorRaceCarIndex = leIndex; } // +0x60

            s32             GetNumEliminations() const             { return miNumEliminations; }          // +0x64
            void            SetNumEliminations(s32 liEliminations) { miNumEliminations = liEliminations; } // +0x64

            CgsSystem::Time GetTimeInCurrentTeam() const           { return mTimeInCurrentTeam; }         // +0x6C
            void            SetTimeInCurrentTeam(CgsSystem::Time lTime) { mTimeInCurrentTeam = lTime; }   // +0x6C

            // Per-team accumulated time, indexed by team (8-byte Time stride at +0x74, 2 teams).
            CgsSystem::Time GetTimeInTeam(u32 luTeam) const
            {
                CGS_ASSERT(luTeam < 2, "luTeam < 2");
                return maTimeInTeam[luTeam];
            }
            void SetTimeInTeam(u32 luTeam, CgsSystem::Time lTime)
            {
                CGS_ASSERT(luTeam < 2, "luTeam < 2");
                maTimeInTeam[luTeam] = lTime;
            }

            bool            GetEliminated() const                  { return mbEliminated; }               // +0xD9
            void            SetEliminated(bool lbEliminated)       { mbEliminated = lbEliminated; }       // +0xD9

            // +0x6B flag. ClearData (0x821F2B28) zeroes it (stb 0,0x6B); UpdateNetworkPlayerResults
            // (0x8231FA90) sets it to 1 (stb 1,0x6B) for each car that received a valid per-player
            // network result this update. FLAG: identity inferred from those two writes only -- no
            // DWARF name -- so "HasNetworkResults" is a best-guess for the results-applied/valid
            // semantic; treat as provisional (the byte itself is the X360-proven +0x6B slot).
            bool            GetHasNetworkResults() const            { return mbHasNetworkResults; }        // +0x6B
            void            SetHasNetworkResults(bool lbHasResults) { mbHasNetworkResults = lbHasResults; } // +0x6B

            CgsSystem::Time GetTimeInFirstPlace() const            { return mTimeInFirstPlace; }          // +0xE0
            void            SetTimeInFirstPlace(CgsSystem::Time lTime) { mTimeInFirstPlace = lTime; }     // +0xE0

            CgsSystem::Time GetTimeInLastPlace() const             { return mTimeInLastPlace; }           // +0xE8
            void            SetTimeInLastPlace(CgsSystem::Time lTime) { mTimeInLastPlace = lTime; }       // +0xE8

            CgsSystem::Time GetTimeBoosting() const                { return mTimeBoosting; }              // +0xF0
            void            SetTimeBoosting(CgsSystem::Time lTime) { mTimeBoosting = lTime; }             // +0xF0

            // Distance-to-player (f32). UpdateDistanceToPlayer (X360 0x8232B408) computes
            // |GetPlayerPosition() - thisCar.mPosition| each frame and stores it here (X360
            // stfs 0x1C(GetCarData)). First of the two f32 words ClearData zeroes at +0x1C/+0x20.
            f32             GetDistanceToPlayer() const            { return mfDistanceToPlayer; }         // +0x1C
            void            SetDistanceToPlayer(f32 lfDistance)    { mfDistanceToPlayer = lfDistance; }   // +0x1C
            f32             GetDistanceToNextCheckpointLive() const { return mfDistanceToNextCheckpointLive; }  // +0x20
            void            SetDistanceToNextCheckpointLive(f32 lf) { mfDistanceToNextCheckpointLive = lf; }    // +0x20
            bool            GetRacePositionImproved() const         { return mbRacePositionImproved; }          // +0x44
            void            SetRacePositionImproved(bool lbImproved) { mbRacePositionImproved = lbImproved; }   // +0x44

            // Per-frame distance accumulator (f32). UpdateGeneralStats (X360 0x8232B8C0) reads it,
            // adds |currentSpeed| * deltaTime, and writes it back each frame
            // (X360 v27[55] == byte offset 0xDC on GetCarData == this record).
            f32             GetDistanceAccumulator() const         { return mfDistanceAccumulator; }      // +0xDC
            void            SetDistanceAccumulator(f32 lfDistance) { mfDistanceAccumulator = lfDistance; } // +0xDC

            // Longest-drift / longest-continuous-run max distance (f32). UpdateGeneralStats
            // (X360 0x8232B8C0) writes the recorded maximum here (stfs 0xF8(GetCarData)).
            f32             GetLongestDrift() const                { return mfLongestDrift; }             // +0xF8
            void            SetLongestDrift(f32 lfDistance)        { mfLongestDrift = lfDistance; }       // +0xF8

            // Per-car online stunt score (+0xD4). Summed per-team by ScoringSystem::GetTeamStuntScore
            // and read by the online stunt-run scorer's gather loop.
            s32             GetOnlineStuntScore() const            { return miOnlineStuntScore; }         // +0xD4
            void            SetOnlineStuntScore(s32 liScore)       { miOnlineStuntScore = liScore; }      // +0xD4

            // ===== Chainable-stunt-multiplier trio accessors (grown for ScoringSystem) =====
            //
            // The "chainable stunt multiplier" bookkeeping is the per-car miChainableScore (+0xC8) /
            // miChainableField (+0xCC) / miCurrentChainableMultiplier (+0xD0) / mbChainActive (+0xD8)
            // group GetChainableStuntMultipliers (0x8231C170) reads. ScoringSystem also WRITES this
            // group from two paths whose bodies live in BrnScoringSystem_UpdateB.cpp:
            //   * SetNetworkStuntMultiplier (0x8231FC50): std 0xC8 (miChainableScore), stw 0xD0
            //     (miCurrentChainableMultiplier), stb 1,0xD8 (mbChainActive=1).
            //   * UpdateOnlineStuntModeScorePreWorld (0x8234CE48): the per-car expire path clears the
            //     trio + miChainableField (stw 0,0xC8 / sth 0,0xCC / sth 0,0xCE / stw 0,0xD0 / stb 0,0xD8),
            //     and the gather path reads all four into a ChainableMultiplierInfo element.
            // Those slots are PRIVATE here, so publish additive named accessors (grow-only; no layout
            // change) for the embedder to reach them by name.
            s32             GetChainableScore() const              { return miChainableScore; }              // +0xC8
            void            SetChainableScore(s32 liScore)         { miChainableScore = liScore; }           // +0xC8
            s32             GetChainableField() const              { return miChainableField; }              // +0xCC
            void            SetChainableField(s32 liField)         { miChainableField = liField; }           // +0xCC
            s32             GetCurrentChainableMultiplier() const  { return miCurrentChainableMultiplier; }  // +0xD0
            void            SetCurrentChainableMultiplier(s32 liMultiplier) { miCurrentChainableMultiplier = liMultiplier; } // +0xD0
            bool            GetChainActive() const                 { return mbChainActive != 0; }            // +0xD8
            void            SetChainActive(bool lbActive)          { mbChainActive = lbActive ? 1 : 0; }     // +0xD8

            // ===== Per-car barrel-roll tally (carved from old maStorageFC; +0xFC) =====
            //
            // PlayerPerformedBarrelRolls (X360 0x823634D0) adds the supplied count to this per-car slot
            // (asm 0x8236352C lwz 0xFC(r3) / add / 0x82363534 stw 0xFC(r3)). Carved as a named s32 out of
            // the old maStorageFC[12] pad PRESERVING the record's sizeof (an 8-byte pad now covers
            // +0x100..+0x108; ClearData zeroes +0xFC/+0x100/+0x104).
            s32             GetBarrelRollCount() const             { return miBarrelRollCount; }          // +0xFC
            void            SetBarrelRollCount(s32 liCount)        { miBarrelRollCount = liCount; }       // +0xFC
            void            AddBarrelRolls(s32 liCount)            { miBarrelRollCount += liCount; }      // +0xFC

        private:
            CgsSystem::Time mTotalTime;            // +0x00 (8)  Time(0) in ClearData; the finish time
            s32  miRacePosition;                   // +0x08      live race position (ClearData=8 == last-of-8). GetCarRacePosition return *(CarData+8); also read by RaceCarHasReachedCheckPointWithinEvent (lwz 8(r28) compared to total-1). The checkpoint COUNTER it increments lives on CarData @+0x154 (lbz/stb 0x154), not here -- so +0x08 is unambiguously the race position. (was miField08)
            s32  miHighestRacePosition;            // +0x0C      best (lowest) race position seen; ClearData=9. ClearHighestPositions copies +0x08 -> +0x0C (result[3]=result[2]). (was miField0C)
            s32  miCompletedLaps;                  // +0x10      completed-lap count; ClearData=0. GetRaceCarNumCompletedLaps return *(r3+0x10); RegisterFinishForCar increments it. (was miField10)
            s32  miFinishPosition;                 // +0x14      race finish position; ClearData=9. GetCarRaceFinishPosition return *(r3+0x14); RegisterFinishForCar writes it. (was miField14)
            f32  mfDistanceToFinishLive;           // +0x18      live distance-to-finish (f32). GetRaceCarDistanceToFinish: lfs f1,0x18(r3). DISTINCT from mfDistanceToFinish@+0x48 -- SetPlayerEliminated copies +0x18 -> +0x48 (lfs 0x18 / stfs 0x48), i.e. captures the live distance into the +0x48 slot at elimination, proving two separate fields. (carved from old maStorage18)
            f32  mfDistanceToPlayer;               // +0x1C      distance-to-player (f32). UpdateDistanceToPlayer (X360 0x8232B408) stfs 0x1C(GetCarData). First of the two f32 words ClearData zeroes at +0x1C/+0x20. (carved from old maStorage1C[8])
            f32  mfDistanceToNextCheckpointLive;   // +0x20      live distance-to-next-checkpoint (f32; name provisional, offset X360-proven from UpdateRacePositions 0x8232A668; ClearData stfs 0x20). (carved from old maStorage20)
            CgsSystem::Time maaLapTimes[4];        // +0x24..+0x44  per-lap times, KU_MAX_LAPS==4 (8-byte Time stride). RegisterFinishForCar: base addi r28,r31,0x24; stride +=8; element[laps] write stw 0x24(r11)/stfs 0x28(r11) with r11=laps<<3. GetRaceCarTotalTime sums [0..laps); GetRaceCarFastestLapTime mins [1..laps). ClearData inits 4 elements from +0x24 stride 8.
            bool mbRacePositionImproved;           // +0x44      race-position-improved flag (name provisional, offset X360-proven from UpdateRacePositions; ClearData stb 0x44). (carved from old maStorage44)
            bool mbHasFinished;                    // +0x45       has-crossed-the-line flag; RegisterFinishForCar stb 1,0x45(r31) at finish; ClearData stb 0,0x45. (carved from old maStorage18)
            u8   maStorage46[2];                   // +0x46..+0x48  padding to +0x48
            f32  mfDistanceToFinish;               // +0x48      captured/finish distance-to-finish (Race gather lfs 0x48; written from +0x18 by SetPlayerEliminated)
            s32  miTakedowns;                      // +0x4C      takedown count (BHR gather lwz 0x4C)
            s32  miTakedownsAgainst;               // +0x50      takedowns-against count; ClearData=0. GetNumberOfTakedownsAgainst: lwz r3,0x50(r3). (carved from old maStorage50)
            s32  miMarkedManTakedownsFor;          // +0x54      marked-man takedowns-for; ClearData=0. SaveNetworkRoundData reads lwz 0x54. (carved from old maStorage54)
            s32  miOnlineFinishPositionScore;      // +0x58      ClearData = 0; online points/finish-position countdown writeback. UpdateCumulativeResults reads it (lwz 0x58) and adds into CarData cumulative @+0x130.
            s32  miOnlineStandingsPosition;        // +0x5C      ClearData = -1; online standings rank writeback (was miInvalidIndex5C)
            EActiveRaceCarIndex meEliminatorRaceCarIndex; // +0x60  race-car index that eliminated this car; ClearData = -1 == E_ACTIVE_RACE_CAR_INDEX_INVALID. GetRaceCarEliminatorIndex return *(r3+0x60); SetPlayerEliminated stw a3,0x60(r31). (was miInvalidIndex60)
            s32  miNumEliminations;                // +0x64      number of cars this car eliminated; ClearData=0. GetNumberOfEliminations: lwz *(r3+0x64); SetPlayerEliminated increments the ELIMINATOR's +0x64. (carved from old maStorage64)
            bool mbTimedOut;                       // +0x68      timed-out flag (both gathers lbz 0x68)
            bool mbDisconnected;                   // +0x69      disconnected flag (both gathers lbz 0x69)
            u8   maStorage6A[1];                    // +0x6A       byte (carved from old maStorage6A[2])
            bool mbHasNetworkResults;              // +0x6B       per-car "valid network result applied" flag; ClearData stb 0,0x6B / UpdateNetworkPlayerResults stb 1,0x6B. PROVISIONAL name (no DWARF). (carved from old maStorage6A[2])
            CgsSystem::Time mTimeInCurrentTeam;    // +0x6C (8)  time spent in the current team; reset to 0 by SetPlayerTeam on a team change (addi r3,r11,0x6C; SetFloatVal), accumulated by UpdateTeamStats (addi r3,r31,0x6C; Time::operator+=). Ctor zeroes via stw 0x6C/stfs 0x70. (carved from old maStorage6A)
            CgsSystem::Time maTimeInTeam[2];       // +0x74..+0x84  per-team accumulated time, indexed by team; UpdateTeamStats: base addi r3,r11,0x74 with r11=(team<<3)+r31. Team index itself lives on CarData @+0x13C, outside this record. (carved from old maStorage6A)
            CgsSystem::Time mTimeAsRunner;         // +0x84 (8)  time-as-runner (BHR gather lwz 0x84 / lfs 0x88)
            u8   maStorage8C[48];                  // +0x8C..+0xBC  (incl. chainable-stunt Time pairs region the ctor zeroes)
            bool mbCompletedBurningHomeRun;        // +0xBC      completed-burning-home-run flag (BHR gather lbz 0xBC)
            u8   maStorageBD[7];                   // +0xBD..+0xC4
            s32  miCumulativeCheckpoints;          // +0xC4      cumulative checkpoints (BHR gather lwz 0xC4)
            s32  miChainableScore;                 // +0xC8      GetChainableStuntMultipliers source
            s32  miChainableField;                 // +0xCC      GetChainableStuntMultipliers source
            s32  miCurrentChainableMultiplier;     // +0xD0      guard (this[52] < liMaxMultiplier) + source
            s32  miOnlineStuntScore;               // +0xD4      per-car online stunt score. ScoringSystem::GetTeamStuntScore (0x82320650) sums this across cars whose team matches (lwz 0xD4(GetCarData)); also the gather source for OnlineStuntRunModeScoring::GetTeamScore/UpdatePlayerPoints (X360 *(CarData+212)). (carved from old maStorageD4[4])
            u8   mbChainActive;                    // +0xD8      guard flag (*(this+216))
            bool mbEliminated;                     // +0xD9      eliminated flag; ClearData stb 0,0xD9. SetPlayerEliminated/HasStuntAttackModeEnded stb 1,0xD9(r3); IsBlueTeamEliminated reads lbz 0xD9 (team==2 && !eliminated). (carved from old maStorageTail head)
            u8   maStorageDA[2];                   // +0xDA..+0xDC  bytes
            f32  mfDistanceAccumulator;            // +0xDC      per-frame distance accumulator (f32). UpdateGeneralStats (X360 0x8232B8C0) v27[55] (== byte 0xDC): reads, adds |speed|*dt, writes back. (carved from old maStorageDA[6])
            CgsSystem::Time mTimeInFirstPlace;     // +0xE0 (8)  time spent in 1st place. GetTimeSpentInFirstPlace: lwz 0xE0(r11)/lfs 0xE4(r11). Ctor/ClearData zero via +0xE0. (carved from old maStorageTail)
            CgsSystem::Time mTimeInLastPlace;      // +0xE8 (8)  time spent in last place. GetTimeSpentInLastPlace: *(CarData+232)/+236 == +0xE8/+0xEC. (carved from old maStorageTail)
            CgsSystem::Time mTimeBoosting;         // +0xF0 (8)  time spent boosting. GetTimeSpentBoosting: *(CarData+240)/+244 == +0xF0/+0xF4. (carved from old maStorageTail)
            f32  mfLongestDrift;                   // +0xF8      longest-drift / longest-continuous-run max distance (f32). UpdateGeneralStats (X360 0x8232B8C0) keeps a per-frame run accumulator on CarData @+0x138 (lfs/stfs 0x138(r30), fmadds |*(carIO+0x3CC)|*dt while *(carIO+0x400)!=0); when that condition lapses it compares the run length (+0x138) against this max (asm 0x8232BC44 lfs f13,0xF8(r30) / 0x8232BC48 fcmpu) and, if greater, stores it here (0x8232BC50 stfs f0,0xF8(r30)) then resets +0x138 to 0.0 (0x8232BC54 stfs f31,0x138). r30 is GetCarData(==CarData), CarScoreData embedded at 0, so +0xF8 lands inside this record. (carved from old maStorageF8; gameplay identity inferred from the max-of-a-conditional-accumulator pattern -- name is provisional, see FLAG)
            s32  miBarrelRollCount;                // +0xFC      per-car barrel-roll tally. PlayerPerformedBarrelRolls (X360 0x823634D0) adds the supplied count here (lwz/add/stw 0xFC). ClearData zeroes +0xFC. (carved from old maStorageFC[12] head)
            u8   maStoragePad100[8];               // +0x100..+0x108  trailing pad PRESERVING sizeof (ClearData zeroes +0x100/+0x104)
            // ===== Marked-man takedown-event tallies (carved from old maStorageFC) =====
            // UpdateTakedowns (X360 0x8232AC88) increments these ONLY when the current TakedownEvent's
            // mbMarkedManTakeDown flag is set (lbz 0x24 of the 40-byte event): the aggressor's +0x108
            // and the victim's +0x10C (asm 0x8232ADD8 lwz/stw 0x108(r31) aggressor; 0x8232ADE4 lwz/stw
            // 0x10C(r30) victim; r31/r30 == sub_8231DCD0 == CarScoreData*). These are DISTINCT from
            // miMarkedManTakedownsFor@+0x54 (written by UpdateCrashes, a separate path) -- see FLAG.
            s32  miMarkedManTakedownEventsFor;     // +0x108     marked-man takedown events made (aggressor)
            s32  miMarkedManTakedownEventsAgainst; // +0x10C     marked-man takedown events suffered (victim)
            // ===== Payback / dirty-trick takedown tallies (carved from old maStorageFC) =====
            // UpdatePaybackTakedowns (X360 0x82338320) switches on the DirtyTrickEvent status word
            // (meDirtyTrickStatus, the 4th event word). For status==2 it increments aggressor's +0x110
            // (asm 0x823384BC lwz/stw 0x110(r30)) and victim's +0x114 (0x823384C8 lwz/stw 0x114(r31));
            // for status==4 it increments aggressor's +0x118 (0x82338438 lwz/stw 0x118(r30)) and
            // victim's +0x11C (0x82338444 lwz/stw 0x11C(r31)). Names keep the X360-proven status value;
            // EDirtyTrickStatus only names E_DIRTY_TRICK_NONE==0 so the 2/4 semantics stay provisional.
            s32  miPaybackTakedownsType2For;       // +0x110     dirty-trick status-2 payback made (aggressor)
            s32  miPaybackTakedownsType2Against;   // +0x114     dirty-trick status-2 payback suffered (victim)
            s32  miPaybackTakedownsType4For;       // +0x118     dirty-trick status-4 payback made (aggressor)
            s32  miPaybackTakedownsType4Against;   // +0x11C     dirty-trick status-4 payback suffered (victim)
            s32  miTraitorousTakedownsFor;         // +0x120     traitorous takedowns-for; SaveNetworkRoundData reads lwz 0x120. (carved from old maStorageF8)
            s32  miTraitorousTakedownsAgainst;     // +0x124     traitorous takedowns-against; SaveNetworkRoundData reads lwz 0x124. (carved from old maStorageF8)
            // Byte accounting (each named field carved at its X360-proven offset, blobs padding the
            // gaps so total stays 0x128). Region carves (each sums to the old blob it replaced):
            //   old maStorage18[48]@0x18 -> mfDistanceToFinishLive(4)@0x18 + mfDistanceToPlayer(4)@0x1C +
            //       maStorage20[4]@0x20 + maaLapTimes[4](32)@0x24 + maStorage44[1]@0x44 + mbHasFinished(1)@0x45 +
            //       maStorage46[2]@0x46 = 48.
            //   old maStorage50[8]@0x50 -> miTakedownsAgainst(4)@0x50 + maStorage54[4]@0x54 = 8.
            //   old maStorage64[4]@0x64 -> miNumEliminations(4)@0x64 = 4.
            //   old maStorage6A[26]@0x6A -> maStorage6A[1]@0x6A + mbHasNetworkResults(1)@0x6B +
            //       mTimeInCurrentTeam(8)@0x6C + maTimeInTeam[2](16)@0x74 = 26.
            //   old maStorageTail[79]@0xD9 -> mbEliminated(1)@0xD9 + maStorageDA[2]@0xDA +
            //       mfDistanceAccumulator(4)@0xDC + mTimeInFirstPlace(8)@0xE0 + mTimeInLastPlace(8)@0xE8 +
            //       mTimeBoosting(8)@0xF0 + mfLongestDrift(4)@0xF8 + maStorageFC[36]@0xFC +
            //       miTraitorousTakedownsFor(4)@0x120 + miTraitorousTakedownsAgainst(4)@0x124 = 79.
            //   (the +0x120/+0x124 traitorous fields and +0xF8 longest-drift are carved from the same
            //    tail region; the old single maStorageF8[40]@0xF8 -> mfLongestDrift(4)@0xF8 + maStorageFC[36]@0xFC.)
            //   old maStorageFC[36]@0xFC -> miBarrelRollCount(4)@0xFC + maStoragePad100[8]@0x100 +
            //       miMarkedManTakedownEventsFor(4)@0x108 +
            //       miMarkedManTakedownEventsAgainst(4)@0x10C + miPaybackTakedownsType2For(4)@0x110 +
            //       miPaybackTakedownsType2Against(4)@0x114 + miPaybackTakedownsType4For(4)@0x118 +
            //       miPaybackTakedownsType4Against(4)@0x11C = 36. (UpdateTakedowns 0x8232AC88 writes
            //       +0x108/+0x10C; UpdatePaybackTakedowns 0x82338320 writes +0x110/+0x114/+0x118/+0x11C.)
            //   FLAG: UpdateCrashes (0x8231F9B8) increments +0x54 per crash event (the field this header
            //   already names miMarkedManTakedownsFor). The "marked-man" label on +0x54 predates this work
            //   and may be a misnomer for a crash-aggressor tally; left UNCHANGED (committed field, grow-only).
            // Unchanged named anchors: mTotalTime@0x00, mfDistanceToFinish@0x48, miTakedowns@0x4C,
            // miOnlineFinishPositionScore@0x58, miOnlineStandingsPosition@0x5C, mbTimedOut@0x68,
            // mbDisconnected@0x69, mTimeAsRunner@0x84, maStorage8C@0x8C, mbCompletedBurningHomeRun@0xBC,
            // maStorageBD@0xBD, miCumulativeCheckpoints@0xC4, miChainable*@0xC8, mbChainActive@0xD8.
            // Carved-from-blob: miOnlineStuntScore(4)@0xD4 replaces the old maStorageD4[4]@0xD4;
            // miBarrelRollCount(4)@0xFC carved from the old maStorageFC[12] head (8-byte maStoragePad100 keeps sizeof).
            // Renamed placeholders: miField08/0C/10/14 -> miRacePosition/
            // miHighestRacePosition/miCompletedLaps/miFinishPosition; miInvalidIndex60 -> meEliminatorRaceCarIndex.

            // Compile-time offset guards on the X360-proven scoring-gather slots. These members are
            // PRIVATE, so offsetof on them must run from a context that (a) has private access and
            // (b) sees CarScoreData as a complete type. A member-function body is exactly that: the
            // class is regarded as complete inside it and private members are accessible. (The same
            // asserts at file/namespace scope fail MSVC /permissive- with C2248 -- the access error
            // that used to block every TU including this header.) Never called; the static_asserts
            // fire at compile time. Do NOT change the offsets -- they pin the on-disk record layout.
            static void _AssertLayout()
            {
                static_assert(offsetof(CarScoreData, mfDistanceToPlayer)          == 0x1C, "CarScoreData::mfDistanceToPlayer offset");
                static_assert(offsetof(CarScoreData, mfDistanceToNextCheckpointLive) == 0x20, "CarScoreData::mfDistanceToNextCheckpointLive offset");
                static_assert(offsetof(CarScoreData, mbRacePositionImproved)      == 0x44, "CarScoreData::mbRacePositionImproved offset");
                static_assert(offsetof(CarScoreData, mfDistanceToFinish)          == 0x48, "CarScoreData::mfDistanceToFinish offset");
                static_assert(offsetof(CarScoreData, mfDistanceAccumulator)       == 0xDC, "CarScoreData::mfDistanceAccumulator offset");
                static_assert(offsetof(CarScoreData, mfLongestDrift)              == 0xF8, "CarScoreData::mfLongestDrift offset");
                static_assert(offsetof(CarScoreData, miTakedowns)                 == 0x4C, "CarScoreData::miTakedowns offset");
                static_assert(offsetof(CarScoreData, miOnlineFinishPositionScore) == 0x58, "CarScoreData::miOnlineFinishPositionScore offset");
                static_assert(offsetof(CarScoreData, miOnlineStandingsPosition)   == 0x5C, "CarScoreData::miOnlineStandingsPosition offset");
                static_assert(offsetof(CarScoreData, mbTimedOut)                  == 0x68, "CarScoreData::mbTimedOut offset");
                static_assert(offsetof(CarScoreData, mbDisconnected)              == 0x69, "CarScoreData::mbDisconnected offset");
                static_assert(offsetof(CarScoreData, mTimeAsRunner)               == 0x84, "CarScoreData::mTimeAsRunner offset");
                static_assert(offsetof(CarScoreData, mbCompletedBurningHomeRun)   == 0xBC, "CarScoreData::mbCompletedBurningHomeRun offset");
                static_assert(offsetof(CarScoreData, miCumulativeCheckpoints)     == 0xC4, "CarScoreData::miCumulativeCheckpoints offset");
                static_assert(offsetof(CarScoreData, miChainableScore)            == 0xC8, "CarScoreData::miChainableScore offset");
                static_assert(offsetof(CarScoreData, miOnlineStuntScore)          == 0xD4, "CarScoreData::miOnlineStuntScore offset");
                static_assert(offsetof(CarScoreData, miBarrelRollCount)           == 0xFC, "CarScoreData::miBarrelRollCount offset");
                static_assert(offsetof(CarScoreData, miMarkedManTakedownsFor)     == 0x54,  "CarScoreData::miMarkedManTakedownsFor offset");
                static_assert(offsetof(CarScoreData, miMarkedManTakedownEventsFor)     == 0x108, "CarScoreData::miMarkedManTakedownEventsFor offset");
                static_assert(offsetof(CarScoreData, miMarkedManTakedownEventsAgainst) == 0x10C, "CarScoreData::miMarkedManTakedownEventsAgainst offset");
                static_assert(offsetof(CarScoreData, miPaybackTakedownsType2For)       == 0x110, "CarScoreData::miPaybackTakedownsType2For offset");
                static_assert(offsetof(CarScoreData, miPaybackTakedownsType2Against)   == 0x114, "CarScoreData::miPaybackTakedownsType2Against offset");
                static_assert(offsetof(CarScoreData, miPaybackTakedownsType4For)       == 0x118, "CarScoreData::miPaybackTakedownsType4For offset");
                static_assert(offsetof(CarScoreData, miPaybackTakedownsType4Against)   == 0x11C, "CarScoreData::miPaybackTakedownsType4Against offset");
                static_assert(offsetof(CarScoreData, miTraitorousTakedownsFor)    == 0x120, "CarScoreData::miTraitorousTakedownsFor offset");
                static_assert(offsetof(CarScoreData, miTraitorousTakedownsAgainst)== 0x124, "CarScoreData::miTraitorousTakedownsAgainst offset");
            }
        };
        // Per-member offset guards live in CarScoreData::_AssertLayout above (offsetof on the private
        // members needs the complete-class context a member-function body provides). This file-scope
        // assert needs no member access, so it stays here to pin the total record size.
        static_assert(sizeof(CarScoreData) == 296, "CarScoreData layout drift (0x128)");

        // X360 0x821F2B08. Free predicate over EGameModeType: true for exactly
        // E_MODE_ONLINE_FREE_BURN_LOBBY (15) and E_MODE_ONLINE_SHOWTIME (16).
        bool IsOnlineFreeBurnLobby(EGameModeType leGameMode);

        // OnlineGameResults re-homed to its DWARF home BrnGameActions.h (typed members +
        // GameAction<E_ACTION_ONLINE_GAME_RESULT> base, which is visible there). The provisional
        // u32 mauWords[65] blob + its operator= that used to live here are removed.

        // ===== Added for the RaceCarEntityModuleIO IO-buffer unlock (scoring_director group) =====

        // Scoring snapshot the ScoringSystem publishes each frame. Embedded BY VALUE in
        // RaceCarEntityModuleIO::InputBuffer_PrePhysics (mScoringInterface, IO header :445; the
        // buffer typedefs RaceCarEntityModuleIO::ScoringOutputInterface as ScoringInterface).
        //
        // Layout recovered VERBATIM from the DecFIGS DWARF (BrnGameStateSharedIO.h:538-579) -- the
        // member set + order below is the DWARF order. Cross-checked against what
        // ScoringSystem::WriteDataToOutput (X360 0x8232AE98) writes: a per-car loop memcpy's
        // CarScoreData[8] (296B each) into +0, then writes the cumulative score (CarData+76 ==
        // miFinishPosition), the car id (CarData+37, stored as a QWORD == CgsID), and the
        // per-car eliminated flag (CarData+217 == CarScoreData::mbEliminated); the online/offline
        // tails then fill the per-mode scalar/enum members. The X360 build has internal alignment
        // padding between the array blocks (its dst offsets are not densely packed), but there is
        // NO sizeof assert on this type -- it is accessed BY NAME, never sized -- so the DWARF
        // member set/types/order is authoritative and exact byte size is not required.
        //
        // maStunts[1] (StuntToDisplay) is embedded by value (see #include above);
        // ECurrentMedalTargetTime is forward-declared opaquely (cycle with BrnScoringSystem.h).
        // No Vector*/Matrix*/SIMD/EventQueue member in the DWARF -> natural alignment.
        struct ScoringOutputInterface
        {
            CarScoreData            maCarScoreData[8];          // :538  per-car score records (X360: memcpy 296B/car)
            s32                     maiCumulativeScoreData[8];  // :539  (X360: CarData+76 per car; dst +0x980)
            s32                     maiNumRoadsRuled[8];        // :540  (dst +0x9A0 gap; not written by WriteDataToOutput)
            CgsID                   maCarIds[8];                // :541  (X360: CarData+37 per car, stored as QWORD; dst +0x9C0)
            bool                    mabPlayerEliminated[8];     // :542  (X360: CarData+217 per car; dst +0xA00)
            bool                    mabValid[8];                // :543  (dst +0xA08)
            // Per-team stunt-score output array. X360-AUTHORITATIVE: WriteDataToOutput
            // (0x8232AE98) sets r28 = out+0xA10 then loops t in [0, E_PLAYER_TEAM_COUNT==9)
            // writing out[+0xA10 + 4*t] = ScoringSystem::GetTeamStuntScore(t) (asm
            // 0x8232B050 addi r28,r30,0xA10 / 0x8232B0A4 stw r3,0(r28) / 0x8232B0A8 addi
            // r28,r28,4 / 0x8232B0AC cmpwi r29,9). It sits between mabValid[8] (dst +0xA08)
            // and mePlayerRaceCarIndex (dst +0xA38); 9*4==36 bytes (+0xA10..+0xA34) with 4
            // bytes natural-alignment pad before the +0xA38 enum. (Not in the recorded DWARF
            // member list -- X360-only output slot.)
            s32                     maiTeamStuntScores[9];      // dst +0xA10 (per-team stunt score)
            EActiveRaceCarIndex     mePlayerRaceCarIndex;       // :544  (dst +0xA38)
            s32                     miNumPlayersInGame;         // :545
            EGameModeType           meGameModeType;             // :546
            bool                    mbIsOnlineGameMode;         // :547
            s32                     miPursuitCarDamageLeft;     // :550
            f32                     mfPursuitCarDistanceFromPlayer; // :551
            s32                     miRoadRageNumTakedowns;     // :552
            s32                     miRoadRageTakedownTarget;   // :553
            s32                     miShowtimeCarsCrashed;      // :554
            s32                     miShowtimeScoreMultiplier;  // :555
            s32                     miShowtimeComboMultiplier;  // :556
            f32                     mfShowtimeDistanceTravelled;// :557
            s32                     miCurrentScore;             // :559
            s32                     miTargetScore;              // :560
            s32                     miComboScore;               // :561
            s32                     miComboMultiplier;          // :562
            u32                     muCurrentStunts;            // :563
            u32                     muAllStunts;                // :564
            BrnGameState::StuntToDisplay maStunts[1];           // :565
            f32                     mfComboWarningTimeActive;   // :566
            bool                    mbComboWarningActive;       // :567
            bool                    mbComboInProgress;          // :568
            s32                     miNumberOfRivals;           // :570
            f32                     mfModeTimeElapsed;          // :571
            f32                     mfModeTimeRemaining;        // :572
            f32                     mfCurrentTargetModeTime;    // :573
            BrnGameState::ECurrentMedalTargetTime meCurrentMedalTarget;   // :574
            BrnGameState::ECurrentMedalTargetTime meCurrentMedalAchieved; // :575
            f32                     mfDistanceDrivenInCurrentCar; // :577
            bool                    mbTimerActive;              // :579

            void operator=(const ScoringOutputInterface&);      // :583 (declare-only)
        };

        // Network output interface for the online-mode scoring results. Embedded BY VALUE in
        // RaceCarEntityModuleIO::InputBuffer_PrePhysics (mOnlineScoringInterface, IO header :446)
        // and written by the online scorers' WriteDataToOutput. Layout is X360-AUTHORITATIVE:
        // OnlineBurningHomeRunModeScoring::WriteDataToOutput (X360 0x82315650) copies three 8-word
        // blocks into this struct at dst offsets +0x20 (maOnlineAwards), +0x40
        // (maiOnlineAwardVariables) and +0x60 (maePlayerTeam) -- which pins maiNumEliminations[8]
        // at +0x00, maOnlineAwards[8] at +0x20, maiOnlineAwardVariables[8] at +0x40 and
        // maePlayerTeam[8] at +0x60. The remaining members are from the recorded DWARF layout
        // (BrnGameStateSharedIO.h:655-660): maeBlueTeamFinishTypes[8] at +0x80 then mbRedTeamWon.
        // Single owner: grow in place, do not fork.
        struct OnlineScoringOutputInterface
        {
            s32                 maiNumEliminations[8];        // +0x00  (base WriteDataToOutput writes this; BHR omits it)
            EOnlineAwardID      maOnlineAwards[8];            // +0x20
            s32                 maiOnlineAwardVariables[8];   // +0x40
            EPlayerTeam         maePlayerTeam[8];             // +0x60
            EBlueTeamFinishType maeBlueTeamFinishTypes[8];    // +0x80
            bool                mbRedTeamWon;                 // +0xA0
        };
        static_assert(offsetof(OnlineScoringOutputInterface, maOnlineAwards)          == 0x20, "OnlineScoringOutputInterface +0x20 drift");
        static_assert(offsetof(OnlineScoringOutputInterface, maiOnlineAwardVariables) == 0x40, "OnlineScoringOutputInterface +0x40 drift");
        static_assert(offsetof(OnlineScoringOutputInterface, maePlayerTeam)           == 0x60, "OnlineScoringOutputInterface +0x60 drift");
        static_assert(offsetof(OnlineScoringOutputInterface, maeBlueTeamFinishTypes)  == 0x80, "OnlineScoringOutputInterface +0x80 drift");
    }
}
