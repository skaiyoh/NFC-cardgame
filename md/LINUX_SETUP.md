# Linux Setup

This project is documented for Linux first. The commands here are written for Debian, Ubuntu, and Raspberry Pi OS, but the build itself is standard C, CMake, Raylib, and SQLite.

## Install System Dependencies

Install the compiler toolchain, CMake, SQLite, and the native libraries Raylib needs:

```bash
sudo apt update
sudo apt install build-essential git cmake pkg-config libsqlite3-dev sqlite3 \
  libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev \
  libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev
```

## Install Raylib

If your distro already packages Raylib, prefer that. Otherwise build and install it from source:

```bash
git clone https://github.com/raysan5/raylib.git /tmp/raylib
cmake -S /tmp/raylib -B /tmp/raylib/build
cmake --build /tmp/raylib/build -j"$(nproc)"
sudo cmake --install /tmp/raylib/build
sudo ldconfig
pkg-config --modversion raylib
```

Official Raylib Linux guide:
https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux

## Configure And Build

From the project root:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

If `cardgame.db` is missing or you want a clean reset:

```bash
cmake --build build --target init-db
```

## Run The Game

Run from the project root so relative asset paths resolve correctly:

```bash
./build/cardgame
```

For NFC hardware, set the Linux serial devices explicitly:

```bash
export NFC_PORT_P1=/dev/ttyACM0
export NFC_PORT_P2=/dev/ttyACM1
./build/cardgame
```

For single-Arduino testing:

```bash
export NFC_PORT=/dev/ttyACM0
./build/cardgame
```

## Find And Access Serial Devices

Most Linux serial devices will appear as `/dev/ttyACM*` or `/dev/ttyUSB*`:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

If your user cannot open the device, add it to the serial-access group and log in again:

```bash
sudo usermod -aG dialout "$USER"
```

## Raspberry Pi Notes

- The same CMake build works on Raspberry Pi OS.
- If you launch the game locally on the Pi but want to watch logs remotely, use line-buffered output:

```bash
stdbuf -oL -eL ./build/cardgame > game.log 2>&1
tail -f ~/NFC-cardgame/game.log
```

## Related Docs

- [README](../README.md)
- [Card Data Guide](CARD_DATA_GUIDE.md)
