#include <Windows.h>
// Deterministic optional-loader failure for lifecycle tests. Never packaged.
extern "C" __declspec(dllexport) int __cdecl Inject(DWORD,const wchar_t*,int){return 500;}
