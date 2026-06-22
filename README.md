This is the Sound Effect Generator that I did for the Software and Engineering for Embedded Systems course(1). This course is included in the Master in Electrical Engineering programme taught together by the Vrije Universiteit Brussel and the Université Libre de Bruxelles.

# Sound Effect Generator

A microcontroller-based audio synthesizer that generates sine-wave sound effects and melodies.

## Features

* **Direct Digital Synthesis (DDS):** Uses a pre-calculated 256-step sine wave lookup table and phase accumulation to generate smooth audio frequencies.
* **12-bit DAC Output:** Audio is driven through the microcontroller's onboard Digital-to-Analog Converter (DAC) at a 16 kHz sample rate, controlled by a hardware timer (TIM6).
* **Pre-programmed Tracks:** Includes 10 built-in audio tracks ranging from UI effects (beeps, power up/down, coin) to full melodies (Happy Birthday, Game of Thrones).
* **Dual Serial Control:** * **USART1:** 115200 baud rate for PC/Terminal control.
  * **USART2:** 9600 baud rate for an HC-05 Bluetooth module, allowing remote smartphone control.
* **Real-time Audio Manipulation:** Dynamically adjust the playback speed (tempo) and pitch (shift up or down by semitones) on the fly via serial commands.

---

### Pin Mapping

| Peripheral | Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **DAC** | `PA4` | Audio Output | Connect to an amplifier or piezo speaker. |
| **USART1** | `PA9` / `PA10` | PC Serial Communication | TX = `PA9`, RX = `PA10`. Set terminal to **115200 baud**, AF7, 8N1. |
| **USART2** | `PD5` / `PD6` | HC-05 Bluetooth Module | TX = `PD5`, RX = `PD6`. Set Bluetooth module to **9600 baud**, AF7, 8N1. |

---

## Usage & Controls

Once the firmware is flashed and the device is powered, you can interface with it using any serial terminal program on your PC or a Bluetooth serial app on your smartphone.

Computer App used: TeraTerm.

Android Smartphone App used: Serial Bluetooth Terminal (Search in Bluetooth LE).

Send the following characters over either serial connection to control the generator:

### Playback Controls
* `0` - `9` : Play the corresponding track (see tracklist below).

### Pitch Controls
* `u` or `U` : Pitch Up (Increase by 1 semitone, max +36).
* `d` or `D` : Pitch Down (Decrease by 1 semitone, min -24).

### Tempo Controls
* `f` or `F` : Faster (Subtracts 20ms from base note duration).
* `l` or `L` : Slower (Adds 20ms to base note duration).
* `+` : Scale Tempo Up (Increases duration by 10%).
* `-` : Scale Tempo Down (Decreases duration by 10%).

### System Controls
* `r` or `R` : Reset all tempo and pitch modifications to default.
* `m` or `M` : Reprint the serial menu where all the available instructions are shown.

---

## Tracklist

0. Simple beep
1. Double beep
2. Alarm
3. Power up
4. Power down
5. Coin
6. Scale (C major)
7. Arpeggio
8. Happy Birthday
9. Game of Thrones

---

## Code
**main** code can be found in folder V3F/User/main.c

---

## Explanatory video

This is the link of the video that explains how all the tools are implemented and a demonstration of how the embedded system works.
Link: https://drive.google.com/file/d/1522UncHBX1AvvaWoOTnAcAxWUqtf0qMw/view?usp=sharing

---

(1) Software and Engineering for Embedded Systems course link: https://caliweb.vub.be/?page=course-offer&id=009201&anchor=1&target=pr&year=2627&language=en&output=html
