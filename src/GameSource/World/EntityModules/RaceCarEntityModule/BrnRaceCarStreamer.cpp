#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h"

#include "GameShared/GameClasses/Core/CgsID.h"                       // CgsIDCompress / CgsIDUnCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"         // CgsDev::E_PRINTMODE_HEX / DECIMAL

#include <cstring>   // strcpy

// ============================================================================
// BrnWorld::RaceCarStreamer
//   (GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Member access is BY NAME; the X360 absolute offsets in the asm (maxLoadFlags @+4,
// the five contained streamers @+16/+4992/+9968/+14944/+19920, the resource tables
// @+25488/+25744/+26008 on a 32-byte ResourcePtr stride, mabAttribsLoaded @+26000,
// mabAudioLoaded @+26264) are X360 32-bit facts -- they are reproduced by the member
// ORDER/types in the header, not by raw offset arithmetic here.
//
// The "STRM: ..." debug prints stream through CgsDev::Log::gpDebugPrint, gated on the
// global message-filter bit (CgsDev::Message::KX_FILTER_GLOBAL == 1), matching the X360
// filter test and the sibling BrnRaceCarBaseComponentStreamer.cpp.
//
// LADDER NOTE: the load-flag CONSTANTS (gfx=4, physics=8, attrs=16, wheelgfx=32,
// audio=64, ALL_EXCEPT_AUDIO=60) are the immediates the ARTIST asm stores/compares;
// the Feb-2007 partial source carries an OLDER ELoadFlags numbering and lacks the audio
// path -- it is used here only for idiom (the strcpy("VEH_")/("WHE_") + ConvertToString
// + Compress shape of HACKGetValidModelIds, and the per-On* log/flag idiom).
// ============================================================================

namespace BrnWorld
{

// The X360 logs gate on (gxMessageFilterFlags & KX_FILTER_GLOBAL); GLOBAL == bit 0.
static const u64 KX_FILTER_GLOBAL = 1;

// BrnRaceCarStreamer.h:278 (DWARF namespace-scope const). UpdateDesiredCars compares
// mfTimeSinceLastLoad against this gate (X360 asm @0x822EC974: lfs flt_820054D0; the
// Hex-Rays-resolved float literal is 7.0). Seconds.
const f32 KF_TIME_BETWEEN_DESIRED_LOADS = 7.0f;

// @ 0x827E4BC8. Default ctor. The ARTIST asm installs the five contained streamers'
// vtables and zeroes the three intrusively-linked resource-pointer tables (each slot's
// next/prev/this self-links); in human C++ that is exactly the contained members'
// own default construction (each RaceCar*Streamer ctor + each ResourcePtr() zeroing its
// BaseResourcePtr), plus clearing the scalar bookkeeping the asm leaves at 0.
RaceCarStreamer::RaceCarStreamer()
    : mpVehicleList( 0 )
    , mfTimeSinceLastLoad( 0.0f )
    , mbHACK_WaitingForAudioAfterCarSelect( false )
{
    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; liActiveRaceCar++ )
    {
        maxLoadFlags[liActiveRaceCar]      = 0;
        mabAttribsLoaded[liActiveRaceCar]  = false;
        mabAudioLoaded[liActiveRaceCar]    = false;
        maDesiredCarIds[liActiveRaceCar]   = 0;
        maDesiredWheelIds[liActiveRaceCar] = 0;
        maDesiredPriorities[liActiveRaceCar] = 0;
    }
}

// @ 0x822A13C8. True iff every bit in leFlag is set in this car's load-flag byte.
bool RaceCarStreamer::IsFlagSet( s32 liActiveRaceCar, ELoadFlags leFlag ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    return ( maxLoadFlags[liActiveRaceCar] & leFlag ) == leFlag;
}

u8 RaceCarStreamer::GetFlags( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    return maxLoadFlags[liActiveRaceCar];
}

// @ 0x822A1500. The car slot is active (its E_LOADFLAG_ACTIVE bit is set).
bool RaceCarStreamer::IsRaceCarActive( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    return ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) != 0;
}

