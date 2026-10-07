#define NOMINMAX
#include "follow_metadata.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
namespace {
int klass=1,method=2,field=3,type=4,allocations=0,frees=0;
void* __fastcall Class(void*){return &klass;}
void* __fastcall Parent(void*){return nullptr;}
const char* __fastcall Name(void* object){return object==&klass?"FollowCandidate":object==&method?"PauseFollow":"followState";}
const char* __fastcall Namespace(void*){return "Beyond.Gameplay.Core";}
void* __fastcall Method(void*,void** iterator){if(*iterator)return nullptr;*iterator=&method;return &method;}
void* __fastcall Field(void*,void** iterator){if(*iterator)return nullptr;*iterator=&field;return &field;}
void* __fastcall Type(void*){return &type;}
uint32_t __fastcall Count(void*){return 1;}
const void* __fastcall Parameter(void*,uint32_t){return &type;}
const char* __fastcall TypeName(void*){++allocations;char* value=static_cast<char*>(std::malloc(32));strcpy_s(value,32,"System.Boolean");return value;}
void __fastcall Free(void* value){++frees;std::free(value);}
}
int main(){
 FollowMetadata::Api api;api.objectClass=Class;api.parent=Parent;api.className=Name;api.classNamespace=Namespace;api.methods=Method;api.fields=Field;
 api.methodName=Name;api.fieldName=Name;api.fieldType=Type;api.returnType=Type;api.typeName=TypeName;api.count=Count;api.parameter=Parameter;api.free=Free;
 std::vector<std::string> lines;auto sink=[&](const std::string& s){lines.push_back(s);};int budget=10;
 api.Dump(&klass,"partner",sink,budget);
 bool ok=lines.size()==3&&lines[1].find("PauseFollow(System.Boolean)")!=std::string::npos&&lines[2].find("field System.Boolean followState")!=std::string::npos&&allocations==frees;
 lines.clear();budget=1;api.Dump(&klass,"bounded",sink,budget);ok=ok&&budget==0&&lines.back().find("limit reached")!=std::string::npos&&allocations==frees;
 lines.clear();budget=10;api.Dump(nullptr,"missing",sink,budget);ok=ok&&lines.size()==1&&lines[0].find("unavailable")!=std::string::npos;
 std::puts(ok?"PASS: metadata signatures, output limit, null objects and allocated-name cleanup":"FAIL: metadata inspection");return ok?0:1;
}
