name: Build 


on:
  workflow_dispatch:
  push:
    branches: [ main ]

jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: cmake -B build -A x64

      - name: Build
        run: cmake --build build --config Release

      - name: Upload plugin
        uses: actions/upload-artifact@v4
        with:
          name: PhoneMonitor-Windows
          path: |
            build/PhoneMonitor_artefacts/Release/VST3
            build/PhoneMonitor_artefacts/Release/Standalone
            
