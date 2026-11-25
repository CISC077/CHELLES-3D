# CHELLES 3D - 25/11/2026 : decoupe de la Bugatti de Rachid (Desktop\Buggati-3D\base.fbx, un seul bloc)
# en carrosserie + 4 roues, sortie en FBX texte (Z en haut, cm, X vers l avant). Script de reference,
# lance par Claude hors d Unreal (python3 + numpy) : lire_fbx.py lit le fbx binaire -> bug.npz.
import numpy as np, json, os
d=np.load('bug.npz'); V=d['V'].reshape(-1,3).astype(np.float64); P=d['P'].astype(np.int64).copy()
P[P<0]=~P[P<0]; T=P.reshape(-1,3)
UV=d['UV'].reshape(-1,2); UVI=d['UVI'].reshape(-1,3)
import struct,zlib,sys
# normals
exec(open('fbx.py').read().split("def find")[0].replace("sys.argv[1]","'/mnt/user-data/uploads/Buggati-3D/base.fbx'").replace("for n in nodes:\n    if n[0] in('Objects','GlobalSettings'): walk(n,0,2)",""))
def find(n,name):
    for c in n[2]:
        if c[0]==name: return c
geo=[c for n in nodes if n[0]=='Objects' for c in n[2] if c[0]=='Geometry'][0]
nm=find(geo,'LayerElementNormal'); N=find(nm,'Normals')[1][0].reshape(-1,3); NI=find(nm,'NormalsIndex')[1][0].reshape(-1,3)
lab=np.load('lab.npy')
S=250.0
def tofile(A): # raw -> fichier FBX (UE avec Y inverse)
    return np.c_[-A[:,1],A[:,0],A[:,2]]
def toue(A):
    f=tofile(A); return np.c_[f[:,0],-f[:,1],f[:,2]]
ROUES={'AVG':(1,-0.505,0.155),'AVD':(-1,-0.505,0.155),'ARG':(1,0.565,0.137),'ARD':(-1,0.565,0.137)}
def fmt(a,dec):
    return ",".join(np.char.mod('%.'+str(dec)+'f',a.ravel()) if dec else a.ravel().astype(str))
