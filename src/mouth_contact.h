#pragma once
#include "pose_math.h"
#include <array>
#include <string_view>
#include <vector>

namespace MouthContact {
using PoseMath::Vector3;
inline Vector3 Add(Vector3 a,Vector3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vector3 Subtract(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vector3 Scale(Vector3 a,float scale){return {a.x*scale,a.y*scale,a.z*scale};}
inline float Dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vector3 Cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Length(Vector3 a){return std::sqrt(Dot(a,a));}
inline bool Normalize(Vector3& a){
 auto length=Length(a);
 if(!std::isfinite(length)||length<.0001f)return false;
 a=Scale(a,1/length);
 return true;
}
struct Entrance {
 Vector3 center,right,up,outward;
 float width=.015f,height=.0045f,gap=0;
};
struct Probe {Vector3 base,tip;};

enum class Attachment {Off,Mouth,Pelvis,LeftHand,RightHand};
inline constexpr std::array<std::string_view,5> AttachmentKeys{"off","mouth","pelvis","left_hand","right_hand"};
inline std::string_view AttachmentKey(Attachment target){return AttachmentKeys[static_cast<size_t>(target)];}
inline constexpr size_t AttachmentCount=AttachmentKeys.size()-1;
inline constexpr std::array<float,3> AttachmentOffsetLimits{5,10,10};
inline constexpr unsigned long long AttachmentMarkerMs=5000;
inline bool AttachmentMarkerVisible(unsigned long long now,unsigned long long expires){return now<expires;}
using AttachmentOffset=std::array<float,3>;
using AttachmentOffsets=std::array<AttachmentOffset,AttachmentCount>;

struct AttachmentAxes {Vector3 right,up,forward;};
inline Vector3 OffsetAnchor(Vector3 anchor,const AttachmentAxes& axes,const AttachmentOffset& cm){
 return Add(anchor,Scale(Add(Scale(axes.right,cm[0]),Add(Scale(axes.up,cm[1]),Scale(axes.forward,cm[2]))),.01f));
}

inline Vector3 ProjectToTipDepth(const Entrance& mouth,const Probe& probe,Vector3 target){
 const auto shaft=Subtract(probe.tip,probe.base);
 const float axial=Dot(shaft,mouth.outward);
 const auto radial=Subtract(shaft,Scale(mouth.outward,axial));
 const auto center=Subtract(probe.tip,radial);
 auto direction=Subtract(target,center);
 direction=Subtract(direction,Scale(mouth.outward,Dot(direction,mouth.outward)));
 return Normalize(direction)?Add(center,Scale(direction,Length(radial))):probe.tip;
}

enum class FollowGate {Accepted,Invalid,Distance,Behind,Lateral,Reach};
inline const char* FollowGateName(FollowGate gate){
 switch(gate){
 case FollowGate::Accepted:return "accepted";
 case FollowGate::Invalid:return "invalid";
 case FollowGate::Distance:return "distance";
 case FollowGate::Behind:return "behind";
 case FollowGate::Lateral:return "lateral";
 case FollowGate::Reach:return "reach";
 }
 return "invalid";
}

inline FollowGate FollowTarget(const Entrance& mouth,const Probe& probe,bool engaged,Vector3& offset){
 offset={};
 if(!Finite(mouth.center)||!Finite(mouth.outward)||!Finite(mouth.right)||!Finite(mouth.up))return FollowGate::Invalid;
 auto entry=Subtract(probe.tip,mouth.center);
 const float distance=Length(entry),length=Length(Subtract(probe.tip,probe.base));
 if(!std::isfinite(distance)||!std::isfinite(length)||length<.015f)return FollowGate::Invalid;
 if(distance>(engaged?.145f:.095f))return FollowGate::Distance;
 const float baseFront=Dot(Subtract(probe.base,mouth.center),mouth.outward);
 const float tipFront=Dot(entry,mouth.outward);
 const bool atLips=Length(entry)<.024f&&std::abs(tipFront)<.018f;
 // An oblique pose can put the base behind the lip plane while the tip
 // approaches the opening. Reject a deep rear approach, not a tilted head.
 if(!atLips&&(baseFront<-.085f||(baseFront<-.025f&&tipFront<-.035f)))return FollowGate::Behind;
 if(!engaged&&(std::abs(Dot(entry,mouth.right))>mouth.width+.025f||
               std::abs(Dot(entry,mouth.up))>.040f))return FollowGate::Lateral;
 const auto tip=ProjectToTipDepth(mouth,probe,mouth.center);
 if(Length(Subtract(tip,mouth.center))>(engaged?.09f:.065f))return FollowGate::Reach;
 offset=Subtract(tip,probe.tip);
 return FollowGate::Accepted;
}

inline FollowGate FollowPoint(const Probe& probe,Vector3 center,bool engaged){
 if(!Finite(center)||!Finite(probe.base)||!Finite(probe.tip))return FollowGate::Invalid;
 const float distance=Length(Subtract(center,probe.tip));
 if(!std::isfinite(distance))return FollowGate::Invalid;
 if(distance>(engaged?.09f:.055f))return FollowGate::Distance;
 const auto shaft=Subtract(probe.tip,probe.base);
 const auto direction=Subtract(center,probe.base);
 const float radius=Length(shaft),reach=Length(direction);
 if(!std::isfinite(radius)||!std::isfinite(reach)||radius<.015f||reach<.015f||
    std::abs(radius-reach)>(engaged?.055f:.03f)||
    Dot(shaft,direction)<.5f*radius*reach)return FollowGate::Reach;
 return FollowGate::Accepted;
}

struct PelvicApproach {
 FollowGate gate=FollowGate::Invalid;
 float reach=0,length=0;
 PoseMath::Quaternion rotation{};
};

inline PelvicApproach FollowPelvis(const Probe& probe,Vector3 center){
 auto axis=Subtract(probe.tip,probe.base);
 auto offset=Subtract(center,probe.base);
 const float length=Length(axis);
 const float reach=Length(offset);
 if(!std::isfinite(reach)||length<.015f||!Normalize(axis))return {};
 constexpr float attachmentReachMultiplier=1.3f;
 if(reach>length*attachmentReachMultiplier+1e-5f)return {FollowGate::Reach,reach,length};
 if(!Normalize(offset))return {FollowGate::Accepted,reach,length};
 auto perpendicular=Cross(axis,offset);
 const float sine=Length(perpendicular),cosine=Dot(axis,offset);
 if(sine<1e-6f){
  if(cosine>=0)return {FollowGate::Accepted,reach,length};
  perpendicular=Cross(axis,std::abs(axis.x)<.8f?Vector3{1,0,0}:Vector3{0,1,0});
  Normalize(perpendicular);
  return {FollowGate::Accepted,reach,length,{perpendicular.x,perpendicular.y,perpendicular.z,0}};
 }
 const float half=std::atan2(sine,cosine)*.5f;
 perpendicular=Scale(perpendicular,std::sin(half)/sine);
 return {FollowGate::Accepted,reach,length,{perpendicular.x,perpendicular.y,perpendicular.z,std::cos(half)}};
}

inline Vector3 PalmAnchor(Vector3 wrist,Vector3 middle){
 return Add(wrist,Scale(Subtract(middle,wrist),.55f));
}

inline bool PalmAxes(Vector3 wrist,Vector3 middle,Vector3 index,Vector3 little,bool left,AttachmentAxes& axes){
 auto up=Subtract(middle,wrist);
 if(!Normalize(up))return false;
 auto side=Subtract(little,index);
 if(!Normalize(side))return false;
 auto forward=Cross(side,up);
 if(!Normalize(forward))return false;
 if(!left)forward=Scale(forward,-1);
 axes={Cross(up,forward),up,forward};
 return true;
}

inline bool EnterRootContact(float distance,bool& touching){
 constexpr float enterDistance=.032f,releaseDistance=.042f;
 const bool wasTouching=touching;
 touching=std::isfinite(distance)&&distance>=0&&distance<=(wasTouching?releaseDistance:enterDistance);
 return touching&&!wasTouching;
}

inline float RootSurfaceDistance(const Probe& probe,Vector3 anchor){
 // Approximate the exposed root region with the first part of the current axis.
 const auto rootSection=Scale(Subtract(probe.tip,probe.base),.35f);
 const auto offset=Subtract(anchor,probe.base);
 const float lengthSquared=Dot(rootSection,rootSection);
 if(lengthSquared<=1e-8f)return Length(offset);
 const float along=std::clamp(Dot(offset,rootSection)/lengthSquared,0.f,1.f);
 return Length(Subtract(offset,Scale(rootSection,along)));
}

inline bool PelvicAnchor(Vector3 left,Vector3 right,Vector3 spine,Vector3 bodyForward,Vector3& anchor,AttachmentAxes* axes=nullptr){
 auto across=Subtract(right,left);
 const float width=Length(across);
 const auto hips=Scale(Add(left,right),.5f);
 if(!Normalize(across))return false;
 auto up=Subtract(spine,hips);
 up=Subtract(up,Scale(across,Dot(up,across)));
 if(!Normalize(up))return false;
 auto forward=Cross(across,up);
 if(Dot(forward,bodyForward)<0)forward=Scale(forward,-1);
 if(axes)*axes={Cross(up,forward),up,forward};
 anchor=Add(hips,Scale(Subtract(Scale(forward,.27f),Scale(up,.24f)),width));
 return Finite(anchor);
}

inline PoseMath::Quaternion AimRotation(Vector3 original,Vector3 aim){
 if(!Normalize(original)||!Normalize(aim))return {};
 auto axis=Cross(original,aim);
 const float sine=Length(axis),cosine=std::clamp(Dot(original,aim),-1.f,1.f);
 if(sine<.0001f)return {};
 axis=Scale(axis,1/sine);
 const float half=std::min(std::atan2(sine,cosine),1.04719755f)*.5f;
 return {axis.x*std::sin(half),axis.y*std::sin(half),axis.z*std::sin(half),std::cos(half)};
}

inline Vector3 RotateAround(Vector3 pivot,PoseMath::Quaternion rotation,Vector3 point){
 return Add(pivot,PoseMath::Rotate(rotation,Subtract(point,pivot)));
}

inline float RootRotationWeight(float rootToTip){
 const auto t=std::clamp((rootToTip-.20f)/(.42f-.20f),0.f,1.f);
 return t*t*(3-2*t);
}

inline Vector3 RotateAttachedVertex(Vector3 pivot,PoseMath::Quaternion rotation,Vector3 point,float weight){
 if(weight<=0)return point;
 if(weight<1)rotation=PoseMath::Nlerp({},rotation,weight);
 return RotateAround(pivot,rotation,point);
}

inline bool ToLocalDelta(Vector3 x,Vector3 y,Vector3 z,Vector3 world,Vector3& local){
 const float determinant=Dot(x,Cross(y,z));
 if(!Finite(world)||!std::isfinite(determinant)||std::abs(determinant)<.00001f)return false;
 local={Dot(world,Cross(y,z))/determinant,Dot(world,Cross(z,x))/determinant,
        Dot(world,Cross(x,y))/determinant};
 return Finite(local);
}

inline bool TriangleOverlapsBox(const std::array<Vector3,3>& triangle,Vector3 half){
 const auto separated=[&](Vector3 axis){
  const float reach=std::abs(axis.x)*half.x+std::abs(axis.y)*half.y+std::abs(axis.z)*half.z;
  const auto a=Dot(triangle[0],axis),b=Dot(triangle[1],axis),c=Dot(triangle[2],axis);
  return std::min({a,b,c})>reach||std::max({a,b,c})<-reach;
 };
 constexpr std::array<Vector3,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
 for(auto axis:axes)if(separated(axis))return false;
 const auto e0=Subtract(triangle[1],triangle[0]),e1=Subtract(triangle[2],triangle[1]);
 if(separated(Cross(e0,e1)))return false;
 for(auto edge:{e0,e1,Subtract(triangle[0],triangle[2])})
  for(auto axis:axes)if(separated(Cross(edge,axis)))return false;
 return true;
}

// Surface intersection handles first contact. Parity handles a box fully
// enclosed by the closed mesh after entry.
inline bool LipBoxContact(const std::vector<Vector3>& points,const std::vector<size_t>& indices,
                          Vector3 half,std::vector<float>& rayHits){
 rayHits.clear();
 for(size_t i=0;i+2<indices.size();i+=3){
  const std::array<Vector3,3> triangle{points[indices[i]],points[indices[i+1]],points[indices[i+2]]};
  if(TriangleOverlapsBox(triangle,half))return true;
  const auto a=triangle[0],u=Subtract(triangle[1],a),v=Subtract(triangle[2],a);
  const auto det=u.x*v.y-u.y*v.x;
  if(std::abs(det)<1e-9f)continue;
  constexpr float rayX=.000137f,rayY=.000211f;
  const auto x=rayX-a.x,y=rayY-a.y;
  const auto b=(x*v.y-y*v.x)/det,c=(u.x*y-u.y*x)/det;
  if(b<0||c<0||b+c>1)continue;
  const auto distance=a.z+b*u.z+c*v.z;
  if(std::abs(distance)>1e-6f&&std::isfinite(distance))rayHits.push_back(distance);
 }
 std::sort(rayHits.begin(),rayHits.end());
 size_t front=0,back=0;
 float previous=0;
 for(auto z:rayHits){
  if((z<0)==(previous<0)&&std::abs(z-previous)<.00001f&&previous!=0)continue;
  if(z>0)++front;
  else ++back;
  previous=z;
 }
 return front%2!=0&&back%2!=0;
}

// Measure across the shaft at the mouth position. An oblique lip plane can
// cut along the length of the mesh and is unsuitable for measuring thickness.
inline float ShaftThickness(const std::vector<Vector3>& points,const std::vector<size_t>& indices,
                            const Probe& probe,std::array<std::vector<float>,3>& hits){
 auto axis=Subtract(probe.tip,probe.base);
 const auto length=Length(axis);
 if(length<.004f||!Normalize(axis))return 0;
 auto up=Subtract(Vector3{0,1,0},Scale(axis,axis.y));
 if(!Normalize(up))return 0;
 const auto across=Cross(axis,up);
 const auto center=Add(probe.base,Scale(axis,std::clamp(-Dot(probe.base,axis),.001f,length-.001f)));
 const float mouthX=-Dot(center,across),mouthY=-Dot(center,up);
 const std::array<float,3> rays{mouthX,mouthX-.003f,mouthX+.003f};
 for(auto& ray:hits)ray.clear();
 for(size_t i=0;i+2<indices.size();i+=3){
  const std::array<Vector3,3> triangle{points[indices[i]],points[indices[i+1]],points[indices[i+2]]};
  const std::array<float,3> distances{Dot(Subtract(triangle[0],center),axis),
                                      Dot(Subtract(triangle[1],center),axis),
                                      Dot(Subtract(triangle[2],center),axis)};
  std::array<Vector3,2> section{};
  size_t count=0;
  for(size_t edge=0;edge<3;++edge){
   const auto next=(edge+1)%3;
   const auto a=distances[edge],b=distances[next];
   if((a<0)==(b<0))continue;
   section[count++]=Subtract(PoseMath::Lerp(triangle[edge],triangle[next],a/(a-b)),center);
  }
  if(count!=2)continue;
  const float x0=Dot(section[0],across),x1=Dot(section[1],across);
  for(size_t ray=0;ray<rays.size();++ray){
   const auto x=rays[ray];
   if((x0<x)==(x1<x))continue;
   const auto y=std::lerp(Dot(section[0],up),Dot(section[1],up),(x-x0)/(x1-x0));
   if(std::isfinite(y))hits[ray].push_back(y);
  }
 }
 float thickness=0;
 for(auto& ray:hits){
  std::sort(ray.begin(),ray.end());
  for(size_t i=0;i+1<ray.size();i+=2){
   const auto low=ray[i],high=ray[i+1];
   if(low<=mouthY+.0045f&&high>=mouthY-.0045f)thickness=std::max(thickness,high-low);
  }
 }
 return thickness;
}

inline float OpeningForThickness(float thickness){
 if(!std::isfinite(thickness)||thickness<=0)return 0;
 return .036f*(1-std::exp(-thickness/.04f));
}

inline float JawAngle(float opening,float gap,float lever){
 if(!std::isfinite(opening)||!std::isfinite(gap)||!std::isfinite(lever))return 0;
 auto needed=std::max(0.f,opening-std::max(0.f,gap));
 return std::min(.42f,std::atan2(needed,std::clamp(lever,.07f,.13f)));
}
}
