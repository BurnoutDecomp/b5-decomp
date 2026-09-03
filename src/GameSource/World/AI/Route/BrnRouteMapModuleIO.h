#pragma once

// BrnAI::RouteMapModuleIO -- the IO surface for the AI route-map module. The
// module's input buffer carries route-request queues (RaceRouteRequest,
// ExtrapolatedRouteRequest) and its output buffer carries a RouteResponse queue.
// Each element type derives from the empty CgsModule::Event base (EBO -> 0 bytes)
// and is stored by its byte image in a fixed-capacity CgsModule::EventQueue<T,N>.
//
// FIELD LAYOUT NOW FINALISED (was opaque). The member names/types/order come from
// the DecFIGS DWARF (BrnRouteMapModuleIO.h); every byte offset is X360-attested by
// the field reads/writes in RouteMapModule::ProcessRaceRoute (@0x8278C2E0) and
// ProcessExtrapolatedRoute (@0x8278C4C8) and pinned with static_assert below.
//
// X360-attested element strides + queue offsets (all from BURNOUT_X360_ARTIST.XEX):
//   * RaceRouteRequest         stride 128 (AddEventSafe @0x8277AF20: `slwi r,r,7`,
//                              16 x `std` 8-byte stores). EventQueue<...,1>::Construct
//                              @0x82789FB8 puts maEvents at this+0x10, N=1.
//   * ExtrapolatedRouteRequest stride 64  (AddEventSafe @0x8277AFD8: `slwi r,r,6`,
//                              8 x `std` 8-byte stores). EventQueue<...,12>::Construct
//                              @0x8278A028 puts maEvents at this+0x10, N=12.
//   * RouteResponse            stride 5136 (AddEvent @0x8277B090: memcpy 5136 bytes;
//                              Append @0x8277B668: XMemCpy 5136*count). EventQueue<...,16>
//                              ::Construct @0x8278A098 puts maEvents at this+0xC, N=16.
//
// The two REQUEST records (RaceRouteRequest, ExtrapolatedRouteRequest) embed the rw
// vpu Vector2/Vector3 lane registers (alignas(16)); the resulting natural offsets land
// EXACTLY on the X360-attested offsets, so they are modelled as honest typed structs
// (sizeof pinned to the 128/64 stride with static_assert).
//
// The RESPONSE record is DELIBERATELY a 5136-byte, 4-byte-aligned OPAQUE image with a
// typed Route* view aliased at +0x00 and the owner/event ids overlaid at +0x140C/+0x140E
// -- NOT a value-typed `Route mRoute` member. Reason (X360-faithful): the committed
// BrnAI::Route embeds Vector4 maNodes[320] (rw vpu Vector4 is alignas(16)), so sizeof(Route)
// rounds up to 5136 and a value member would push RouteResponse to a 16-aligned 5152-byte
// object -- which would (a) break the X360-attested 5136 stride and (b) move EventQueue
// <RouteResponse,16>::maEvents off the committed +0xC (4-byte) landing onto +0x10. The
// X360 instead reuses Route's trailing 4 alignment-pad bytes (5132..5135) for the two ids,
// giving the tight 5136 image reproduced here. GetRoute() reinterprets the image start.

#include "types.hpp"                                            // u8/u16/u32/s32
#include "GameShared/GameClasses/Module/CgsEventQueue.h"        // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event (empty event base)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"          // CgsModule::IOBuffer base
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (the lock-bit tripwires)
#include "GameShared/GameClasses/Containers/CgsArray.h"         // Array<T,N> (block-section list)
#include "GameSource/World/AI/Route/BrnAStar.h"                 // AStarQuality / AStarDistanceFunction
#include "GameSource/World/AI/Route/BrnRoute.h"                 // BrnAI::Route (response view target)
#include "BrnCommonTypes.h"                                     // Vector2 / Vector3 (rw vpu)

#include <cstddef>   // offsetof / size_t (layout pinning)

namespace BrnAI
{
    namespace RouteMapModuleIO
    {
        // ---- shared constants (DWARF BrnRouteMapModuleIO.h:43/44) ----
        const s32 KI_MAX_PLAYER_ROUTE_EXTRAPOLATION_GENERATED_SECTIONS = 16;
        const s32 KI_MAX_BLOCK_SECTIONS = 16;

