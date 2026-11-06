from pathlib import Path
import sys,json,re
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/tools/re')
import x360rd
assert x360rd.u32(0x82815570)==0x7C8A2378
assert x360rd.u32(0x82F264A8)==4
print('swizzle',x360rd.rd(0x82CDA350,16).hex())
for ea in [0x820475FC,0x82003F40,0x820037C8,0x82001DA0]:print(hex(ea),x360rd.rd(ea,4).hex(),x360rd.f32(ea))
d=json.loads(Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82402308.json').read_text(encoding='utf-8'))
print('BLUR constants')
for line in d['assembly'].splitlines():
 if 'lfs' in line or 'BeginShader' in line or 'BeginVert' in line:print(line)
