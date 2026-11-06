import idaapi,ida_bytes,json
blocks={}
for e,n in [(0x82146f20,0x270),(0x82147010,128),(0x82147090,125),(0x82147110,128),(0x82147190,0x90)]:
 blocks[hex(e)]={'length':n,'bytes':(ida_bytes.get_bytes(e,n) or b'').hex()}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-trust-native-data.json','w').write(json.dumps(blocks,indent=2));idaapi.qexit(0)
