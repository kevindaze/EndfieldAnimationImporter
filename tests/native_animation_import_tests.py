"""Smoke-test the standalone Windows importer with generated humanoid motion."""
import json
import math
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

BONES = [
    ("Pelvis", "Hips", None, (0, 1, 0)),
    ("Spine", "Spine", "Pelvis", (0, 1.2, 0)),
    ("Neck", "Neck", "Spine", (0, 1.5, 0)),
    ("Head", "Head", "Neck", (0, 1.65, 0)),
]
for side, prefix, sign in [("L", "Left", 1), ("R", "Right", -1)]:
    BONES.extend([
        (side+"_UpperArm", prefix+"Arm", "Spine", (.22*sign, 1.42, 0)),
        (side+"_Forearm", prefix+"ForeArm", side+"_UpperArm", (.38*sign, 1.34, 0)),
        (side+"_Hand", prefix+"Hand", side+"_Forearm", (.55*sign, 1.25, 0)),
        (side+"_Thigh", prefix+"UpLeg", "Pelvis", (.12*sign, 1.0, 0)),
        (side+"_Calf", prefix+"Leg", side+"_Thigh", (.12*sign, .55, -.015)),
        (side+"_Foot", prefix+"Foot", side+"_Calf", (.12*sign, .12, 0)),
    ])


def matrix(x=0, y=0, z=0):
    return [[1,0,0,x], [0,1,0,y], [0,0,1,z], [0,0,0,1]]


def reference():
    bones, palette = [], []
    for name, _, parent, p in BONES:
        record = {"name":"Bip001_"+name, "key":name}
        bones.append(dict(record, actor=1, parent_key=parent, world_matrix=matrix(*p),
                          world_position=p, world_rotation_xyzw=(0,0,0,1)))
        palette.append(dict(record, inverse_bind_matrix=matrix(*(-v for v in p))))
    return {"format":"endfield-skeleton-pose-reference", "version":1,
            "partner":"chr_test", "bones":bones,
            "mesh_bindposes":[{"actor":1, "root_world_to_local":matrix(),
                               "renderer_local_to_world":matrix(), "bones":palette}]}


def vmd(ik=False):
    def name(s,size):
        return s.encode("cp932").ljust(size,b"\0")

    def bone(s,frame,position,rotation):
        curve=bytes([20]*8+[107]*8)+bytes(48)
        return name(s,15)+struct.pack("<I7f",frame,*position,*rotation)+curve

    entries = [bone("センター",0,(0,0,0),(0,0,0,1)),
               bone("センター",30,(2,1,1),(0,0,0,1)),
               bone("上半身",0,(0,0,0),(0,0,0,1)),
               bone("上半身",30,(0,0,0),(0,0,math.sin(.2),math.cos(.2)))]
    if ik:
        for frame, offset in [(0,(3,10,-6)),(30,(1,7,-7))]:
            entries.append(bone("左足ＩＫ",frame,offset,(0,0,0,1)))
            entries.append(bone("左つま先ＩＫ",frame,(.1,-.4,.2),(0,0,0,1)))
    return name("Vocaloid Motion Data 0002",30)+name("fixture",20)+struct.pack("<I",len(entries))+b"".join(entries)+bytes(20)


