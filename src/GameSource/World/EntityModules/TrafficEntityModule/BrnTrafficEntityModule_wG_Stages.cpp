// ============================================================================
// BrnTrafficEntityModule_wG_Stages.cpp -- TrafficEntityModule::PostSceneUpdate.
// ============================================================================

#include "BrnTrafficEntityModule.h"
#include "BrnTrafficEntityModuleIO.h"

#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // StartMonitor / StopMonitor

namespace BrnTraffic
{

// The update-set bit that says the simulation is paused this frame (same local constant the
// module's other stage files carry).
static const BrnUpdateSet KU_UPDATESET_SIM_PAUSED = 0x1;

void TrafficEntityModule::PostSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                           CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                           BrnTrafficIO::InputBuffer_PostScene*  lpInput,
                                           BrnTrafficIO::OutputBuffer_PostScene* lpOutput,
                                           BrnUpdateSet lUpdateSet )
{
    CgsDev::PerfMonCpu::StartMonitor( miPerfMon_PostSceneUpdate );

    // Write-lock the OUTPUT first, then read-lock the INPUT (same order as PrePhysicsUpdate).
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    const bool lbSimPaused = ( ( lUpdateSet & KU_UPDATESET_SIM_PAUSED ) != 0 );

    // Three republishes out of the race-car -> traffic interface, unconditional and BEFORE the
    // state ladder. The first two are single-BYTE stores of bit 0 and bit 1 of the interface's
    // one flags word; the third is a single-precision float copy.
    {
        typedef BrnWorld::RaceCarEntityModuleIO::RaceCarToTrafficInterface RaceCarToTrafficInterface;
        const RaceCarToTrafficInterface* lpRaceCarToTraffic = lpInput->GetRaceCarToTrafficInterface();

        mbPlayerIsPowerParking =
            lpRaceCarToTraffic->IsFlagSet( RaceCarToTrafficInterface::E_FLAG_PLAYER_IS_POWER_PARKING );
        mbShowtimePlayerOnGround =
            lpRaceCarToTraffic->IsFlagSet( RaceCarToTrafficInterface::E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND );
        mfShowtimeTrafficDensityScale = lpRaceCarToTraffic->GetShowtimeTrafficDensityScale();
    }

    switch ( meState )
    {
    case E_STATE_STARTING_UP:
        // Nothing runs while starting up; the arm exists only to validate meStartingUpState.
        switch ( meStartingUpState )
        {
        case E_STARTINGUPSTATE_WAITING_FOR_PLAYER:
        case E_STARTINGUPSTATE_POPULATING:
        case E_STARTINGUPSTATE_WAITING_FOR_STREAMING:
            break;
        default:
            CGS_ASSERT( false, "Invalid starting up state" );
            break;
        }
        break;

    case E_STATE_RUNNING:
        if ( lbSimPaused )
        {
            // A paused frame runs the crashed-vehicle clean-up and nothing else -- the SAME
            // call the tearing-down arm makes (the console folds the two into one tail).
            CleanUpCrashedVehicles( lpInput );
        }
        else
        {
            PostNearbyTrafficSceneQueryRequest( lpInput, lpOutput );
            CleanUpCrashedVehicles( lpInput );
            if ( mbIsOnlineGameMode )
            {
                HandleCrashingNetworkTraffic( lpInput );
            }
            ConvertSceneResultsToTrafficDataForAI( lpOutput );
            AIPostSceneQueryRequests( lpInput, lpOutput );
        }
        break;

    case E_STATE_TEARING_DOWN:
        switch ( meTearingDownState )
        {
        case E_TEARINGDOWNSTATE_WIPING:
        case E_TEARINGDOWNSTATE_WAITING_TO_RESET:
            break;
        case E_TEARINGDOWNSTATE_FLUSHING:
            CleanUpCrashedVehicles( lpInput );
            break;
        default:
            CGS_ASSERT( false, "Invalid tearing down state" );
            break;
        }
        break;

    default:
        CGS_ASSERT( false, "Invalid state in traffic system" );
        break;
    }

    // Unlock order here is WRITE then READ -- the reverse of the lock order, and the reverse
    // of what the trigger module's post-scene stage does. Store-for-store from the image.
    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();

    CgsDev::PerfMonCpu::StopMonitor( miPerfMon_PostSceneUpdate );
}

}
