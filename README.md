# Air Piano

An air piano for the Microprocessors Lab (Shiraz University, Assignment 3), plus a web piano that stays linked to the Wokwi simulation.

- Move a virtual hand over an HC-SR04 (5 to 45 cm) to play the C major scale on a buzzer.
- An RGB LED shows a color per note and gets brighter in the middle of each band.
- Record up to 32 notes with their lengths and play them back with the same rhythm.
- A web page with a piano look and sound mirrors the simulation, and its keys and slider drive the simulated sensor.

## Structure

| Path | What it is |
|---|---|
| `AirPianoLive/AirPianoLive.ino` | Arduino sketch (assignment behavior plus a serial link) |
| `diagram.json`, `wokwi.toml` | Wokwi circuit and project settings |
| `bridge.js` | Node bridge: Wokwi serial (port 4000) to WebSocket, and serves the page |
| `public/index.html` | The web piano |

## Run it

1. Install Node.js, VS Code with the Wokwi extension (free license), and Arduino IDE.
2. In Arduino IDE, select **Arduino Uno** and use **Sketch > Export Compiled Binary** on `AirPianoLive.ino`.
3. Check that the paths in `wokwi.toml` point to the generated `.hex` and `.elf` files.
4. `npm install`, then `node bridge.js`.
5. In VS Code press `F1` and run **Wokwi: Start Simulator**.
6. Open http://localhost:8787. The page should say it is linked to Wokwi.

## Serial protocol

Out (115200 baud): `@READY`, `@N <noteIndex> <cm>`, `@OFF`, and note lines like `E4 450ms`.
In: `D<cm>` sets the distance, `R` toggles recording, `P` plays the song.
