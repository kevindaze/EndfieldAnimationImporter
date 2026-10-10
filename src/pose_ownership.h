#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <string>
namespace EaiPose {
// Module-local ownership only. It does not arbitrate other modules or game animation.
class Ownership {
 struct Entry {std::string owner;uint64_t token;};
 std::mutex mutex_;std::map<const void*,Entry> entries_;uint64_t sequence_=0;
public:
 uint64_t Acquire(const void* root,const char* owner){if(!root||!owner||!*owner)return 0;std::lock_guard lock(mutex_);if(entries_.contains(root)||sequence_==std::numeric_limits<uint64_t>::max())return 0;auto token=++sequence_;entries_.emplace(root,Entry{owner,token});return token;}
 int Owns(const void* root,const char* owner,uint64_t token){if(!root||!owner||!token)return 0;std::lock_guard lock(mutex_);auto it=entries_.find(root);return it!=entries_.end()&&it->second.owner==owner&&it->second.token==token;}
 int Release(const void* root,const char* owner,uint64_t token){if(!root||!owner||!token)return 0;std::lock_guard lock(mutex_);auto it=entries_.find(root);if(it==entries_.end()||it->second.owner!=owner||it->second.token!=token)return 0;entries_.erase(it);return 1;}
 // Keep the sequence so stale tokens cannot regain ownership after a restart.
 void Clear(){std::lock_guard lock(mutex_);entries_.clear();}
};
struct Api {uint32_t version;uint64_t (*acquire)(const void*,const char*);int (*owns)(const void*,const char*,uint64_t);int (*release)(const void*,const char*,uint64_t);};
inline Ownership ownership;
inline const Api* GetApi(){static const Api api{1,[](const void* root,const char* owner){return ownership.Acquire(root,owner);},[](const void* root,const char* owner,uint64_t token){return ownership.Owns(root,owner,token);},[](const void* root,const char* owner,uint64_t token){return ownership.Release(root,owner,token);}};return &api;}
}
