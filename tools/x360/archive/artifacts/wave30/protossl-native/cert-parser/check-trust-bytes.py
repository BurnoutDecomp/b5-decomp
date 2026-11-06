import hashlib,json,pathlib,re
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-trust-native-data.json');native=json.loads(p.read_text());src=pathlib.Path('vendor/dirtysdk/src/x360/protossl_cert_data.inc').read_text();rows=[]
for i,address in enumerate(['0x82147010','0x82147090','0x82147110']):
 m=re.search(r'kCAModulus%d\[\d+\]\s*=\s*\{(.*?)\};'%i,src,re.S);actual=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',m.group(1)));expected=bytes.fromhex(native[address]['bytes']);rows.append({'address':address,'size':len(actual),'sha256':hashlib.sha256(actual).hexdigest(),'match':actual==expected})
assert all(x['match'] for x in rows);pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-parser/trust-byte-check.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(rows,indent=2))
