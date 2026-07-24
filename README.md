# QtUIAuto

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**QtUIAuto** is a lightweight, non-invasive UI automation testing tool for Qt/QML applications.

It uses accessibility APIs (QAccessibility) to traverse the control tree, read properties, and simulate user interactions — all without requiring any modification to the target application source code.

## Features (In Development)

- **Record & Playback** — Record user interactions and replay them as automated tests
- **Control Tree Inspection** — Browse the QML control hierarchy
- **JSON Script Format** — Human-readable, version-controllable test scripts
- **Non-invasive** — No source code modification required (DLL injection approach)
- **Report Generation** — Visual test result reports

## Architecture

```
QtUIAuto (GUI) ──Named Pipe──► QtUIAuto_Inject.dll
                                    │
                            (injected into target process)
                                    │
                            QAccessibility API
                                    │
                            Target QML Application
```

## Prerequisites

- Qt 5.15.2 MSVC 2019 64-bit (`D:\Qt\5.15.2\msvc2019_64`)
- Visual Studio 2019 (Community/Professional)
- CMake 3.18+
- Windows SDK (for DLL injection APIs)

## Build

```bash
# Build the main application (Release)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target QtUIAuto

# Build with Spike subproject for QAccessibility verification
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SPIKE=ON
cmake --build . --target QtUIAuto_Spike
```

Or use the convenience script:
```bash
scripts\build.bat
```

## Spike Verification

The Spike subproject (`spike/`) validates QAccessibility API coverage on standard QML controls. Build with `-DBUILD_SPIKE=ON` and run `QtUIAuto_Spike.exe` to see the accessibility report.

## License

MIT License — see [LICENSE](LICENSE) for details.
