#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/AI/SharedIO/BrnAIModuleIO.h
// ============================================================================
// BrnAI::AIModuleIO -- the AI module's per-frame IO buffer slice the world bridges
// drive. InputBuffer is a large CgsModule::IOBuffer payload (~113 KB on the console)
// the world bridges fill (write-locked) and the AI module reads (read-locked).
//
// NAMED LAYOUT 2026-09-03 (aiwave lane A4 -- the five AI world bridges).
// Until this wave InputBuffer was an attested-offset IMAGE BLOB: every accessor
// returned `MemberImage() + <X360 byte offset>` and Construct built exactly ONE of
// the ten members. That model cannot survive the bridges going live on the x64 host:
//   * CgsModule::EventQueue<T,N> leads with a pointer, so the host
//     EventQueue<RaceRouteRequest,1> parked at the console's +0x137D0 is longer than
//     the console's 144-byte gap to mRaceCarRaceDistanceInterface -- the very first
//     AppendRaceRouteRequestQueue would have written the race-distance block;
//   * the un-Constructed queues (game-action, takedown, race-route, scene-result)
//     carry mpEvents == NULL, and the bridges' Append / Clear+Append go through it.
// The DWARF (references/DecFIGS/dwarfdump/GameSource/World/AI/BrnAIModuleIO.h,
// struct InputBuffer :158-:168) names all ten members and every one of them has a
// reconstructed host home, so the blob is gone: members BY NAME in DWARF order,
// accessors return `&mMember`, and Construct builds everything the console's
// Construct @0x8278AB80 builds (see BrnAIModuleIO_InputBuffer_Accessors.cpp).
//
// Console member map -- documentation and identity evidence ONLY, never host
// addressing (pointer widths and alignment differ; the host layout is whatever the
// compiler gives these named members):
//   +0x00010 mRaceCarAIInterface            RaceCarAIInterface                 (Set copies 0x43D0)
//   +0x043E0 mTrafficAIInterface            TrafficAIInterface                 (Set copies 0xB7A0)
//   +0x0FB80 mTimerInterface                CgsSystem::TimerStatusInterface    (Set copies 48)
//   +0x0FBB0 mAIModuleRequestInterface      AIModuleRequestInterface
//   +0x103BC mGameActionQueue               VariableEventQueue<13312,16>
//   +0x137D0 mRaceRouteRequestQueue         EventQueue<RaceRouteRequest,1>
//   +0x13860 mRaceCarRaceDistanceInterface  GameStateModuleIO::RaceCarRaceDistanceInterface (10 words)
//   +0x13888 mSceneResultQueue              VariableEventQueue<32768,16>
//   +0x1B898 mTakedownEventQueue            EventQueue<TakedownEvent,8>
//   +0x1B9E8 mPlayerVehicleControls         BrnWorld::PlayerVehicleControls    (60 bytes)
//
// Lock-bit guard per the recurring IOBuffer prologue (the trailing "\n" matches the
// X360 rodata aNotLockedForRe/aNotLockedForWr; the non-null asserts have NO "\n"):
//   read-lock  (status>>4 & 1) => IsBufferLockedForReading()  ("Not locked for reading\n")
//   write-lock (status>>3 & 1) => IsBufferLockedForWriting()  ("Not locked for writing\n")
// getters read-lock (bit4) EXCEPT the MUTABLE game-action-queue getter @0x8279C4F8
// (IDA-truncated to "InputBuffer::Get") which asserts WRITE (bit3), reproduced
// verbatim; setters/appends write-lock (bit3). The X360-baked file path +
// line-number assert args are dropped per project policy.
//
// Signature compatibility: every pre-wave accessor NAME is kept. Three read getters
// that returned `const void*` (GetTrafficAI, GetGameActionQueue, GetPlayerVehicleControls)
// and the mutable Get() now return the DWARF pointer type instead; a typed pointer
// converts to the old void* implicitly, so existing callers still compile.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                       // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                     // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // CgsModule::VariableEventQueue<N,A>
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"     // CgsSystem::TimerStatusInterface (mTimerInterface, DWARF :160)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"          // BrnPhysics::ContactSpy::ContactSpyInterface (embedded by value in InputBuffer_PostPhysics)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"                   // BrnAI::RouteMapModuleIO::RaceRouteRequestQueue (DWARF :163)
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"             // BrnAI::AIModuleIO::RaceCarAIInterface (DWARF :158)
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"        // BrnAI::AIModuleIO::AIModuleRequestInterface (DWARF :161)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"      // BrnTraffic::BrnTrafficIO::TrafficAIInterface (DWARF :75/:159)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"    // BrnWorld::PlayerVehicleControls (DWARF :168)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                       // GameStateModuleIO::GameActionQueue (DWARF :78) / RaceCarRaceDistanceInterface (DWARF :66)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"    // BrnGameState::TakedownEvent (DWARF :80 -> BrnTakedownManagerTypes.h:97)