// IsRaceCarLoaded (declared in header; bodied here as it gates the resource getters and
// IsDesiredRaceCarLoadedForCarSelect in this TU). All resource bits present.
bool RaceCarStreamer::IsRaceCarLoaded( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) != 0,
                "maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE" );

    return ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ALLRESOURCES ) == E_LOADFLAG_ALLRESOURCES;
}

// @ 0x822A1578. The base car model id with its 4-char livery/group prefix stripped:
// decode the graphics streamer's currently-desired CgsID into its printable form, then
// re-compress only from char index 4 onward. Inactive slot -> 0.
CgsID RaceCarStreamer::GetCarModelId( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    if( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) == 0 )
        return 0;

    const CgsID lDesiredAsset = mGraphicsStreamer.GetDesiredAsset( liActiveRaceCar );

    char lacIDBuffer[KI_CGSID_STRING_LEN];
    CgsIDUnCompress( lDesiredAsset, lacIDBuffer );
    return CgsIDCompress( lacIDBuffer + 4 );
}

// @ 0x822B72F8. For car-select: the desired car (maDesiredCarIds) is either unset (treat
// as satisfied) or it must match the current model id AND be loaded (or all-non-audio
// resources present).
bool RaceCarStreamer::IsDesiredRaceCarLoadedForCarSelect( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    if( maDesiredCarIds[liActiveRaceCar] == 0 )
        return true;

    return maDesiredCarIds[liActiveRaceCar] == GetCarModelId( liActiveRaceCar )
        && ( IsRaceCarLoaded( liActiveRaceCar )
          || IsFlagSet( liActiveRaceCar, E_LOADFLAG_ALL_EXCEPT_AUDIO ) );
}

// @ 0x822CC650. The loaded vehicle graphics resource for an already-loaded car.
const RaceCarStreamer::GraphicsResourcePtr& RaceCarStreamer::GetGraphicsResource( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( IsRaceCarLoaded( liActiveRaceCar ), "IsRaceCarLoaded( liActiveRaceCar )" );
    CGS_ASSERT( maGraphicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr,
                "maGraphicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr" );

    return maGraphicsResources[liActiveRaceCar];
}

// (Physics getter -- DWARF home completeness; idiom matches the graphics/wheel getters.)
const RaceCarStreamer::PhysicsResourcePtr& RaceCarStreamer::GetPhysicsResource( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( IsRaceCarLoaded( liActiveRaceCar ), "IsRaceCarLoaded( liActiveRaceCar )" );
    CGS_ASSERT( maPhysicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr,
                "maPhysicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr" );

    return maPhysicsResources[liActiveRaceCar];
}

// @ 0x822CC780. The loaded wheel graphics resource for an already-loaded car.
const RaceCarStreamer::WheelGraphicsResourcePtr& RaceCarStreamer::GetWheelGraphicsResource( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( IsRaceCarLoaded( liActiveRaceCar ), "IsRaceCarLoaded( liActiveRaceCar )" );
    CGS_ASSERT( maWheelGraphicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr,
                "maWheelGraphicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr" );

    return maWheelGraphicsResources[liActiveRaceCar];
}

// ----------------------------------------------------------------------------
// Per-resource load/unload bookkeeping.
// ----------------------------------------------------------------------------

// @ 0x822A1698. Fold lxResourceFlag into this car's load-flag byte. The slot must be
// active and the bit must not already be set (loading the same resource twice is a bug).
void RaceCarStreamer::OnResourceLoaded( s32 liActiveRaceCar, u8 lxResourceFlag )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    const u8 lxLoadFlags = maxLoadFlags[liActiveRaceCar];

    CGS_ASSERT( ( lxLoadFlags & E_LOADFLAG_ACTIVE ) != 0, "lxLoadFlags & E_LOADFLAG_ACTIVE" );
    CGS_ASSERT( ( lxResourceFlag & lxLoadFlags ) == 0,
                "RaceCar resource to have been loaded twice" );

    maxLoadFlags[liActiveRaceCar] = static_cast<u8>( lxLoadFlags | lxResourceFlag );
}

