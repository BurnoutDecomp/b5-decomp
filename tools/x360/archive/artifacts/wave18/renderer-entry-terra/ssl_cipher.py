import idaapi,ida_bytes,json
r={hex(e): (ida_bytes.get_bytes(e,n) or b'').hex() for e,n in [(0x82146f10,10),(0x82146eb0,96)]}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/ssl-cipher-image.json','w').write(json.dumps(r,indent=2));idaapi.qexit(0)
