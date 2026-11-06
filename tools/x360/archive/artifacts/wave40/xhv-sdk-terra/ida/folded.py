import idaapi,ida_bytes,json
addr=0x82962fe0
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave40/xhv-sdk-terra/folded-artist.json','w').write(json.dumps({'address':hex(addr),'bytes':ida_bytes.get_bytes(addr,12).hex()}));idaapi.qexit(0)
