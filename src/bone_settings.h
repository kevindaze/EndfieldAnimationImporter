#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <map>
#include <string>
#include <cmath>
namespace Bones {
using Json=nlohmann::json;using Values=std::array<float,9>;
inline Values Defaults(){return {0,0,0,0,0,0,1,1,1};}
inline bool ValidKey(const std::string& key){return key.size()>=3&&key.size()<=2048&&(key.starts_with("0|")||key.starts_with("1|"))&&key.find_first_of("\r\n") == std::string::npos;}
inline bool Valid(const Values& v,bool root=false){for(int i=0;i<9;++i)if(!std::isfinite(v[i])||v[i]<(i<3?(root&&i!=1?-200.f:-100.f):i<6?-180.f:.1f)||v[i]>(i<3?(root&&i!=1?200.f:100.f):i<6?180.f:3.f))return false;return true;}
struct Settings {std::map<std::string,Values> offsets;Values Get(const std::string& key)const{auto it=offsets.find(key);return it==offsets.end()?Defaults():it->second;}bool Set(const std::string& key,Values v){if(!ValidKey(key)||!Valid(v,key=="0|@root"||key=="1|@root"))return false;if(v==Defaults())offsets.erase(key);else{if(!offsets.contains(key)&&offsets.size()>=1200)return false;offsets[key]=v;}return true;}Json Encode()const{return Json(offsets);}bool Read(const Json& j){try{if(!j.is_object()||j.size()>1200)return false;Settings next;for(auto it=j.begin();it!=j.end();++it)if(!next.Set(it.key(),it.value().get<Values>()))return false;*this=next;return true;}catch(...){return false;}}void Reset(int actor=-1){if(actor<0)offsets.clear();else std::erase_if(offsets,[&](auto& pair){return pair.first.starts_with(std::to_string(actor)+"|");});}};
inline std::string Group(const std::string& name){if(name=="@root")return "人物根節點";if(name.find("Finger")!=std::string::npos)return "手指";if(name.find("Hand")!=std::string::npos||name.find("Arm")!=std::string::npos||name.find("Clavicle")!=std::string::npos)return "手臂／手腕";if(name.find("Head")!=std::string::npos||name.find("Neck")!=std::string::npos||name.find("Jaw")!=std::string::npos)return "頭／頸／臉";if(name.find("Spine")!=std::string::npos||name.find("Pelvis")!=std::string::npos||name=="Bip001")return "腰／脊椎／骨架根";if(name.find("Thigh")!=std::string::npos||name.find("Calf")!=std::string::npos||name.find("Foot")!=std::string::npos||name.find("Toe")!=std::string::npos)return "腿／腳";return "其他骨架／附屬";}
}
