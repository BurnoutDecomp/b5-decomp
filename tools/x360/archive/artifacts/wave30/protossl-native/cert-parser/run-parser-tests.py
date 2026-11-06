import json,pathlib,subprocess
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-parser')
def tlv(tag,content):
 n=len(content)
 enc=bytes([n]) if n<128 else bytes([0x81,n]) if n<256 else bytes([0x82,n>>8,n&255])
 return bytes([tag])+enc+content
def seq(*v):return tlv(0x30,b''.join(v))
def oid(x):return tlv(6,x)
def name(fields):return seq(*(tlv(0x31,seq(oid(bytes([0x55,0x04,key])),tlv(0x13,val.encode()))) for key,val in fields))
def alg(last):return seq(oid(bytes.fromhex('2a864886f70d0101')+bytes([last])),tlv(5,b''))
def cert(issuer,outer_oid=5,siglen=128,subject='example.com'):
 n=bytes([0])+bytes([0x81])+bytes(126)+bytes([0x01]);pub=seq(tlv(2,n),tlv(2,bytes.fromhex('010001')))
 spki=seq(alg(1),tlv(3,b'\0'+pub))
 valid=seq(tlv(0x17,b'240101000000Z'),tlv(0x17,b'300101000000Z'))
 tbs=seq(tlv(2,b'\1'),alg(5),name(issuer),valid,name([(3,subject)]),spki)
 return seq(tbs,alg(outer_oid),tlv(3,b'\0'+bytes(siglen)))
trusted=[(6,'US'),(8,'California'),(7,'Redwood City'),(10,'Electronic Arts, Inc.'),(11,'Online Technology Group'),(3,'OTG3 Certificate Authority')]
untrusted=[(6,'US'),(10,'Untrusted CA'),(3,'Untrusted Root')]
fixtures=[('empty',b'', 'example.com',-1),('outer_only',b'\x30\x00','example.com',-2),('subject_mismatch',cert(trusted), 'wrong.example',-24),('untrusted_issuer',cert(untrusted), 'example.com',-25),('wrong_signature_length',cert(trusted,siglen=127),'example.com',-26),('sha1_bad_signature',cert(trusted), 'example.com',-27),('unknown_outer_oid',cert(trusted,outer_oid=99), 'example.com',0),('nonhash_known_outer_oid',cert(trusted,outer_oid=1), 'example.com',0)]
results=[]
for name_,data,host,expected in fixtures:
 (p/(name_+'.der')).write_bytes(data)
 r=subprocess.run([str(p/'host-parser.exe'),host,data.hex()],capture_output=True,text=True,timeout=60)
 results.append({'name':name_,'bytes':len(data),'expected':expected,'actual':r.stdout,'returncode':r.returncode,'passed':r.returncode==0 and r.stdout==str(expected)})
(p/'malformed-and-trust-tests.json').write_text(json.dumps({'host_only':True,'no_network':True,'cases':results},indent=2)+'\n')
print(json.dumps(results,indent=2));assert all(x['passed'] for x in results)