        // The request "owner" tag (DWARF BrnRouteMapModuleIO.h:51).
        enum RequestOwner
        {
            E_OWNER_AI           = 0,
            E_OWNER_GUI          = 1,
            E_OWNER_MODE_MANAGER = 2,
            E_OWNER_COUNT        = 3,
        };

        // The extrapolated look-ahead flavour (DWARF BrnAI::EExtrapolatedType, used by
        // meRouteType @0x34 in ExtrapolatedRouteRequest). ProcessExtrapolatedRoute
        // (@0x8278C650): `lwz 0x34; cmpwi 0; bne -> ExtrapolateTwistyRoute, else
        // ExtrapolateRouteForwards` -- so NORMAL == 0, TWISTY != 0.
        enum EExtrapolatedType
        {
            E_EXTRAPOLATED_NORMAL = 0,
            E_EXTRAPOLATED_TWISTY = 1,
        };

        // AISection::AISectionId is a plain u32 (see AISectionsResourceType.h: AISection::mId
        // == u32). Spelled out here for the block-section list, matching BrnAStar.h's
        // `u32 maBlockSectionIds[...]` convention (no separate typedef is committed).

        // =====================================================================
        // Input request: a full race-route query. X360 stride 128, embeds two
        // Vector3 lane registers so the natural offsets are the attested ones.
        // =====================================================================
        struct RaceRouteRequest : public CgsModule::Event
        {
            // ProcessRaceRoute (@0x8278C340..): lvx128 v0,r0,r31 (mStartPosition @0x00),
            // lvx128 v0,r31,0x10 (mEndPosition @0x10); lhz 0x20/0x22 (start/end section);
            // base 0x24 + 0x40 count (Array<u32,16>); lhz 0x68/0x6A (owner/event ids);
            // lwz 0x6C/0x70, lbz 0x74 (quality / distance function / shortcuts).
            Vector3                mStartPosition;        // +0x00
            Vector3                mEndPosition;          // +0x10
            u16                    muStartSectionIndex;   // +0x20
            u16                    muEndSectionIndex;     // +0x22
            Array<u32, 16>         mauBlockSections;      // +0x24 (16 ids; count @ +0x64)
            u16                    muOwnerId;             // +0x68
            u16                    muEventId;             // +0x6A
            AStarQuality           meQuality;             // +0x6C
            AStarDistanceFunction  meDistanceFunction;    // +0x70
            bool                   mbUseAIShortcuts;      // +0x74

            // ---- builder (RouteRequestManager calls these by name) ----
            // RouteRequestManager passes (position, endMiddle, startSection, endSection,
            // raceCarIndex); the DWARF declares the canonical arg order (ownerId, eventId,
            // startPos, endPos, startSection, endSection).
            //
            // ⭐ ID SEMANTICS CORRECTED 2026-09-03 (aiwave A5): every AI-side builder on the
            // console stores muOwnerId = 0 == E_OWNER_AI and muEventId = AICar::miRaceCarIndex --
            // GenerateStandardRouteRequest @0x82791568..0x82791578 (`li r28,0 ; sth r29(+0x14C4),
            // +0x6A ; sth r28, +0x68`), GenerateAlternativeRouteRequest @0x82789028..0x82789038,
            // and the two extrapolated builders (@0x82789128/0x8278915C, @0x82789248/0x8278927C
            // for the 64-byte record). The consumer, AIModule::UpdateCarRoutes @0x827956B4, only
            // accepts responses whose muOwnerId == 0 and looks the car up by muEventId. The
            // previous overload stored the race-car index into muOwnerId, so no AI car could ever
            // have received its route. The last parameter is therefore the EVENT id.
            void Construct(Vector3 lStartPosition, Vector3 lEndPosition,
                           u16 luStartSectionIndex, u16 luEndSectionIndex, u16 luEventId)
            {
                mStartPosition      = lStartPosition;
                mEndPosition        = lEndPosition;
                muStartSectionIndex = luStartSectionIndex;
                muEndSectionIndex   = luEndSectionIndex;
                mauBlockSections.Construct();           // live count -> 0
                muOwnerId           = static_cast<u16>(E_OWNER_AI);
                muEventId           = luEventId;
                meQuality           = E_ASTAR_QUALITY_LOW;
                meDistanceFunction  = E_ASTAR_DISTANCE_EUCLIDEAN;
                mbUseAIShortcuts    = false;
            }

