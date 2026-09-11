# Building Zen

[Back to README](../README.md)

Zen is built with Python 3 and [PlatformIO Core](https://platformio.org/install/cli).
Clone the repository and install PlatformIO:

```sh
git clone https://github.com/MJCopper/MeshCore-Zen.git
cd MeshCore-Zen
python3 -m pip install --upgrade platformio
```

## Firmware

Build one display target directly:

```sh
pio run -e WioTrackerL1_companion_solo_dual
pio run -e WioTrackerL1Eink_companion_solo_dual
```

The UF2 is written to `.pio/build/<target>/firmware.uf2` and the BLE-DFU package
to `.pio/build/<target>/firmware.zip`.

For release-style filenames, build both targets through the repository script:

```sh
FIRMWARE_VERSION=vX.Y.Z sh build.sh build-zen-firmwares
```

Release files are written to `out/`. The script recreates that directory at the
start of every invocation, so copy any files you want to retain before running
it again.

## Tests

Run the host-side test suite before building firmware:

```sh
pio test -e native
```

GitHub Actions runs the same tests and builds both Zen display targets. Release
tags beginning with `v` create a draft release containing UF2 and BLE-DFU files.
