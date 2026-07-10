# SimpleErrorManager

X-Macro based error tracking for Arduino / ESP32.

Track up to **256 error codes** in a compact **32-byte bitmask**, with:

- **Mercy times** – add an error as *pending* and let it become *active* only if it
  survives a debounce window.
- **Acknowledge flags** – mark an error as seen for one-time actions, without clearing it.
- **Optional names/info strings** – human-readable text for debugging, opt-in to keep
  Flash/RAM small in production.
- **No enum bookkeeping** – the error enum, count and (optional) string tables are all
  generated from a single `.def` file via X-Macros.

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/Loaded-Dice/SimpleErrorManager.git
```

### Arduino IDE

Download the repository as a ZIP and use **Sketch → Include Library → Add .ZIP Library…**,
or copy the folder into your `Arduino/libraries/` directory.

## Quick start

The library does not ship your error codes – you define them per project in a `.def`
file and tell the library which file to use.

**1. Create `ErrorCodes.def` next to your sketch / in `src/`:**

```cpp
ERROR_CODE(WIFI_CONNECT_FAILED)
ERROR_CODE(SENSOR_TIMEOUT, "Sensor Timeout")
ERROR_CODE(MQTT_DISCONNECT, "MQTT Disconnect", "Broker connection lost")
```

**2. Select the file and include the library:**

```cpp
#include <Arduino.h>

#define INCLUDE_ERROR_NAMES               // optional: enable getErrorName()/getErrorInfo()
#define ERROR_CODES_FILE "ErrorCodes.def" // <-- must come BEFORE the include
#include "ErrorManager.h"

ErrorManager EM;

void setup() {
    EM.add(SENSOR_TIMEOUT);               // active immediately
    EM.add(WIFI_CONNECT_FAILED, 3000);    // pending, becomes active after 3 s
}

void loop() {
    EM.tick();                            // promotes pending -> active when mercy expires
    if (EM.isActive(SENSOR_TIMEOUT)) {
        // handle it, then:
        EM.clear(SENSOR_TIMEOUT);
    }
    delay(100);
}
```

>  If you forget to `#define ERROR_CODES_FILE`, the library still compiles using a
> tiny built-in example set (`src/ErrorCodes_example.def`) and emits a compiler
> `#warning` pointing you here. Define your own file to get rid of the warning.

## Error-code definition formats

Inside your `.def` file, each line is one `ERROR_CODE(...)` entry:

```cpp
ERROR_CODE(ALIAS)                          // name = "ALIAS", info = ""
ERROR_CODE(ALIAS, "Short Name")            // name = "Short Name", info = ""
ERROR_CODE(ALIAS, "Short Name", "Info")    // name = "Short Name", info = "Info"
```

`ALIAS` is the enum constant you use in code (`EM.add(ALIAS)`). Names/info strings are
only compiled in when `INCLUDE_ERROR_NAMES` is defined.

## API overview

| Method | Purpose |
| --- | --- |
| `add(code, mercy_ms = 0)` | Add error; `mercy_ms > 0` makes it pending until the window expires. Returns `false` if already set. |
| `clear(code)` | Clear an active/pending error (also resets its acknowledge flag). |
| `acknowledge(code)` / `ack(code)` | Mark an existing error as acknowledged (for one-time actions). Persists until cleared. |
| `isAcknowledged(code)` | Whether the error is acknowledged. |
| `tick()` | Call periodically; promotes pending errors to active when their mercy time expires. |
| `isActive(code)` / `isPending(code)` | State queries. |
| `getActiveCount()` / `getPendingCount()` | Counters. |
| `getTimestamp(code)` | `millis()` value when the error was added. |
| `clearAll()` | Reset everything. |
| `allErrorCodes()` | Range for `for (ErrCode c : EM.allErrorCodes())`. |
| `getErrorName(code)` / `getErrorInfo(code)` | Strings (require `INCLUDE_ERROR_NAMES`). |

## Examples

See the [`examples/`](examples/) folder:

- **Basic** – full demo with a FreeRTOS error task, mercy times, string names and
  random error generation.
- **Minimal** – smallest possible usage, handy for measuring code size.

## Repository layout

```
src/
  ErrorManager.h            core library (single header)
  ErrorCodes_example.def    built-in fallback codes (used only if you don't define your own)
examples/
  Basic/    Basic.ino + ErrorCodes.def
  Minimal/  Minimal.ino + ErrorCodes.def
library.json / library.properties   package manifests (PlatformIO / Arduino)
platformio.ini            builds examples/Basic for in-place development
```

## License

MIT © Marlon Graeber
