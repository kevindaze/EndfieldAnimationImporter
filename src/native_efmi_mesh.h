#pragma once
#include "external_mesh.h"
#include <nlohmann/json.hpp>
#include <optional>

namespace NativeEfmi {
struct Profile {
 std::filesystem::path ini;
 std::string draw,character,renderer,bone,uvResource;
 int actor=0,uvOffset=0;
 bool shape=false;
 std::optional<ExternalMesh::Vec> previewScale;
};
struct Geometry {
 std::vector<ExternalMesh::Vec> positions,deltas;
 std::vector<std::array<float,2>> uv;
 std::vector<int32_t> triangles;
};

inline Profile ReadProfile(const std::filesystem::path& path){
 auto data=nlohmann::json::parse(EfmiBridge::ReadFile(path));
 if(data.at("format")!="eai-efmi-component"||data.at("version")!=1)
  throw std::runtime_error("Unsupported EAI EFMI profile format");
 Profile profile;
 auto ini=std::filesystem::u8path(data.at("ini").get<std::string>());
 profile.ini=ini.is_absolute()?ini:path.parent_path()/ini;
 profile.draw=data.at("draw").get<std::string>();
 profile.character=data.at("character").get<std::string>();
 profile.renderer=data.at("renderer").get<std::string>();
 profile.bone=data.at("bone").get<std::string>();
 if(!data.at("actor").is_number_integer())throw std::runtime_error("Actor must be an integer");
 profile.actor=data.at("actor").get<int>();
 profile.uvResource=data.value("uv_resource",std::string{});
 profile.uvOffset=data.value("uv_offset",0);
 profile.shape=data.value("packed_position_shape",false);
 if(data.contains("preview_shape_scale")){
  const auto& scale=data.at("preview_shape_scale");
  if(profile.shape||!scale.is_array()||scale.size()!=3)
   throw std::runtime_error("Use exactly one EFMI shape source");
  profile.previewScale=scale.get<ExternalMesh::Vec>();
  for(float value:*profile.previewScale)if(!std::isfinite(value)||value<0.5f||value>1.5f)
   throw std::runtime_error("Preview shape scale must be between 0.5 and 1.5");
 }
 if(ini.empty()||EfmiBridge::Lower(EfmiBridge::Utf8(ini.extension()))!=".ini"||
    profile.draw.empty()||!profile.character.starts_with("chr_")||profile.renderer.empty()||profile.bone.empty()||
    (profile.actor!=0&&profile.actor!=1)||profile.uvOffset<0||profile.uvOffset>248)
  throw std::runtime_error("Invalid EFMI profile binding");
 return profile;
}

inline Geometry ReadGeometry(const Profile& profile){
 const auto mod=EfmiBridge::Parse(profile.ini,EfmiBridge::ReadFile(profile.ini),{});
 const auto draw=std::find_if(mod.meshes.begin(),mod.meshes.end(),[&](const auto& mesh){return mesh.name==profile.draw;});
 if(draw==mod.meshes.end()||!draw->count||draw->count%3||draw->count>600000||draw->base<INT32_MIN||draw->base>INT32_MAX)
  throw std::runtime_error("Missing or unsupported indexed EFMI draw");
 const auto& resource=ExternalMesh::PositionResource(mod,*draw);
 const auto positionPath=ExternalMesh::ResourceFile(mod,resource);
 const auto positions=ExternalMesh::Read(positionPath);
 const auto stride=static_cast<size_t>(resource.stride);
 if(positions.size()%stride)throw std::runtime_error("EFMI position stride mismatch");
 const auto vertexCount=positions.size()/stride;
 ExternalMesh::Bytes uvBuffer;
 size_t uvStride=0;
 if(!profile.uvResource.empty()){
  auto uvResource=mod.resources.find(profile.uvResource);
  if(uvResource==mod.resources.end()||uvResource->second.stride<profile.uvOffset+8||uvResource->second.stride>256)
   throw std::runtime_error("Invalid EFMI float2 UV resource");
  uvStride=static_cast<size_t>(uvResource->second.stride);
  uvBuffer=ExternalMesh::Read(ExternalMesh::ResourceFile(mod,uvResource->second));
  if(uvBuffer.size()!=vertexCount*uvStride)throw std::runtime_error("EFMI UV buffer count mismatch");
 }
 auto indexResource=mod.resources.find(draw->indexResource);
 if(indexResource==mod.resources.end())throw std::runtime_error("Missing EFMI index resource");
 const auto format=EfmiBridge::Lower(indexResource->second.format);
 const size_t width=format=="dxgi_format_r16_uint"?2:format=="dxgi_format_r32_uint"?4:0;
 if(!width)throw std::runtime_error("Unsupported EFMI index format");
 auto indices=ExternalMesh::Read(ExternalMesh::ResourceFile(mod,indexResource->second));
 if(indices.size()%width||draw->start>indices.size()/width||draw->count>indices.size()/width-draw->start)
  throw std::runtime_error("EFMI draw exceeds index buffer");
 ExternalMesh::Bytes shapeMap,shapeData;
 if(profile.shape){
  const auto stem=positionPath.stem().wstring();
  shapeMap=ExternalMesh::Read(positionPath.parent_path()/(stem+L"_merged_map.buf"));
  shapeData=ExternalMesh::Read(positionPath.parent_path()/(stem+L"_merged_packed_pos_delta.buf"));
  if(shapeMap.size()!=vertexCount*4||shapeData.size()%12)
   throw std::runtime_error("Expected a single-slot EFMI position shape map");
 }
 Geometry geometry;
 geometry.triangles.reserve(static_cast<size_t>(draw->count));
 std::map<size_t,int32_t> remap;
 for(size_t i=0;i<draw->count;++i){
  const size_t offset=(static_cast<size_t>(draw->start)+i)*width;
  const int64_t index=int64_t(width==4?ExternalMesh::Get<uint32_t>(indices,offset):ExternalMesh::Get<uint16_t>(indices,offset))+draw->base;
  if(index<0||static_cast<uint64_t>(index)>=vertexCount)
   throw std::runtime_error("EFMI index outside position buffer");
  auto [target,inserted]=remap.try_emplace(static_cast<size_t>(index),static_cast<int32_t>(geometry.positions.size()));
  if(inserted){
   if(geometry.positions.size()>=65535)throw std::runtime_error("EFMI draw exceeds 65535 unique vertices");
   geometry.positions.push_back(ExternalMesh::Vertex(positions,static_cast<size_t>(index)*stride));
   if(uvStride){
    size_t uvAt=static_cast<size_t>(index)*uvStride+profile.uvOffset;
    std::array<float,2> uv{ExternalMesh::Get<float>(uvBuffer,uvAt),ExternalMesh::Get<float>(uvBuffer,uvAt+4)};
    if(!std::isfinite(uv[0])||!std::isfinite(uv[1])||std::abs(uv[0])>10000||std::abs(uv[1])>10000)
     throw std::runtime_error("EFMI UV data is not a supported float2 stream");
    geometry.uv.push_back(uv);
   }
   if(profile.shape){
    auto delta=ExternalMesh::Get<int32_t>(shapeMap,static_cast<size_t>(index)*4);
    if(delta<-1||(delta>=0&&static_cast<size_t>(delta)>=shapeData.size()/12))
     throw std::runtime_error("EFMI shape index outside delta buffer");
    geometry.deltas.push_back(delta<0?ExternalMesh::Vec{}:ExternalMesh::Vertex(shapeData,static_cast<size_t>(delta)*12));
   }
  }
  geometry.triangles.push_back(target->second);
 }
 if(profile.previewScale){
  auto low=geometry.positions.front(),high=low;
  for(const auto& point:geometry.positions)for(int axis=0;axis<3;++axis){
   low[axis]=std::min(low[axis],point[axis]);high[axis]=std::max(high[axis],point[axis]);
  }
  geometry.deltas.reserve(geometry.positions.size());
  for(const auto& point:geometry.positions){
   ExternalMesh::Vec delta{};
   for(int axis=0;axis<3;++axis)delta[axis]=(point[axis]-(low[axis]+high[axis])*0.5f)*((*profile.previewScale)[axis]-1);
   geometry.deltas.push_back(delta);
  }
 }
 return geometry;
}
}
