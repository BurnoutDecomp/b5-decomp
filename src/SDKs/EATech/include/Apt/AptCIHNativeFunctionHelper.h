#pragma once

// ===========================================================================
// EATech Apt -- AptCIHNativeFunctionHelper.
//
// The bundle of static ActionScript native-method callbacks for an AptCIH
// (movie-clip / text-field scene node). Each sMethod_* is the C implementation
// the apt VM's CallMethod dispatch invokes as f(thisValue, argCount), reading its
// AS arguments off the global native-method argument stack and returning an
// AptValue* (the shared `undefined` singleton when the method has no result).
//
// The signatures match the engine's native-callback contract used across the apt
// runtime (see AptCommunicator::sMethod_* / AptActionInterpreter::cbCallMethod_*):
//     AptValue* sMethod_X(AptValue* pContext, int nArgCount);
// where pContext is the AptCIH the method was called on. These are the AS movie-
// clip builtins: attachMovie / createEmptyMovieClip / createTextField /
// duplicateMovieClip / getBounds / getBytesTotal / getDepth / getNewTextFormat /
// getTextFormat / gotoAndPlay / gotoAndStop / hitTest / loadMovie / loadVariables /
// localToGlobal / nextFrame / play / removeMovieClip / removeTextField / setMask /
// setTextFormat / startDrag / swapDepths.
//
// SHAPE + BODIES from the X360 ARTIST.XEX (0x82AD6F80 .. 0x82B0DF68). The class is
// a pure namespace of static methods -- it carries no instance state.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue (callback arg / return)

struct AptCIH;

class AptCIHNativeFunctionHelper
{
public:
    // ---- AS movie-clip native methods (the apt VM CallMethod callbacks) ----
    static AptValue* sMethod_attachMovie(AptValue* pContext, int nArgCount);            // @0x82B0D440
    static AptValue* sMethod_createEmptyMovieClip(AptValue* pContext, int nArgCount);   // @0x82B0BC38
    static AptValue* sMethod_createTextField(AptValue* pContext, int nArgCount);        // @0x82B0BA40
    static AptValue* sMethod_duplicateMovieClip(AptValue* pContext, int nArgCount);     // @0x82B0DEE8
    static AptValue* sMethod_getBounds(AptValue* pContext, int nArgCount);              // @0x82AF5E28
    static AptValue* sMethod_getBytesTotal(AptValue* pContext, int nArgCount);          // @0x82AED8E8
    static AptValue* sMethod_getDepth(AptValue* pContext, int nArgCount);               // @0x82AED6D8
    static AptValue* sMethod_getNewTextFormat(AptValue* pContext, int nArgCount);       // @0x82AFB9F8
    static AptValue* sMethod_getTextFormat(AptValue* pContext, int nArgCount);          // @0x82AFBBC0
    static AptValue* sMethod_gotoAndPlay(AptValue* pContext, int nArgCount);            // @0x82B0D438
    static AptValue* sMethod_gotoAndStop(AptValue* pContext, int nArgCount);            // @0x82B0D430
    static AptValue* sMethod_hitTest(AptValue* pContext, int nArgCount);                // @0x82AED730
    static AptValue* sMethod_loadMovie(AptValue* pContext, int nArgCount);              // @0x82B06D10
    static AptValue* sMethod_loadVariables(AptValue* pContext, int nArgCount);          // @0x82B09C10
    static AptValue* sMethod_localToGlobal(AptValue* pContext, int nArgCount);          // @0x82AF5CE8
    static AptValue* sMethod_nextFrame(AptValue* pContext, int nArgCount);              // @0x82B0D568
    static AptValue* sMethod_play(AptValue* pContext, int nArgCount);                   // @0x82AE2AC8
    static AptValue* sMethod_removeMovieClip(AptValue* pContext, int nArgCount);        // @0x82B09AF0
    static AptValue* sMethod_removeTextField(AptValue* pContext, int nArgCount);        // @0x82B09B80
    static AptValue* sMethod_setMask(AptValue* pContext, int nArgCount);                // @0x82AF8EE0
    static AptValue* sMethod_setTextFormat(AptValue* pContext, int nArgCount);          // @0x82AED470
    static AptValue* sMethod_startDrag(AptValue* pContext, int nArgCount);              // @0x82AD6F80
    static AptValue* sMethod_swapDepths(AptValue* pContext, int nArgCount);             // @0x82AFBE10
};
