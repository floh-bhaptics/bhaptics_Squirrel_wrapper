# lib/bhaptics/

bHaptics SDK2 files, kept here so the source tree is self-contained and you
always know which version you built against.

  bhaptics_library.dll   <- bHaptics runtime DLL (x64), required at runtime
  library.h              <- original SDK header, for reference only
                            (NOT included in the build - we use GetProcAddress)

No import library (.lib) is needed: the wrapper loads the DLL with LoadLibrary.

---

## Runtime location

bhaptics_library.dll must sit next to the module the wrapper is linked into
(or pass a full path to bh::Initialize). For the Titanfall 2 plugin that is:

  <Titanfall2>\TF2VR\plugins\
      TF2VR_bHaptics.dll      <- Northstar plugin (links BHapticsWrapper.lib)
      bhaptics_library.dll    <- copy from here

For WrapperTest the post-build step copies it to bin\x64\<Config>\ automatically.

Note: the DLL depends on the Visual C++ 2015-2022 runtime (MSVCP140.dll,
VCRUNTIME140.dll, VCRUNTIME140_1.dll).

---

## Export names

This SDK version exports plain C names (checked with dumpbin /EXPORTS):

  registryAndInit, registryAndInitHost, reInitMessage, wsIsConnected, wsClose,
  play, playParam, playWithoutResult, playWithStartTime, playLoop, playDot,
  playWaveform, playPath, getEventTime, pause, resume, stop, stopByEventId,
  stopAll, isbHapticsConnected, isPlaying, isPlayingByEventId,
  isPlayingByRequestId, isPlayerRunning, isPlayerInstalled, launchPlayer,
  getDeviceInfoJson, getHapticMappingsJson, ping, pingAll, swapPosition,
  setDeviceVsm, bHapticsGetHapticMessage, bHapticsGetHapticMappings

The wrapper resolves 14 of them (see ResolveAll() in src\bhaptics_wrapper.cpp).
If bh::Initialize returns ExportMissing after an SDK update, bh::LastError()
names the missing export; run in a VS Developer Command Prompt:

  dumpbin /EXPORTS bhaptics_library.dll
