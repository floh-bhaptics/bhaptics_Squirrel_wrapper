#define _CRT_SECURE_NO_WARNINGS
// WrapperTest - quick smoke test for bhaptics_wrapper outside of the game.
//
// Usage:
//   WrapperTest.exe <workspaceId> <sdkApiKey> [eventName]
//
// Credentials can also come from the environment variables
// BHAPTICS_WORKSPACE_ID and BHAPTICS_API_KEY.
// The bHaptics Player must be running and bhaptics_library.dll must sit next to
// WrapperTest.exe (the post-build step copies it there).

#include "bhaptics_wrapper.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
    const char* StatusName(bh::InitStatus s)
    {
        switch (s)
        {
            case bh::InitStatus::Ok:                 return "Ok";
            case bh::InitStatus::AlreadyInitialized: return "AlreadyInitialized";
            case bh::InitStatus::NotConnected:       return "NotConnected";
            case bh::InitStatus::DllNotFound:        return "DllNotFound";
            case bh::InitStatus::ExportMissing:      return "ExportMissing";
        }
        return "?";
    }

    void Sleep(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

    void WaitWhilePlaying(int timeoutMs = 5000)
    {
        for (int waited = 0; bh::IsPlaying() && waited < timeoutMs; waited += 50)
            Sleep(50);
    }
}

int main(int argc, char** argv)
{
    const char* workspaceId = argc > 1 ? argv[1] : std::getenv("BHAPTICS_WORKSPACE_ID");
    const char* apiKey      = argc > 2 ? argv[2] : std::getenv("BHAPTICS_API_KEY");
    const char* eventName   = argc > 3 ? argv[3] : nullptr;

    if (!workspaceId || !apiKey)
    {
        std::printf("Usage: WrapperTest.exe <workspaceId> <sdkApiKey> [eventName]\n");
        return 1;
    }

    const bh::InitStatus status = bh::Initialize(workspaceId, apiKey);
    std::printf("Initialize: %s %s\n", StatusName(status), bh::LastError());
    if (status == bh::InitStatus::DllNotFound || status == bh::InitStatus::ExportMissing)
        return 2;

    std::printf("Waiting for bHaptics Player");
    for (int i = 0; i < 50 && !bh::IsConnected(); ++i)
    {
        std::printf(".");
        Sleep(200);
    }
    std::printf(bh::IsConnected() ? " connected\n" : " NOT connected\n");

    std::printf("Vest: %d  ForearmL: %d  ForearmR: %d  Head: %d\n",
                bh::IsDeviceConnected(bh::Position::Vest),
                bh::IsDeviceConnected(bh::Position::ForearmL),
                bh::IsDeviceConnected(bh::Position::ForearmR),
                bh::IsDeviceConnected(bh::Position::Head));

    if (eventName)
    {
        std::printf("Play(\"%s\") -> request %d\n", eventName, bh::Play(eventName));
        Sleep(200);
        WaitWhilePlaying();

        // Directional sweep around the vest, as the damage handler will use it.
        for (int angle = 0; angle < 360; angle += 45)
        {
            const int id = bh::PlayParam(eventName, 1.0f, 1.0f, static_cast<float>(angle), 0.0f);
            std::printf("PlayParam angle %3d -> request %d\n", angle, id);
            Sleep(200);
            WaitWhilePlaying();
            Sleep(300);
        }
    }
    else
    {
        std::printf("No event name given, skipping playback tests.\n");
    }

    bh::Shutdown();
    std::printf("Done.\n");
    return 0;
}