// @ 0x822A1840. Log the unload (hex flag + the pre-clear flag byte). The X360 only logs
// here; the bit is cleared by the caller's path / the base streamer notification.
void RaceCarStreamer::OnResourceUnloading( s32 liActiveRaceCar, u8 lxResourceFlag )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Unloading resource "
                                   << CgsDev::E_PRINTMODE_HEX << static_cast<u32>( lxResourceFlag )
                                   << ". Flags were "
                                   << static_cast<u32>( maxLoadFlags[liActiveRaceCar] )
                                   << CgsDev::E_PRINTMODE_DECIMAL << "\n";
    }
}

// ----------------------------------------------------------------------------
// Component-streamer load/unload notifications. Each logs, folds the bit, and (for the
// resource-carrying ones) caches the loaded ResourcePtr.
// ----------------------------------------------------------------------------

// @ 0x822CC818.
void RaceCarStreamer::OnGraphicsLoaded( s32 liActiveRaceCar, GraphicsResourcePtr lResource )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Graphics loaded: " << liActiveRaceCar << "\n";

    OnResourceLoaded( liActiveRaceCar, E_LOADFLAG_LOADEDGFX );

    CGS_ASSERT( lResource != CgsResource::NULLResourcePtr, "lpResource != CgsResource::NULLResourcePtr" );
    maGraphicsResources[liActiveRaceCar] = lResource;
}

// @ 0x822CC930.
void RaceCarStreamer::OnPhysicsLoaded( s32 liActiveRaceCar, PhysicsResourcePtr lResource )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Physics loaded: " << liActiveRaceCar << "\n";

    OnResourceLoaded( liActiveRaceCar, E_LOADFLAG_LOADEDPHYSICS );

    CGS_ASSERT( lResource != CgsResource::NULLResourcePtr, "lpResource != CgsResource::NULLResourcePtr" );
    maPhysicsResources[liActiveRaceCar] = lResource;
}

// @ 0x822B73E8. Attributes carry no resource pointer; fold the bit + flag mabAttribsLoaded.
void RaceCarStreamer::OnAttributesLoaded( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Attributes loaded: " << liActiveRaceCar << "\n";

    OnResourceLoaded( liActiveRaceCar, E_LOADFLAG_LOADEDATTRS );
    mabAttribsLoaded[liActiveRaceCar] = true;
}

// @ 0x822CCA48.
void RaceCarStreamer::OnWheelGraphicsLoaded( s32 liActiveRaceCar, WheelGraphicsResourcePtr lResource )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Wheel graphics loaded: " << liActiveRaceCar << "\n";

    OnResourceLoaded( liActiveRaceCar, E_LOADFLAG_LOADEDWHEELGFX );

    CGS_ASSERT( lResource != CgsResource::NULLResourcePtr, "lpResource != CgsResource::NULLResourcePtr" );
    maWheelGraphicsResources[liActiveRaceCar] = lResource;
}

// @ 0x822B7488. Audio may be (re-)notified while already flagged: only fold the bit when
// it is not already set (avoids the loaded-twice assert); always mark mabAudioLoaded.
void RaceCarStreamer::OnAudioLoaded( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Audio loaded: " << liActiveRaceCar << "\n";

    if( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_LOADEDAUDIO ) != E_LOADFLAG_LOADEDAUDIO )
        OnResourceLoaded( liActiveRaceCar, E_LOADFLAG_LOADEDAUDIO );

    mabAudioLoaded[liActiveRaceCar] = true;
}

// @ 0x822B7538.
void RaceCarStreamer::OnGraphicsUnloading( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Graphics unloading: " << liActiveRaceCar << "\n";

    OnResourceUnloading( liActiveRaceCar, E_LOADFLAG_LOADEDGFX );
    maGraphicsResources[liActiveRaceCar] = CgsResource::NULLResourcePtr;
}

