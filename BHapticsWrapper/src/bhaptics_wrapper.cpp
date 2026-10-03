#include "bhaptics_wrapper.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

namespace
{
    // ---- Function pointer types, matching library.h (plain C exports) ----
    using FnRegistryAndInit   = bool (*)(const char* sdkAPIKey, const char* workspaceId, const char* initJson);
    using FnWsIsConnected     = bool (*)();
    using FnWsClose           = void (*)();
    using FnPlay              = int  (*)(const char* key);
    using FnPlayParam         = int  (*)(const char* key, int requestId, float intensity, float duration, float angleX, float offsetY);
    using FnPlayWithoutResult = void (*)(const char* key, int requestId, float intensity, float duration, float angleX, float offsetY);
    using FnPlayLoop          = int  (*)(const char* key, int requestId, float intensity, float duration, float angleX, float offsetY, int interval, int maxCount);
    using FnStop              = bool (*)(int requestId);
    using FnStopByEventId     = bool (*)(const char* eventId);
    using FnStopAll           = bool (*)();
    using FnIsDeviceConnected = bool (*)(int position);
    using FnIsPlaying         = bool (*)();
    using FnIsPlayingByEvent  = bool (*)(const char* eventId);
    using FnIsPlayingByReq    = bool (*)(int requestId);

    struct Api
    {
        FnRegistryAndInit   registryAndInit   = nullptr;
        FnWsIsConnected     wsIsConnected     = nullptr;
        FnWsClose           wsClose           = nullptr;
        FnPlay              play              = nullptr;
        FnPlayParam         playParam         = nullptr;
        FnPlayWithoutResult playWithoutResult = nullptr;
        FnPlayLoop          playLoop          = nullptr;
        FnStop              stop              = nullptr;
        FnStopByEventId     stopByEventId     = nullptr;
        FnStopAll           stopAll           = nullptr;
        FnIsDeviceConnected isDeviceConnected = nullptr;
        FnIsPlaying         isPlaying         = nullptr;
        FnIsPlayingByEvent  isPlayingByEvent  = nullptr;
        FnIsPlayingByReq    isPlayingByReq    = nullptr;
    };

    Api               g_api;
    HMODULE           g_dll = nullptr;
    std::atomic<bool> g_ready{ false };   // all pointers resolved, app registered
    std::mutex        g_initMutex;        // guards Initialize/Shutdown only
    std::string       g_lastError;

    // Directory of the module (DLL or EXE) this code is linked into.
    std::wstring OwnModuleDirectory()
    {
        HMODULE self = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&OwnModuleDirectory), &self))
            return L"";

        wchar_t path[MAX_PATH] = {};
        const DWORD len = GetModuleFileNameW(self, path, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
            return L"";

        std::wstring dir(path, len);
        const auto slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? L"" : dir.substr(0, slash + 1);
    }

    template <typename T>
    bool Resolve(T& target, const char* name)
    {
        target = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(g_dll, name)));
        if (target)
            return true;
        g_lastError = std::string("export not found in bhaptics_library.dll: ") + name;
        return false;
    }

    bool ResolveAll()
    {
        return Resolve(g_api.registryAndInit,   "registryAndInit")
            && Resolve(g_api.wsIsConnected,     "wsIsConnected")
            && Resolve(g_api.wsClose,           "wsClose")
            && Resolve(g_api.play,              "play")
            && Resolve(g_api.playParam,         "playParam")
            && Resolve(g_api.playWithoutResult, "playWithoutResult")
            && Resolve(g_api.playLoop,          "playLoop")
            && Resolve(g_api.stop,              "stop")
            && Resolve(g_api.stopByEventId,     "stopByEventId")
            && Resolve(g_api.stopAll,           "stopAll")
            && Resolve(g_api.isDeviceConnected, "isbHapticsConnected")
            && Resolve(g_api.isPlaying,         "isPlaying")
            && Resolve(g_api.isPlayingByEvent,  "isPlayingByEventId")
            && Resolve(g_api.isPlayingByReq,    "isPlayingByRequestId");
    }

    inline bool Ready() { return g_ready.load(std::memory_order_acquire); }
    inline const char* Safe(const char* s) { return s ? s : ""; }
}

