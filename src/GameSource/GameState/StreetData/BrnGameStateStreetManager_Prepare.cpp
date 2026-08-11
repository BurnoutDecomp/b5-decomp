// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_Prepare.cpp
//   BrnGameState::StreetManager::Prepare  @ 0x82350900
//
// ⭐ THIS IS THE DISTRICT-MAP LEG. GameStateModule::Prepare @0x8239E578 stage 23
// (E_PREPARESTAGE_STREET_MANAGER) is its only caller:
//     *(this+552) = 23;
//     if (!StreetManager::Prepare(this + 284520, out, this + 232384)) break;
// and Prepare is what drives LoadDistrictMap @0x8234FB98 -- the ONLY writer of
// mDistrictMapResourceHandle, which StreetManager::SetupParRivals @0x8233F560
// dereferences unconditionally.
//
// SIBLING TU (split out of the wave-B group-0 partfile BrnGameStateStreetManager_wB_00.cpp,
// which also carries Construct @0x82335978 and Destruct @0x82335D40) ON PURPOSE, and it is
// MEASURED (cl /c with the build's own flags + dumpbin /SYMBOLS against the linked obj set):
// mounting the whole wB_00 partfile costs ONE unresolved external --
// BrnStreetData::operator++(ScoreType&, int), whose body lives in the street-DATA side's
// SharedClasses/StreetData/BrnChallengeData.cpp -- pulled in by Construct's and Destruct's
// score-type loops. Prepare touches none of it, so this split costs ZERO. Established repo
// pattern (BrnTriggerQueryManager_Prepare.cpp, BrnCarSelectManager_CarChange.cpp,
// BrnGameStateStreetManager_wB_01.cpp). Fold back into wB_00 when that one symbol lands.
//
// ⚠️ StreetManager::Construct is still not called by GameStateModule::Construct (see the
// DELETE-WHEN block there): the embedded sub-object's stage words / owner pointers come from
// the frozen header's in-class initialisers instead, which is exactly why
// meDistrictMapLoadStage starts at E_DISTRICT_MAP_LOAD_REQUEST rather than an indeterminate
// value on the first tick.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"  // the embedded component
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // [diagnostic] gpDebugPrint
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"    // [diagnostic] BinaryFileResource::GetData

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82350900. Streamed prepare: pump LoadAIData then LoadDistrictMap; once
    // both complete, (re)construct + register the debug component and report done.
    //
    // The asm is literally:
    //     assert(lpOutput)         BrnGameStateStreetManager.cpp:281
    //     assert(lpReceiverQueue)  BrnGameStateStreetManager.cpp:282
    //     if (LoadAIData(...) && LoadDistrictMap(...)) {
    //         <DebugComponent base Construct on this+0x1DF0 -- the COMDAT-folded symbol IDA
    //          names BaseCollisionGenerator::Destruct>
    //         *(this+0x1E00) = this;   // mpStreetManager
    //         *(this+0x1DFC) = 0;      // miNumberOfRoadRulesToWin
    //         DebugComponent::Register(this + 0x1DF0);
    //         return 1;
    //     }
    //     return 0;
    // -- the two stores are StreetManagerDebugComponent::Construct's inlined body (its
    // declaration in BrnStreetManagerDebugComponent.h documents both attestation sites), so
    // they go through the named method here.
    // -----------------------------------------------------------------------
    bool StreetManager::Prepare( GameStateModuleIO::OutputBuffer* lpOutput,
                                 CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue )
    {
        CGS_ASSERT( lpOutput, "lpOutput" );
        CGS_ASSERT( lpReceiverQueue, "lpReceiverQueue" );

        if ( LoadAIData( lpOutput, lpReceiverQueue ) && LoadDistrictMap( lpOutput, lpReceiverQueue ) )
        {
            mStreetManagerDebugComponent.Construct( this );
            mStreetManagerDebugComponent.Register();

            // [diagnostic, one-shot] RESIDENT IS NOT USABLE. This walks the EXACT path
            // SetupParRivals @0x8233F560 takes -- handle -> the BinaryFileResource pointer the
            // handle slot holds -> GetData() -> the packed {u16 width, u16 height} grid header
            // WorldMap2D::Construct binds -- so a 256x256 line here is proof the district map is
            // genuinely usable, not merely "acquired". Delete with the rest of the bring-up
            // diagnostics.
            //
            // ⭐ IT ALREADY EARNED ITS KEEP (2026-08-11). The previous boot printed
            // `[StreetManager] district map: handle=0` two lines after the bundle loaded, and
            // that was NOT a diagnostic-timing artifact -- the placement is right. Traced:
            // LoadDistrictMap's ACQUIRE_RESPONSE stage only advances to DONE inside the
            // `GetCount() > 0` branch that also does the bind (console 0x8234FC50 case 3), and
            // Prepare only reaches this block once LoadDistrictMap has returned true, i.e. one
            // full tick AFTER the bind ran. So `handle=0` meant the bind stored a null, which
            // means the pool replied with the both-null handle, which means the acquire itself
            // never matched -- and it didn't: the request carried a POOL-TAGGED resource id and
            // was posted at the console's 32-bit record size. Both fixed in
            // BrnGameStateStreetManager_wB_01.cpp (full evidence there).
            // NEXT BOOT MUST PRINT: `district map: handle=1 grid=256x256`. Anything else and
            // SetupParRivals' PC guard will fire and name itself; do not re-park the leg on a
            // "timing" story without re-reading that stage machine.
            //
            // Latched: GameStateModule::Prepare re-arms its stage word on completion, so this
            // block is otherwise re-entered on every subsequent prepare pass.
            static bool sbDistrictMapLogged = false;
            if ( !sbDistrictMapLogged
              && ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
              && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbDistrictMapLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[StreetManager] district map: handle="
                    << ( mDistrictMapResourceHandle.mpResourceMemory != 0 ? 1 : 0 );

                if ( mDistrictMapResourceHandle.mpResourceMemory != 0 )
                {
                    const CgsResource::BinaryFileResource* lpBinaryFileResource =
                        *reinterpret_cast<const CgsResource::BinaryFileResource* const*>(
                            mDistrictMapResourceHandle.mpResourceMemory );

                    if ( lpBinaryFileResource != 0 )
                    {
                        // The blob's own header, as WorldMap2D::Construct @0x82907FD0 reads it.
                        const u16* lpuGridHeader =
                            static_cast<const u16*>( lpBinaryFileResource->GetData() );
                        *CgsDev::Log::gpDebugPrint
                            << " grid=" << static_cast<u32>( lpuGridHeader[0] )
                            << "x"      << static_cast<u32>( lpuGridHeader[1] );
                    }
                    else
                    {
                        *CgsDev::Log::gpDebugPrint << " grid=NO-MEMORY-RESOURCE";
                    }
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }

            return true;
        }

        return false;
    }
}
