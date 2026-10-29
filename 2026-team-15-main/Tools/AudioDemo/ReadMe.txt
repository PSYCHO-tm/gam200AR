KOPITWIN AUDIO SYSTEM (SDL3)  -  8 sound effects + 1 background music
=====================================================================

FILES
    Core/Header/Audio/audioSystem.hpp     the AudioSystem class
    Core/Source/Audio/audioSystem.cpp
    Assets/Json/Audio.json                the list of sounds (edit this to add / remove sounds)
    Assets/Audio/Sfx/*.wav                8 placeholder beeps (replace with real sounds)
    Assets/Audio/Music/cafe_theme.wav     placeholder 8 second music loop
    Tools/AudioDemo/                      console demo to try everything


STEP 1 - PUT THE SDL3 LIBRARY IN Extern/SDL3   (headers alone are not enough)
------------------------------------------------------------------------------
The headers (include/) only DESCRIBE SDL. The actual code is in SDL3.lib + SDL3.dll.
    1. Go to https://github.com/libsdl-org/SDL/releases
    2. Download  SDL3-devel-<version>-VC.zip   (use the SAME version as the headers you already have)
    3. Open the zip. Inside the SDL3-<version> folder copy  include/  lib/  cmake/  into  Extern/SDL3/
       so you end up with:
           Extern/SDL3/include/SDL3/SDL.h
           Extern/SDL3/lib/x64/SDL3.lib
           Extern/SDL3/lib/x64/SDL3.dll


STEP 2 - BUILD AND RUN THE DEMO (PowerShell / Android Studio Terminal, in the repo root)
---------------------------------------------------------------------------------------
    cmake -S Tools/AudioDemo -B build-audio
    cmake --build build-audio
    build-audio\Debug\audio_demo.exe

Press keys (no ENTER needed):
    1 - 8      play sound effect 1..8. Press 1 then 2 and BOTH play on top of each other.
               A playing sound is never cut off. If all 8 channels are busy, the new sound is skipped.
    9          background music on / off (loops)
    0          stop all sound effects
    p          pause / resume music          + / -   master volume        m   mute
    l          show the 8 channels           r       re-read Audio.json   c   type a command
    h          show the keys                 q       quit

Keys 1-8 follow the order of the "Sfx" list in Audio.json, key 9 is the first "Music" entry.
To change what a key plays, change the order in the file and press r.


STEP 3 - ADD OR REMOVE A SOUND
------------------------------
A) With the JSON file (no recompiling):
    1. Drop the file in Assets/Audio/Sfx/  (must be .wav)
    2. Add ONE line in Assets/Json/Audio.json, inside "Sfx":
           "door": "Audio/Sfx/door.wav",
       or with its own volume:
           "door": { "File": "Audio/Sfx/door.wav", "Volume": 0.5 },
    3. Restart the demo, or press  r  while it is running. It gets the next free key.
    To REMOVE a sound: delete its line and press  r.

B) While the program is running (in the demo press c first, then type the command):
    load door Audio/Sfx/door.wav         audio.Load("door", "Audio/Sfx/door.wav");
    unload door                          audio.Unload("door");
    (a sound added with load takes the first key whose sound was unloaded)

Only .wav works. Convert .mp3 / .ogg to .wav first (e.g. Audacity: File > Export > WAV).


USING IT IN GAME CODE
---------------------
    #include "Audio/audioSystem.hpp"

    core::AudioSystem audio;
    audio.Init("Assets");                  // desktop: folder that holds Audio/ and Json/
    audio.LoadManifest("Json/Audio.json");
    audio.PlayMusic("cafe_theme");         // loops by itself, no Update() needed
    audio.PlaySfx("pour");                 // plays on top of other sounds, returns channel 0..7
                                           // (-1 if all 8 are busy: nothing gets cut off)
    audio.SetSfxVolume(0.8f);
    audio.Shutdown();                      // also done automatically when audio is destroyed

Hooking it to the coffee simulation, for example:
    core::Verdict verdict = sim.OnInteraction(event);
    if (verdict.hasError) audio.PlaySfx("mistake");
    else if (verdict.stepAdvanced) audio.PlaySfx("pour");


ADDING IT TO THE TEAM BUILD (when the team is ready)
----------------------------------------------------
Core/CMakeLists.txt compiles every .cpp in Core/Source, so ci_core will now also need SDL3:
    target_include_directories(ci_core PUBLIC ${CMAKE_SOURCE_DIR}/Extern/SDL3/include)
    target_link_libraries(ci_core PUBLIC <the SDL3 library>)

ANDROID
    - In the APK, Init("") is right: SDL reads relative paths like "Audio/Sfx/pour.wav"
      straight from the app's assets folder (Assets/ must be packed as the APK assets).
    - Android needs SDL3 built for Android (the SDL3-<version>.aar from the same release page),
      and SDL3 normally expects the app to start from SDL's own SDLActivity.
      Agree with whoever owns the Android platform code how SDL3 is set up there.
