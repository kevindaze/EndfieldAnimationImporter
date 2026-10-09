#pragma once
#include "preset_settings.h"
#include "pose_math.h"
#include <set>
#include <optional>
#include <algorithm>
namespace ImportedAnimation {
struct Key {float time=0;PoseMath::Quaternion rotation;std::optional<PoseMath::Vector3> position;};
struct Track {int actor=0;std::string bone;std::vector<Key> keys;
 size_t Upper(float time)const{return size_t(std::upper_bound(keys.begin(),keys.end(),time,[](float t,const Key& key){return t<key.time;})-keys.begin());}
 PoseMath::Vector3 SamplePosition(float time)const{auto i=Upper(time);if(i==0)return *keys.front().position;if(i>=keys.size())return *keys.back().position;return PoseMath::Lerp(*keys[i-1].position,*keys[i].position,(time-keys[i-1].time)/(keys[i].time-keys[i-1].time));}
 PoseMath::Quaternion Sample(float time)const{auto i=Upper(time);if(i==0)return keys.front().rotation;if(i>=keys.size())return keys.back().rotation;return PoseMath::Nlerp(keys[i-1].rotation,keys[i].rotation,(time-keys[i-1].time)/(keys[i].time-keys[i-1].time));}};

struct Clip {std::string id,name,targetCharacter,managerAction="none";int targetActor=1;bool absolute=false;float duration=0,distance=1.2f;std::vector<Track> tracks;
 bool Read(const Presets::Json& j){try{if(j.at("format")!="endfield-interaction-animation"||(j.at("version")!=1&&j.at("version")!=2))return false;Clip candidate;candidate.absolute=j.at("version")==2;if(candidate.absolute){if(j.at("rotation_space")!="absolute_local")return false;candidate.targetCharacter=j.at("target_character").get<std::string>();if(!candidate.targetCharacter.starts_with("chr_")||candidate.targetCharacter.size()>120||candidate.targetCharacter.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos)return false;}candidate.managerAction=j.value("manager_action",std::string("none"));if(candidate.managerAction!="none"&&candidate.managerAction!="standing"&&candidate.managerAction!="waist_hold"&&candidate.managerAction!="front_waist"&&candidate.managerAction!="back_waist"&&candidate.managerAction!="head_pat")return false;if(!candidate.absolute&&candidate.managerAction!="none")return false;candidate.id=j.at("id").get<std::string>();candidate.name=j.at("name").get<std::string>();candidate.duration=j.at("duration").get<float>();candidate.distance=j.value("distance",1.2f);if(candidate.id.empty()||candidate.id.size()>80||candidate.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos||candidate.name.empty()||candidate.name.size()>160||!std::isfinite(candidate.duration)||candidate.duration<.2f||!std::isfinite(candidate.distance)||candidate.distance<.4f||candidate.distance>3)return false;
  const auto& actors=j.at("actors");if(!actors.is_array()||(candidate.absolute?actors.size()!=1:actors.size()!=2))return false;std::set<int> roles;for(const auto& actor:actors){auto role=actor.at("role").get<std::string>();int index=role=="controlled"?0:role=="partner"?1:-1;if(index<0||!roles.insert(index).second)return false;const auto& tracks=actor.at("tracks");if(!tracks.is_array()||tracks.empty()||tracks.size()>64)return false;std::set<std::string> bones;for(const auto& source:tracks){Track track;track.actor=index;track.bone=source.at("bone").get<std::string>();if(!track.bone.starts_with("Bip001")||track.bone.size()>100||track.bone.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos||!bones.insert(track.bone).second)return false;const auto& keys=source.at("keys");if(!keys.is_array()||keys.size()<2)return false;float previous=-1;for(const auto& k:keys){Key key;key.time=k.at("time").get<float>();auto rotation=k.at("rotation");if(!std::isfinite(key.time)||key.time<0||key.time>candidate.duration||key.time<=previous||!rotation.is_array()||rotation.size()!=4)return false;key.rotation={rotation[0].get<float>(),rotation[1].get<float>(),rotation[2].get<float>(),rotation[3].get<float>()};if(!PoseMath::Normalize(key.rotation))return false;if(k.contains("position")){auto p=k.at("position");if(!candidate.absolute||p.size()!=3||!p.is_array())return false;key.position=PoseMath::Vector3{p[0].get<float>(),p[1].get<float>(),p[2].get<float>()};if(!PoseMath::Finite(*key.position)||std::abs(key.position->x)>10000||std::abs(key.position->y)>10000||std::abs(key.position->z)>10000)return false;}if(!track.keys.empty()&&key.position.has_value()!=track.keys.front().position.has_value())return false;previous=key.time;track.keys.push_back(key);}if(track.keys.front().time!=0||std::abs(track.keys.back().time-candidate.duration)>.0001f)return false;if(candidate.absolute)candidate.targetActor=index;candidate.tracks.push_back(std::move(track));}}
 if(candidate.absolute&&candidate.targetActor==0&&(candidate.targetCharacter!="chr_0003_endminf"&&candidate.targetCharacter!="chr_0002_endminm"||candidate.managerAction!="none"))return false;
 *this=std::move(candidate);return true;}catch(...){return false;}}
};
inline Presets::Json Slice(const Clip& clip,float start,float end,const std::string& id,const std::string& name){
 if(!std::isfinite(start)||!std::isfinite(end)||start<0||end>clip.duration||end-start<.2f)throw std::runtime_error("片段至少 0.2 秒，且必須在動畫範圍內");
 Presets::Json result{{"format","endfield-interaction-animation"},{"version",clip.absolute?2:1},{"id",id},{"name",name},{"duration",end-start},{"distance",clip.distance},{"manager_action",clip.managerAction}};
 if(clip.absolute){result["rotation_space"]="absolute_local";result["target_character"]=clip.targetCharacter;}
 auto actors=Presets::Json::array();
 for(int actor=0;actor<2;++actor){auto tracks=Presets::Json::array();for(auto& track:clip.tracks){if(track.actor!=actor)continue;auto keys=Presets::Json::array();
  auto add=[&](float t){auto q=track.Sample(t);Presets::Json key{{"time",t-start},{"rotation",{q.x,q.y,q.z,q.w}}};if(track.keys.front().position){auto v=track.SamplePosition(t);key["position"]={v.x,v.y,v.z};}keys.push_back(std::move(key));};
  add(start);for(auto& k:track.keys)if(k.time>start&&k.time<end)add(k.time);add(end);tracks.push_back({{"bone",track.bone},{"keys",keys}});
 }if(!tracks.empty())actors.push_back({{"role",actor?"partner":"controlled"},{"tracks",tracks}});}
 result["actors"]=actors;Clip check;if(!check.Read(result))throw std::runtime_error("裁切結果未通過動畫驗證");return result;
}

}
