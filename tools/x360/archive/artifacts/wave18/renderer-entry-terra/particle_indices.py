import idaapi,ida_bytes,json
rows={hex(e): (ida_bytes.get_bytes(e,n) or b'').hex() for e,n in [(0x82101268,40),(0x82101290,8)]}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/particle-indices.json','w').write(json.dumps(rows,indent=2));idaapi.qexit(0)
