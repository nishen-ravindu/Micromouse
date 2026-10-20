# MMS simulator build (fixed)

This folder is deliberately separate from `mouse-firmware/`.

**Do not compile the hardware `mouse-firmware/API.cpp`, `motors.cpp`, `sensors.cpp`, or `.ino` in MMS.** Those files require Arduino/ESP32 libraries.

For MMS on Windows with MinGW, set the algorithm **Directory** to this `simulator` folder and use:

```text
g++ -std=c++17 Main.cpp API.cpp ../mouse-firmware/floodfill.cpp -I../mouse-firmware -o mouse.exe
```

Run command:

```text
mouse.exe
```

For Linux/macOS, build similarly but output `mouse` and run `./mouse`.

The simulator `API.cpp` talks to MMS over stdin/stdout. The physical robot's `mouse-firmware/API.cpp` talks to motors and sensors. Both expose the same `API.h` interface, so `floodfill.cpp` is shared unchanged.
