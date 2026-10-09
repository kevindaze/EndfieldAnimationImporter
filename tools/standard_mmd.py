"""EAI-authored generic MMD humanoid, no mesh or third-party model data.

Coordinates are an approximate 18-unit MMD A-pose in MMD's Y-up space.
This profile defines common Japanese bones, groove/root motion, arm twists,
30 finger joints and leg/toe IK. It is an approximation, not the original rig
that authored every VMD. Bump PROFILE when changing rest geometry or hierarchy.
"""
import math

PROFILE = 'eai-standard-mmd-v1'


def create_model(pmx):
    model = pmx.Model()
    model.name = model.name_e = PROFILE
    model.comment = model.comment_e = 'EAI generic motion-only skeleton; no mesh or textures.'
    indices = {}

    def bone(name, location, parent=None, tail=(0, .5, 0), order=0):
        b = pmx.Bone()
        b.name = b.name_e = name
        b.location = list(location)
        b.parent = indices[parent] if parent else -1
        b.displayConnection = tuple(tail)
        b.transform_order = order
        indices[name] = len(model.bones)
        model.bones.append(b)
        return b

    bone('全ての親', (0,0,0))
    bone('センター', (0,8,0), '全ての親')
    bone('グルーブ', (0,8.5,0), 'センター')
    bone('腰', (0,10,0), 'グルーブ')
    bone('下半身', (0,10,0), '腰', (0,-2,0))
    bone('上半身', (0,10,0), '腰', (0,2.5,0))
    bone('上半身2', (0,12.5,0), '上半身', (0,1.5,0))
    bone('上半身3', (0,14,0), '上半身2', (0,1.5,0))
    bone('首', (0,15.5,0), '上半身3', (0,.7,0))
    bone('頭', (0,16.2,0), '首', (0,1.8,0))
    for jp, sign in [('左',1),('右',-1)]:
        def p(x,y,z=0): return (sign*x,y,z)
        bone(jp+'肩',p(.4,14.8), '上半身3', p(1, -.3))
        bone(jp+'腕',p(1.4,14.5),jp+'肩',p(2,-1.5))
        twist=bone(jp+'腕捩',p(2.4,13.75),jp+'腕',p(1,-.75))
        twist.axis=p(2,-1.5)
        bone(jp+'ひじ',p(3.4,13),jp+'腕捩',p(2,-1.5))
        twist=bone(jp+'手捩',p(4.4,12.25),jp+'ひじ',p(1,-.75))
        twist.axis=p(2,-1.5)
        bone(jp+'手首',p(5.4,11.5),jp+'手捩',p(.65,-.48))
        # Fingers extend along the forearm; spread in depth across the palm.
        for digit, finger in enumerate(['親指','人指','中指','薬指','小指']):
            parent=jp+'手首'
            z=(digit-2)*.16
            for joint in range(3):
                number=joint if digit==0 else joint+1
                name=jp+finger+'０１２３'[number]
                x=5.55+joint*.23+(0 if digit==0 else .35)
                y=11.38-(x-5.55)*.75
                bone(name,p(x,y,z),parent,p(.23,-.1725))
                parent=name
        bone(jp+'足',p(1,10), '下半身', (0,-4.7,-.2))
        bone(jp+'ひざ',p(1,5.3,-.2),jp+'足', (0,-4.5,.2))
        bone(jp+'足首',p(1,.8),jp+'ひざ', (0,-.5,-1.8))
        bone(jp+'つま先',p(1,.3,-1.8),jp+'足首', (0,0,-.5))
        bone(jp+'足IK親',p(1,0), '全ての親')
        ik=bone(jp+'足ＩＫ',p(1,.8),jp+'足IK親', order=1)
        ik.isIK=True;ik.target=indices[jp+'足首']
        ik.loopCount=40;ik.rotationConstraint=math.pi
        knee=pmx.IKLink();knee.target=indices[jp+'ひざ']
        knee.minimumAngle=(-math.pi,0,0);knee.maximumAngle=(-.001,0,0)
        hip=pmx.IKLink();hip.target=indices[jp+'足']
        ik.ik_links=[knee,hip]
        toe_ik=bone(jp+'つま先ＩＫ',p(1,.3,-1.8),jp+'足ＩＫ', order=2)
        toe_ik.isIK=True;toe_ik.target=indices[jp+'つま先']
        toe_ik.loopCount=8;toe_ik.rotationConstraint=math.pi
        ankle=pmx.IKLink();ankle.target=indices[jp+'足首']
        toe_ik.ik_links=[ankle]
    return model
