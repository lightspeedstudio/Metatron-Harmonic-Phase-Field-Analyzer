# Metatron Harmonic Phase Field Analyzer

![Metatron Harmonic Phase Field Analyzer](assets/cover.jpg)

Sensor Array · Harmonic Observation · Phase Field Analysis

LightSpeed Studio, LLC

Public client for the pillar firmware. This repository is the SDK. It does not contain the host application or the firmware source.

The wire frame is `0xAA`, length, type, payload, CRC-8, `0x55`. The CRC table is the firmware table, polynomial `0x07`. A second CRC window that skips the length byte is accepted and labeled legacy.

The hall bitmap is bit 0 for hall sensor 0 on A0, bit 1 for A1, bit 2 for A2, bit 3 for A3.

Pillars 01–03 are module type `0x17`. Channel 0 is the 6-byte pattern. Channels 1–4 at 12 bytes are hall sensors 0–3. Channels 4 and 5 at 8 bytes are the ultrasonic sensors. Any other length stays unparsed. Pillars 04–06 stay raw. `MSG_STATUS` stays raw. Firmware `millis` and the host receive time are stored as two different values.

The only frame the library will send is an empty `MSG_IDENT`. `MSG_CALIBRATE` and `MSG_COMMAND` are rejected.

A COM port is only the Windows location of a serial device. The pillar name is the firmware id `PILLAR_01` through `PILLAR_06`. Tower grouping is `config/towers.txt`. If a port is already open, `mt_port_open` returns `PORT_IN_USE`. Baud is 115200 8N1 with DTR and RTS disabled.

Build:

```
powershell -File build.ps1
```

That compiles `build/metatron.dll`, `build/read_pillar.exe`, and runs the offline checks. It does not open a COM port.

Read a pillar when you choose the port:

```
build\read_pillar.exe COM3 record.bin
python examples\read_pillar.py COM3 record.bin
```
