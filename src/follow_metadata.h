#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <algorithm>

// Metadata inspection only: no field values, native offsets, detours or
// invocation of any discovered method. Exported IL2CPP API names are verified
// in this client's GameAssembly and match upstream Host's resolver approach.
namespace FollowMetadata {
struct Api {
 using Pointer=void*(__fastcall*)(void*);
 using Text=const char*(__fastcall*)(void*);
 using Next=void*(__fastcall*)(void*,void**);
 using Count=uint32_t(__fastcall*)(void*);
 using Parameter=const void*(__fastcall*)(void*,uint32_t);
 Pointer objectClass=nullptr,parent=nullptr,fieldType=nullptr,returnType=nullptr;
 Text className=nullptr,classNamespace=nullptr,methodName=nullptr,fieldName=nullptr,typeName=nullptr;
 Next methods=nullptr,fields=nullptr;
 Count count=nullptr;Parameter parameter=nullptr;
 void(__fastcall* free)(void*)=nullptr;
 bool Load(){
  auto module=GetModuleHandleW(L"GameAssembly.dll");if(!module)return false;
  objectClass=reinterpret_cast<Pointer>(GetProcAddress(module,"il2cpp_object_get_class"));
  parent=reinterpret_cast<Pointer>(GetProcAddress(module,"il2cpp_class_get_parent"));
  className=reinterpret_cast<Text>(GetProcAddress(module,"il2cpp_class_get_name"));
  classNamespace=reinterpret_cast<Text>(GetProcAddress(module,"il2cpp_class_get_namespace"));
  methods=reinterpret_cast<Next>(GetProcAddress(module,"il2cpp_class_get_methods"));
  fields=reinterpret_cast<Next>(GetProcAddress(module,"il2cpp_class_get_fields"));
  methodName=reinterpret_cast<Text>(GetProcAddress(module,"il2cpp_method_get_name"));
  fieldName=reinterpret_cast<Text>(GetProcAddress(module,"il2cpp_field_get_name"));
  fieldType=reinterpret_cast<Pointer>(GetProcAddress(module,"il2cpp_field_get_type"));
  returnType=reinterpret_cast<Pointer>(GetProcAddress(module,"il2cpp_method_get_return_type"));
  typeName=reinterpret_cast<Text>(GetProcAddress(module,"il2cpp_type_get_name"));
  count=reinterpret_cast<Count>(GetProcAddress(module,"il2cpp_method_get_param_count"));
  parameter=reinterpret_cast<Parameter>(GetProcAddress(module,"il2cpp_method_get_param"));
  free=reinterpret_cast<decltype(free)>(GetProcAddress(module,"il2cpp_free"));
  return objectClass&&parent&&className&&classNamespace&&methods&&fields&&methodName&&fieldName&&fieldType&&returnType&&typeName&&count&&parameter&&free;
 }
 std::string Type(void* type){
  if(!type)return "?";const char* text=typeName(type);if(!text)return "?";
  std::string result(text);free(const_cast<char*>(text));return result;
 }
 template<class Sink> void Dump(void* object,const char* label,Sink log,int& budget){
  if(!object){log(std::string("[follow-research] ")+label+" unavailable");return;}
  auto klass=objectClass(object);
  for(int depth=0;klass&&depth<3&&budget>0;++depth,klass=parent(klass)){
   const char* ns=classNamespace(klass),*name=className(klass);
   if(ns&&std::string(ns).starts_with("System"))break;
   log(std::string("[follow-research] ")+label+" class="+(ns?ns:"")+"."+(name?name:"?")+" depth="+std::to_string(depth));
   void* iterator=nullptr;int visited=0;
   while(budget>0&&visited++<256){auto method=methods(klass,&iterator);if(!method)break;--budget;
    const auto parameters=count(method);const char* methodText=methodName(method);
    std::string line="  method "+Type(returnType(method))+" "+(methodText?methodText:"?")+"(";
    for(uint32_t i=0;i<std::min(parameters,16u);++i){if(i)line+=", ";line+=Type(const_cast<void*>(parameter(method,i)));}
    if(parameters>16)line+=", ...";line+=")";log(line);
   }
   iterator=nullptr;visited=0;
   while(budget>0&&visited++<192){auto field=fields(klass,&iterator);if(!field)break;--budget;
    const char* fieldText=fieldName(field);log("  field "+Type(fieldType(field))+" "+(fieldText?fieldText:"?"));
   }
  }
  if(budget<=0)log("[follow-research] output limit reached");
 }
};
}
