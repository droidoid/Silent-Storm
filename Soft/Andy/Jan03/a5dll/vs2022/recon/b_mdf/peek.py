import sys
fn=sys.argv[1]; pg=int(sys.argv[2]); n=int(sys.argv[3]) if len(sys.argv)>3 else 512
with open(fn,'rb') as f:
    f.seek(pg*8192); d=f.read(8192)
for i in range(0,n,16):
    c=d[i:i+16]
    print('%04x %s  %s'%(i,' '.join('%02x'%b for b in c),''.join(chr(b) if 32<=b<127 else '.' for b in c)))