def ecrire(nom,mask,centre_raw):
    tris=T[mask]; uvi=UVI[mask]; ni=NI[mask]
    used,inv=np.unique(tris,return_inverse=True); inv=inv.reshape(-1,3)
    pos=tofile(V[used]*S)
    if centre_raw is not None:
        c=tofile(np.array([centre_raw])*S)[0]; pos=pos-c
    pvi=inv.copy(); pvi[:,2]=~pvi[:,2]
    nrm=tofile(N[ni.ravel()].astype(np.float64)); nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-9
    uvu,uinv=np.unique(uvi.ravel(),return_inverse=True)
    o=[]
    o.append('; FBX 7.4.0 project file\n; CHELLES 3D - Bugatti decoupee par Claude\n')
    o.append('FBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n\tCreator: "CHELLES 3D"\n}\n')
    o.append('GlobalSettings:  {\n\tVersion: 1000\n\tProperties70:  {\n'
      '\t\tP: "UpAxis", "int", "Integer", "",2\n\t\tP: "UpAxisSign", "int", "Integer", "",1\n'
      '\t\tP: "FrontAxis", "int", "Integer", "",1\n\t\tP: "FrontAxisSign", "int", "Integer", "",-1\n'
      '\t\tP: "CoordAxis", "int", "Integer", "",0\n\t\tP: "CoordAxisSign", "int", "Integer", "",1\n'
      '\t\tP: "OriginalUpAxis", "int", "Integer", "",2\n\t\tP: "OriginalUpAxisSign", "int", "Integer", "",1\n'
      '\t\tP: "UnitScaleFactor", "double", "Number", "",1\n\t\tP: "OriginalUnitScaleFactor", "double", "Number", "",1\n\t}\n}\n')
    o.append('Definitions:  {\n\tVersion: 100\n\tCount: 4\n\tObjectType: "GlobalSettings" {\n\t\tCount: 1\n\t}\n\tObjectType: "Model" {\n\t\tCount: 1\n\t}\n\tObjectType: "Geometry" {\n\t\tCount: 1\n\t}\n\tObjectType: "Material" {\n\t\tCount: 1\n\t}\n}\n')
    o.append('Objects:  {\n\tGeometry: 1000001, "Geometry::%s", "Mesh" {\n'%nom)
    o.append('\t\tVertices: *%d {\n\t\t\ta: %s\n\t\t}\n'%(pos.size,fmt(pos,3)))
    o.append('\t\tPolygonVertexIndex: *%d {\n\t\t\ta: %s\n\t\t}\n'%(pvi.size,fmt(pvi,0)))
    o.append('\t\tGeometryVersion: 124\n')
    o.append('\t\tLayerElementNormal: 0 {\n\t\t\tVersion: 102\n\t\t\tName: ""\n\t\t\tMappingInformationType: "ByPolygonVertex"\n\t\t\tReferenceInformationType: "Direct"\n\t\t\tNormals: *%d {\n\t\t\t\ta: %s\n\t\t\t}\n\t\t}\n'%(nrm.size,fmt(nrm,4)))
    o.append('\t\tLayerElementUV: 0 {\n\t\t\tVersion: 101\n\t\t\tName: "UVMap"\n\t\t\tMappingInformationType: "ByPolygonVertex"\n\t\t\tReferenceInformationType: "IndexToDirect"\n\t\t\tUV: *%d {\n\t\t\t\ta: %s\n\t\t\t}\n\t\t\tUVIndex: *%d {\n\t\t\t\ta: %s\n\t\t\t}\n\t\t}\n'%(uvu.size*2,fmt(UV[uvu],5),uinv.size,fmt(uinv,0)))
    o.append('\t\tLayerElementMaterial: 0 {\n\t\t\tVersion: 101\n\t\t\tName: ""\n\t\t\tMappingInformationType: "AllSame"\n\t\t\tReferenceInformationType: "IndexToDirect"\n\t\t\tMaterials: *1 {\n\t\t\t\ta: 0\n\t\t\t}\n\t\t}\n')
    o.append('\t\tLayer: 0 {\n\t\t\tVersion: 100\n\t\t\tLayerElement:  {\n\t\t\t\tType: "LayerElementNormal"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t\tLayerElement:  {\n\t\t\t\tType: "LayerElementMaterial"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t\tLayerElement:  {\n\t\t\t\tType: "LayerElementUV"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t}\n\t}\n')
    o.append('\tModel: 2000001, "Model::%s", "Mesh" {\n\t\tVersion: 232\n\t\tProperties70:  {\n\t\t}\n\t\tShading: T\n\t\tCulling: "CullingOff"\n\t}\n'%nom)
    o.append('\tMaterial: 3000001, "Material::M_Bugatti", "" {\n\t\tVersion: 102\n\t\tShadingModel: "phong"\n\t\tMultiLayer: 0\n\t\tProperties70:  {\n\t\t}\n\t}\n}\n')
    o.append('Connections:  {\n\tC: "OO",2000001,0\n\tC: "OO",1000001,2000001\n\tC: "OO",3000001,2000001\n}\n')
    open('/mnt/project-files/VoituresHoro/Bugatti/%s.fbx'%nom,'w').write(''.join(o))
    print(nom,len(tris),'tris',pos.min(0).round(1),pos.max(0).round(1))
ecrire('SM_Bugatti_Carrosserie',lab<0,None)
info={}
for k,(nm,(s,yc,zc)) in enumerate(ROUES.items()):
    c=np.array([0.0*s,yc,zc]); 
    # centre lateral = milieu de la roue
    m=lab==k; xs=V[np.unique(T[m]),0]; c[0]=(xs.min()+xs.max())/2
    ecrire('SM_Bugatti_Roue_'+nm,m,c)
    cu=toue(np.array([c])*S)[0]; info[nm]=[round(float(x),2) for x in cu]
Vb=toue(V*S); info['BoiteMin']=[round(float(x),1) for x in Vb.min(0)]; info['BoiteMax']=[round(float(x),1) for x in Vb.max(0)]
print(json.dumps(info))
json.dump(info,open('/mnt/project-files/VoituresHoro/Bugatti/roues.json','w'),indent=1)