// @ 0x822B75E8.
void RaceCarStreamer::OnPhysicsUnloading( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Physics unloading: " << liActiveRaceCar << "\n";

    OnResourceUnloading( liActiveRaceCar, E_LOADFLAG_LOADEDPHYSICS );
    maPhysicsResources[liActiveRaceCar] = CgsResource::NULLResourcePtr;
}

// @ 0x822B7698.
void RaceCarStreamer::OnAttributesUnloading( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Attributes unloading: " << liActiveRaceCar << "\n";

    OnResourceUnloading( liActiveRaceCar, E_LOADFLAG_LOADEDATTRS );
    mabAttribsLoaded[liActiveRaceCar] = false;
}

// @ 0x822B7738.
void RaceCarStreamer::OnWheelGraphicsUnloading( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Wheel graphics unloading: " << liActiveRaceCar << "\n";

    OnResourceUnloading( liActiveRaceCar, E_LOADFLAG_LOADEDWHEELGFX );
    maWheelGraphicsResources[liActiveRaceCar] = CgsResource::NULLResourcePtr;
}

// @ 0x822B77E8.
void RaceCarStreamer::OnAudioUnloading( s32 liActiveRaceCar )
{
    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Audio unloading: " << liActiveRaceCar << "\n";

    OnResourceUnloading( liActiveRaceCar, E_LOADFLAG_LOADEDAUDIO );
    mabAudioLoaded[liActiveRaceCar] = false;
}

// @ 0x822A1620. Record whether this car's audio "load data" arrived (mabAudioLoaded).
void RaceCarStreamer::SetAudioLoadDataStatus( s32 liActiveRaceCar, bool lbStatus )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >=0" );
    CGS_ASSERT( liActiveRaceCar < static_cast<s32>( sizeof( mabAudioLoaded ) / sizeof( mabAudioLoaded[0] ) ),
                "liActiveRaceCar < static_cast<int32_t>(sizeof(mabAudioLoaded)/sizeof(mabAudioLoaded[0]))" );

    mabAudioLoaded[liActiveRaceCar] = lbStatus;
}

// @ 0x822A1950. "VEH_" + decode(model id) -> compress; "WHE_" + decode(wheel id) ->
// compress. Each id's printable form is written into lacIDBuffer+4 (after the 4-char
// prefix) and the whole buffer re-compressed back into the ref.
void RaceCarStreamer::HACKGetValidModelIds( CgsID& lrModelId, CgsID& lrWheelId )
{
    char lacIDBuffer[32];

    strcpy( lacIDBuffer, "VEH_" );
    CgsIDConvertToString( lrModelId, lacIDBuffer + 4 );
    lrModelId = CgsIDCompress( lacIDBuffer );

    strcpy( lacIDBuffer, "WHE_" );
    CgsIDConvertToString( lrWheelId, lacIDBuffer + 4 );
    lrWheelId = CgsIDCompress( lacIDBuffer );
}

// ============================================================================
// The eight asset-director functions homed in THIS .cpp (the dossier's ledger
// set for BrnRaceCarStreamer.cpp). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct              @ 0x822F7FA0  (boot-trace EXECUTED)
//   Destruct               @ 0x822EBC98
//   AddVehicleData         @ 0x822EBE18
//   RemoveVehicleData      @ 0x822A53E8
//   SetRequiredVehicleData @ 0x822EC650
//   SetDesiredVehicleData  @ 0x822A5468
//   Update                 @ 0x822F80C0
//   UpdateDesiredCars      @ 0x822EC6F8
//
// The four trivial component streamers (mGraphicsStreamer / mPhysicsStreamer /
// mAttributeStreamer / mWheelGraphicsStreamer) supply their own
// Construct(this)/Destruct() (in-scope callees -- their declarations satisfy
// cl /c here; their bodies land with the RaceCarComponentStreamers leaves). The
// X360 INLINED those leaf Construct/Destruct into RaceCarStreamer::Construct/
// Destruct (calling RaceCarBaseComponentStreamer::Construct directly then writing
// each leaf mpStreamer back-pointer); the human form below delegates to the leaf
// Construct/Destruct, which is where that pool-id/asset-set knowledge belongs.
// ============================================================================

