#pragma once

#include "types.hpp"

namespace Nicotine { class IDynamicMixer; class SnapshotMixer; }
namespace EA { namespace GameTalk { class GameTalkMessage; } }

// X360 layout established by 82B46C38, 82B45CB8, 82B46248, 82B465C8.
class NFSLiveLink
{
public:
    explicit NFSLiveLink(Nicotine::IDynamicMixer* apMixer);
    static void DynMixReceiveGameTalkMessage(EA::GameTalk::GameTalkMessage* apMessage);
    void SetupChannelMonitoring();
    void ReadSnapshots();
    void ReadSnapshotData();
    void ReadSnapshotRecall();
    void ReadMasterVolumes();
    void ReadMixControlVolumes();
    void ReadVolPresets();
    void RefreshMap(const void* apMap, int aiMapSize, const void* apSnapshots, int aiSnapshotSize);
    void CheckInputBuffer(int aiCommand);
    void UpdateChannelMonitoring();
    void UpdateActiveSnapshots(Nicotine::SnapshotMixer* apSnapshots, float afDeltaTime);
    void ProcessLiveLink(Nicotine::SnapshotMixer* apSnapshots, float afDeltaTime);

    Nicotine::IDynamicMixer* mpMixer;   // +0x0000
    bool mbMonitorEnabled;             // +0x0004
    void* mpRefreshedMap;              // +0x0008
    void* mpRefreshedSnapshots;        // +0x000c
    int* mpMonitorWrite;               // +0x0010
    int miMonitorRecordSize;           // +0x0014
    bool mbFirstMonitorRecord;         // +0x0018
    int mInput[512];                   // +0x001c
    int mMonitorOutput[512];           // +0x081c
    int mSnapshotOutput[512];          // +0x101c
    int* mpSnapshotWrite;              // +0x181c
    float mfLastPingTime;              // +0x1820
    float mfTime;                      // +0x1824
    float mfSnapshotTime;              // +0x1828
    bool mbSnapshotMonitoring;         // +0x182c
    int miMonitorState;                // +0x1830
    int miMonitorChannel;              // +0x1834
    int mMonitorChannels[32];          // +0x1838
    int miSnapshotChannel;             // +0x18b8
};

extern NFSLiveLink* g_pNFSLiveLink;