namespace bh
{
    InitStatus Initialize(const char* workspaceId, const char* sdkApiKey, const char* defaultConfigJson, const wchar_t* dllPath)
    {
        std::lock_guard<std::mutex> lock(g_initMutex);

        if (Ready())
            return InitStatus::AlreadyInitialized;

        g_lastError.clear();

        if (!g_dll)
        {
            const std::wstring path = dllPath ? std::wstring(dllPath) : OwnModuleDirectory() + L"bhaptics_library.dll";

            // LOAD_WITH_ALTERED_SEARCH_PATH: dependencies of the bHaptics DLL are
            // searched in its own folder first, not in the game's folder.
            g_dll = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!g_dll)
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), " (Win32 error %lu)", GetLastError());
                g_lastError = "could not load bhaptics_library.dll" + std::string(buf);
                return InitStatus::DllNotFound;
            }
        }

        if (!ResolveAll())
        {
            g_api = Api{};
            return InitStatus::ExportMissing;
        }

        // Note the argument order of the SDK: (apiKey, workspaceId, json).
        const bool connected = g_api.registryAndInit(Safe(sdkApiKey), Safe(workspaceId), Safe(defaultConfigJson));
        g_ready.store(true, std::memory_order_release);

        if (!connected)
        {
            g_lastError = "registered, but bHaptics Player not reachable (is it running?)";
            return InitStatus::NotConnected;
        }
        return InitStatus::Ok;
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_initMutex);
        if (!Ready())
            return;

        g_ready.store(false, std::memory_order_release);
        g_api.stopAll();
        g_api.wsClose();

        // The DLL is intentionally NOT freed: the bHaptics library runs its own
        // websocket thread, and unloading it while that thread winds down can crash
        // the host process. Keeping it loaded also makes re-initialization after a
        // Northstar plugin reload cheap (LoadLibrary returns the same handle).
    }

    bool IsLoaded()                      { return Ready(); }
    bool IsConnected()                   { return Ready() && g_api.wsIsConnected(); }
    bool IsDeviceConnected(Position pos) { return Ready() && g_api.isDeviceConnected(static_cast<int>(pos)); }

    int Play(const char* eventName)
    {
        if (!Ready() || !eventName) return 0;
        return g_api.play(eventName);
    }

    int PlayParam(const char* eventName, float intensity, float duration, float angleX, float offsetY)
    {
        if (!Ready() || !eventName) return 0;
        return g_api.playParam(eventName, 0, intensity, duration, angleX, offsetY);
    }

    void PlayParamNoResult(const char* eventName, float intensity, float duration, float angleX, float offsetY)
    {
        if (!Ready() || !eventName) return;
        g_api.playWithoutResult(eventName, 0, intensity, duration, angleX, offsetY);
    }

    int PlayLoop(const char* eventName, float intensity, float duration, float angleX, float offsetY, int intervalMs, int maxCount)
    {
        if (!Ready() || !eventName) return 0;
        return g_api.playLoop(eventName, 0, intensity, duration, angleX, offsetY, intervalMs, maxCount);
    }

    bool Stop(int requestId)              { return Ready() && g_api.stop(requestId); }
    bool StopEvent(const char* eventName) { return Ready() && eventName && g_api.stopByEventId(eventName); }
    bool StopAll()                        { return Ready() && g_api.stopAll(); }

    bool IsPlaying()                           { return Ready() && g_api.isPlaying(); }
    bool IsPlayingEvent(const char* eventName) { return Ready() && eventName && g_api.isPlayingByEvent(eventName); }
    bool IsPlayingRequest(int requestId)       { return Ready() && g_api.isPlayingByReq(requestId); }

    const char* LastError() { return g_lastError.c_str(); }
}