// @ 0x822F7FA0. Install the five component streamers and clear the per-car tables.
// The X360 clears maxLoadFlags / the three graphics-style ResourcePtr tables (to the
// NULL handle) / mabAttribsLoaded / mabAudioLoaded / maDesiredCarIds / maDesiredPriorities
// per car, then constructs each contained streamer and zeroes the scalar bookkeeping.
// (maDesiredWheelIds is intentionally NOT cleared here -- the asm leaves it, paired with
// maDesiredCarIds only when SetDesiredVehicleData writes a fresh request.)
void RaceCarStreamer::Construct()
{
    mpVehicleList = 0;

    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; liActiveRaceCar++ )
    {
        maxLoadFlags[liActiveRaceCar] = 0;

        maGraphicsResources[liActiveRaceCar]      = CgsResource::NULLResourcePtr;
        maPhysicsResources[liActiveRaceCar]       = CgsResource::NULLResourcePtr;
        mabAttribsLoaded[liActiveRaceCar]         = false;
        maWheelGraphicsResources[liActiveRaceCar] = CgsResource::NULLResourcePtr;
        mabAudioLoaded[liActiveRaceCar]           = false;

        maDesiredCarIds[liActiveRaceCar]    = 0;
        maDesiredPriorities[liActiveRaceCar] = -1;
    }

    mGraphicsStreamer.Construct( this );
    mPhysicsStreamer.Construct( this );
    mAttributeStreamer.Construct( this );
    mWheelGraphicsStreamer.Construct( this );
    mAudioStreamer.Construct( this );

    mbHACK_WaitingForAudioAfterCarSelect = false;
    mfTimeSinceLastLoad                  = 0.0f;
}

// @ 0x822EBC98. Tear the streamer down: destruct the five component streamers (the X360
// inlines each leaf Destruct -- clear its mpStreamer back-pointer and reset its base
// linkage) then NULL the three ResourcePtr tables and clear mabAudioLoaded.
void RaceCarStreamer::Destruct()
{
    mWheelGraphicsStreamer.Destruct();
    mAttributeStreamer.Destruct();
    mPhysicsStreamer.Destruct();
    mGraphicsStreamer.Destruct();
    mAudioStreamer.Destruct();

    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; liActiveRaceCar++ )
    {
        maGraphicsResources[liActiveRaceCar]      = CgsResource::NULLResourcePtr;
        maPhysicsResources[liActiveRaceCar]       = CgsResource::NULLResourcePtr;
        mabAudioLoaded[liActiveRaceCar]           = false;
        maWheelGraphicsResources[liActiveRaceCar] = CgsResource::NULLResourcePtr;
    }
}