            void SetUseAIShortcuts(bool lbUseAIShortcuts)        { mbUseAIShortcuts   = lbUseAIShortcuts; }
            void SetDistanceFunction(AStarDistanceFunction leDF) { meDistanceFunction = leDF; }
            void SetQuality(AStarQuality leQuality)              { meQuality          = leQuality; }
            void AddBlockSectionId(u32 luSectionId)              { mauBlockSections.Append(luSectionId); }

            // ---- accessors (ProcessRaceRoute reads via these) ----
            u16                   GetOwnerId() const             { return muOwnerId; }
            u16                   GetEventId() const             { return muEventId; }
            Vector3               GetStartPosition() const       { return mStartPosition; }
            Vector3               GetEndPosition() const         { return mEndPosition; }
            u16                   GetStartSectionIndex() const   { return muStartSectionIndex; }
            u16                   GetEndSectionIndex() const     { return muEndSectionIndex; }
            u32                   GetBlockSectionId(s32 liIndex) const
                                  { return mauBlockSections.GetItem(static_cast<u32>(liIndex)); }
            s32                   GetBlockSectionIdCount() const { return mauBlockSections.GetCount(); }
            AStarQuality          GetQuality() const             { return meQuality; }
            AStarDistanceFunction GetDistanceFunction() const    { return meDistanceFunction; }
            bool                  UseAIShortcuts() const         { return mbUseAIShortcuts; }
        };

        // =====================================================================
        // Input request: an extrapolated (look-ahead) route query. X360 stride 64.
        // =====================================================================
        struct ExtrapolatedRouteRequest : public CgsModule::Event
        {
            // ProcessExtrapolatedRoute (@0x8278C4C8..): lhz 0(owner)/2(event); the racing-line
            // generators read mCarPosition (a2+0x10), mCarDirection (a2+0x20),
            // muCurrentSectionIndex (a2+0x30 -> clrlwi to u16) and meRouteType (a2+0x34).
            u16                muOwnerId;                    // +0x00
            u16                muEventId;                    // +0x02
            u8                 muNumberOfSectionsToGenerate; // +0x04
            Vector2            mCarPosition;                 // +0x10
            Vector2            mCarDirection;                // +0x20
            u32                muCurrentSectionIndex;        // +0x30
            EExtrapolatedType  meRouteType;                  // +0x34

            // RouteRequestManager calls Construct(direction2D, position2D, currentSection,
            // raceCarIndex). Same id correction as the race-route builder above (aiwave A5,
            // 2026-09-03): GenerateExtrapolatedRouteRequest @0x82789128 `sth r30(+0x14C4), +2`
            // (muEventId = race car index) and @0x8278915C `sth r10(0), +0` (muOwnerId =
            // E_OWNER_AI); GenerateRouteFleeingRouteRequest @0x82789248/0x8278927C identical.
            void Construct(Vector2 lCarDirection, Vector2 lCarPosition,
                           u16 luCurrentSectionIndex, u16 luEventId)
            {
                muOwnerId                    = static_cast<u16>(E_OWNER_AI);
                muEventId                    = luEventId;
                muNumberOfSectionsToGenerate = static_cast<u8>(
                    KI_MAX_PLAYER_ROUTE_EXTRAPOLATION_GENERATED_SECTIONS);
                mCarDirection                = lCarDirection;
                mCarPosition                 = lCarPosition;
                muCurrentSectionIndex        = luCurrentSectionIndex;
                meRouteType                  = E_EXTRAPOLATED_NORMAL;
            }

            u16               GetOwnerId() const              { return muOwnerId; }
            u16               GetEventId() const              { return muEventId; }
            u8                GetNumSectionsToGenerate() const { return muNumberOfSectionsToGenerate; }
            u32               GetCurrentSectionIndex() const  { return muCurrentSectionIndex; }
            Vector2           GetCarPosition() const          { return mCarPosition; }
            Vector2           GetCarDirection() const         { return mCarDirection; }
            EExtrapolatedType GetRouteType() const            { return meRouteType; }
        };

