# D450 Driver for macOS 27

The D450 thermal label printer's Mac driver is Intel-only. On Apple silicon Macs running macOS 27 without Rosetta, printing fails with an "incompatible" error or sits at "Sending data to printer."

This replaces the one Intel-only part of that driver, the `rastertolabeltspl` filter, with a native version. No Rosetta needed, your printer setup stays the same, and every app can print again.

## Install

1. Install the printer's own Mac driver from the manufacturer, if it isn't installed already.
2. Open Terminal: press Command and Space, type Terminal, and press Return.
3. Copy this line, paste it into Terminal, and press Return:

   ```
   curl -fsSL https://github.com/ryleighnewman/D450-Driver-macOS-27/archive/refs/heads/main.tar.gz | tar -xz -C /tmp && sh /tmp/D450-Driver-macOS-27-main/install.sh
   ```

4. Type your Mac password and press Return. Nothing shows while you type.
5. When Terminal says "Installed", print a label.

If you ever reinstall the printer's own driver, run the line again.

## Uninstall

Do the same steps with this line instead:

```
curl -fsSL https://github.com/ryleighnewman/D450-Driver-macOS-27/archive/refs/heads/main.tar.gz | tar -xz -C /tmp && sh /tmp/D450-Driver-macOS-27-main/uninstall.sh
```

## Printers

Made for the D450. The same driver also runs the D520, D530, D550, PM-241, PM-246S, Q300 and T-series over USB, and the output matches the original byte for byte. Bluetooth and Wi-Fi models still use the original filter, which needs Rosetta.
