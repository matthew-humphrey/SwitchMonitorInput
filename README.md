# SwitchMonitorInput

This is a small Windows command-line utility that uses the DDC interface to switch monitor inputs. I wrote it to allow me to quickly switch my monitors between my work and home desktops.

## Usage

```
Usage: D:\repos\SwitchMonitorInput\x64\Debug\SwitchMonitorInput.exe [options]
Options:
  -l: List all physical monitors
  -i input_name: Set monitor input (DP1, DP2, HDMI1, HDMI2, ...)
  -d display_index: Index of the display (default: 1)
  -m monitor_index: Physical monitor index for the specified display (default: 1)
  -s : Show list of valid input names
```
