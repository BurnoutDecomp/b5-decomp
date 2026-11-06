from pathlib import Path
import json,sys
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
p=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82BACA30.json')
a=json.loads(p.read_text())['assembly'].splitlines()
for line in a:
 if '0x82BAD2' in line or '0x82BAD3' in line:print(line)
print('RAW',[(hex(e),hex(x360rd.u32(e)),ppcdis.vmx_text(x360rd.u32(e)))for e in [0x82BAD344,0x82BAD350,0x82BAD324]])
