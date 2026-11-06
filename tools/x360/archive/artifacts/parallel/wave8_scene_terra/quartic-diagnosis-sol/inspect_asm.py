from pathlib import Path
import json
s=json.loads(Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82BACA30.json').read_text())['assembly'].splitlines()
for line in s:
 a=int(line.split()[0],16)
 if 0x82BACF00<=a<=0x82BAD170 or 0x82BACB14<=a<=0x82BACBF0:print(line)
