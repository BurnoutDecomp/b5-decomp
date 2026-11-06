from pathlib import Path
import json,hashlib
r=Path.cwd();out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol');p=r/'src/GameSource/Director/BrnDirectorICEWrapper.h'
b=p.read_bytes();s=b.decode('utf-8');crlf='\r\n' in s;s=s.replace('\r\n','\n')
def swap(o,n):
 global s
 assert s.count(o)==1,(o[:90],s.count(o));s=s.replace(o,n)
swap('namespace BrnResource { namespace GameDataIO { class AllocatorList; } }', 'namespace BrnResource { class ICEList; namespace GameDataIO { class AllocatorList; } }')
swap('// NAME; the offsets are recorded for provenance, never used as casts. The memory\n// manager, manager, mover, camera, anchor and action queue are embedded BY VALUE and our\n// rebuilt sizes differ from the recorded build, so the relative offsets below are NOT\n// reproduced with padding -- members are declared in their recorded ORDER and parity is\n// by name.', '// NAME; the offsets are recorded for provenance, never used as casts. The native\n// embedded extents and member offsets are guarded below. The inactive list pointer\n// and next-movie counter retain the DecFIGS declaration shape within independently\n// bounded ARTIST storage; no new initialization or active use is added.')
o='''    // FLAG (provisional names): two ICE load-state scalars the DWARF lists just before
    // mVehicleRef. Construct zeroes exactly these two (+0x120E4 and +0x120E8); their
    // roles beyond "ICE load-state" are not recovered, so the names are provisional.
    // They are modelled with an explicit pad so the two scalars land at +0x120E4/+0x120E8
    // and mVehicleRef stays at +0x120F0 (the +0x120F0..+0x120FC ref fields Construct seeds).
    // +0x120E4  s32  miICELoadStateA  (zeroed by Construct)
    // +0x120E8  s32  miICELoadStateB  (zeroed by Construct)
    s32 miICELoadStateA;
    s32 miICELoadStateB;
    u8  maVehicleRefPad[4];   // pad so mVehicleRef lands at +0x120F0 (0x120E8 + 4 + 4)'''
n='''    // +0x120E0  DecFIGS ICEWrapper.hpp:232 names this list pointer after the
    // 0x210-byte anchor. ARTIST stores the next two words at120E4/120E8.
    // No active ARTIST dereference identifies this slot; its pointer shape is
    // corroborated by the near-ancestor declaration and native neighborhood.
    const BrnResource::ICEList* mpICEList;

    // Provisional names for DecFIGS's EICELoadStage / EICEPrepareStage words.
    // Construct clears exactly these two; Prepare switches on the second.
    s32 miICELoadStateA;                       // native +120E4
    s32 miICELoadStateB;                       // native +120E8
    // DecFIGS ICEWrapper.hpp:235; native Construct leaves this word untouched.
    s32 miNextMovieToAcquire;                  // native +120EC'''
swap(o,n)
target='    bool mbAcceptInput;\n};'
guards=[('mICEMemory',0),('mICEManager',0xa40),('mICETimer',0x9b20),('mICEFileHandler',0x9b24),('mpResourceManager',0x11b24),('mActionQueue',0x11b28),('mCameraMover',0x11bd0),('mICECamera',0x11d60),('mICECameraAnchor',0x11ed0),('mpICEList',0x120e0),('miICELoadStateA',0x120e4),('miICELoadStateB',0x120e8),('miNextMovieToAcquire',0x120ec),('mVehicleRef',0x120f0),('mCurrentMovieID',0x12100),('mbAcceptInput',0x12108)]
body='    bool mbAcceptInput;\n\n    static void AssertNativeLayout()\n    {\n'
body+='        CGS_STATIC_ASSERT((sizeof(ICE::CameraSpaceHandler) == 0x210), "native camera-space handler extent");\n'
body+='        CGS_STATIC_ASSERT((sizeof(ICE::ICECameraAnchor) == 0x210), "native ICE anchor extent");\n'
body+='        CGS_STATIC_ASSERT((sizeof(VehicleRef) == 0x10), "native vehicle reference extent");\n'
body+='        CGS_STATIC_ASSERT((__alignof(VehicleRef) == 4), "native vehicle reference natural alignment");\n'
for name,off in guards:body+=f'        CGS_STATIC_ASSERT((offsetof(ICEWrapper, {name}) == 0x{off:X}), "native wrapper {name}");\n'
body+='        CGS_STATIC_ASSERT((sizeof(ICEWrapper) == 0x12110), "native ICE wrapper containment extent");\n    }\n};'
swap(target,body)
nb=(s.replace('\n','\r\n') if crlf else s).encode('utf-8');p.write_bytes(nb)
(out/'WRAPPER_EDIT_RECEIPT.json').write_text(json.dumps({'base':'ef8c37c279e070814a59880ff1ea6bed31c6d148','source':'src/GameSource/Director/BrnDirectorICEWrapper.h','pre_sha256':hashlib.sha256(b).hexdigest(),'post_sha256':hashlib.sha256(nb).hexdigest(),'original_four_paths_unchanged':True,'frozen_for_selected_gate':True},indent=2)+'\n',encoding='utf-8')
print('Wrapper header frozen',hashlib.sha256(nb).hexdigest())
