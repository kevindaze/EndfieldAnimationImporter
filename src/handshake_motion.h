#pragma once
#include "pose_math.h"
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <filesystem>
namespace Handshake {
using Q=PoseMath::Quaternion; using V=PoseMath::Vector3;
inline bool Unit(V& v){float n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(!PoseMath::Finite(v)||n<1e-6f)return false;v={v.x/n,v.y/n,v.z/n};return true;}
inline Q Axis(char axis,float degrees){float a=degrees*0.00872664626f,s=std::sin(a),c=std::cos(a);return {axis=='x'?s:0,axis=='y'?s:0,axis=='z'?s:0,c};}
inline Q XYZ(float x,float y,float z){return PoseMath::Multiply(Axis('z',z),PoseMath::Multiply(Axis('y',y),Axis('x',x)));}
inline V Add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V Sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V Scale(V a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Q AngleAxis(V axis,float degrees){if(!Unit(axis))return {};float a=degrees*0.00872664626f;return {axis.x*std::sin(a),axis.y*std::sin(a),axis.z*std::sin(a),std::cos(a)};}
inline Q Aim(V from,V to);
inline Q PalmRotation(Q reference,V localForward,V localNormal,V forward,V normal){
 Q aligned=PoseMath::Multiply(Aim(PoseMath::Rotate(reference,localForward),forward),reference);
 Unit(forward);auto observed=PoseMath::Rotate(aligned,localNormal);
 observed=Sub(observed,Scale(forward,Dot(observed,forward)));normal=Sub(normal,Scale(forward,Dot(normal,forward)));
 if(!Unit(observed)||!Unit(normal))return aligned;
 float degrees=std::atan2(Dot(forward,Cross(observed,normal)),Dot(observed,normal))*57.29577951f;
 return PoseMath::Multiply(AngleAxis(forward,degrees),aligned);
}
inline float Length(V a){return std::sqrt(Dot(a,a));}
inline bool StablePlane(V aim,V preferred,V previous,V& normal){
 if(!Unit(aim))return false;
 normal=Sub(preferred,Scale(aim,Dot(preferred,aim)));
 if(!Unit(normal)){normal=Sub(previous,Scale(aim,Dot(previous,aim)));if(!Unit(normal)){V axis=std::abs(aim.x)<.8f?V{1,0,0}:V{0,0,1};normal=Cross(aim,axis);if(!Unit(normal))return false;}}
 if(Unit(previous)&&Dot(normal,previous)<0)normal=Scale(normal,-1.f);
 return true;
}
inline Q TwistFraction(Q rotation,V axis,float fraction){
 if(!Unit(axis))return {};float projection=rotation.x*axis.x+rotation.y*axis.y+rotation.z*axis.z;
 float length=std::sqrt(projection*projection+rotation.w*rotation.w);if(length<1e-6f)return {};
 float w=rotation.w/length,s=projection/length;if(w<0){w=-w;s=-s;}
 float half=std::atan2(s,w)*fraction;return {axis.x*std::sin(half),axis.y*std::sin(half),axis.z*std::sin(half),std::cos(half)};
}
inline bool Elbow(V shoulder,V wrist,float upper,float fore,V pole,V& elbow){
 V aim=Sub(wrist,shoulder);float d=Length(aim);
 if(!PoseMath::Finite(shoulder)||!PoseMath::Finite(wrist)||!std::isfinite(upper)||!std::isfinite(fore)||upper<0.05f||fore<0.05f||d<=std::abs(upper-fore)+0.005f||d>=upper+fore-0.005f||!Unit(aim))return false;
 pole=Sub(pole,Scale(aim,Dot(pole,aim)));
 if(!Unit(pole)){pole=Sub(V{1,0,0},Scale(aim,aim.x));if(!Unit(pole))return false;}
 float along=(upper*upper-fore*fore+d*d)/(2*d),height=std::sqrt(std::max(0.0f,upper*upper-along*along));
 elbow=Add(shoulder,Add(Scale(aim,along),Scale(pole,height)));return PoseMath::Finite(elbow);
}
inline bool Contact(V a,V b,float reachA,float reachB,V& point){
 V line=Sub(b,a);float d=Length(line);
 if(!PoseMath::Finite(a)||!PoseMath::Finite(b)||!std::isfinite(reachA)||!std::isfinite(reachB)||reachA<=0||reachB<=0||d<0.05f||d>reachA+reachB-0.06f)return false;
 float along=std::clamp(d*0.5f,std::max(0.0f,d-reachB+0.03f),std::min(d,reachA-0.03f));
 point=Add(a,Scale(line,along/d));
 // A conservative downward contact offset avoids fully extended elbows.
 float margin=std::min(std::sqrt(std::max(0.0f,reachA*reachA-along*along)),std::sqrt(std::max(0.0f,reachB*reachB-(d-along)*(d-along))));
 point.y-=std::min(0.16f,margin*0.4f);return PoseMath::Finite(point);
}
struct BodyFrame { V right,up,forward; };
inline bool ContactFrame(V up,V rightHint,V forwardHint,BodyFrame& frame){
 if(!Unit(up))return false;
 auto right=Sub(rightHint,Scale(up,Dot(rightHint,up)));
 if(!Unit(right)){right=Cross(up,forwardHint);if(!Unit(right))return false;}
 auto forward=Cross(right,up);if(!Unit(forward))return false;
 frame={right,up,forward};return true;
}
inline Q WristLimit(Q rest,Q desired,V axis){
 auto delta=PoseMath::Multiply(PoseMath::Inverse(rest),desired);
 auto twist=TwistFraction(delta,axis,1.f);
 auto swing=PoseMath::Multiply(delta,PoseMath::Inverse(twist));
 swing=PoseMath::LimitRotation({},swing,60.f);
 twist=PoseMath::LimitRotation({},twist,110.f);
 return PoseMath::Multiply(rest,PoseMath::Multiply(swing,twist));
}
inline Q Aim(V from,V to){if(!Unit(from)||!Unit(to))return {};float d=from.x*to.x+from.y*to.y+from.z*to.z;V c{from.y*to.z-from.z*to.y,from.z*to.x-from.x*to.z,from.x*to.y-from.y*to.x};if(d < -0.9999f){V axis=std::abs(from.x)<0.8f?V{1,0,0}:V{0,1,0};c={from.y*axis.z-from.z*axis.y,from.z*axis.x-from.x*axis.z,from.x*axis.y-from.y*axis.x};Unit(c);return {c.x,c.y,c.z,0};}float n=std::sqrt(c.x*c.x+c.y*c.y+c.z*c.z+(1+d)*(1+d));return {c.x/n,c.y/n,c.z/n,(1+d)/n};}
struct Bone {V direction{};Q axis{};std::vector<char> dof;std::string parent;};
struct Frame {V upper{},fore{};};
struct Clip {
 std::vector<Frame> frames;
 bool Load(const std::filesystem::path& asf,const std::filesystem::path& amc){
  try{if(LoadUnchecked(asf,amc))return true;}catch(const std::exception&){}
  frames.clear();return false;
 }
 bool LoadUnchecked(const std::filesystem::path& asf,const std::filesystem::path& amc){
  frames.clear();std::ifstream skeleton(asf),motion(amc);if(!skeleton||!motion)return false;
  std::map<std::string,Bone> bones;std::string line,name,section;Bone bone;bool inside=false;
  while(std::getline(skeleton,line)){std::istringstream s(line);std::string k;s>>k;if(k.empty()||k[0]=='#')continue;if(k[0]==':'){section=k;continue;}
   if(section==":bonedata"){if(k=="begin"){bone=Bone{};name.clear();inside=true;}else if(k=="end"){if(name.empty())return false;bones[name]=bone;inside=false;}else if(inside){if(k=="name")s>>name;else if(k=="direction")s>>bone.direction.x>>bone.direction.y>>bone.direction.z;else if(k=="axis"){float x=0,y=0,z=0;std::string order;s>>x>>y>>z>>order;if(order!="XYZ")return false;bone.axis=XYZ(x,y,z);}else if(k=="dof"){std::string d;while(s>>d){if(d.size()!=2||d[0]!='r')return false;bone.dof.push_back(d[1]);}}}}
   else if(section==":hierarchy"&&k!="begin"&&k!="end"){std::string child;while(s>>child){if(!bones.count(child))return false;bones[child].parent=k;}}
  }
  for(const auto& n:{"lowerback","upperback","thorax","rclavicle","rhumerus","rradius"})if(!bones.count(n))return false;
  std::map<std::string,std::vector<float>> values;int number=0;bool degrees=false,full=false;
  auto flush=[&](){if(!number)return true;std::map<std::string,Q> world;world["root"]={};
   // Remove global root translation/heading: this prototype uses existing facing anchors.
   for(const auto& n:{"lowerback","upperback","thorax","rclavicle","rhumerus","rradius"}){auto& b=bones[n];auto it=values.find(n);if(it==values.end()||it->second.size()!=b.dof.size()||!world.count(b.parent))return false;Q m{};for(size_t i=0;i<b.dof.size();++i)m=PoseMath::Multiply(Axis(b.dof[i],it->second[i]),m);Q local=PoseMath::Multiply(PoseMath::Multiply(b.axis,m),PoseMath::Inverse(b.axis));world[n]=PoseMath::Multiply(world[b.parent],local);}
   auto u=PoseMath::Rotate(world["rhumerus"],bones["rhumerus"].direction),f=PoseMath::Rotate(world["rradius"],bones["rradius"].direction);
   // ASF right side is negative X; reflect X into Unity's left-handed body frame.
   u.x=-u.x;f.x=-f.x;if(!Unit(u)||!Unit(f))return false;frames.push_back({u,f});return frames.size()<=10000;
  };
  while(std::getline(motion,line)){std::istringstream s(line);std::string k;s>>k;if(k.empty()||k[0]=='#')continue;if(k==":DEGREES"){degrees=true;continue;}if(k==":FULLY-SPECIFIED"){full=true;continue;}if(k[0]==':')return false;
   if(k.find_first_not_of("0123456789")==std::string::npos){if(!flush())return false;int next=std::stoi(k);if(next!=number+1)return false;number=next;values.clear();}else{std::vector<float> v;float x;while(s>>x){if(!std::isfinite(x))return false;v.push_back(x);}if(!s.eof()||values.count(k))return false;values[k]=v;}
  }return degrees&&full&&flush()&&frames.size()>1;
 }
 Frame Sample(float seconds,float fps=120)const{if(frames.empty()||!std::isfinite(seconds)||!std::isfinite(fps)||fps<=0)return {};float p=std::clamp(seconds*fps,0.0f,float(frames.size()-1));size_t a=size_t(p),b=std::min(a+1,frames.size()-1);auto u=PoseMath::Lerp(frames[a].upper,frames[b].upper,p-float(a)),f=PoseMath::Lerp(frames[a].fore,frames[b].fore,p-float(a));Unit(u);Unit(f);return {u,f};}
};
}
