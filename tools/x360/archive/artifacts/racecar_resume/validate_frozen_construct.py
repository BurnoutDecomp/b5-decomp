"""Compare only the29 initialization fields, not unrecovered whole-module ABI/behavior."""
from pathlib import Path
import hashlib,json,random,struct,sys
HERE=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(PARENT/'scratch/x360_sdk'))
import native_atomic_fixture as F
OUT=HERE/'frozen_9769_cpp'
layout=json.loads((HERE/'frozen_9769/measured-layout.json').read_text())
comp=json.loads((OUT/'compile-results.json').read_text()); layout['object']=next(x['object'] for x in comp['results'] if x['source'].endswith('/BrnRaceCarEntityModule.cpp'))
obj=F.Coff(Path(layout['object']),'?Construct@RaceCarEntityModule@')
bundle=F.Bundle([obj]);bundle.select('?Construct@RaceCarEntityModule@')
artist_path=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822FD898.json')
artist=json.loads(artist_path.read_text())
original_offsets={'meIndexOfCarPlayerIsTailgating':0x182f4,'mePlayerActiveRaceCarIndex':0x182f8,'mabCarSelectWaitForStreaming':0x18360,
 'DEBUG_mbOverrideDamage':0x18778,'DEBUG_mfVehicleScratchAmount':0x1877c,'DEBUG_mfVehicleDustAmount':0x18780,'DEBUG_mfVehicleCrumpleAmount':0x18784,
 'DEBUG_mfSelfIlluminationR':0x18788,'DEBUG_mfSelfIlluminationG':0x1878c,'DEBUG_mfSelfIlluminationB':0x18790,
 'DEBUG_mbSpawnDebugRaceCarThisFrame':0x18740,'DEBUG_mbCleanDebugRaceCarThisFrame':0x18741,'DEBUG_mbDebugRaceCarIsActive':0x18742,
 'DEBUG_mDebugRaceCarPosition':0x18750,'DEBUG_mDebugRaceCarDirection':0x18760,'DEBUG_mfDebugRaceCarVelocity':0x18770,'DEBUG_mpDebugRaceCar':0x18774,
 'DEBUG_mbOverrideCarColor':0x18794,'DEBUG_mfOverridePaintColorR':0x18798,'DEBUG_mfOverridePaintColorG':0x1879c,'DEBUG_mfOverridePaintColorB':0x187a0,
 'DEBUG_mfOverridePearlColorR':0x187a4,'DEBUG_mfOverridePearlColorG':0x187a8,'DEBUG_mfOverridePearlColorB':0x187ac,
 'DEBUG_mbOverrideCarPalette':0x187b0,'DEBUG_miPaletteIndex':0x187b4,'DEBUG_miColourIndex':0x187b8,
 'mbDisplayPlayerCarPosition':0x18349,'mbRivalDebug':0x1834a}
