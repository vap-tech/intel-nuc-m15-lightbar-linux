# Reverse Notes

## Result

The current Intel/Uniwill front light bar can be controlled from Linux without Windows runtime dependencies.

Working path:
- WMI GUID: `F3517D45-0E66-41EF-8472-FCB7C98AE932`
- raw transport via a small Linux WMI helper module
- method family: `0x0603`
- semantic name from newer Intel NUC Software Studio builds: `AlexaLedControl`

## Runtime dependency status

Not needed at runtime:
- `UniwillService.exe`
- `UWACPIDriver.sys`
- `NucSoftwareStudioService.exe`
- Wine
- Windows

Used only as reverse sources:
- Intel NUC Software Studio packages
- older Uniwill service stack

## Why this repo exists

Older public reverse work appears to target previous revisions, likely using a different HID-based path.
This hardware revision still exposes a controllable panel, but the working Linux path is different.

## High-level history

1. Standard Linux LED/OpenRGB probing found nothing usable.
2. ACPI and WMI exploration identified OEM method families.
3. Older Uniwill service binaries were unpacked enough to recover EC and driver model details.
4. Newer Intel NUC Software Studio packages revealed `AlexaLedControl` and the state constants.
5. Raw `F351` transport was confirmed as the practical Linux-side path.
6. `0x0603` payloads were tested until the first visible state change was confirmed.

## Current practical API

Raw packet:
- total 256 bytes
- `packet[0] = 0x06`
- `packet[1] = 0x03`
- `packet[2] = 0x01` function selector
- `packet[3] = state`
- `packet[4] = speed`
- `packet[5] = brightness`
- `packet[6] = direction`

Confirmed examples:

```bash
sudo python tools/lightbar_ctl.py talking
sudo python tools/lightbar_ctl.py off
```

Equivalent raw calls:

```bash
sudo python tools/raw_f351_call.py 0x0603 1 6 5 100
sudo python tools/raw_f351_call.py 0x0603 1 1 5 100
```

## Known caveats

- Some states may still rely on extra context in vendor software.
- `mics_off` returned an `e3` response in testing but still changed the panel state.
- `bluetooth_connect` and `bluetooth_disconnect` looked identical in observed behavior.
- `direction` is kept in the API, but current confirmed path seems to work without needing it.