namespace BrnAI
{
namespace AIModuleIO
{
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // ---- DWARF typedefs (BrnAIModuleIO.h:66-:80) ------------------------------------
        typedef BrnTraffic::BrnTrafficIO::TrafficAIInterface                    TrafficAIInterface;            // :75
        typedef BrnGameState::GameStateModuleIO::GameActionQueue                GameActionQueue;               // :78 (BaseGameActionQueue<13312> == VariableEventQueue<13312,16>)
        typedef RouteMapModuleIO::RaceRouteRequestQueue                         RaceRouteRequestQueue;         // :163 (EventQueue<RaceRouteRequest,1>)
        typedef BrnGameState::GameStateModuleIO::RaceCarRaceDistanceInterface   RaceCarRaceDistanceInterface;  // :66  (Construct calls ITS Clear @0x82357470)
        // :72 `typedef OutputBuffer::OutSmSceneQueryResultsQueue SceneResultQueue` -- the
        // console Construct calls VariableEventQueue<32768,16>::Construct on this member.
        typedef CgsModule::VariableEventQueue<32768, 16>                        SceneResultQueue;
        typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>           TakedownEventQueue;            // :80
        typedef BrnWorld::PlayerVehicleControls                                 PlayerVehicleControls;         // :168

        // X360 0x8278AB80 -- builds every member (see the .cpp for the console's own order).
        void Construct();                                                                     // :95

        // ---- setters / appends (write-lock bit3) ---------------------------------------
        void SetRaceCarAIInterface(const RaceCarAIInterface* lpInterface);                    // 0x8279C700 :108
        void SetTrafficAIInterface(const TrafficAIInterface* lpInterface);                    // 0x8279C7E0 :112
        void AppendAIModuleRequestInterface(const AIModuleRequestInterface* lpRequestInterface); // 0x827AC960 :116 (Clear()+Append)
        void SetTimerInterface(const CgsSystem::TimerStatusInterface* lpTimerInterface);      // 0x8279C8C0 :120
        void AppendRaceRouteRequestQueue(const RaceRouteRequestQueue* lpQueue);               // 0x827A9560 :140
        void SetRaceCarRaceDistanceInterface(const RaceCarRaceDistanceInterface* lpObject);   // 0x8279C428 :143 (10-word copy)
        void SetTakedownEventQueue(const TakedownEventQueue* lpQueue);                        // 0x827A9618 :152 (Clear()+Append)
        void SetPlayerVehicleControls(const PlayerVehicleControls* lpControls);               // 0x8279C5A0 :155 (60-byte copy)

