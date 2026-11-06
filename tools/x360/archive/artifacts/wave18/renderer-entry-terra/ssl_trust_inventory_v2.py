import idaapi,ida_bytes,json
rows=[]
for i in range(64):
 e=0x82147190+i*36;vals=[ida_bytes.get_dword(e+j*4) for j in range(9)]
 if not vals[0]:break
 def string(a):
  d=ida_bytes.get_strlit_contents(a,-1,0)
  return d.decode('ascii',errors='replace') if d is not None else None
 rows.append({'ea':hex(e),'pointers':[hex(x) for x in vals[:6]],'identity':[string(x) for x in vals[:6]],'modulus':hex(vals[6]),'size':vals[7],'exponent_bytes':vals[8].to_bytes(4,'big').hex()})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-trust-inventory-v2.json','w').write(json.dumps(rows,indent=2));idaapi.qexit(0)
