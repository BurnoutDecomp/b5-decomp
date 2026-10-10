#pragma once

// BrnGameState::GameStateModuleIO::GameStateToControllerInterface -- the controller bind / unbind
// requests the game state hands the input module through its OutputBuffer (console +0x43AC).
// GameStateInviteManager::Update appends its own two request queues onto these every frame.
//
// Layout: the two EventQueue<BaseInputEvent,8> the console's OutputBuffer::Construct builds at
// +0x00 and +0x4C of this member (76 bytes each), each followed by its length store -- which is
// this interface's Construct (both queue Constructs) then Clear (both lengths zeroed).

#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"   // CgsInput::InputIO::PostWorldInputBuffer::{Bind,UnBind}RequestQueue

namespace BrnGameState
{
namespace GameStateModuleIO
{
    struct GameStateToControllerInterface
    {
        typedef CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue   BindRequestQueue;
        typedef CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue UnBindRequestQueue;

        void Construct()
        {
            mBindRequestQueue.Construct();
            mUnbindRequestQueue.Construct();
            Clear();
        }

        void Clear()
        {
            mBindRequestQueue.Clear();
            mUnbindRequestQueue.Clear();
        }

        const BindRequestQueue*   GetBindRequestQueue() const   { return &mBindRequestQueue; }
        BindRequestQueue*         GetBindRequestQueue()         { return &mBindRequestQueue; }
        const UnBindRequestQueue* GetUnBindRequestQueue() const { return &mUnbindRequestQueue; }
        UnBindRequestQueue*       GetUnBindRequestQueue()       { return &mUnbindRequestQueue; }

    private:
        BindRequestQueue   mBindRequestQueue;     // console +0x00
        UnBindRequestQueue mUnbindRequestQueue;   // console +0x4C
    };

    // ControllerToGameStateInterface -- the controller module's answer to the game state, carried
    // in the PreWorldInputBuffer (console +0x2BE8, 0xE0 bytes): the bind / unbind results of the
    // requests above, and the controller port of the active (signed-in) player. The bridge from
    // the input module appends both result queues and stores the port every frame; the
    // achievement, rich-presence and training managers read the port as their Xbox user index.
    //
    // Console layout: bind results +0x00 (108 bytes), unbind results +0x6C (108 bytes), port +0xD8.
    // Construct runs both queue Constructs and stores -1 (no active port).
    struct ControllerToGameStateInterface
    {
        typedef CgsInput::InputIO::OutputBuffer::BindResultQueue   BindResultQueue;
        typedef CgsInput::InputIO::OutputBuffer::UnBindResultQueue UnBindResultQueue;

        void Construct()
        {
            mBindResultQueue.Construct();
            mUnBindResultQueue.Construct();
            miActiveControllerPort = -1;
        }

        const BindResultQueue*   GetBindResultsQueue() const   { return &mBindResultQueue; }
        BindResultQueue*         GetBindResultsQueue()         { return &mBindResultQueue; }
        const UnBindResultQueue* GetUnBindResultsQueue() const { return &mUnBindResultQueue; }
        UnBindResultQueue*       GetUnBindResultsQueue()       { return &mUnBindResultQueue; }

        s32  GetActiveControllerPort() const            { return miActiveControllerPort; }
        void SetActiveControllerPort(s32 liControllerPort) { miActiveControllerPort = liControllerPort; }

    private:
        BindResultQueue   mBindResultQueue;          // console +0x00
        UnBindResultQueue mUnBindResultQueue;        // console +0x6C
        s32               miActiveControllerPort;    // console +0xD8 (-1 == none)
    };
}
}
