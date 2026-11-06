import idaapi,ida_bytes,json
addresses=[0x82cdb404,0x82101268,0x82101290]
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/particle-data-image.json','w').write(json.dumps({hex(e):(ida_bytes.get_bytes(e,4) or b'').hex() for e in addresses},indent=2));idaapi.qexit(0)
