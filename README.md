# BHapticsWrapper

Thin C++ wrapper around the bHaptics SDK2 runtime (`bhaptics_library.dll`).
Loads the DLL at runtime, exposes a small `bh::` API, and does nothing (instead
of crashing) when the DLL or the bHaptics Player is missing.

Intended to be linked into the Titanfall 2 VR Northstar plugin, but has no
Northstar or game dependencies.

## Layout

```
BHapticsWrapper.sln
BHapticsWrapper/          static library (BHapticsWrapper.lib)
  include/bhaptics_wrapper.h
  src/bhaptics_wrapper.cpp
WrapperTest/              console app to test against the bHaptics Player
  src/main.cpp
lib/bhaptics/             SDK files (DLL + original header)
```

Visual Studio 2022 (v143), x64 only, C++17, static CRT (`/MT`, `/MTd`).
The plugin that links the library must use the same runtime setting.

## Testing

1. Start the bHaptics Player and connect a device.
2. Build the solution (Debug|x64 or Release|x64).
3. Run from `bin\x64\<Config>\`:

```
WrapperTest.exe <workspaceId> <sdkApiKey> [eventName]
```

Credentials can also be set via `BHAPTICS_WORKSPACE_ID` and `BHAPTICS_API_KEY`.
With an event name, it plays the event once, then sweeps it around the vest in
45-degree steps via `PlayParam` (same call the damage handler will use).

## API

| Function | Notes |
|---|---|
| `Initialize(workspaceId, apiKey, json = "", dllPath = nullptr)` | `NotConnected` is not fatal: playback starts once the Player is reachable |
| `Shutdown()` | stops all, closes the websocket; the DLL stays loaded on purpose |
| `IsLoaded()`, `IsConnected()`, `IsDeviceConnected(Position)` | |
| `Play(event)` | returns request id |
| `PlayParam(event, intensity, duration, angleX, offsetY)` | angleX in degrees around the body, offsetY -0.5..0.5 |
| `PlayParamNoResult(...)` | fire-and-forget, for high-frequency events like recoil |
| `PlayLoop(event, ..., intervalMs, maxCount)` | e.g. low-health heartbeat |
| `Stop(id)`, `StopEvent(event)`, `StopAll()` | |
| `IsPlaying()`, `IsPlayingEvent(event)`, `IsPlayingRequest(id)` | |
| `LastError()` | reason for the last load/init problem |
