#ifndef BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
#define BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDownloadableConfig.h"

// ===========================================================================
// BrnNetwork::BrnServerInterfaceDownloadableConfig
//   Home: GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.{h,cpp}
//
// The game's downloadable-config component. It derives from the DirtySock
// CgsNetwork::ServerInterfaceDownloadableConfig and owns the storage the downloaded
// "conf" news record is parsed into: Prepare registers one named field per tunable
// (name, type, byte offset into this object) and the base parser writes each field
// it finds straight into that member.
//
// Console layout (32-bit), after the 0x24-byte base:
//   +0x024  maIDsToMemAddrs[20]            (12-byte descriptors, 0xF0 bytes)
//   +0x114  miSizeOfMemLookupTable
//   +0x118  macTelemetryFiltersFirstUse[256]
//   +0x218  macTelemetryFiltersNormalUse[256]
//   +0x318  miIdleTimeOut                  +0x31C  miSearchQueryTimeInterval
//   +0x320  miNatTestPacketTimeout         +0x324  miTOSBufferSize
//   +0x328  miNewsBufferSize               +0x32C  miNumDemanglePlayers
//   +0x330  mfMinSyncingTime               +0x334  mfMaxSyncingTime
//   +0x338  mfWaitForStartTime             +0x33C  mfTimeToWaitForSilentClientReady
//   +0x340  mfTimeToWaitForCommunicatingClientReady
//   +0x344  mfTimeGapToLeaveBeforeStartTime
//   +0x348  mfTimeBetweenStatChecks        +0x34C  mfTimeBetweenRoadRulesUploads
//   +0x350  mfTimeBetweenRoadRulesDownloads
//   +0x354  mfTimeUntilRetryAfterFailedBuddyUpload
//   +0x358  mfTimeBetweenOfflineProgressionUpload
//   +0x35C  mbDoLogOffOnExitOnline
//   sizeof == 0x360
// Every offset is the one Prepare registers for the field's config key, so the
// member each key feeds is fixed by the registration, not by position.
// miNumDemanglePlayers ("NUM_DEMANGLE_PLAYERS") has no counterpart in the reference
// layout; its member and getter names are taken from the key.
// ===========================================================================

namespace BrnNetwork
{
    class BrnServerInterfaceDownloadableConfig : public CgsNetwork::ServerInterfaceDownloadableConfig
    {
    public:
        BrnServerInterfaceDownloadableConfig();

        // Vector deleting destructor.
        virtual ~BrnServerInterfaceDownloadableConfig();

        virtual void Construct();
        virtual void Destruct();
        // A new virtual (its own vtable slot, after the base's Prepare): the parameter
        // type differs from the base's ServerInterfaceDirtySock*.
        virtual bool Prepare( CgsNetwork::ServerInterface* lpServerInterface );
        virtual bool Release();

        // Re-request the configuration record from news item liNewsIndex, parsing it
        // through this object's registered field table.
        void GetConfigDataFromNews( s32 liNewsIndex );

        bool        LogOffOnExitOnline()                          { return mbDoLogOffOnExitOnline; }
        s32         IdleTimeOut()                                 { return miIdleTimeOut; }
        s32         SearchQueryTimeInterval()                     { return miSearchQueryTimeInterval; }
        s32         NatTestPacketTimeout()                        { return miNatTestPacketTimeout; }
        s32         TOSBufferSize()                               { return miTOSBufferSize; }
        s32         NewsBufferSize()                              { return miNewsBufferSize; }
        s32         NumDemanglePlayers()                          { return miNumDemanglePlayers; }
        f32         MinSyncTime()                                 { return mfMinSyncingTime; }
        f32         MaxSyncTime()                                 { return mfMaxSyncingTime; }
        f32         WaitForStartTime()                            { return mfWaitForStartTime; }
        f32         TimeToWaitForSilentClientReady()              { return mfTimeToWaitForSilentClientReady; }
        f32         TimeToWaitForCommunicatingClientReady()       { return mfTimeToWaitForCommunicatingClientReady; }
        f32         TimeGapBeforeStartTime()                      { return mfTimeGapToLeaveBeforeStartTime; }
        const char* TelemetryFiltersFirstUse()                    { return macTelemetryFiltersFirstUse; }
        const char* TelemetryFiltersNormalUse()                   { return macTelemetryFiltersNormalUse; }
        const f32   TimeTillStatsExpire()                         { return mfTimeBetweenStatChecks; }
        const f32   TimeBetweenRoadRulesUploads()                 { return mfTimeBetweenRoadRulesUploads; }
        const f32   TimeBetweenRoadRulesDownloads()               { return mfTimeBetweenRoadRulesDownloads; }
        const f32   TimeUntilRetryAfterFailedBuddyUpload()        { return mfTimeUntilRetryAfterFailedBuddyUpload; }
        const f32   TimeBetweenOfflineProgressionUpload()         { return mfTimeBetweenOfflineProgressionUpload; }

        // Whether lpcGamertag appears in the downloaded "FEVER_CARRIERS" list.
        bool        IsGamertagInFeverList( const char* lpcGamertag );

        // The downloaded "TELE_DISABLE" country list (the tag-field value, or NULL).
        const char* GetTelemetryDisabledList();

    private:
        static const s32 KI_MAX_ELEMENTS_IN_MEMADDR_LOOKUP = 20;
        static const s32 KI_TELEMETRY_FILTERS_MAX_LENGTH   = 256;

        CgsNetwork::DataIDToMemoryAddr maIDsToMemAddrs[KI_MAX_ELEMENTS_IN_MEMADDR_LOOKUP];
        s32  miSizeOfMemLookupTable;

        char macTelemetryFiltersFirstUse[KI_TELEMETRY_FILTERS_MAX_LENGTH];
        char macTelemetryFiltersNormalUse[KI_TELEMETRY_FILTERS_MAX_LENGTH];

        s32  miIdleTimeOut;
        s32  miSearchQueryTimeInterval;
        s32  miNatTestPacketTimeout;
        s32  miTOSBufferSize;
        s32  miNewsBufferSize;
        s32  miNumDemanglePlayers;

        f32  mfMinSyncingTime;
        f32  mfMaxSyncingTime;
        f32  mfWaitForStartTime;
        f32  mfTimeToWaitForSilentClientReady;
        f32  mfTimeToWaitForCommunicatingClientReady;
        f32  mfTimeGapToLeaveBeforeStartTime;
        f32  mfTimeBetweenStatChecks;
        f32  mfTimeBetweenRoadRulesUploads;
        f32  mfTimeBetweenRoadRulesDownloads;
        f32  mfTimeUntilRetryAfterFailedBuddyUpload;
        f32  mfTimeBetweenOfflineProgressionUpload;

        bool mbDoLogOffOnExitOnline;
    };
}

#endif // BRN_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_H
