import struct,zlib,sys
import numpy as np
d=open(sys.argv[1],'rb').read()
ver=struct.unpack_from('<I',d,23)[0]
big=ver>=7500
def rd(o):
    if big: end,np_,pl=struct.unpack_from('<QQQ',d,o);o+=24
    else: end,np_,pl=struct.unpack_from('<III',d,o);o+=12
    nl=d[o];o+=1;name=d[o:o+nl].decode();o+=nl
    props=[]
    for i in range(np_):
        t=chr(d[o]);o+=1
        if t in 'YCIFDL':
            f={'Y':'<h','C':'?','I':'<i','F':'<f','D':'<d','L':'<q'}[t];props.append(struct.unpack_from(f,d,o)[0]);o+=struct.calcsize(f)
        elif t in 'fdlibc':
            n,enc,cl=struct.unpack_from('<III',d,o);o+=12;raw=d[o:o+cl];o+=cl
            if enc: raw=zlib.decompress(raw)
            props.append(np.frombuffer(raw,{'f':'<f4','d':'<f8','l':'<i8','i':'<i4','b':'b','c':'b'}[t]))
        elif t in 'SR':
            n=struct.unpack_from('<I',d,o)[0];o+=4;props.append(d[o:o+n]);o+=n
    ch=[]
    while o<end-(25 if big else 13):
        c,o=rd(o)
        if c is None: break
        ch.append(c)
    if end==0: return None,o
    return (name,props,ch),end
o=27;nodes=[]
while o<len(d)-200:
    n,o2=rd(o)
    if n is None: break
    nodes.append(n);o=o2
def walk(n,depth=0,lim=3):
    name,props,ch=n
    ps=[(p.shape if hasattr(p,'shape') else (p[:60] if isinstance(p,bytes) else p)) for p in props]
    if depth<=lim: print('  '*depth+name,ps[:4])
    for c in ch: walk(c,depth+1,lim)
for n in nodes:
    if n[0] in('Objects','GlobalSettings'): walk(n,0,2)
    for c in n[2]:
        if c[0]=='Geometry':
            for g in c[2]:
                if g[0]=='Vertices':
                    v=g[1][0].reshape(-1,3);print('verts',v.shape,v.min(0),v.max(0))

def find(n,name):
    for c in n[2]:
        if c[0]==name: return c
geo=[c for n in nodes if n[0]=='Objects' for c in n[2] if c[0]=='Geometry'][0]
out={}
out['V']=find(geo,'Vertices')[1][0]
out['P']=find(geo,'PolygonVertexIndex')[1][0]
uv=find(geo,'LayerElementUV')
for c in uv[2]: print('UV',c[0],[ (p.shape if hasattr(p,'shape') else p) for p in c[1]])
out['UV']=find(uv,'UV')[1][0]
ui=find(uv,'UVIndex'); 
if ui: out['UVI']=ui[1][0]
nm=find(geo,'LayerElementNormal')
for c in nm[2]: print('N',c[0],[ (p.shape if hasattr(p,'shape') else p) for p in c[1]])
for n in nodes:
    if n[0]=='GlobalSettings':
        for p in find(n,'Properties70')[2]: print(p[1])
np.savez('/tmp/claude-0/s/bug.npz',**out)
