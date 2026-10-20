# Micromouse v4 - simulator/hardware separation fix

## Important

There are TWO different API implementations:

- `simulator/API.cpp` = for MMS only; no Arduino headers.
- `mouse-firmware/API.cpp` = for the physical ESP32 only; uses motors/sensors.

The shared algorithm is `mouse-firmware/floodfill.cpp`.

If MMS reports `Arduino.h: No such file or directory`, it means MMS is compiling the hardware API by mistake. Point the MMS configuration at the `simulator` directory and use the build command in `simulator/README.md`.
