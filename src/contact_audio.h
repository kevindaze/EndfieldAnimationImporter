#pragma once
#include "external_mesh.h"

namespace ContactAudio {
inline ExternalMesh::Bytes WithVolume(ExternalMesh::Bytes wave,int percent){
 using ExternalMesh::Get;
 using ExternalMesh::Put;
 if(wave.size()<12||std::memcmp(wave.data(),"RIFF",4)||std::memcmp(wave.data()+8,"WAVE",4)||percent<0||percent>100)
  throw std::runtime_error("Invalid contact WAVE");
 bool pcm16=false;
 size_t samples=0,sampleBytes=0;
 for(size_t pos=12;wave.size()-pos>=8;){
  const auto size=static_cast<size_t>(Get<uint32_t>(wave,pos+4));
  const auto payload=pos+8;
  if(size+(size&1)>wave.size()-payload)throw std::runtime_error("Invalid contact WAVE chunk");
  if(size>=16&&!std::memcmp(wave.data()+pos,"fmt ",4))
   pcm16=Get<uint16_t>(wave,payload)==1&&Get<uint16_t>(wave,payload+14)==16;
  if(!std::memcmp(wave.data()+pos,"data",4)){samples=payload;sampleBytes=size;}
  pos=payload+size+(size&1);
 }
 if(!pcm16||!samples||!sampleBytes||sampleBytes%2)throw std::runtime_error("Contact WAVE must use 16-bit PCM");
 for(size_t pos=samples;pos<samples+sampleBytes;pos+=2){
  const auto sample=Get<int16_t>(wave,pos);
  Put<int16_t>(wave,pos,static_cast<int16_t>(std::lround(sample*percent/100.f)));
 }
 return wave;
}
}