extra={
'mAirTimeManager':(0x180d8,16),'mTrafficCheckManager':(0x180e8,8),
'mfClippedTime':(0x228,4),'mePrepareStage':(0x22c,4),'miPrepareCarIndex':(0x230,4),'meLoadGlobalResourcesStage':(0x234,4),'miExpectedResponseCount':(0x238,4),
'mfCurrentTailgateDuration':(0x182f0,4),'miOpponentCount':(0x18340,4),'mbIsInGameMode':(0x18344,1),'mbIsInOnlineGameMode':(0x18345,1),'mbOnlineModeJustFinished':(0x18346,1),'mbCarSelectAllowedInGameMode':(0x18347,1),'mbWaitingForStreaming':(0x18348,1),'mbRenderCarsDuringCrash':(0x1834b,1),'mbRenderWheels':(0x1834c,1),'mbSixaxisSteeringEnabled':(0x1834d,1),'mbPaybackSixaxisSteering':(0x1834e,1),'mbRenderRaceCarCoronas':(0x1834f,1),'mbModeStartedPlaying':(0x18354,1),'mxGameModeFlags':(0x18358,8),'meGameModeType':(0x18368,4),'mbSendStreamingComplete':(0x18371,1),'miPendingRequestCount':(0x18394,4),'mfTimeStep':(0x18398,4),'mfIntroTimer':(0x1839c,4),'mfSimTime':(0x183a0,4),'mfTimeStepMultiplier':(0x183a4,4),'meActivePaybackType':(0x183e4,4),'meActivePaybackAggressor':(0x183e8,4),'mbDEBUGPlayerUsingReset':(0x18430,1),'mpVehicleList':(0x18434,4),'mpWheelList':(0x18438,4),'mfLastPlayerCarSpeed':(0x184c0,4),'mfRandomBoostTime':(0x184c4,4),'mbRandomBoostOn':(0x184c8,1),'mfResetOnWaterHeight':(0x184cc,4),'miPlayerBaseDeformationTypeMirror':(0x184d0,4),'miPlayerBaseDeformationTypeSaved':(0x184d4,4),'mfPlayerBaseDeformAmountMirror':(0x184d8,4),'mbPlayerBaseDeformRequestPending':(0x184dc,1),'mfPlayerBaseDeformAmountSaved':(0x184e0,4),'miStoredStompeeCount':(0x185f0,4),'mbInReplay':(0x185f4,1),'mbReplayRestored':(0x185f5,1),'mbInvalidatedRaceCars':(0x186c0,1),'mabRaceCarsRendered':(0x186c1,8),'mbInCarSelectScreen':(0x186c9,1),'mbInCarModScreen':(0x186ca,1),'meCarSelectResetType':(0x186cc,4),'mbCarSelectDontStreamAudio':(0x186d0,1),'mbHACK_ExitingCarSelectWaitForAudio':(0x186d1,1),'miHidingEvents':(0x18714,4),'mbOncomingTimerActive':(0x18718,1),'mfOncomingNoClueTimer':(0x1871c,4)}
sizes={name:(8 if name=='mabCarSelectWaitForStreaming' else 16 if name in ['DEBUG_mDebugRaceCarPosition','DEBUG_mDebugRaceCarDirection'] else 1 if name.startswith(('DEBUG_mb','mb')) else 4) for name in original_offsets}
for name,(offset,size) in extra.items(): original_offsets[name]=offset;sizes[name]=size

MODULE=0x41000000;rng=random.Random(3601029);mismatches=[]
def noop(c):c.setg(3,0)
def reject(c):raise AssertionError('unexpected assert')
for index in range(64):
    image=rng.randbytes(0x30000);outputs=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.add(MODULE,len(image),image);c.setg(3,MODULE)
        if compiled:
            for name,offset in original_offsets.items():
                c.mem.wr(MODULE+layout['offsets'][name],image[offset:offset+sizes[name]])
        for row in artist['xrefs_from']:
            if row['name'].startswith('__'):continue
            c.hooks[int(row['address'],16)]=reject if 'Assert::' in row['name'] else noop
        for address,name in bundle.unresolved.items():c.hooks[address]=reject if 'Assert' in name else noop
        values=[]
        for lifecycle in range(2):
            if lifecycle:
                for name,offset in (({n:layout['offsets'][n] for n in original_offsets}) if compiled else original_offsets).items():
                    c.mem.wr(MODULE+offset,bytes([0xa5+(index%16)])*sizes[name])
            c.setg(3,MODULE);c.run(bundle.entry if compiled else 0x822fd898)
            offsets={n:layout['offsets'][n] for n in original_offsets} if compiled else original_offsets
            values.append({name:c.mem.rd(MODULE+offset,sizes[name]).hex() for name,offset in offsets.items()})
        outputs.append(values)
    if outputs[0]!=outputs[1]:mismatches.append({'case':index,'original':outputs[0],'compiled':outputs[1]})
report={'cases':128,'input_images':64,'repeated_construct_calls_per_image':2,'fields':len(original_offsets),'mismatches':mismatches,
        'object':str(obj.path),'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),
        'artist':str(artist_path),'artist_sha256':hashlib.sha256(artist_path.read_bytes()).hexdigest(),
        'artist_offsets':original_offsets,'compiled_layout':layout,
        'scope':'84 mapped fields compared using actual whole native CPP COFF versus full ARTIST Construct; identical per-field poisoned inputs and re-poisoned second call',
        'limits':'foreign construction/monitor services are controlled no-op callbacks; only mapped fields compared. Existing current module layout differs from ARTIST and is explicitly mapped by field; this is not ABI parity, complete Construct parity, boot or gameplay validation.'}
(HERE/'frozen-construct-expanded.json').write_text(json.dumps(report,indent=2)+'\n')
print('mismatches:',len(mismatches));
if mismatches:
 a,b=mismatches[0]['original'][0],mismatches[0]['compiled'][0];print({k:(a[k],b[k]) for k in a if a[k]!=b[k]})
print('Expanded mapped field comparison; foreign callees controlled, not whole ABI proof.')