        // =====================================================================
        // Output response: a computed route. X360 stride 5136, 4-byte aligned.
        // OPAQUE image + typed views (see header note: a value Route member would
        // repad the record and move the committed EventQueue maEvents offset).
        // =====================================================================
        struct RouteResponse : public CgsModule::Event
        {
            // 5136-byte 4-byte-aligned image. The Route occupies [0x0000 .. 0x1407]
            // (5132 bytes: maNodes[320] + miNodeCount + miDefaultStartNode + meStatus),
            // and the owner/event ids reuse the trailing pad at +0x140C / +0x140E.
            u32 maImage[1284];   // 5136 bytes

            // Route view aliased at the image start (X360 BuildRoute / Route::Prepare
            // operate directly on this; GetRoute() @ DWARF :172/:175).
            Route*       GetRoute()       { return reinterpret_cast<Route*>(maImage); }
            const Route* GetRoute() const { return reinterpret_cast<const Route*>(maImage); }

            u16  GetOwnerId() const { return OwnerIdRef(); }
            u16  GetEventId() const { return EventIdRef(); }

            // Initialise the id tail (X360 ProcessRaceRoute stores the cached ids at
            // +0x140C/+0x140E; ProcessExtrapolatedRoute writes them from the request).
            void Construct(u16 luOwnerId, u16 luEventId)
            {
                OwnerIdRef() = luOwnerId;
                EventIdRef() = luEventId;
            }

        private:
            // muOwnerId @ +0x140C (5132), muEventId @ +0x140E (5134) inside the image.
            u16&       OwnerIdRef()       { return ViewU16(0x140C); }
            const u16& OwnerIdRef() const { return ViewU16(0x140C); }
            u16&       EventIdRef()       { return ViewU16(0x140E); }
            const u16& EventIdRef() const { return ViewU16(0x140E); }

            u16&       ViewU16(size_t luByteOffset)
            {
                return *reinterpret_cast<u16*>(reinterpret_cast<u8*>(maImage) + luByteOffset);
            }
            const u16& ViewU16(size_t luByteOffset) const
            {
                return *reinterpret_cast<const u16*>(
                    reinterpret_cast<const u8*>(maImage) + luByteOffset);
            }
        };

        // ---- queue typedefs (DWARF :146 / :243 / :183) ----
        typedef CgsModule::EventQueue<RaceRouteRequest, 1>          RaceRouteRequestQueue;
        typedef CgsModule::EventQueue<ExtrapolatedRouteRequest, 12> ExtrapolatedRouteRequestQueue;
        typedef CgsModule::EventQueue<RouteResponse, 16>           RouteResponseQueue;

        // =====================================================================
        // InputBuffer : public CgsModule::IOBuffer (DWARF :258). Holds the two
        // request queues; Update drains them under a read lock.
        // =====================================================================
        struct InputBuffer : public CgsModule::IOBuffer
        {
            // ⭐ MOVED INLINE 2026-08-15 (IO-buffer zero-fill removal audit). This body used to
            // live in BrnRouteMapModuleIO.cpp -- a TU that is NOT on tools/build/build_game_exe.bat,
            // so it was never compiled into the exe and `Construct()` silently resolved to the
            // inherited CgsModule::IOBuffer::Construct, leaving both request queues with
            // mbIsConstructed == false. Inline here it is actually emitted.
            //
            // X360-attested by the CreateIOBuffer<InputBuffer> instantiation @0x82791878, which
            // inlines it whole after `Alloc(this, 0x3B0, name)`:
            //     li r11,1 ; stb r11,0(r31)                              -- IOBuffer::Construct
            //     addi r3,r31,0x10 ; bl EventQueue<RaceRouteRequest,1>::Construct
            //     addi r3,r31,0xA0 ; bl EventQueue<ExtrapolatedRouteRequest,12>::Construct
            // +0x10 / +0xA0 are mRaceRouteRequestQueue / mExtrapolatedRouteRequestQueue (the
            // queues are 16-aligned because their records embed rw vpu Vector3s, which is what
            // puts the first one at +0x10 rather than +0x04).
            void Construct()
            {
                CgsModule::IOBuffer::Construct();
                mRaceRouteRequestQueue.Construct();
                mExtrapolatedRouteRequestQueue.Construct();
            }
            // BODIED 2026-08-15 (same audit): CgsIOBufferStack.h's DestroyIOBuffer<T> is the
            // console's mirror now and calls T::Destruct. DestroyIOBuffer<RouteMapModuleIO::
            // InputBuffer> @0x8278A200 (Free 944) shows the call resolving straight to
            // `CgsModule::IOBuffer::Destruct` -- base-only, no queue teardown.
            void Destruct() { CgsModule::IOBuffer::Destruct(); }

