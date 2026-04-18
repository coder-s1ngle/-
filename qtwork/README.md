# Qt C++ Carrier Sync Workspace

This workspace is prepared for carrier synchronization simulation coursework.
It uses a local, self-contained toolchain so the project does not depend on
global PATH changes.

## Active toolchain

- Qt: `6.5.3`
- Compiler: `MinGW-w64 GCC 11.2.0`
- Build system: `CMake + MinGW Makefiles`
- IDE workflow: `VS Code tasks + launch config`

## Directory highlights

- `tools/Qt6/6.5.3/mingw_64`: local Qt SDK
- `tools/Qt6/Tools/mingw1120_64/mingw64`: local GCC toolchain
- `.venv/Scripts`: local `cmake`, `ninja`, and helper Python tooling
- `scripts`: verification, build, and run entry points
- `src`: starter simulation application

## Verify the environment

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Verify-Toolchain.ps1
```

## Build the starter app

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build-Debug.ps1
```

## Run the starter app

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Run-Debug.ps1
```

## Fast smoke test

```powershell
$env:Path="E:\work\qtwork\tools\Qt6\6.5.3\mingw_64\bin;E:\work\qtwork\tools\Qt6\Tools\mingw1120_64\mingw64\bin;$env:Path"
.\build\debug-mingw\carrier_sync_lab.exe --smoke-test -platform offscreen
```

## What the starter app gives you

- A `QMainWindow` based desktop app
- A generic carrier synchronization placeholder loop
- A phasor visualizer for received, local, and corrected carrier states
- A phase-error history plot for loop convergence inspection
- Controls for sample rate, carrier offset, loop gains, phase offset, and noise
- A clean place to plug in the final modulation after the assignment direction is fixed

## Notes

- The validated workflow in this workspace uses Qt 6.5.3.
- The current signal model is intentionally modulation-agnostic; modulation
  choice such as QMBOC, OQPSK, or others should be decided before the final
  receiver chain is implemented.
- An earlier Qt 5.15.2 download exists in `tools/Qt`, but it is not used by
  the starter project because that package was distributed as an enterprise
  build and is unsuitable as the main teaching workflow here.
