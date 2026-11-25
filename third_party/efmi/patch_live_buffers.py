"""Apply the EAI file-backed buffer extension to the XXMI 1.2.2 sources."""
import sys
from pathlib import Path

root = Path(sys.argv[1]) / "DirectX11"

def edit(file, old, new, *, count=1):
    path = root / file
    source = path.read_text(encoding="utf-8-sig")
    if source.count(old) != count:
        raise SystemExit(f"Unexpected XXMI source in {file}")
    path.write_text(source.replace(old, new), encoding="utf-8-sig")

edit("CommandList.h", "\tvoid expire(ID3D11Device *mOrigDevice1, ID3D11DeviceContext *mOrigContext1);", """\tvoid expire(ID3D11Device *mOrigDevice1, ID3D11DeviceContext *mOrigContext1);
\tvoid RefreshEaiBuffer(ID3D11DeviceContext *context);
\tFILETIME eai_buffer_write_time{};
\tULONGLONG eai_buffer_next_check = 0;
\tbool eai_buffer_applied = false;
\tID3D11Resource *eai_follow_resource = nullptr;
\tULONGLONG eai_follow_generation = 0;
\tbool eai_follow_applied = false;
\tID3D11Resource *eai_buffer_resource = nullptr;""")

edit("CommandList.cpp", "void CustomResource::LoadBufferFromFile(ID3D11Device *mOrigDevice1)", """// Transient position buffers published by EAI in the same process.
static SRWLOCK eai_follow_lock = SRWLOCK_INIT;
struct EaiFollowBuffer {
    wstring filename;
    std::vector<unsigned char> bytes;
};
static EaiFollowBuffer eai_follow_pending;
static EaiFollowBuffer eai_follow_frame;
static bool eai_follow_dirty = false;
static ULONGLONG eai_follow_frame_generation = 0;
static volatile LONG64 eai_follow_consumed = 0;

static wstring EaiFullPath(const wchar_t *path)
{
    DWORD size = GetFullPathNameW(path, 0, nullptr, nullptr);
    if (!size || size > 32768)
        return {};
    wstring full(size, L'\\0');
    DWORD length = GetFullPathNameW(path, size, &full[0], nullptr);
    if (!length || length >= size)
        return {};
    full.resize(length);
    return full;
}

extern "C" __declspec(dllexport) BOOL WINAPI EaiSubmitMeshFollow(
    const wchar_t *filename, const void *data, UINT size)
{
    if ((!data && size) || (data && (!filename || !size || size > 8 * 1024 * 1024)))
        return FALSE;
    try {
        wstring path = data ? EaiFullPath(filename) : wstring();
        if (data && path.empty())
            return FALSE;
        AcquireSRWLockExclusive(&eai_follow_lock);
        try {
            if (data) {
                auto bytes = static_cast<const unsigned char*>(data);
                eai_follow_pending.bytes.assign(bytes, bytes + size);
            } else {
                eai_follow_pending.bytes.clear();
            }
            eai_follow_pending.filename.swap(path);
            eai_follow_dirty = true;
        } catch (...) {
            ReleaseSRWLockExclusive(&eai_follow_lock);
            return FALSE;
        }
        ReleaseSRWLockExclusive(&eai_follow_lock);
        return TRUE;
    } catch (...) {
        return FALSE;
    }
}

// Present and the immediate context run on the render thread. Freeze one
// position buffer for the next frame so the depth and shading passes agree.
void EaiLatchMeshFollow()
{
    if (!TryAcquireSRWLockExclusive(&eai_follow_lock))
        return;
    if (eai_follow_dirty) {
        eai_follow_frame.filename.swap(eai_follow_pending.filename);
        eai_follow_frame.bytes.swap(eai_follow_pending.bytes);
        ++eai_follow_frame_generation;
        eai_follow_dirty = false;
    }
    ReleaseSRWLockExclusive(&eai_follow_lock);
}

extern "C" __declspec(dllexport) ULONGLONG WINAPI EaiMeshFollowUpdates()
{
    return static_cast<ULONGLONG>(InterlockedCompareExchange64(&eai_follow_consumed, 0, 0));
}

// EAI atomically replaces only its own fixed-size generated files.
// The render thread updates the existing buffer so aliases remain valid.
void CustomResource::RefreshEaiBuffer(ID3D11DeviceContext *context)
{
    if (!resource || !context || filename.empty() || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return;
    const auto& follow = eai_follow_frame;
    bool matches = false;
    if (!follow.bytes.empty()) {
        // Resolve the absolute path only for a candidate basename.
        const auto first = filename.find_last_of(L"/\\\\");
        const auto second = follow.filename.find_last_of(L"/\\\\");
        const auto native_name = filename.c_str() + (first == wstring::npos ? 0 : first + 1);
        const auto follow_name = follow.filename.c_str() + (second == wstring::npos ? 0 : second + 1);
        try {
            matches = !_wcsicmp(native_name, follow_name) &&
                !_wcsicmp(EaiFullPath(filename.c_str()).c_str(), follow.filename.c_str());
        } catch (...) {}
    }
    if (matches) {
        if (eai_follow_applied && resource == eai_follow_resource &&
            eai_follow_generation == eai_follow_frame_generation)
            return;
        D3D11_RESOURCE_DIMENSION type{};
        resource->GetType(&type);
        if (type == D3D11_RESOURCE_DIMENSION_BUFFER) {
            D3D11_BUFFER_DESC desc{};
            static_cast<ID3D11Buffer*>(resource)->GetDesc(&desc);
            if (desc.Usage == D3D11_USAGE_DEFAULT && desc.ByteWidth == follow.bytes.size()) {
                context->UpdateSubresource(resource, 0, nullptr, follow.bytes.data(), 0, 0);
                eai_follow_resource = resource;
                eai_follow_generation = eai_follow_frame_generation;
                eai_follow_applied = true;
                InterlockedIncrement64(&eai_follow_consumed);
                return;
            }
        }
    }
    const auto now = GetTickCount64();
    if (now < eai_buffer_next_check && !eai_follow_applied)
        return;
    eai_buffer_next_check = now + 33;

    D3D11_RESOURCE_DIMENSION type{};
    resource->GetType(&type);
    if (type != D3D11_RESOURCE_DIMENSION_BUFFER)
        return;
    D3D11_BUFFER_DESC desc{};
    static_cast<ID3D11Buffer*>(resource)->GetDesc(&desc);
    const auto sidecar = filename + L".eai-live-buffer";
    WIN32_FILE_ATTRIBUTE_DATA info{};
    const bool exists = GetFileAttributesExW(sidecar.c_str(), GetFileExInfoStandard, &info) != FALSE;
    if (!exists && ((!eai_buffer_applied && !eai_follow_applied) || GetLastError() != ERROR_FILE_NOT_FOUND))
        return;
    if (exists && !eai_follow_applied && eai_buffer_applied && resource == eai_buffer_resource &&
        !CompareFileTime(&info.ftLastWriteTime, &eai_buffer_write_time))
        return;
    const char magic[] = "EAILIVE2";
    constexpr DWORD magic_size = sizeof(magic) - 1;
    constexpr DWORD header_size = magic_size + sizeof(FILETIME);
    if (desc.Usage != D3D11_USAGE_DEFAULT || !desc.ByteWidth || desc.ByteWidth > 64 * 1024 * 1024 ||
        (exists && (info.nFileSizeHigh || info.nFileSizeLow != desc.ByteWidth + header_size)))
        return;

    std::unique_ptr<unsigned char[]> data(new (std::nothrow) unsigned char[desc.ByteWidth]);
    if (!data)
        return;
    HANDLE file = CreateFileW((exists ? sidecar : filename).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD received = 0;
    FILETIME write_time{};
    LARGE_INTEGER size{};
    char header[magic_size]{};
    FILETIME original_time{}, expected_time{};
    WIN32_FILE_ATTRIBUTE_DATA original{};
    const bool source_ready = !exists || GetFileAttributesExW(filename.c_str(), GetFileExInfoStandard, &original);
    if (exists)
        original_time = original.ftLastWriteTime;
    const bool ready = source_ready && GetFileSizeEx(file, &size) && size.QuadPart == desc.ByteWidth + (exists ? header_size : 0) &&
        (!exists || (ReadFile(file, header, magic_size, &received, nullptr) &&
            received == magic_size && memcmp(header, magic, magic_size) == 0 &&
            ReadFile(file, &expected_time, sizeof(expected_time), &received, nullptr) &&
            received == sizeof(expected_time) && !CompareFileTime(&original_time, &expected_time))) &&
        GetFileTime(file, nullptr, nullptr, &write_time) &&
        ReadFile(file, data.get(), desc.ByteWidth, &received, nullptr) &&
        received == desc.ByteWidth;
    CloseHandle(file);
    if (!ready)
        return;
    context->UpdateSubresource(resource, 0, nullptr, data.get(), 0, 0);
    eai_buffer_write_time = write_time;
    eai_buffer_resource = resource;
    eai_buffer_applied = exists;
    eai_follow_applied = false;
}

void CustomResource::LoadBufferFromFile(ID3D11Device *mOrigDevice1)""")