        // ---- getters (read-lock bit4 unless noted) -------------------------------------
        const RaceCarAIInterface*              GetRaceCarAIInterface() const;                 // 0x8276D728 :124
        const TrafficAIInterface*              GetTrafficAIInterface() const;                 // 0x8276D7D0 :127
        const TrafficAIInterface*              GetTrafficAI() const;                          // same seat, pre-wave spelling (kept)
        const CgsSystem::TimerStatusInterface* GetTimerInterface() const;                     // 0x8276D920 :130
        const AIModuleRequestInterface*        GetAIModuleRequestInterface() const;           // 0x8276D878 :133
        AIModuleRequestInterface*              GetAIModuleRequestInterface();                 // :136 (W; no out-of-line X360 symbol -- inlined)
        const RaceRouteRequestQueue*           GetRaceRouteRequestQueue() const;              // 0x8276D488 :138
        RaceRouteRequestQueue*                 GetRaceRouteRequestQueue();                    // :139 (W; inlined on the console)
        const RaceCarRaceDistanceInterface*    GetRaceCarRaceDistanceInterface() const;       // :142 (inlined on the console)
        const SceneResultQueue*                GetSceneResultQueue() const;                   // :145 (inlined on the console)
        SceneResultQueue*                      GetSceneResultQueue();                         // :146 (W; inlined on the console)
        const GameActionQueue*                 GetGameActionQueue() const;                    // 0x8276D530 :148
        GameActionQueue*                       GetGameActionQueue();                          // 0x8279C4F8 :149 -- asserts the WRITE lock
        GameActionQueue*                       Get();                                         // the IDA-truncated spelling of :149 (kept); same seat
        const TakedownEventQueue*              GetTakedownEventQueue() const;                 // :151 (inlined on the console)
        const PlayerVehicleControls*           GetPlayerVehicleControls() const;              // 0x8276D5D8 :154

        // (DWARF :99 Destruct / :103 Clear are not reconstructed here: no caller in the
        //  tree, and the IOBufferStack path binds DestroyIOBuffer to the base Destruct.)

    private:
        RaceCarAIInterface              mRaceCarAIInterface;            // :158  X360 +0x00010
        TrafficAIInterface              mTrafficAIInterface;            // :159  X360 +0x043E0
        CgsSystem::TimerStatusInterface mTimerInterface;                // :160  X360 +0x0FB80
        AIModuleRequestInterface        mAIModuleRequestInterface;      // :161  X360 +0x0FBB0
        GameActionQueue                 mGameActionQueue;               // :162  X360 +0x103BC
        RaceRouteRequestQueue           mRaceRouteRequestQueue;         // :163  X360 +0x137D0
        RaceCarRaceDistanceInterface    mRaceCarRaceDistanceInterface;  // :165  X360 +0x13860
        SceneResultQueue                mSceneResultQueue;              // :166  X360 +0x13888
        TakedownEventQueue              mTakedownEventQueue;            // :167  X360 +0x1B898
        PlayerVehicleControls           mPlayerVehicleControls;         // :168  X360 +0x1B9E8
    };

    // ------------------------------------------------------------------------
    // InputBuffer_PostPhysics -- the AI module's post-physics INPUT buffer (a
    // SEPARATE, small CgsModule::IOBuffer from the large InputBuffer above). The
    // physics side fills it with the frame's contact-spy handle; the AI post-physics
    // step (AIModule::PostPhysicsUpdate @0x8276E428) drains it. Exactly one member
    // per the DWARF (BrnAIModuleIO.h:263/:286): the embedded contact-spy interface
    // at console +0x04 (right after the 1-byte IOBuffer status flag).
    // Construct/Destruct bodied in BrnAIModuleIO_InputBuffer_PostPhysics.cpp.
    // ------------------------------------------------------------------------
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::ContactSpy::ContactSpyInterface ContactSpyInterface;

        void Construct();   // X360 0x8277BCD0
        void Destruct();    // X360 0x8277BCE8

        // ---- ADDITIVE 2026-09-03 (aiwave lane A4) -- the two DWARF accessors the console
        //      INLINES (no out-of-line symbol for either; neither carries a lock assert
        //      at its inlined sites -- reproduced as plain inlines, not "fixed"): ----
        // DWARF :278. Inlined into AIModule::PostPhysicsUpdate @0x8276E488
        // (`lwz r10, 4(r31)` -- the handle word straight out of the buffer).
        const ContactSpyInterface* GetContactSpyInterface() const { return &mContactInterface; }
        // DWARF :282. Inlined into WorldModule::BridgePhysicsModuleToAIModule_PostPhysics
        // @0x827A56EC..0x827A56F0 (`lwz r11, 0(r3); stw r11, 4(r30)`): the interface IS
        // one handle word, so "append the contacts" is a copy of that handle.
        void AppendContacts(const ContactSpyInterface* lpInterface) { mContactInterface = *lpInterface; }

    private:
        static void _AssertLayout();

        ContactSpyInterface mContactInterface;   // +0x04  DWARF BrnAIModuleIO.h:286
    };
}
}
