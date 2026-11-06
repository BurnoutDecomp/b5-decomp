import ida_bytes,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave27\netconn-user-terra\locale_tables.txt','w') as f:
 for name,base,n in [('language',0x82F7A7E8,9),('country',0x82F7A838,110)]:
  f.write(name+'\n')
  for i in range(n):
   b=ida_bytes.get_bytes(base+i*4,4); f.write('%d 0x%s %d 0x%s\n'%(i,b[:2].hex(),int.from_bytes(b[:2],'big'),b[2:].hex()))
ida_pro.qexit(0)