// @ 0x822EBE18. Begin streaming a car's five asset components for slot liActiveRaceCar.
// If the slot is flagged E_LOADFLAG_UNUSED (a pending removal), first tear the old
// entries down. Re-prefix the ids (HACKGetValidModelIds), probe each component streamer
// for an already-loaded asset (folding the matching LOADED* bit + skipping a redundant
// load when the cached ResourcePtr is non-NULL), commit the assembled flag byte, then
// register the desired asset with each component streamer. lbActive flows to the audio
// streamer as the is-player flag.
void RaceCarStreamer::AddVehicleData( s32 liActiveRaceCar, CgsID lModelId, CgsID lWheelId, bool lbActive )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( lModelId != 0, "lModelId" );
    CGS_ASSERT( lWheelId != 0, "lWheelId" );

    // Re-adding a car to a slot that is already ACTIVE and NOT pending-removal is an error
    // (asm 0x822EBE48: (flags & E_LOADFLAG_ACTIVE) != 0 && (flags & E_LOADFLAG_UNUSED) == 0).
    if( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) != 0
        && ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_UNUSED ) == 0 )
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Flags = " << static_cast<u32>( maxLoadFlags[liActiveRaceCar] )
                   << " Car index = " << liActiveRaceCar << "\n";
        CGS_ASSERT( false, lStrStream.GetBuffer() );
    }

    mfTimeSinceLastLoad                  = 0.0f;
    maDesiredCarIds[liActiveRaceCar]     = 0;
    maDesiredPriorities[liActiveRaceCar] = -1;

    if( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_UNUSED ) != 0 )
    {
        CGS_ASSERT( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) != 0,
                    "( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) != 0" );

        if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "STRM: " << "Removing racecar for streaming: car="
                                       << liActiveRaceCar << ", model=" << lModelId
                                       << ", wheel=" << lWheelId << "\n";
            *CgsDev::Log::gpDebugPrint << "STRM: " << "  Previous flags were "
                                       << CgsDev::E_PRINTMODE_HEX
                                       << static_cast<u32>( maxLoadFlags[liActiveRaceCar] )
                                       << CgsDev::E_PRINTMODE_DECIMAL << "\n";
        }

        mGraphicsStreamer.RemoveEntry( liActiveRaceCar );
        mPhysicsStreamer.RemoveEntry( liActiveRaceCar );
        mAttributeStreamer.RemoveEntry( liActiveRaceCar );
        mWheelGraphicsStreamer.RemoveEntry( liActiveRaceCar );
        mAudioStreamer.RemoveEntry( liActiveRaceCar );
    }

    HACKGetValidModelIds( lModelId, lWheelId );

    char lacModelName[32];
    char lacWheelName[32];
    CgsIDUnCompress( lModelId, lacModelName );
    CgsIDUnCompress( lWheelId, lacWheelName );

    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "Adding racecar for streaming: car="
                                   << liActiveRaceCar
                                   << ", model=" << lacModelName
                                   << ", wheel=" << lacWheelName
                                   << ", model id=" << lModelId
                                   << ", wheel id=" << lWheelId << "\n";
    }

    u8 lxLoadFlags = E_LOADFLAG_ACTIVE;

    if( mGraphicsStreamer.IsVehicleAssetLoaded( lModelId, liActiveRaceCar )
        && maGraphicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr )
    {
        if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
            *CgsDev::Log::gpDebugPrint << "STRM: " << "  Gfx already loaded\n";
        lxLoadFlags |= E_LOADFLAG_LOADEDGFX;
    }
    else if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "  Need to load Gfx\n";
    }

    if( mPhysicsStreamer.IsVehicleAssetLoaded( lModelId, liActiveRaceCar )
        && maPhysicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr )
    {
        if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
            *CgsDev::Log::gpDebugPrint << "STRM: " << "  Phys already loaded\n";
        lxLoadFlags |= E_LOADFLAG_LOADEDPHYSICS;
    }
    else if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "  Need to load Phys\n";
    }

    if( mAttributeStreamer.IsVehicleAssetLoaded( lModelId, liActiveRaceCar )
        && mabAttribsLoaded[liActiveRaceCar] )
    {
        if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
            *CgsDev::Log::gpDebugPrint << "STRM: " << "  Attrs already loaded\n";
        lxLoadFlags |= E_LOADFLAG_LOADEDATTRS;
    }
    else if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "  Need to load Attrs\n";
    }

    if( mWheelGraphicsStreamer.IsVehicleAssetLoaded( lWheelId, liActiveRaceCar )
        && maWheelGraphicsResources[liActiveRaceCar] != CgsResource::NULLResourcePtr )
    {
        if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
            *CgsDev::Log::gpDebugPrint << "STRM: " << "  WheelGfx already loaded\n";
        lxLoadFlags |= E_LOADFLAG_LOADEDWHEELGFX;
    }
    else if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "  Need to load WheelGfx\n";
    }

    if( mAudioStreamer.IsVehicleAssetLoaded( lModelId, liActiveRaceCar, lbActive )
        && mabAudioLoaded[liActiveRaceCar] )
    {
        if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
            *CgsDev::Log::gpDebugPrint << "STRM: " << "  Audio already loaded\n";
        lxLoadFlags |= E_LOADFLAG_LOADEDAUDIO;
    }
    else if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "  Need to load Audio\n";
    }

    if( ( CgsDev::Message::gxMessageFilterFlags & KX_FILTER_GLOBAL ) != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "  Flags are "
                                   << CgsDev::E_PRINTMODE_HEX << static_cast<u32>( lxLoadFlags )
                                   << CgsDev::E_PRINTMODE_DECIMAL << "\n";
    }

    maxLoadFlags[liActiveRaceCar] = lxLoadFlags;

    mGraphicsStreamer.AddEntry( lModelId, liActiveRaceCar );
    mPhysicsStreamer.AddEntry( lModelId, liActiveRaceCar );
    mAttributeStreamer.AddEntry( lModelId, liActiveRaceCar );
    mWheelGraphicsStreamer.AddEntry( lWheelId, liActiveRaceCar );
    mAudioStreamer.AddEntry( lModelId, static_cast<u64>( liActiveRaceCar ), lbActive );
}

