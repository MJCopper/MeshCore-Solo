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
pio run -e WioTrackerL1_Zen_OLED
pio run -e WioTrackerL1_Zen_E-INK
```

The versioned UF2 files are written directly to:

```text
.pio/build/WioTrackerL1_Zen_OLED/WioTrackerL1_Zen_OLED.<version>.uf2
.pio/build/WioTrackerL1_Zen_E-INK/WioTrackerL1_Zen_E-INK.<version>.uf2
```

The version is taken from `FIRMWARE_VERSION` in
`examples/companion_radio/MyMesh.h`, without its leading `v`. PlatformIO also
creates `firmware.zip` in each target directory for BLE DFU.

For release-style filenames, build both targets through the repository script:

```sh
bash build.sh build-zen-firmwares
```

Release files are written to `out/` using the same UF2 names; BLE-DFU packages
use `<target>.<version>.ota.zip`. The script recreates `out/` at the start of
every invocation, so copy any files you want to retain before running it again.

## Tests

Run the host-side test suite before building firmware:

```sh
pio test -e native
```

GitHub Actions runs the same tests and builds both Zen display targets. Release
tags beginning with `v` create a draft release containing UF2 and BLE-DFU files.