def fbx():
    ids={name:100+i for i,(name,_,_,_) in enumerate(BONES)}
    positions={name:p for name,_,_,p in BONES}
    models, connections = [], []
    for name, source, parent, point in BONES:
        parent_point=positions.get(parent,(0,0,0))
        x,y,z=(100*(a-b) for a,b in zip(point,parent_point))
        models.append(f'''Model: {ids[name]}, "Model::{source}", "LimbNode" {{
            Version: 232
            Properties70: {{ P: "Lcl Translation", "Lcl Translation", "", "A",{x},{y},{z} }}
        }}''')
        connections.append(f'C: "OO",{ids[name]},{ids.get(parent,0)}')
    fragment='''AnimationStack: 500, "AnimStack::Fixture", "" {
        Properties70: {
            P: "LocalStart", "KTime", "Time", "",0
            P: "LocalStop", "KTime", "Time", "",46186158000
            P: "ReferenceStart", "KTime", "Time", "",0
            P: "ReferenceStop", "KTime", "Time", "",46186158000
        }
    }
    AnimationLayer: 501, "AnimLayer::BaseLayer", "" {}
    AnimationCurveNode: 502, "AnimCurveNode::T", "" {
        Properties70: {
            P: "d|X", "Number", "", "A",0
            P: "d|Y", "Number", "", "A",100
            P: "d|Z", "Number", "", "A",0
        }
    }
    AnimationCurve: 503, "AnimCurve::", "" {
        Default: 0
        KeyVer: 4008
        KeyTime: *2 { a: 0,46186158000 }
        KeyValueFloat: *2 { a: 0,40 }
        KeyAttrFlags: *1 { a: 4 }
        KeyAttrDataFloat: *4 { a: 0,0,0,0 }
        KeyAttrRefCount: *1 { a: 2 }
    }'''
    links='''C: "OO",501,500
    C: "OO",502,501
    C: "OP",502,100,"Lcl Translation"
    C: "OP",503,502,"d|X"'''
    return ('; FBX 7.4.0 project file\nFBXHeaderExtension: { FBXHeaderVersion: 1003\n FBXVersion: 7400 }\n'
            'Objects: {\n'+'\n'.join(models)+'\n'+fragment+'\n}\nConnections: {\n'+'\n'.join(connections)+'\n'+links+'\n}\n').encode()


def invoke(worker, root, name, data, accept=True):
    source=root/name
    source.write_bytes(data)
    destination=root/(source.stem+"-output")
    destination.mkdir()
    run=subprocess.run([str(worker),str(source),str(destination),"--reference",str(root/"reference.json"),
                        "--actor","partner"],capture_output=True,timeout=20)
    result=json.loads((destination/"result.json").read_bytes())
    assert result["ok"]==accept and (run.returncode==0)==accept, result
    if accept:
        assert len(result["files"])==1
        clip=json.loads((destination/result["files"][0]).read_bytes())
        assert clip["target_character"]=="chr_test" and clip["duration"]==1
        tracks=clip["actors"][0]["tracks"]
        assert len(tracks)>=16
        pelvis=next(track for track in tracks if track["bone"]=="Bip001_Pelvis")
        assert math.dist(pelvis["keys"][0]["position"],pelvis["keys"][-1]["position"])>.05
        return tracks


def main():
    worker=Path(sys.argv[1]).resolve(strict=True)
    scratch=Path(sys.argv[2])
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        root=Path(directory)
        (root/"reference.json").write_text(json.dumps(reference()),encoding="utf-8")
        invoke(worker,root,"測試動作.vmd",vmd())
        tracks=invoke(worker,root,"leg-ik.vmd",vmd(ik=True))
        # Blender 5.2.2 and the bundled MMD profile, evaluated on this generated fixture.
        blender={
            "Thigh":((.854707,.113781,.314543,.396978),(.675214,-.010469,-.068893,.734323)),
            "Calf":((-.654299,0,0,.756236),(-.303499,0,0,.952832)),
            "Foot":((-.417788,.323334,-.088155,.844475),(-.516999,-.101333,.027628,.849518)),
        }
        for bone,(first,last) in blender.items():
            keys=next(t["keys"] for t in tracks if t["bone"]=="Bip001_L_"+bone)
            for key,expected in ((keys[0],first),(keys[-1],last)):
                actual=key["rotation"]
                dot=abs(sum(a*b for a,b in zip(actual,expected)))/math.hypot(*actual)/math.hypot(*expected)
                angle=math.degrees(2*math.acos(min(1,dot)))
                assert angle<1,(bone,key["time"],angle)
        invoke(worker,root,"測試骨架.fbx",fbx())
        invoke(worker,root,"truncated.vmd",vmd()[:139],False)
        invoke(worker,root,"invalid.fbx",b"not an FBX",False)
    print("PASS: native VMD and FBX motion, VMD leg and toe IK, Unicode paths and malformed inputs")


if __name__=="__main__":
    main()