            // The two NON-const producer getters are emitted out-of-line by the X360 build
            // (write-lock-guarded, X360 0x8276AE00 / 0x8276AF50); bodies live in
            // BrnRouteMapModuleIO_InputBuffer_Accessors.cpp. Declaration-only here to avoid an
            // ODR clash with those out-of-line definitions (mirrors CgsModelModuleIO.h
            // GetLoadRequests()). The const overloads stay inline (no const getter was emitted
            // out-of-line in this batch).
            RaceRouteRequestQueue*                GetRaceRouteRequestQueue();                                                       // X360 0x8276AE00 (W)
            ExtrapolatedRouteRequestQueue*        GetExtrapolatedRouteRequestQueue();                                               // X360 0x8276AF50 (W)

            // The two const consumer getters the X360 emits out-of-line at 0x8276AEA8 (race) and
            // 0x8276AFF8 (extrapolated), both ARTIST export holes; RouteMapModule::Update
            // @0x82793FA0/0x82793FAC calls them on the read-locked input. Their sibling
            // OutputBuffer::GetRouteResponseQueue const @0x8276B148 (exported) is the shape:
            // `lbz 0(this); extrwi 1,27` == the READ bit, "Not locked for reading\n", then the
            // member address. Reproduced here (asserts added 2026-09-03, aiwave A5).
            const RaceRouteRequestQueue*          GetRaceRouteRequestQueue() const
            {
                CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
                return &mRaceRouteRequestQueue;
            }
            const ExtrapolatedRouteRequestQueue*  GetExtrapolatedRouteRequestQueue() const
            {
                CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
                return &mExtrapolatedRouteRequestQueue;
            }

        private:
            RaceRouteRequestQueue         mRaceRouteRequestQueue;         // :282
            ExtrapolatedRouteRequestQueue mExtrapolatedRouteRequestQueue; // :283
        };

        // =====================================================================
        // OutputBuffer : public CgsModule::IOBuffer (DWARF :295). Holds the
        // response queue; ProcessRaceRoute / ProcessExtrapolatedRoute post into it
        // under a write lock.
        // =====================================================================
        struct OutputBuffer : public CgsModule::IOBuffer
        {
            // ⭐ MOVED INLINE 2026-08-15 (IO-buffer zero-fill removal audit) -- same reason as
            // the InputBuffer twin above: the body's old home, BrnRouteMapModuleIO.cpp, is not on
            // tools/build/build_game_exe.bat, so it never reached the exe.
            //
            // X360-attested by CreateIOBuffer<OutputBuffer> @0x82791960 (`Alloc(this, 82192,
            // name)` then `*p = 1; EventQueue<RouteResponse,16>::Construct(p+4)`). +4 is
            // mRouteResponseQueue (== KU_ROUTE_RESPONSE_QUEUE_OFFSET, pinned below).
            void Construct()
            {
                CgsModule::IOBuffer::Construct();
                mRouteResponseQueue.Construct();
            }
            // BODIED 2026-08-15 (same audit). DestroyIOBuffer<RouteMapModuleIO::OutputBuffer>
            // @0x8278A2D8 (Free 82192) calls `CgsModule::IOBuffer::Destruct` directly -- this
            // buffer's Destruct ICF-folded into the base. Reached every frame through
            // IOHelper<OutputBuffer>::~IOHelper @0x82791820.
            void Destruct() { CgsModule::IOBuffer::Destruct(); }

