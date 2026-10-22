#pragma once

#include "types.hpp"

struct HWND__;
struct tagMSG;
struct XOVERLAPPED;

// FLAG PC-platform leaf: native keyboard UI ownership, not an XAM implementation.
// Pump/ProcessMessage/OwnerClosing run only on the existing main window thread.
// Other calls never use HWNDs or dereference title storage from that thread.
namespace CgsSystem { namespace KeyboardPC
{
    void Pump(HWND__* lpOwner);
    bool ProcessMessage(tagMSG* lpMessage);
    void OwnerClosing(HWND__* lpOwner);
    void Forget(XOVERLAPPED* lpOverlapped);
    bool IsCapturingInput();
    bool BlockGameInput(bool lbControlsNeutral);
    bool BlockDebugInput(bool lbControlsNeutral);
} }
