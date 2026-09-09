// ============================================================================
// PARKED -- NOT MOUNTED. BrnTraffic::TrafficEntityModule::PostSceneUpdate.
//
// Missing declarations (none has a declaration or a body in the tree):
//   1. TrafficEntityModule::PostNearbyTrafficSceneQueryRequest
//   2. TrafficEntityModule::CleanUpCrashedVehicles -- NOT the already-landed
//      CleanUpCrashedVehiclePhysics, which takes the pre-physics OUTPUT buffer
//   3. TrafficEntityModule::HandleCrashingNetworkTraffic
//   4. TrafficEntityModule::ConvertSceneResultsToTrafficDataForAI
//   5. TrafficEntityModule::AIPostSceneQueryRequests
// Missing names (fields that exist but are unnamed in the committed headers):
//   a. post-scene input interface +0x6A0 bit0/bit1 and the float at +0x6A4, and their
//      three module destinations (+0x717E5, +0x717E6 byte lanes, +0x71824 float lane)
//   b. the network-traffic-enabled byte the middle arm tests (+0x717DC)
// The control flow below is store-for-store and mounts unchanged once those land.
// ============================================================================
#if 0   // PARKED: see banner. Enabling this without the five declarations above is an LNK2019 wall.

#include "BrnTrafficEntityModule.h"
#include "BrnTrafficEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // StartMonitor / StopMonitor

namespace BrnTraffic
{

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

    // Three republishes out of the post-scene input interface, unconditional and BEFORE the
    // state ladder. The first two are single-BYTE stores of bit0 and bit1 of one flags word;
    // the third is a single-precision float copy (a float store, not the int copy the
    // decompiler's integer rendering suggests).
    // FLAG (a) above: the three destinations and the two source fields need names.
    //   mbXXX          = ( lpInput->GetXXXInterface()->muFlags & 1 ) != 0;
    //   mbYYY          = ( lpInput->GetXXXInterface()->muFlags & 2 ) != 0;
    //   mfZZZ          = lpInput->GetXXXInterface()->mfWWW;

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
            CGS_ASSERT( false, "Invalid starting up state" );   // baked line 0x8E2 == 2274
            break;
        }
        break;

    case E_STATE_RUNNING:
        if ( lbSimPaused )
        {
            // A paused frame runs the crashed-vehicle clean-up and nothing else -- the SAME
            // call the tearing-down arm makes (the console folds the two into one tail).
            // CleanUpCrashedVehicles( lpInput );                     // BLOCKER 2
        }
        else
        {
            // PostNearbyTrafficSceneQueryRequest( lpInput, lpOutput );   // BLOCKER 1
            // CleanUpCrashedVehicles( lpInput );                         // BLOCKER 2
            // if ( mbNetworkTrafficEnabled )                             // FLAG (b)
            //     HandleCrashingNetworkTraffic( lpInput );               // BLOCKER 3
            // ConvertSceneResultsToTrafficDataForAI( lpOutput );         // BLOCKER 4
            // AIPostSceneQueryRequests( lpInput, lpOutput );             // BLOCKER 5
        }
        break;

    case E_STATE_TEARING_DOWN:
        switch ( meTearingDownState )
        {
        case 0:
            break;
        case 1:
            // CleanUpCrashedVehicles( lpInput );                     // BLOCKER 2
            break;
        default:
            CGS_ASSERT( false, "Invalid tearing down state" );        // baked line 0x8FF == 2303
            break;
        }
        break;

    default:
        CGS_ASSERT( false, "Invalid state in traffic system" );       // baked line 0x908 == 2312
        break;
    }

    // Unlock order here is WRITE then READ -- the reverse of the lock order, and the reverse
    // of what the trigger module's post-scene stage does. Store-for-store from the export.
    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();

    CgsDev::PerfMonCpu::StopMonitor( miPerfMon_PostSceneUpdate );
}

}

#endif  // PARKED
