# QU

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**QU** is a lightweight, non-invasive UI automation testing tool for Qt/QML applications.

It uses a **dual-track interaction engine** — combining QQuickItem tree traversal + scene-level event delivery (primary) with QAccessibility API (fallback) — to inspect and control the UI, all without requiring modification to the target application's architecture or framework code.

## Features (In Development)

- **Record & Playback** — Record user interactions and replay them as automated tests
- **Control Tree Inspection** — Browse the QML control hierarchy via QQuickItem tree traversal
- **JSON Script Format** — Human-readable, version-controllable test scripts
- **Non-invasive** — No framework/architecture changes required in the target application
  (hook-based DLL injection; the target is not rebuilt or relinked)
- **Dual-Track Interaction** — scene-level `sendEvent` (primary) + Accessibility (fallback)

## Architecture

```
QU (GUI) ──Named Pipe──► QU_Inject.dll
                            |
                    (injected into target process)
                            |
            +----------------+----------------+
            |           InputSimulator         |
            |                                  |
            |  Track 1: QQuickWindow delivery   |
            |    -> sendEvent at scene pos      |
            |  Track 2/3: MouseArea / item      |
            |    -> QCoreApplication::sendEvent |
            |  Track 4: QAccessibility API      |
            |    -> doAction (last resort)      |
            +----------------+-----------------+
                             |
                     Target QML Application
```

**Injection mechanism:** a `WH_GETMESSAGE` hook is installed on the target's GUI thread
(`SetWindowsHookEx` + `PostThreadMessage` to force the hook to fire). The DLL then initialises
itself on the target's **main thread**, which is mandatory — QQuickItem tree access and event
delivery are only valid there. Commands and responses travel over a named pipe as **NDJSON**
(one compact JSON object per line).

Administrator privileges are *not* required for same-user targets.

## User Preparation (Recommended)

For best results, QU recommends adding **Accessible** markup to target QML controls.
This is a standard industry practice (similar to Android `content-desc` / iOS `accessibilityIdentifier` / Web `data-testid`):

```qml
Button {
    objectName: "myButton"
    text: "Submit"
    Accessible.role: Accessible.Button
    Accessible.name: "Submit Button"
}

Item {
    objectName: "customControl"
    Accessible.role: Accessible.Button
    Accessible.onPressAction: mouseArea.clicked()

    MouseArea {
        id: mouseArea
        onClicked: { /* ... */ }
    }
}
```

If Accessible markup is not present, QU still works through **scene event delivery**
(`QMouseEvent` / `QKeyEvent` dispatched to the `QQuickWindow` with scene coordinates, letting
Qt Quick perform hit-testing), which works for all QML controls.

> **Note on `Accessible.onPressAction`:** accessibility actions can fail *silently* —
> `doAction(Press)` only emits a signal that may have no receiver. This is why accessibility is
> a **fallback**, not the primary route. See
> [docs/DEVELOPMENT_NOTES.md](docs/DEVELOPMENT_NOTES.md) for the full analysis and for other
> pitfalls recorded during development.

## Interaction Strategy

| User Action | Primary Route (scene delivery) | Fallback Route |
|:------------|:-------------------------------|:---------------|
| **Click** | `sendEvent(QQuickWindow, QMouseEvent)` at scene pos | MouseArea / item `sendEvent` → `doAction(Press)` |
| **Type text** | `setFocus` + `setProperty("text")` | `sendEvent(QKeyEvent)` |
| **Read text** | `item->property("text" / "displayText")` | `ai->text(Name)` |
| **Set value** | `item->setProperty("value")` | `valueInterface->setCurrentValue()` |
| **Select** | scene delivery `doAction(Press)` on option | `sendEvent` + key nav |

## Prerequisites

- Qt 5.15.2 MSVC 2019 64-bit (`D:\Qt\5.15.2\msvc2019_64`)
- Visual Studio 2019 (Community/Professional)
- CMake 3.18+

## Build

The Visual Studio generator is multi-config, so the build type must be passed with
`--config` at build time (`-DCMAKE_BUILD_TYPE` alone has no effect).

```bash
# Build the main application (Release)
mkdir build && cd build
cmake ..
cmake --build . --config Release --target QU

# Build with Spike subproject
cmake .. -DBUILD_SPIKE=ON
cmake --build . --config Release --target QU_Spike
```

## Testing

Two suites cover the two halves of the system.

**`QU_E2E`** — the injected side. It launches `QU_TestTarget`,
injects `QU_Inject.dll`, then exercises `dumpTree` / `listControls` / `click` / `doubleClick` /
`typeText` / `getText` / `setFocus` over the pipe and asserts against the target's actual QML state.

**`QU_HostChain`** — the host side. It links the real `ProcessManager` / `PipeServer` /
`ScriptEngine` sources and reproduces what `Application` orchestrates: launch → listen → wait for the
window → inject → command → playback → teardown. Beyond the happy path it pins down the behaviours
that are easy to regress silently: request/response stay aligned across a timeout, `startPlayback()`
returns immediately instead of blocking the caller, and `stopPlayback()` actually halts remaining steps.

```bash
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Release --target QU_E2E
cmake --build . --config Release --target QU_HostChain

# Run directly (exit code 0 = all cases passed)
cd tests/Release && ./QU_E2E.exe && ./QU_HostChain.exe

# Or via CTest
ctest -C Release
```

Notes:

- The E2E target's POST_BUILD step copies `QU_TestTarget.exe` and `QU_Inject.dll`
  next to `QU_E2E.exe`, so the suite can never run against a stale binary.
- Cross-process injection means the suite cannot run inside a restricted sandbox, but it does
  **not** need elevation.
- Put the 64-bit Qt `bin` first on `PATH`. A third-party 32-bit `qt5core.dll` earlier on `PATH`
  makes the executable die at load time with `0xC000007B` before `main()` — leaving the previous
  run's log in place, ready to be mistaken for this run's result.
- A per-run trace is written to `e2e_diag.log` / `host_chain_diag.log` next to the executables
  (UTF-8); the injected side logs to `%TEMP%\QU_Inject.log`. Each command appears on both
  sides, which makes it easy to tell "never dispatched" apart from "dispatched but had no effect".

Current status: **E2E 10/10, HostChain 15/15**.

## Documentation

- [docs/DEVELOPMENT_NOTES.md](docs/DEVELOPMENT_NOTES.md) — pitfalls hit during development,
  each with symptom, root cause, correct approach and the evidence that confirmed it.

## License

MIT License