// @ 0x822A53E8. Flag a slot for removal (sets E_LOADFLAG_UNUSED). The actual teardown
// happens later in AddVehicleData / UpdateDesiredCars when the slot is re-evaluated.
void RaceCarStreamer::RemoveVehicleData( s32 liActiveRaceCar )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    maxLoadFlags[liActiveRaceCar] |= E_LOADFLAG_UNUSED;
}

// @ 0x822EC650. Ensure slot liActiveRaceCar is loading exactly (lModelId, lWheelId): if
// the car is inactive, add it; if active but its desired model OR wheel asset differs from
// the (validated) request, flag the old data for removal and re-add. lbActive is the audio
// is-player flag passed through to AddVehicleData.
void RaceCarStreamer::SetRequiredVehicleData( s32 liActiveRaceCar, CgsID lModelId, CgsID lWheelId, bool lbActive )
{
    CgsID lValidModelId = lModelId;
    CgsID lValidWheelId = lWheelId;
    HACKGetValidModelIds( lValidModelId, lValidWheelId );

    if( IsRaceCarActive( liActiveRaceCar ) )
    {
        if( mGraphicsStreamer.GetDesiredAsset( liActiveRaceCar ) != lValidModelId
            || mWheelGraphicsStreamer.GetDesiredAsset( liActiveRaceCar ) != lValidWheelId )
        {
            RemoveVehicleData( liActiveRaceCar );
        }
        else
        {
            return;
        }
    }

    AddVehicleData( liActiveRaceCar, lModelId, lWheelId, lbActive );
}

// @ 0x822A5468. Record a deferred "desired" car request for slot liSlotIndex at priority
// liPriority. No-op when the requested model already matches the slot's current model id;
// otherwise stash (model, wheel, priority) for UpdateDesiredCars to action when budget allows.
void RaceCarStreamer::SetDesiredVehicleData( s32 liSlotIndex, CgsID lModelId, CgsID lWheelId, s32 liPriority )
{
    CGS_ASSERT( liSlotIndex >= 0, "liSlotIndex >= 0" );
    CGS_ASSERT( liSlotIndex < KI_MAX_ACTIVE_RACE_CARS, "liSlotIndex < KI_MAX_ACTIVE_RACE_CARS" );

    if( GetCarModelId( liSlotIndex ) != lModelId )
    {
        maDesiredCarIds[liSlotIndex]     = lModelId;
        maDesiredWheelIds[liSlotIndex]   = lWheelId;
        maDesiredPriorities[liSlotIndex] = liPriority;
    }
}

