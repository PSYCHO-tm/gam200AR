KopiTwin desktop shell (M1G05 viewer)

Window + GL 4.3 core context + input + the fixed-timestep loop. All game code lives in source/ and is shared with the Android shell.

Build (needs SDL2; glad is vendored in third_party/glad):
    cmake -S Platform/desktop -B build-desktop [-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake]
    cmake --build build-desktop
    build-desktop/kopitwin_desktop

Controls:
    Left click   pick equipment (yellow pulse = selected)
    1 - 9        run the Nth interaction of the selected equipment; the timed event prints as "[event] ..."
    E            debug: show a fake assessment error on the selected equipment (red for 3 s)
    Esc          quit
The window title shows the selected equipment and its interactions (no on-screen text until FreeType lands).

No-input check (CI friendly): kopitwin_desktop --selftest frame.ppm
    picks the kettle, rejects a wrong action, presses pour_water, writes one frame, exits 0 only if all of it worked.

Regenerate the loader (only if you need another GL version):
    pip install glad2
    glad --api gl:core=4.3 --out-path Platform/desktop/third_party/glad c
