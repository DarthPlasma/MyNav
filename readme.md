# MyNAV

MyNAV is a **personal fork of [INAV](https://github.com/iNavFlight/inav)** that adds an **advanced ADS-B traffic display**: it picks the aircraft that is actually coming at you and shows whether your own motion is taking you out of its way. It ports to INAV the ADS-B work done in [MyTAflight](https://github.com/DarthPlasma/MytaFlight), a Betaflight fork, on top of INAV's own ADS-B support.

- **Base:** INAV **`9.1.0`** · **Licence:** GPLv3
- **Branch:** `feature/adsb-cone`
- **Documentation:** [docs/ADSB.md](docs/ADSB.md#mynav-critical-approach-warning-and-approach-cone) for pilots, [MYNAV.md](MYNAV.md) for the implementation, decisions and build notes
- **Status:** builds and passes its unit tests, **not yet validated in flight**. Unofficial firmware: fly it at your own risk.

## What's added

| Feature | Details |
| --- | --- |
| Threat detection | Picks the aircraft on a collision course: approach cone, time to arrival and range limits |
| `OSD_ADSB_CRITICAL_WARNING` | A steady `AIRCRAFT APPROACHING 45S` |
| `OSD_ADSB_CONE` | Two-row approach cone: where you are in the aircraft's cone now, and where your motion is taking you |
| `OSD_ADSB_STATUS` | ADS-B symbol with aircraft received / within range, e.g. `5/2` |
| More tracked aircraft | `adsb_max_vehicles` from 5 to 12 at runtime; traffic beyond 64 km is no longer dropped |
| Extra targets | MyTAflight boards INAV lacks: see [Targets](#targets) |
| Tools | OSD layout editor and build tool in [`mynav-tools/`](mynav-tools/) |

Everything else is stock INAV 9.1.0: INAV's own `OSD_ADSB_WARNING` and `OSD_ADSB_INFO` elements keep working as before.

> **Flashing MyNAV over INAV resets the OSD settings and layouts**, because their parameter group versions changed. Save a `diff` first and paste it back afterwards.

## Requirements

- A MAVLink ADS-B receiver (TT-SC1, ADSBee 1090, pingRX, SoftRF...) on a UART set to MAVLink telemetry, as described in [docs/ADSB.md](docs/ADSB.md).
- A GPS fix with more than 4 satellites: distance and direction to the traffic come from your own position.

## Threat detection

An aircraft is a **critical threat** when all of these are true:

1. **In range:** it is within `osd_adsb_distance_warning` and, when `osd_adsb_ignore_plane_above_me_limit` is not 0, no higher than that above you. Traffic below you always counts.
2. **Reliable data:** it reports a valid heading and ground speed.
3. **Pointing at you:** its course is within ± half of `osd_adsb_detection_cone` from the direction towards you. With the default 20°, it must point at you within ±10°.
4. **Close in time:** its time to arrival, distance ÷ its ground speed, is at most `osd_adsb_aircraft_toa` seconds.

When several aircraft qualify, the one arriving **first** is shown. The critical warning and the cone disappear as soon as no aircraft qualifies.

| CLI setting | Default | Meaning |
| --- | --- | --- |
| `osd_adsb_detection_cone` | `20` | Full width of the approach cone in degrees (2–180); also the scale of the cone element |
| `osd_adsb_aircraft_toa` | `60` | Time to arrival limit in seconds (1–600) |
| `osd_adsb_distance_warning` | `20000` | Range limit in metres (stock INAV setting) |
| `osd_adsb_ignore_plane_above_me_limit` | `0` | Ignore traffic higher than this above you, in metres; 0 = off (stock INAV setting) |
| `adsb_max_vehicles` | `5` | Aircraft tracked at the same time (5–12) |

## The approach cone

The cone belongs to the **incoming aircraft**: its tip is the aircraft where it is now, its axis is the aircraft's course. The element shows **you** inside that cone, as seen while you face the aircraft coming at you.

```
-10-------0-------+10      scale: ± half of osd_adsb_detection_cone, in degrees
        ▲    +             ▲ = you now       + = you after the time to arrival
```

**Row 1** is the scale. With the default 20° cone every column is one degree, and `0` is the aircraft's course line.

**The arrow** is where you are **now**: in the centre you are right on the aircraft's path. It points the way you are moving compared to the aircraft:

- **▲** head-on, flying towards it
- **▼** same direction, it is catching up with you
- **◄ / ►** crossing its path towards that side

It becomes **`H`** while your heading is not valid yet (no compass and no GPS course): the position is still right, only the direction is unknown.

**The crosshair** is where **your own motion** takes you **after the time to arrival**, in the same, current cone: the aircraft is not moved forward. It is hidden when it falls on the arrow, or while your heading is not valid.

- Hovering, or flying straight at the aircraft: the crosshair stays on the arrow.
- Sidestepping: it moves to that side, more the faster you go and the closer the aircraft is. Head-on at 3 km with 60 s to arrival, dodging right at 5 m/s puts it about 6° right.
- When that position is outside the cone, an **arrow on the edge** points to the side you are leaving by.

**Reading it:** the gap between arrow and crosshair is the trend. A crosshair moving towards the centre means things are getting worse; moving away or off the edge means they are resolving. As the aircraft gets closer the same distance from its path becomes a wider angle, so unless you are right under it your position slides towards the edge until you leave the cone and the elements disappear.

The cone is 21 columns wide: on 30-column analog OSDs place it at column 9 or less.

## Placing the elements

The INAV Configurator does not know the new elements. Place them from the CLI with `osd_layout <layout> <item> <column> <row> V`:

```
osd_layout 0 169 14 13 V    # OSD_ADSB_CRITICAL_WARNING
osd_layout 0 170 16 5 V     # OSD_ADSB_CONE
osd_layout 0 171 1 12 V     # OSD_ADSB_STATUS
save
```

Or visually with the [OSD layout tool](mynav-tools/osd-layout.html): open it in a browser, paste a `diff`, move the elements (each one shows an example value) and copy the `osd_layout` lines back into the CLI.

## Testing on the bench

Widen the limits so a simulated aircraft triggers easily, and restore them afterwards:

```
set osd_adsb_distance_warning = 64000
set osd_adsb_ignore_plane_above_me_limit = 0
set osd_adsb_aircraft_toa = 600
set osd_adsb_detection_cone = 180
save
```

Simulated traffic can come from the **[ESP32 ADS-B injector](https://github.com/DarthPlasma/MytaFlight/tree/integration/mytaflight-tools/adsb-injector)** of MyTAflight: it streams MAVLink `ADSB_VEHICLE` frames for up to 5 aircraft set from a web page.

## Targets

Besides every INAV target, MyNAV builds these MyTAflight boards that INAV does not have under the same name:

| Board (Betaflight name) | MyNAV target | Notes |
| --- | --- | --- |
| SPEEDYBEE_F745_AIO | `SPEEDYBEEF745AIO` | Same board |
| FLYWOOF722PROV2 | `FLYWOOF722PRO` | INAV's target already has the V2 gyro |
| FOXEERF745V4_AIO | `FOXEERF745AIO` | DPS310 baro added |
| SPEEDYBEEF405AIOV2 | `SPEEDYBEEF405AIOV2` | Gyro orientation, current scale, 9V BEC switch on the USER1 mode |
| FLYWOOF745AIOV2 | `FLYWOOF745AIOV2` | Gyro orientation |
| TMOTORF7_AIO | `TMOTORF7_AIO` | MPU6500-family gyro, baro on SPI |

These were matched pin by pin against the Betaflight configurations and they build, but none has been flown yet: check the board orientation in the Configurator's Setup tab before the first flight.

## Building

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make DAKEFPVH743
```

Or run `python3 mynav-tools/build-tool.py` and open http://localhost:8792 for a target menu with `.hex` download. [MYNAV.md](MYNAV.md) has the toolchain notes.

---

*The original INAV README follows.*

# INAV - navigation capable flight controller

# F411 PSA

> INAV no longer accepts targets based on STM32 F411 MCU.

> INAV 7 was the last INAV official release available for F411 based flight controllers. INAV 8 is not officially available for F411 boards and the team has not tested either. Issues that can't be reproduced on other MCUs may not be fixed and the targets for F411 targets may eventually be completelly removed from future releases.

# ICM426xx IMUs PSA

> The filtering settings for the ICM426xx has changed to match what is used by Ardupilot and Betaflight in INAV 7.1. When upgrading from older versions you may need to recalibrate the Accelerometer and if you are not using INAV's default tune you may also want to check if the tune is still good.

# M7, M6 and older UBLOX GPS units PSA

> INAV 8.0 will mark those GPS as deprecated and INAV 9.0.0 will require UBLOX units with Protocol version 15.00 or newer. This means that you need a GPS unit based on UBLOX M8 or newer.

> If you want to check the protocol version of your unit, it is displayed in INAV's 7.0.0+ status cli command.
> INAV 8.0.0 will warn you if your GPS is too old.
> ```GPS: HW Version: Unknown Proto: 0.00 Baud: 115200 (UBLOX Proto >= 15.0 required)```


> M8, M9 and M10 GPS are the most common units in use today, are readly available and have similar capabilities.
>Mantaining and testing GPS changes across this many UBLOX versions is a challenge and takes a lot of time. Removing the support for older devices will simplify code.

![INAV](http://static.rcgroups.net/forums/attachments/6/1/0/3/7/6/a9088858-102-inav.png)

# PosHold, Navigation and RTH without compass PSA

Attention all drone pilots and enthusiasts,

Are you ready to take your flights to new heights with INAV 7.1? We've got some important information to share with you.

INAV 7.1 brings an exciting update to navigation capabilities. Now, you can soar through the skies, navigate waypoints, and even return to home without relying on a compass. Yes, you heard that right! But before you launch into the air, there's something crucial to consider.

While INAV 7.1 may not require a compass for basic navigation functions, we strongly advise you to install one for optimal flight performance. Here's why:

🛰️ Better Flight Precision: A compass provides essential data for accurate navigation, ensuring smoother and more precise flight paths.

🌐 Enhanced Reliability: With a compass onboard, your drone can maintain stability even in challenging environments, low speeds and strong wind.

🚀 Minimize Risks: Although INAV 7.1 can get you where you need to go without a compass, flying without one may result in a bumpier ride and increased risk of drift or inaccurate positioning.

Remember, safety and efficiency are paramount when operating drones. By installing a compass, you're not just enhancing your flight experience, but also prioritizing safety for yourself and those around you.

So, before you take off on your next adventure, make sure to equip your drone with a compass. It's the smart choice for smoother flights and better navigation.

Fly safe, fly smart with INAV 7.1 and a compass by your side!

# INAV Community

* [INAV Discord Server](https://discord.gg/peg2hhbYwN)
* [INAV Official on Facebook](https://www.facebook.com/groups/INAVOfficial)

## Downloads

### INAV Configurator

**Get the latest version:** **[Download INAV Configurator](https://github.com/iNavFlight/inav-configurator/releases/latest)** - Available for Windows, macOS, and Linux

The INAV Configurator is the official desktop application for configuring your INAV flight controller. Choose your platform from the Assets section on the releases page.

### INAV Firmware

**Get the latest firmware:** **[Download INAV Firmware](https://github.com/iNavFlight/inav/releases/latest)**

Download the latest INAV flight controller firmware. Flash it to your flight controller using the configurator.

## Features

* Runs on the most popular F4, AT32, F7 and H7 flight controllers
* On Screen Display (OSD) - both character and pixel style
* DJI OSD integration: all elements, system messages and warnings
* Outstanding performance out of the box
* Position Hold, Altitude Hold, Return To Home and Waypoint Missions
* Excellent support for fixed wing UAVs: airplanes, flying wings
* Blackbox flight recorder logging
* Advanced gyro filtering
* Fully configurable mixer that allows to run any hardware you want: multirotor, fixed wing, rovers, boats and other experimental devices
* Multiple sensor support: GPS, Pitot tube, sonar, lidar, temperature, ESC with BlHeli_32 telemetry
* Logic Conditions, Global Functions and Global Variables: you can program INAV with a GUI
* SmartAudio and IRC Tramp VTX support
* Telemetry: SmartPort, FPort, MAVlink, LTM, CRSF
* Multi-color RGB LED Strip support
* And many more!

For a list of features, changes and some discussion please review consult the releases [page](https://github.com/iNavFlight/inav/releases) and the documentation.

## Tools

### INAV Configurator

Official tool for INAV can be downloaded [here](https://github.com/iNavFlight/inav-configurator/releases). It can be run on Windows, MacOS and Linux machines and standalone application.

### INAV Blackbox Explorer

Tool for Blackbox logs analysis is available [here](https://github.com/iNavFlight/blackbox-log-viewer/releases)

### INAV Blackbox Tools

Command line tools (`blackbox_decode`, `blackbox_render`) for Blackbox log conversion and analysis [here](https://github.com/iNavFlight/blackbox-tools).

### Telemetry screen for EdgeTX and OpenTX

Users of EdgeTX and OpenTX radios (Taranis, Horus, Jumper, Radiomaster, Nirvana) can use INAV OpenTX Telemetry Widget screen. Software and installation instruction are available here: [https://github.com/iNavFlight/OpenTX-Telemetry-Widget](https://github.com/iNavFlight/OpenTX-Telemetry-Widget)

### OSD layout Copy, Move, or Replace helper tool

[Easy INAV OSD switcher tool](https://www.mrd-rc.com/tutorials-tools-and-testing/useful-tools/inav-osd-switcher-tool/) allows you to easily switch your OSD layouts around in INAV. Choose the from and to OSD layouts, and the method of transfering the layouts.

## Installation

See: https://github.com/iNavFlight/inav/blob/master/docs/Installation.md

## Documentation, support and learning resources
* [INAV 5 on a flying wing full tutorial](https://www.youtube.com/playlist?list=PLOUQ8o2_nCLkZlulvqsX_vRMfXd5zM7Ha)
* [INAV on a multirotor drone tutorial](https://www.youtube.com/playlist?list=PLOUQ8o2_nCLkfcKsWobDLtBNIBzwlwRC8)
* [Fixed Wing Guide](docs/INAV_Fixed_Wing_Setup_Guide.pdf)
* [Autolaunch Guide](docs/INAV_Autolaunch.pdf)
* [Modes Guide](docs/INAV_Modes.pdf)
* [Wing Tuning Masterclass](docs/INAV_Wing_Tuning_Masterclass.pdf)
* [Official documentation](https://github.com/iNavFlight/inav/tree/master/docs)
* [Official Wiki](https://github.com/iNavFlight/inav/wiki)
* [Video series by Paweł Spychalski](https://www.youtube.com/playlist?list=PLOUQ8o2_nCLloACrA6f1_daCjhqY2x0fB)
* [Target documentation](https://github.com/iNavFlight/inav/tree/master/docs/boards)

## Contributing

Contributions are welcome and encouraged.  You can contribute in many ways:

* Documentation updates and corrections.
* How-To guides - received help?  help others!
* Bug fixes.
* New features.
* Telling us your ideas and suggestions.
* Buying your hardware from this [link](https://inavflight.com/shop/u/bg/)

A good place to start is the Discord channel, Telegram channel or Facebook group. Drop in, say hi.

Github issue tracker is a good place to search for existing issues or report a new bug/feature request:

https://github.com/iNavFlight/inav/issues

https://github.com/iNavFlight/inav-configurator/issues

Before creating new issues please check to see if there is an existing one, search first otherwise you waste peoples time when they could be coding instead!

## Developers

Please refer to the development section in the [docs/development](https://github.com/iNavFlight/inav/tree/master/docs/development) folder.

Nightly builds are available for testing on the following links:

https://github.com/iNavFlight/inav-nightly/releases

https://github.com/iNavFlight/inav-configurator-nightly/releases

## INAV Releases
https://github.com/iNavFlight/inav/releases