// @ 0x822F80C0. One frame: pump each component streamer's engine (the four graphics-style
// streamers via the inherited InternalBaseStreamer::Update, the audio streamer via its own
// IO-buffer Update), accumulate the inter-load timer, then service any deferred desired-car
// requests.
void RaceCarStreamer::Update( const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                              RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput, f32 lfTimeStep )
{
    mAttributeStreamer.Update();
    mPhysicsStreamer.Update();
    mGraphicsStreamer.Update();
    mWheelGraphicsStreamer.Update();
    mAudioStreamer.Update( lpInput, lpOutput );

    mfTimeSinceLastLoad += lfTimeStep;

    UpdateDesiredCars();
}

// @ 0x822EC6F8. Service deferred desired-car requests. First scan for any active-but-not-yet
// -loaded car -- if one exists we are still busy, so bail. Otherwise pick the highest-priority
// (numerically lowest, non-negative) pending desired slot and, if its desired id differs from
// the slot's current car, start loading it (flagging the previous data for removal). If no slot
// has an explicit priority but enough time has elapsed since the last load
// (mfTimeSinceLastLoad > KF threshold), sweep slots 7..0 looking for a desired id to action.
void RaceCarStreamer::UpdateDesiredCars()
{
    // While any active car still has assets in flight, do not start a new desired load.
    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; liActiveRaceCar++ )
    {
        CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );

        if( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) != 0
            && !IsRaceCarLoaded( liActiveRaceCar ) )
        {
            return;
        }
    }

    // Pick the highest-priority (lowest non-negative value) pending desired slot.
    s32 liHighestPrioritySlot = -1;
    s32 liHighestPriority     = 0x7FFFFFFF;
    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; liActiveRaceCar++ )
    {
        const s32 liPriority = maDesiredPriorities[liActiveRaceCar];
        if( liPriority > -1 && liPriority < liHighestPriority )
        {
            liHighestPrioritySlot = liActiveRaceCar;
            liHighestPriority     = liPriority;
        }
    }

    if( liHighestPrioritySlot == -1 )
    {
        // No prioritised request: only sweep for any pending desired id once enough time has
        // elapsed since the last load.
        if( mfTimeSinceLastLoad <= KF_TIME_BETWEEN_DESIRED_LOADS )
            return;

        for( s32 liActiveRaceCar = KI_MAX_ACTIVE_RACE_CARS - 1; liActiveRaceCar >= 0; liActiveRaceCar-- )
        {
            const CgsID lDesiredCarModelId = maDesiredCarIds[liActiveRaceCar];

            if( ( ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_UNUSED ) != 0
                  || ( maxLoadFlags[liActiveRaceCar] & E_LOADFLAG_ACTIVE ) == 0 )
                && lDesiredCarModelId != 0 )
            {
                if( lDesiredCarModelId == GetCarModelId( liActiveRaceCar ) )
                {
                    maDesiredCarIds[liActiveRaceCar] = 0;
                }
                else
                {
                    AddVehicleData( liActiveRaceCar, maDesiredCarIds[liActiveRaceCar],
                                    maDesiredWheelIds[liActiveRaceCar], false );
                    maxLoadFlags[liActiveRaceCar] |= E_LOADFLAG_UNUSED;
                    return;
                }
            }
        }
        return;
    }

    // Action the prioritised slot.
    const CgsID lDesiredCarModelId = maDesiredCarIds[liHighestPrioritySlot];

    if( ( ( maxLoadFlags[liHighestPrioritySlot] & E_LOADFLAG_UNUSED ) != 0
          || ( maxLoadFlags[liHighestPrioritySlot] & E_LOADFLAG_ACTIVE ) == 0 )
        && lDesiredCarModelId != 0 )
    {
        if( lDesiredCarModelId == GetCarModelId( liHighestPrioritySlot ) )
        {
            maDesiredCarIds[liHighestPrioritySlot]     = 0;
            maDesiredPriorities[liHighestPrioritySlot] = -1;
        }
        else
        {
            AddVehicleData( liHighestPrioritySlot, maDesiredCarIds[liHighestPrioritySlot],
                            maDesiredWheelIds[liHighestPrioritySlot], false );
            maxLoadFlags[liHighestPrioritySlot] |= E_LOADFLAG_UNUSED;
        }
    }
}

} // namespace BrnWorld