edit("CommandList.cpp", "\t\t\tcustom_resource->Substantiate(mOrigDevice1, bind_flags, misc_flags);",
     "\t\t\tcustom_resource->Substantiate(mOrigDevice1, bind_flags, misc_flags);\n"
     "\t\t\tcustom_resource->RefreshEaiBuffer(mOrigContext1);")

edit("CommandList.h", "extern CommandListVariables command_list_globals;",
     "extern CommandListVariables command_list_globals;\n"
     "void EaiRefreshIniPreviewRegistry();\n"
     "void EaiApplyIniPreviews();\n"
     "void EaiLatchMeshFollow();")
edit("CommandList.cpp", '#include "CommandList.h"',
     '#include "CommandList.h"\n#include <cmath>\n#include <mutex>\n#include <set>')
edit("CommandList.cpp", "std::vector<CommandListVariable*> persistent_variables;", r"""std::vector<CommandListVariable*> persistent_variables;

// Requests can arrive on a panel or host thread. Only the render/config
// thread may access the live variable registry and its values.
static std::mutex eai_ini_mutex;
static std::set<wstring> eai_ini_persistent;
static std::map<wstring, float> eai_ini_pending;

void EaiRefreshIniPreviewRegistry()
{
    std::set<wstring> names;
    for (const auto* variable : persistent_variables)
        names.insert(variable->name);
    std::lock_guard<std::mutex> lock(eai_ini_mutex);
    eai_ini_persistent.swap(names);
}

extern "C" __declspec(dllexport) BOOL WINAPI EaiPreviewIniVariable(
    const wchar_t* ini_namespace, const wchar_t* name, float value)
{
    if (!ini_namespace || !*ini_namespace || !name || !std::isfinite(value))
        return FALSE;
    try {
        const wstring scope(ini_namespace);
        wstring binding(name);
        for (auto& character : binding)
            character = ascii_tolower(character);
        if (!valid_variable_name(binding))
            return FALSE;
        auto key = get_namespaced_var_name_lower(binding, &scope);
        std::lock_guard<std::mutex> lock(eai_ini_mutex);
        if (eai_ini_persistent.find(key) == eai_ini_persistent.end())
            return FALSE;
        eai_ini_pending[key] = value;
        return TRUE;
    } catch (...) {
        return FALSE;
    }
}

void EaiApplyIniPreviews()
{
    std::map<wstring, float> pending;
    {
        std::lock_guard<std::mutex> lock(eai_ini_mutex);
        pending.swap(eai_ini_pending);
    }
    for (const auto& request : pending) {
        auto variable = command_list_globals.find(request.first);
        if (variable != command_list_globals.end() && (variable->second.flags & VariableFlags::PERSIST))
            variable->second.fval = request.second;
    }
}
""")
edit("IniHandler.cpp", "\tParseConstantsSection();", "\tParseConstantsSection();\n\tEaiRefreshIniPreviewRegistry();")
edit("HackerDXGI.cpp", "\tRunCommandList(mHackerDevice, mHackerContext, &G->present_command_list, NULL, false);",
     "\tEaiApplyIniPreviews();\n"
     "\tRunCommandList(mHackerDevice, mHackerContext, &G->present_command_list, NULL, false);")

edit("HackerDXGI.cpp", "\t\tRunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, NULL, true);\n\n\t\tif (profiling)",
     "\t\tif (SUCCEEDED(hr))\n"
     "\t\t\tEaiLatchMeshFollow();\n"
     "\t\tRunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, NULL, true);\n\n\t\tif (profiling)",
     count=2)
