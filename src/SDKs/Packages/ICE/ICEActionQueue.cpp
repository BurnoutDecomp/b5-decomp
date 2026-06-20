// Per-instantiation TU for CgsContainers::Stack<ICE::ActionRef,20>.
//
// The generic Stack<Type,N> bodies are fully inline in CgsStack.h, so this TU is just the
// explicit class instantiation that forces the four functions this TU owns to compile:
//   IsFull @ 0x8252D5E8, Peek @ 0x82531550, Pop @ 0x825314A0, Push @ 0x825313D0.
// The X360 emits one out-of-line copy of the stack methods per using-TU; this is the
// <ICE::ActionRef,20> copy (Push/IsFull called by BrnDirector::ICEWrapper::UpdateAction,
// Pop/Peek by ICE::ICEController::JoyHandler). ICE::ActionQueue : public Stack<ActionRef,20>.
#include "SDKs/Packages/ICE/ICEActionQueue.hpp"

// Per-method explicit instantiation of exactly the four functions this TU owns. (A whole-
// class `template struct Stack<ICE::ActionRef,20>;` also works but warns C4661 for the
// declaration-only methods Grow/Contains/operator[] that belong to other TUs.)
template void               CgsContainers::Stack<ICE::ActionRef, 20>::Push(const ICE::ActionRef&);
template void               CgsContainers::Stack<ICE::ActionRef, 20>::Pop();
template const ICE::ActionRef& CgsContainers::Stack<ICE::ActionRef, 20>::Peek() const;
template bool               CgsContainers::Stack<ICE::ActionRef, 20>::IsFull() const;
