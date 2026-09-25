#pragma once

// ============================================================================
// GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h
//
// The director's six scene-query post-office types and the six post-box types that wait on them
// (DWARF BrnDirectorPostOfficeTypes.h:54..:66). Added 2026-09-25 (FX-DIRECTOR2, the camera
// scene-query closure).
//
// One post office per query kind. The ELEMENT type of each is the SceneManagerIO result record the
// query produces, and the CAPACITY is the console's (the offices sit in DirectorModule at
// +0x9A4 / +0x9D0 / +0xA74 / +0xAA0 / +0xACC / +0xAD4, and their length words are at +0x28 / +0xA0
// / +0x28 / +0x28 / +0x04 / +0x28 -- 10 / 40 / 10 / 10 / 1 / 10 pointer slots, stored by
// DirectorModule::Construct @0x8225C638..0x8225C660 and reset by SceneQueryInterface::Clear
// @0x8221CD38).
//
// The fine line-test office is the odd one out: its package is a POINTER to the result record
// (the record is variable-length -- its intersections follow it in the results queue), so the
// post box holds `const OutEventLineTestFineResult*` and its office's Clear is specialised (see
// BrnPostOffice.h).
// ============================================================================

#include "types.hpp"
#include "GameSource/Director/Utils/BrnPostBox.h"                                          // BrnDirector::PostBox<T>
#include "GameSource/Director/Utils/BrnPostOffice.h"                                       // BrnDirector::PostOffice<T,N>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"                   // the nearest / fast-DS / sphere / volume result records
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_LineTestFineResult.hpp"    // OutEventLineTestFineResult

namespace BrnDirector
{
    // ---- the six post offices (DWARF :54..:59) -------------------------------------------------
    typedef PostOffice<const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult*, 10u>
            LineTestFinePostOffice;                                                              // :54
    typedef PostOffice<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult, 40u>
            LineTestNearestPostOffice;                                                           // :55
    typedef PostOffice<CgsSceneManager::SceneManagerIO::OutEventLineTestFastDoubleSidedResult, 10u>
            LineTestFastDoubleSidedPostOffice;                                                   // :56
    typedef PostOffice<CgsSceneManager::SceneManagerIO::OutEventSphereTestFastResult, 10u>
            SphereTestFastPostOffice;                                                            // :57
    typedef PostOffice<CgsSceneManager::SceneManagerIO::OutEventVolumeTestFineResult, 1u>
            VolumeTestFinePostOffice;                                                            // :58
    typedef PostOffice<CgsSceneManager::SceneManagerIO::OutEventVolumeTestDeepestResult, 10u>
            VolumeTestDeepestPostOffice;                                                         // :59

    // ---- the six post boxes (DWARF :61..:66) ---------------------------------------------------
    typedef PostBox<const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult*>
            LineTestFinePostBox;                                                                 // :61
    typedef PostBox<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>
            LineTestNearestPostBox;                                                              // :62
    typedef PostBox<CgsSceneManager::SceneManagerIO::OutEventLineTestFastDoubleSidedResult>
            LineTestFastDoubleSidedPostBox;                                                      // :63
    typedef PostBox<CgsSceneManager::SceneManagerIO::OutEventSphereTestFastResult>
            SphereTestFastPostBox;                                                               // :64
    typedef PostBox<CgsSceneManager::SceneManagerIO::OutEventVolumeTestFineResult>
            VolumeTestFinePostBox;                                                               // :65
    typedef PostBox<CgsSceneManager::SceneManagerIO::OutEventVolumeTestDeepestResult>
            VolumeTestDeepestPostBox;                                                            // :66
}
