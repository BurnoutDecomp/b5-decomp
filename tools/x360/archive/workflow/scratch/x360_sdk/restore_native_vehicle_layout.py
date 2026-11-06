import json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
assert subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip()=='codex/experimental-x360-byte-match'
dwarf=Path('D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h').read_text(encoding='utf-8')
dwarf=dwarf[dwarf.index('\tVehicleAttribs * mpAttribs;'):dwarf.index('\tBrnPhysics::Vehicle::DebugComponent * mpDebugComponent;')+len('\tBrnPhysics::Vehicle::DebugComponent * mpDebugComponent;')]
fields=re.findall(r'^\t(?!extern\b)([^;\n]+?)\s+(\w+);',dwarf,re.M);assert len(fields)==61
names=[n for _,n in fields]
offsets=[0x720,0x730,0xaa0,0xe10,0xed0,0xee0,0xef0,0xf00,0xfd0]+list(range(0xfe0,0x1090,0x10))+[0x1090,0x10e0,0x10f0,0x10f4,0x10f5,0x10f6,0x10f7,0x10f8,0x1100,0x1130,0x1150,0x1158,0x1160,0x1220,0x1230,0x1330,0x1340,0x1350,0x1351,0x1352,0x1353,0x1354,0x1358,0x1359,0x135a,0x135b,0x135c,0x135d,0x135e,0x135f,0x1360,0x1361,0x1362,0x1370,0x13b0,0x13c0,0x13d0,0x13d8,0x13dc,0x13e0,0x13e4]
assert len(offsets)==len(names)
p=ROOT/'src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h';raw=p.read_bytes();s=raw.decode('utf-8').replace('\r\n','\n');decls=[]
for name in names:
    pattern=r'^        ([\w:<>* ]+\b'+name+r'(?:\[[^\]\n]+\])?;)[^\n]*\n'
    matches=list(re.finditer(pattern,s,re.M));assert len(matches)==1,(name,len(matches))
    match=matches[0];decls.append(match[1].strip());s=s[:match.start()]+s[match.end():]
block='        // Native ARTIST own-member block, in DecFIGS declaration order.\n        // The layout TU checks all 61 actual offsets and the complete 0x13F0 size.\n'
block+='\n'.join('        '+d+'  // +0x%X'%o for d,o in zip(decls,offsets))+'\n\n'
s=s.replace('        static void _AssertOwnBlockLayout();',block+'        static void _AssertOwnBlockLayout();')
a=s.index('        // HOST DIVERGENCE,');b=s.index('        // ==========================================================================================',a)
s=s[:a]+'        // Native declarations below now follow this order; the layout TU checks\n        // the actual C++ members against these ARTIST offsets.\n'+s[b:]
s=s.replace('Nothing existing is reordered.','Declarations are grouped in native order below.')
s=s.replace('Parity here is BY NAME,\n        // and no absolute offset in this class is static_asserted.','Native size and offsets\n        // are enforced in VehiclePhysics_layout_check.cpp.')
s=s.replace('it is deliberately NOT an\n    // offsetof() gate, because this block is not width-identical on x64 (two pointers) and several\n    // embedded sub-types in this tree are reconstructions rather than byte-exact console copies.','the native\n    // offsetof checks in VehiclePhysics_layout_check.cpp enforce these anchors directly.')
p.write_bytes(s.replace('\n','\r\n' if b'\r\n' in raw else '\n').encode('utf-8'))
q=ROOT/'src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics_layout_check.cpp';raw=q.read_bytes();s=raw.decode('utf-8').replace('\r\n','\n')
a=s.index('#include ')
s='''// Native Xbox 360 vehicle layouts. The actual C++ member offsets are checked
// against ARTIST, with DecFIGS supplying declaration order and names.
// The older anchor arithmetic remains as an additional check; it cannot prove
// that a C++ object uses that layout, so it is followed by real offsetof gates.

'''+s[a:]
needle='    void VehiclePhysics::_AssertOwnBlockLayout()\n    {\n';assert needle in s
checks='        // ARTIST native object layout, including the individual byte flags.\n        CGS_STATIC_ASSERT((sizeof(VehiclePhysics) == 0x13F0), "X360 VehiclePhysics size");\n'
checks+='\n'.join('        CGS_STATIC_ASSERT((offsetof(VehiclePhysics, '+n+') == 0x%X), "X360 %s");'%(o,n) for n,o in zip(names,offsets))+'\n\n'
s=s.replace(needle,needle+checks)
s=s.replace('// (Absolute offsets are deliberately not asserted -- see the banner.) Referencing them','// (The native absolute offsets are asserted above.) Referencing them')
q.write_bytes(s.replace('\n','\r\n' if b'\r\n' in raw else '\n').encode('utf-8'))
(ROOT.parent/'scratch/x360_sdk/native-vehicle-members.json').write_text(json.dumps([{'name':n,'offset':hex(o),'declaration':d} for n,o,d in zip(names,offsets,decls)],indent=2)+'\n',encoding='utf-8')
print('Restored61 native members and added62 actual layout checks.')
