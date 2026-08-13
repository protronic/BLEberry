# BLEberry

Eine [Berry](https://berry-lang.github.io/)-Sprach-REPL über **BLE NUS**
(Nordic UART Service) als Zephyr-Anwendung für **STM32WBA**-Boards.

Board mit einer NUS-Terminal-App verbinden (z. B. *nRF Toolbox → UART*,
*Serial Bluetooth Terminal* oder Web-BLE-Terminal), Notifications
abonnieren — und direkt Berry-Code tippen:

```
BLEberry - Berry 1.1.0 on Zephyr (stm32wba65i_dk1)
type berry code, e.g.: 1+2  or  print("hi")
> 1+2
3
> led(true)
> millis()
12345
> def fib(n) return n < 2 ? n : fib(n-1) + fib(n-2) end
> fib(20)
6765
```

## Unterstützte Boards

| Hardware | Zephyr-Board | Status |
|---|---|---|
| STM32WBA65I-DK1 (WBA6-DK) | `stm32wba65i_dk1` | unterstützt |
| Nucleo-WBA55CG (WBA5) | `nucleo_wba55cg` | unterstützt |
| STM32WBA55G-DK1 / STM32WBA5MMG-Modul (B-WBA5M-WPAN) | — | kein Upstream-Board in Zephyr; `nucleo_wba55cg` als Basis nutzen (gleicher STM32WBA55CG-Die) und Pins per Devicetree-Overlay anpassen |

Der Code ist board-agnostisch: jedes Zephyr-Board mit BLE-Controller
funktioniert (`led()` gibt es nur, wenn ein `led0`-Alias existiert).

## Workspace aufsetzen

Das Repo ist ein west-Manifest-Repository (Zephyr `main`):

```sh
mkdir bleberry-ws && cd bleberry-ws
git clone <url-dieses-repos> bleberry
west init -l bleberry
west update
west zephyr-export
pip install -r zephyr/scripts/requirements-base.txt

# BLE-Controller-Bibliothek (LinkLayer) für STM32WBA holen:
west blobs fetch hal_stm32
```

Zephyr `main` benötigt **Python ≥ 3.12** und das **Zephyr SDK ≥ 1.0.0**
(bei SDK 1.0.x liegt die Toolchain unter `<sdk>/gnu/arm-zephyr-eabi`).

Zusätzlich wird das [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
(arm-zephyr-eabi) benötigt, zum Flashen der STM32WBA-Boards außerdem
[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html).

## Bauen & Flashen

```sh
# WBA6-DK
west build -b stm32wba65i_dk1 bleberry
west flash

# Nucleo-WBA55 / WBA5-basierte Hardware
west build -p -b nucleo_wba55cg bleberry
west flash
```

## Benutzung

1. Board startet und advertised als **`BLEberry`** (Name via
   `CONFIG_BT_DEVICE_NAME` in `prj.conf`).
2. Mit einer NUS-fähigen App verbinden und Notifications auf der
   TX-Characteristic aktivieren → Banner + Prompt erscheinen.
3. Zeilen, die die App sendet, werden kompiliert und ausgeführt;
   Ausgaben kommen als Notifications zurück. Mehrzeilige Eingaben
   (z. B. `def`/`while`-Blöcke) erkennt die REPL automatisch (`>> `-Prompt).

Eingebaute native Funktionen (`help()` in der REPL zeigt, was das
jeweilige Board tatsächlich anbietet):

| Funktion | Wirkung | Voraussetzung |
|---|---|---|
| `help()` | Funktionsübersicht | immer |
| `millis()` | Millisekunden seit Boot | immer |
| `reboot()` | Kaltstart des Boards | immer |
| `led(on)` / `led(i, on)` | User-LED `i` schalten | `led0..led2`-Alias |
| `button([i])` | User-Button lesen (1 = gedrückt) | `sw0..sw2`-Alias |
| `joy()` | Joystick: `"up"/"down"/"left"/"right"/"enter"/""` | `CONFIG_INPUT` (WBA6-DK) |
| `temp()` | Die-Temperatur in °C | `CONFIG_SENSOR` + `die-temp0` |
| `pinmode(p, pin, mode)` | GPIO konfigurieren, `p="a".."h"`, mode: `in`, `in_pu`, `in_pd`, `out`, `out_od` | immer |
| `dwrite(p, pin, v)` | GPIO-Pin schreiben | immer |
| `dread(p, pin)` | GPIO-Pin lesen | immer |

Beispiel auf dem WBA6-DK:

```
> led(1, true)          # rote LD5 an
> pinmode("a", 5, "out")
> dwrite("a", 5, 1)     # PA5 high (Arduino-Header)
> temp()
27.5
> joy()
up
```

## BlockBerry-Integration

Die Firmware enthält die Laufzeit-API, die der
[BlockBerry-Editor](https://github.com/protronic/blockberry-editor)
in seinem generierten Berry-Code aufruft (`src/berry_generator.ts`):

| Editor-Block | Generierter Aufruf | Umsetzung auf dem Board |
|---|---|---|
| `mini_sps_task` | `sps.every(ms, fn)` | zyklischer Task, läuft während die REPL auf Eingaben wartet (`CONFIG_BLEBERRY_TICK_MS`) |
| `sps_wait_ms` | `sps.wait(ms)` | pausiert nur diesen Task bis zum Ablauf der Wartezeit |
| `sps_digital_input` | `sps.input("KANAL")` | Kanalname → Joystick/Button/GPIO |
| `sps_digital_output` | `sps.output("KANAL", v)` | Kanalname → LED/GPIO |
| `sensor_ready` / `sensor_temp` | `sensor.ready()` / `sensor.temp()` | Die-Temperatursensor |
| `escalation_rule` | `escalation.raise_if(id, cond, level, msg, cooldown)` | zustandsbehaftet mit Sperrzeit |
| `signal_set` | `signal.set(name, state)` | `normal`→grüne LED, `warning`/`alarm`→rote LED |
| `monitor_value` | `monitor.record(metric, value, unit)` | Ausgabe ins Log (NUS + Konsole) |
| `log_print` | `log.print(msg)` | Ausgabe ins Log (NUS + Konsole) |

Alles Boardspezifische ist **Kanal-Parameter** statt eigener Blöcke:

* `"LED0"`..`"LED2"` — User-LEDs
* `"SW0"`..`"SW2"` — User-Buttons (Nucleo-WBA55)
* `"JOY_UP"`, `"JOY_DOWN"`, `"JOY_LEFT"`, `"JOY_RIGHT"`, `"JOY_SELECT"` — Joystick (WBA6-DK)
* `"D0"`..`"D15"` — Arduino-R3-Header (WBA6-DK, per Board-Map)
* `"PA0"`..`"PH15"` — beliebiger GPIO nach Port/Pin

GPIO-Kanäle werden beim ersten Zugriff automatisch konfiguriert
(Eingänge mit Pull-up, Ausgänge push-pull).

Das zugehörige Geräteprofil für den Editor (CouchDB-Dokument, nur
Block-Typen aus `blocks.ts` plus Kanalliste) liegt unter
`descriptors/wba6dk_berry_v1.json`.

Berry-Module: `string`, `math`, `json`, `gc`, `introspect`, `global`,
`strict` (konfiguriert in `berry_conf/berry_conf.h`).

Der UART-Konsole (ST-LINK VCP, 115200 8N1) dient als Log-/Debug-Ausgabe;
die REPL-Ausgabe wird dorthin gespiegelt.

## Konfiguration

* `CONFIG_BLEBERRY_ECHO` — Zeichen-Echo über NUS (für rohe
  Zeichen-Terminals; Standard: aus, da App-Terminals gesendete Zeilen
  selbst anzeigen).
* `CONFIG_BLEBERRY_LINE_MAX` — maximale Zeilenlänge (256).
* `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` — Berry-Heap
  (pro Board in `boards/<board>.conf` gesetzt).
* ATT-MTU bis 247 ist konfiguriert; die Gegenstelle muss den
  MTU-Exchange anstoßen (machen Smartphone-Apps automatisch).

## Aufbau

```
├── west.yml               west-Manifest (Zephyr main, hal_stm32, Berry)
├── CMakeLists.txt         Build inkl. Berry-Codegenerierung (coc)
├── berry_conf/berry_conf.h  Berry-Konfiguration (Embedded-Profil)
├── src/
│   ├── main.c             BT-Init, Advertising, NUS-Callbacks
│   ├── nus_io.c/.h        Terminal-I/O über NUS (RX-Ringbuffer, TX-Chunking)
│   ├── berry_repl.c       REPL-Thread, VM-Lifecycle, native Funktionen
│   ├── berry_port.c       Berry-Port-Layer (be_writebuffer/be_readstring)
│   └── be_modtab.c        Berry-Modultabelle
└── boards/                Board-spezifische Heap-Größen
```

Hinweis: `west.yml` pinnt Zephyr auf `main` und Berry auf `master`.
Für reproduzierbare Builds die `revision:`-Felder auf konkrete
Commits/Tags festnageln.
