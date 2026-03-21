# intel-nuc-m15-lightbar-linux

Linux control for the front RGB light bar found on Intel NUC M15 / related Uniwill-based laptop platforms.

Current status:
- working Linux-side control path found
- no Windows runtime dependency required
- multiple visual states confirmed on real hardware

## What works

Current working stack:
- kernel helper module: `kernel/intel_lightbar_wmi.c`
- userland helpers in `tools/`
- raw WMI transport through GUID `F3517D45-0E66-41EF-8472-FCB7C98AE932`
- lightbar method family `0x0603` (`AlexaLedControl` in newer Intel NSS packages)

Basic usage:

```bash
sudo python tools/lightbar_ctl.py talking
sudo python tools/lightbar_ctl.py off
sudo python tools/lightbar_ctl.py --list-states
```

## Repository layout

- `kernel/`
  - minimal Linux WMI helper module used for method invocation
- `tools/`
  - `lightbar_ctl.py`: high-level CLI
  - `raw_f351_call.py`: raw method caller
  - `run_f351_packet.py`: packet runner
  - `wmi_call.py`: low-level debugfs caller
- `docs/`
  - `state-map.md`: visually confirmed state behavior
  - `reverse-notes.md`: condensed reverse summary
  - `ru-setup.md`: Russian setup and deployment guide

## Build the kernel helper

Requirements:
- matching kernel headers
- `make`

Build:

```bash
cd kernel
make
```

Load:

```bash
sudo insmod intel_lightbar_wmi.ko
```

The helper exposes debugfs entries under:

```bash
/sys/kernel/debug/intel_lightbar_wmi/
```

## Visual state map

See [docs/state-map.md](docs/state-map.md).

Highlights from current testing:
- `talking`: purple/cyan smooth cyclic blend
- `thinking`: cyan expands from center across a purple base
- `notification_incoming`: one soft yellow flash
- `notification_unheard`: slow yellow pulse
- `bluetooth_connect`: three quick blue flashes
- `error`: three red pulses

## Reverse status

This repo does not require vendor binaries at runtime.
Vendor binaries were used only as reverse sources to recover method names, state ids, and transport semantics.

See [docs/reverse-notes.md](docs/reverse-notes.md).

## Russian guide

See [docs/ru-setup.md](docs/ru-setup.md).

## Safety / assumptions

This code is hardware-specific.
It targets the currently tested Intel/Uniwill laptop revision and may not behave the same on earlier HID-based light bar implementations.

## Next work

- package installation and DKMS-friendly module build
- hook lightbar states to Linux desktop/system events
- determine whether `active_direction` can be driven meaningfully from Linux context
