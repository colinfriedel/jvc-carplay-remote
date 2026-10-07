# JVC CarPlay Remote: Gesture controls for a repurposed factory switch in my 2003 Acura RSX

![CarPlay controlling sunroof switch installed in dash](docs/images/installed-switch.jpg)

A DIY Arduino project that adds volume, track skip, mute and voice assistant
controls to a JVC KW-M560BT head unit, using a junkyard-sourced original RSX sunroof switch so that it looks factory.


```mermaid
flowchart LR
    subgraph car["Car"]
        lighter["Switched 12 V<br/>(old lighter-socket wire)"]
        lamps["Headlight-switched<br/>illumination circuit"]
    end

    subgraph dash["Behind the dash"]
        fuse["1 A inline fuse"]
        buck["12 V to 5 V<br/>buck converter"]
        nano["Arduino Nano"]
        subgraph sw["Repurposed factory switch"]
            contacts["Up / down contacts"]
            led["Backlight lamp"]
        end
    end

    lighter --> fuse --> buck
    buck -->|"5 V over USB"| nano
    contacts -->|"D3, D5"| nano
    lamps --> led
    nano -->|"D7, open-drain<br/>JVC remote frames"| head["JVC KW-M560BT<br/>steering-remote input"]
    head -->|"CarPlay over USB"| phone["iPhone"]
```



![Wiring diagram for the project](docs/images/wiring-diagram-white.png)
