#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace ExternalModels {
using Json=nlohmann::json;
struct Parameter {std::string id,name,binding;float low=0,high=1,value=0;};
struct Part {std::string id,name,character,backend,bone,source;std::vector<Parameter> parameters;};
inline bool Read(const Json& j,std::vector<Part>& output){try{
 if(j.at("format")!="endfield-external-model"||j.at("version")!=1||!j.at("parts").is_array()||j.at("parts").size()>128)return false;
 std::vector<Part> next;std::set<std::string> ids;
 for(auto& v:j.at("parts")){Part p;p.id=v.at("id");p.name=v.at("name");p.character=v.at("character");p.backend=v.at("backend");p.bone=v.value("bone",std::string{});p.source=v.value("source_file",std::string{});
 if(p.id.empty()||p.id.size()>128||p.name.empty()||p.name.size()>256||!p.character.starts_with("chr_")||!ids.insert(p.id).second)return false;
 if(p.backend!="unity_transform"&&p.backend!="efmi_shapekey")return false;
 if(p.backend=="unity_transform"&&p.bone.empty())return false;
 if(p.backend=="efmi_shapekey"&&p.source.empty())return false;
 auto params=v.value("parameters",Json::array());if(!params.is_array()||params.size()>32)return false;std::set<std::string> paramIds;
 for(auto& q:params){Parameter a;a.id=q.at("id");a.name=q.at("name");a.binding=q.value("binding",std::string{});a.low=q.at("min");a.high=q.at("max");a.value=q.at("default");if(a.id.empty()||a.name.empty()||!paramIds.insert(a.id).second||!std::isfinite(a.low)||!std::isfinite(a.high)||!std::isfinite(a.value)||a.low>=a.high||a.value<a.low||a.value>a.high)return false;p.parameters.push_back(a);}
 next.push_back(p);
 }output=std::move(next);return true;
 }catch(...){return false;}}
inline std::vector<Part> Load(const std::vector<std::filesystem::path>& directories,std::vector<std::string>& errors){std::vector<Part> result;std::set<std::string> ids;
 for(auto& directory:directories){std::error_code ec;if(!std::filesystem::is_directory(directory,ec))continue;std::vector<std::filesystem::path> files;for(auto& entry:std::filesystem::directory_iterator(directory,ec))if(entry.path().extension()==".json")files.push_back(entry.path());std::sort(files.begin(),files.end());
 for(auto& file:files){try{if(std::filesystem::file_size(file)>1048576)throw std::runtime_error("profile too large");std::ifstream in(file);std::vector<Part> parts;if(!Read(Json::parse(in),parts))throw std::runtime_error("invalid profile");for(auto& part:parts){if(!part.source.empty()){auto path=std::filesystem::u8path(part.source);if(path.is_relative())path=file.parent_path()/path;auto text=path.lexically_normal().u8string();part.source=std::string(text.begin(),text.end());}if(!ids.insert(part.id).second){errors.push_back("duplicate part id: "+part.id);continue;}result.push_back(std::move(part));}}catch(const std::exception& e){errors.push_back(file.filename().string()+": "+e.what());}}
 }return result;}
}
