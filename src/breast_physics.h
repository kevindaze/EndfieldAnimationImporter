#pragma once
#include "pose_math.h"
#include <array>
namespace BreastPhysics {
inline float Length(PoseMath::Vector3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
inline PoseMath::Vector3 Cross(PoseMath::Vector3 a,PoseMath::Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Unit(PoseMath::Vector3& a){float n=Length(a);if(!std::isfinite(n)||n<.0001f)return false;a={a.x/n,a.y/n,a.z/n};return true;}
inline bool ChestFrame(PoseMath::Vector3 chest,PoseMath::Vector3 head,PoseMath::Vector3 left,PoseMath::Vector3 right,PoseMath::Quaternion& q){
 auto x=PoseMath::Vector3{right.x-left.x,right.y-left.y,right.z-left.z};auto y=PoseMath::Vector3{head.x-chest.x,head.y-chest.y,head.z-chest.z};
 if(!Unit(x)||!Unit(y))return false;auto z=Cross(x,y);if(!Unit(z))return false;y=Cross(z,x);
 float trace=x.x+y.y+z.z,s;
 if(trace>0){s=std::sqrt(trace+1)*2;q={(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,s*.25f};}
 else if(x.x>y.y&&x.x>z.z){s=std::sqrt(1+x.x-y.y-z.z)*2;q={s*.25f,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
 else if(y.y>z.z){s=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/s,s*.25f,(z.y+y.z)/s,(z.x-x.z)/s};}
 else{s=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/s,(z.y+y.z)/s,s*.25f,(x.y-y.x)/s};}return PoseMath::Normalize(q);
}
inline PoseMath::Quaternion RotationVector(PoseMath::Vector3 a){float n=Length(a);if(n<.000001f)return {};float k=std::sin(n*.5f)/n;return {a.x*k,a.y*k,a.z*k,std::cos(n*.5f)};}
inline PoseMath::Quaternion BoneOutput(PoseMath::Quaternion base,PoseMath::Quaternion world,PoseMath::Quaternion chest,PoseMath::Vector3 angles){
 auto delta=RotationVector(angles);auto worldDelta=PoseMath::Multiply(PoseMath::Multiply(chest,delta),PoseMath::Inverse(chest));
 auto boneDelta=PoseMath::Multiply(PoseMath::Multiply(PoseMath::Inverse(world),worldDelta),world);return PoseMath::Multiply(base,boneDelta);
}
struct MotionSamples {
 struct Sample {double time=0;PoseMath::Vector3 point{};};std::array<Sample,64> history{};int count=0;double time=0;PoseMath::Vector3 estimatedVelocity{};
 void Reset(){count=0;time=0;estimatedVelocity={};}
 PoseMath::Vector3 Add(PoseMath::Vector3 point,float dt){
  time+=dt;if(count==64){for(int i=1;i<count;++i)history[i-1]=history[i];--count;}history[count++]={time,point};
  int first=0;while(first<count-1&&time-history[first].time>.12)++first;
  if(count-first<4||time-history[first].time<.045)return {};
  double matrix[3][6]{};
  for(int i=first;i<count;++i){double u=(history[i].time-time)/.12,terms[]{1,u,u*u};double p[]{double(history[i].point.x)-point.x,double(history[i].point.y)-point.y,double(history[i].point.z)-point.z};for(int r=0;r<3;++r){for(int c=0;c<3;++c)matrix[r][c]+=terms[r]*terms[c];for(int c=0;c<3;++c)matrix[r][c+3]+=terms[r]*p[c];}}
  for(int c=0;c<3;++c){int pivot=c;for(int r=c+1;r<3;++r)if(std::abs(matrix[r][c])>std::abs(matrix[pivot][c]))pivot=r;if(std::abs(matrix[pivot][c])<1e-10)return {};for(int k=0;k<6;++k)std::swap(matrix[c][k],matrix[pivot][k]);double scale=matrix[c][c];for(int k=c;k<6;++k)matrix[c][k]/=scale;for(int r=0;r<3;++r)if(r!=c){double f=matrix[r][c];for(int k=c;k<6;++k)matrix[r][k]-=f*matrix[c][k];}}
  // A step or an isolated bad sample is not a smooth chest trajectory.
  double error=0;for(int i=first;i<count;++i){double u=(history[i].time-time)/.12;double observed[]{double(history[i].point.x)-point.x,double(history[i].point.y)-point.y,double(history[i].point.z)-point.z};for(int axis=0;axis<3;++axis)error=std::max(error,std::abs(observed[axis]-(matrix[0][axis+3]+u*matrix[1][axis+3]+u*u*matrix[2][axis+3])));}
  if(error>.012){Reset();history[count++]={time,point};return {};}
  estimatedVelocity={float(matrix[1][3]/.12),float(matrix[1][4]/.12),float(matrix[1][5]/.12)};
  return {float(2*matrix[2][3]/(.12*.12)),float(2*matrix[2][4]/(.12*.12)),float(2*matrix[2][5]/(.12*.12))};
 }
};
struct DirectionalResponse {
 bool ready=false;PoseMath::Vector3 previous{},previousVelocity{},filteredAcceleration{},filteredMotionVelocity{},angle{},velocity{};PoseMath::Quaternion previousRotation{};
 MotionSamples samples;bool wasPaused=false;
 void Reset(){*this=DirectionalResponse{};}
 PoseMath::Vector3 Step(PoseMath::Vector3 origin,PoseMath::Quaternion chest,PoseMath::Vector3 lever,float dt,float hz,float damping,float limit,bool paused,float speedLag=0){
  if(!PoseMath::Finite(origin)||!PoseMath::Normalize(chest)||!std::isfinite(dt)||dt<=0||dt>.15f){Reset();return {};}
  auto offset=PoseMath::Rotate(chest,lever);auto point=PoseMath::Vector3{origin.x+offset.x,origin.y+offset.y,origin.z+offset.z};
  if(!ready){ready=true;previous=point;previousRotation=chest;wasPaused=paused;samples.Add(point,dt);return {};}
  auto d=PoseMath::Vector3{point.x-previous.x,point.y-previous.y,point.z-previous.z};auto rotation=PoseMath::Multiply(PoseMath::Inverse(previousRotation),chest);
  if(Length(d)>.5f||std::abs(rotation.w)<.866f){Reset();return {};}
  if(paused!=wasPaused){samples.Reset();wasPaused=paused;}
  auto a=paused?PoseMath::Vector3{}:samples.Add(point,dt);
  previous=point;previousRotation=chest;
  float blend=1-std::exp(-dt/.08f);auto filter=[&](float& f,float raw){f+=((paused?0:std::clamp(raw,-30.f,30.f))-f)*blend;};filter(filteredAcceleration.x,a.x);filter(filteredAcceleration.y,a.y);filter(filteredAcceleration.z,a.z);
  auto smoothSpeed=[&](float& f,float raw){f+=((paused?0:std::clamp(raw,-1.f,1.f))-f)*blend;};smoothSpeed(filteredMotionVelocity.x,samples.estimatedVelocity.x);smoothSpeed(filteredMotionVelocity.y,samples.estimatedVelocity.y);smoothSpeed(filteredMotionVelocity.z,samples.estimatedVelocity.z);
  auto local=PoseMath::Rotate(PoseMath::Inverse(chest),filteredAcceleration);auto arm=lever;if(!Unit(arm))arm={0,0,1};
  auto torque=Cross(arm,{-local.x,-local.y,-local.z});float strength=limit/9.81f;auto target=paused?PoseMath::Vector3{}:PoseMath::Vector3{torque.x*strength,torque.y*strength,torque.z*strength};
  float magnitude=Length(target);if(magnitude>limit)target={target.x*limit/magnitude,target.y*limit/magnitude,target.z*limit/magnitude};
  float w=6.2831853f*std::clamp(hz,.5f,5.f);int steps=std::max(1,int(std::ceil(dt/.008f)));float h=dt/steps;
  auto advance=[&](float& x,float& vstate,float t){vstate+=(w*w*(t-x)-2*std::clamp(damping,.2f,2.f)*w*vstate)*h;x+=vstate*h;};
  for(int n=0;n<steps;++n){advance(angle.x,velocity.x,target.x);advance(angle.y,velocity.y,target.y);advance(angle.z,velocity.z,target.z);float length=Length(angle);if(length>limit){angle={angle.x*limit/length,angle.y*limit/length,angle.z*limit/length};velocity={};}}
  // Smoothed drag is a relative deformation, separate from spring rebound.
  // Passing it through the spring again would invert its phase near resonance.
  auto output=angle;if(speedLag>0){auto motion=PoseMath::Rotate(PoseMath::Inverse(chest),filteredMotionVelocity);auto drag=Cross(arm,{-motion.x,-motion.y,-motion.z});float gain=limit*speedLag/.25f;output={output.x+drag.x*gain,output.y+drag.y*gain,output.z+drag.z*gain};}
  float outputLength=Length(output);if(outputLength>limit)output={output.x*limit/outputLength,output.y*limit/outputLength,output.z*limit/outputLength};return output;
 }
};
// Imported clips have no locomotion gait state. Drive a calibrated oscillator
// from actual chest motion instead of treating the clip as native idle/jump.
struct AnimationResponse {
 bool ready=false;PoseMath::Vector3 previous{};PoseMath::Quaternion previousRotation{};
 float amplitude=0,phase=0,activity=0;
 void Reset(){*this=AnimationResponse{};}
 float Step(PoseMath::Vector3 p,PoseMath::Quaternion q,float dt,float up,float down,float frequency,float attack,float release,bool paused){
  if(!PoseMath::Finite(p)||!PoseMath::Normalize(q)||!std::isfinite(dt)||dt<=0||dt>.15f){Reset();return 0;}
  if(!ready){ready=true;previous=p;previousRotation=q;return 0;}
  auto d=PoseMath::Vector3{p.x-previous.x,p.y-previous.y,p.z-previous.z};float distance=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
  auto rotation=PoseMath::Multiply(PoseMath::Inverse(previousRotation),q);float cosine=std::clamp(std::abs(rotation.w),0.f,1.f);
  previous=p;previousRotation=q;
  if(distance>.5f||cosine<.866f){Reset();return 0;}
  float angular=2*std::atan2(std::sqrt(rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z),cosine)/dt;
  activity=paused?0:std::clamp((distance/dt+angular*.25f)/.1f,0.f,1.f);
  float tau=activity>amplitude?attack:release;
  amplitude+=(activity-amplitude)*(1-std::exp(-dt/std::max(.02f,tau)));
  if(!paused)phase=std::fmod(phase+dt*6.2831853f*std::clamp(frequency,.5f,5.f),6.2831853f);
  float wave=std::sin(phase);return amplitude*wave*(wave>=0?up:down)*.01745329252f;
 }
};
// Animation-driven inertial response. Character calibration is supplied separately.
struct Spring {
 float angle=0,velocity=0,previousVelocity=0;bool ready=false;
 PoseMath::Vector3 previous{};PoseMath::Quaternion previousRotation{};
 void Reset(){*this=Spring{};}
 float Step(PoseMath::Vector3 p,PoseMath::Quaternion q,float dt,float hz,float damping,float limit,float gain,bool paused=false){
  if(!PoseMath::Finite(p)||!PoseMath::Normalize(q)||!std::isfinite(dt)||dt<=0||dt>.15f){Reset();return 0;}
  if(!ready){previous=p;previousRotation=q;ready=true;return 0;}
  auto d=PoseMath::Vector3{p.x-previous.x,p.y-previous.y,p.z-previous.z};
  auto rotation=PoseMath::Multiply(PoseMath::Inverse(previousRotation),q);
  if(rotation.w<0)rotation={-rotation.x,-rotation.y,-rotation.z,-rotation.w};
  if(d.x*d.x+d.y*d.y+d.z*d.z>.25f||std::abs(rotation.w)<.866f){Reset();return 0;}
  auto local=PoseMath::Rotate(PoseMath::Inverse(q),d);
  float speed=paused?0:(local.y+local.z*.5f+rotation.x*.15f)/dt;
  float acceleration=std::clamp((speed-previousVelocity)/dt,-20.f,20.f);
  previous=p;previousRotation=q;previousVelocity=speed;
  float w=6.2831853f*std::clamp(hz,.5f,5.f),force=paused?0:-acceleration*gain;
  int steps=std::max(1,int(std::ceil(dt/.008f)));float h=dt/steps;
  for(int i=0;i<steps;++i){velocity+=(force-w*w*angle-2*std::clamp(damping,.2f,2.f)*w*velocity)*h;angle+=velocity*h;float clamped=std::clamp(angle,-limit,limit);if(clamped!=angle)velocity=0;angle=clamped;}
  return angle;
 }
};
}
