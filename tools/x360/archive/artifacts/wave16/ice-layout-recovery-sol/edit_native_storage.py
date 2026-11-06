from pathlib import Path
import hashlib,json
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_five_luna_04ed')
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
changes={}
def edit(rel,fn):
 p=root/rel;b=p.read_bytes();s=b.decode('utf-8');crlf='\r\n' in s
 n=fn(s.replace('\r\n','\n'))
 if crlf:n=n.replace('\n','\r\n')
 nb=n.encode('utf-8');assert nb!=b
 p.write_bytes(nb);changes[rel]={'pre_sha256':hashlib.sha256(b).hexdigest(),'post_sha256':hashlib.sha256(nb).hexdigest()}
def swap(s,o,n):
 assert s.count(o)==1,(o[:80],s.count(o));return s.replace(o,n)
def header(s):
 s=swap(s,'// 8253DA4C). Native prefix/contained-widget offsets are pinned below. The old\n// target-box gap and trailing status/tail declarations remain unrecovered;\n// no whole Controller or Manager extent is certified by these prefix gates.', '// 8253DA4C). Native offsets and the object-representation extent are pinned below.\n// The three unobserved tail windows preserve bytes bounded by independent native\n// accesses; their original declarations remain unknown. They add no initialization\n// or editor behavior and do not establish a status-string capacity.')
 s=swap(s,'    // Incomplete tail: fields below are in native order, but their absolute\n    // offsets are still early. The word at7200, the\n    // status-string capacity and7390..73B7 need further native evidence.\n    // No filler is introduced to force this tail or the overall extent to fit.\n    s32 miStatusCount;', '    // ARTIST825321AC ends the HUD rectangle at7200;8253DA84 stores the next\n    // named scalar at7204. Only the full game zero-init touches these bytes.\n    u8 maUnobservedHudStatusStorage[0x04];       // native +7200..7203\n    s32 miStatusCount;')
 s=swap(s,'    char miStatusText;                           // native +720C first character only\n    s32 miEditFlag724C;', '    char miStatusText;                           // native +720C first format character\n    //8253DA8C clears only the first character;8253DA90 stores the separate\n    // scalar at724C. This preserves the intervening representation, without\n    // assigning it a string capacity or clearing it during Construct.\n    u8 maUnobservedStatusStorage[0x3F];           // native +720D..724B\n    s32 miEditFlag724C;')
 s=swap(s,'    f32 maRampAccum[KI_ICE_NUM_CHANGE_ELEMENTS];    // native +72FC, acceleration values\n    s32 miMenuTimer;', '    f32 maRampAccum[KI_ICE_NUM_CHANGE_ELEMENTS];    // native +72FC, acceleration values\n    // Native description axes end at36: the last active ramp ends at7390.\n    //8253DADC separately stores the timer at73B8. No extra active entries.\n    u8 maUnobservedPostAccelerationStorage[0x28]; // native +7390..73B7\n    s32 miMenuTimer;')
 s=swap(s,'// Actual containment anchors. These do not certify the unresolved tail.', '// Native member accesses and independently bounded object-representation windows.')
 target='CGS_STATIC_ASSERT((offsetof(ICEController, maRampAccum) == offsetof(ICEController, maChangeBuffer) +'
 guards=[('maUnobservedHudStatusStorage',0x7200),('miStatusCount',0x7204),('miStatusMode',0x7208),('miStatusText',0x720c),('maUnobservedStatusStorage',0x720d),('miEditFlag724C',0x724c),('miShowUi',0x7250),('mfCursorParam',0x7254),('mfCursorRatio',0x7258),('mfBarFade',0x725c),('mi16BubbleYaw',0x7260),('mi16BubblePitch',0x7262),('mfBubbleRadius',0x7264),('maChangeBuffer',0x7268),('maRampAccum',0x72fc),('maUnobservedPostAccelerationStorage',0x7390),('miMenuTimer',0x73b8),('miBarFlipLatch',0x73bc),('miBarFlipSub',0x73bd),('mfMarkIn',0x73c0),('mfMarkOut',0x73c4)]
 g=''.join(f'CGS_STATIC_ASSERT((offsetof(ICEController, {n}) == 0x{v:X}), "native controller {n}");\n' for n,v in guards)
 g+='CGS_STATIC_ASSERT((sizeof(ICEController) == 0x73D0), "native controller representation extent");\n'
 s=swap(s,target,g+target)
 return s
edit('src/SDKs/Packages/ICE/ICEController.hpp',header)
def render(s):
 o='''    // Status caption: only when a status line is live (count > 0) and the status mode
    // selects the print (mode == 2). The status text base is miStatusText (the buffer
    // at +0x720C); the render context rides as the first ScrShadowPrintf arg (the
    // committed convention used by the widget Render bodies).
    if (miStatusCount > 0 && miStatusMode == 2)
    {
        const char* lpcStatus = reinterpret_cast<const char*>(&miStatusText);
        ICERender::ScrShadowPrintf((s32)mRenderContext, 150,
                                   KF_ICE_STATUS_TEXT_SIZE, "%s", lpcStatus);
    }'''
 n='''    // ARTIST8253B2CC..8253B2E8 passes720C as the FORMAT, at150/180 and size16.
    // Its following native bytes are preserved without claiming a text capacity.
    if (miStatusCount > 0 && miStatusMode == 2)
    {
        ICERender::ScrShadowPrintf(150, 180, KF_ICE_STATUS_TEXT_SIZE, &miStatusText);
    }'''
 return swap(s,o,n)
edit('src/SDKs/Packages/ICE/ICEControllerRender.cpp',render)
def controller(s):
 s=swap(s,'// service wiring are recovered; the trailing controller layout remains incomplete.', '// service wiring and native tail representation are pinned in ICEController.hpp.')
 s=swap(s,'// editor state and calls ConstructMenus. Tail field offsets remain incomplete;\n// the base call and service-prefix routing are independently validated.', '// editor state and calls ConstructMenus. The unobserved representation windows\n// have no per-Construct initialization, matching the native stores.')
 s=swap(s,'    // Zero the on-screen status-text block and the spare edit flag.', '    // Reset the counts, first format character and separate edit flag only.')
 return s
edit('src/SDKs/Packages/ICE/ICEController.cpp',controller)
def manager(s):
 s=swap(s,'// FLAG: ICEController mController is a MINIMAL SLICE (see ICEController.hpp).', '// ICEController retains independently bounded unobserved representation windows;\n// their original private declarations remain unknown (see ICEController.hpp).')
 s=swap(s,'} // namespace ICE', 'CGS_STATIC_ASSERT((sizeof(ICEManager) == 0x90E0), "native ICE manager containment extent");\n\n} // namespace ICE')
 return s
edit('src/SDKs/Packages/ICE/ICEManager.hpp',manager)
(out/'SOURCE_EDIT_RECEIPT.json').write_text(json.dumps({'base':'b85657d5feb9147ddc379106605408ea92192d76','changes':changes,'source_frozen_for_gate':True},indent=2)+'\n',encoding='utf-8')
print(json.dumps(changes,indent=2))