            // X360 member offset of mRouteResponseQueue, for the record ONLY (the IOBuffer
            // FlagSet<s8> base pads to 4 on the console, so the queue lands @+4 THERE). On this
            // host BaseEventQueue<T> starts with a pointer, so the queue lands @+8 -- the
            // accessors below return &mRouteResponseQueue BY NAME, never this + 4
            // (fixed 2026-09-03, aiwave A5: the write accessor used to return this + 4).
            enum EMemberOffset { KU_ROUTE_RESPONSE_QUEUE_OFFSET_X360 = 0x4 };

            RouteResponseQueue*       GetRouteResponseQueue()       { return &mRouteResponseQueue; }

            // X360 0x8276B148 (R, BrnRouteMapModuleIO.h:624) -- read-lock (status bit 4, `lbz 0(this);
            // extrwi r11,r11,1,27`), "Not locked for reading\n", returns &mRouteResponseQueue.
            // Callers: AIModule::UpdateCarRoutes / Update / PausedUpdate on the read-locked
            // transient "Route" output buffer.
            const RouteResponseQueue* GetRouteResponseQueue() const
            {
                CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
                return &mRouteResponseQueue;
            }

            // X360 0x8276B0A0 (W, :617) -- write-lock (status bit 3); returns &mRouteResponseQueue
            // as a raw u8*. Body in BrnRouteMapModuleIO.cpp. RouteMapModule::Update is the caller.
            u8* GetRouteResponseQueueForWrite();

        private:
            RouteResponseQueue mRouteResponseQueue;   // :313
        };

        // ---- layout pinning (never called; enforced at compile time) ----
        // The request offsets are X360-attested by the field reads in
        // ProcessRaceRoute / ProcessExtrapolatedRoute; the strides are pinned by the
        // committed EventQueue<T,N> instantiations.
        inline void _AssertIoLayout()
        {
            static_assert(sizeof(RaceRouteRequest) == 128, "RaceRouteRequest stride must be 128");
            static_assert(offsetof(RaceRouteRequest, mStartPosition)      == 0x00, "mStartPosition @0x00");
            static_assert(offsetof(RaceRouteRequest, mEndPosition)        == 0x10, "mEndPosition @0x10");
            static_assert(offsetof(RaceRouteRequest, muStartSectionIndex) == 0x20, "muStartSectionIndex @0x20");
            static_assert(offsetof(RaceRouteRequest, muEndSectionIndex)   == 0x22, "muEndSectionIndex @0x22");
            static_assert(offsetof(RaceRouteRequest, mauBlockSections)    == 0x24, "mauBlockSections @0x24");
            static_assert(offsetof(RaceRouteRequest, muOwnerId)           == 0x68, "muOwnerId @0x68");
            static_assert(offsetof(RaceRouteRequest, muEventId)           == 0x6A, "muEventId @0x6A");
            static_assert(offsetof(RaceRouteRequest, meQuality)           == 0x6C, "meQuality @0x6C");
            static_assert(offsetof(RaceRouteRequest, meDistanceFunction)  == 0x70, "meDistanceFunction @0x70");
            static_assert(offsetof(RaceRouteRequest, mbUseAIShortcuts)    == 0x74, "mbUseAIShortcuts @0x74");

            static_assert(sizeof(ExtrapolatedRouteRequest) == 64, "ExtrapolatedRouteRequest stride must be 64");
            static_assert(offsetof(ExtrapolatedRouteRequest, muOwnerId)                    == 0x00, "muOwnerId @0x00");
            static_assert(offsetof(ExtrapolatedRouteRequest, muEventId)                    == 0x02, "muEventId @0x02");
            static_assert(offsetof(ExtrapolatedRouteRequest, muNumberOfSectionsToGenerate) == 0x04, "muNumberOfSectionsToGenerate @0x04");
            static_assert(offsetof(ExtrapolatedRouteRequest, mCarPosition)                 == 0x10, "mCarPosition @0x10");
            static_assert(offsetof(ExtrapolatedRouteRequest, mCarDirection)                == 0x20, "mCarDirection @0x20");
            static_assert(offsetof(ExtrapolatedRouteRequest, muCurrentSectionIndex)        == 0x30, "muCurrentSectionIndex @0x30");
            static_assert(offsetof(ExtrapolatedRouteRequest, meRouteType)                  == 0x34, "meRouteType @0x34");

            static_assert(sizeof(RouteResponse) == 5136, "RouteResponse stride must be 5136");
        }
    }
}
