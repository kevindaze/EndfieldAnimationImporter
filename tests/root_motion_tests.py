"""Run with Blender: test source-origin removal without private assets."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from mathutils import Matrix, Vector
from retarget_fbx import retarget_frame
source_bind={'Bip001_Pelvis':Matrix.Translation((0,1,0))}
target_bind={'pelvis':Matrix.Translation((0,0.9,0))}
origin=Vector((23,7,-15))
def sample(p):
    local,_=retarget_frame(source_bind,{'Bip001_Pelvis':Matrix.Translation(p)},target_bind,{'pelvis':None},{'pelvis':'Bip001_Pelvis'},Matrix.Identity(3),Matrix.Identity(3),2,align_directions=False,initial_source_position=origin)
    return local['pelvis']
a=sample(origin)
assert (a.translation-Vector((0,0.9,0))).length<1e-5
b=sample(origin+Vector((.5,-.25,.75)))
assert (b.translation-a.translation-Vector((1,-.5,-1.5))).length<1e-5
assert a.to_quaternion().rotation_difference(b.to_quaternion()).angle<1e-5
# An arbitrary translation of the entire source scene must not change output.
origin+=Vector((100,-20,50))
c=sample(origin+Vector((.5,-.25,.75)))
assert (c.translation-b.translation).length<1e-5
print('PASS: initial XYZ origin removed, subsequent XYZ motion retained, source scene translation invariant')
