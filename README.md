<div align="center">

<img src="resources/logo.svg" alt="Huenicorn GUI logo" width="120">

# Huenicorn GUI

**A native Qt tray app and configurator for [Huenicorn](https://gitlab.com/openjowelsofts/huenicorn), the Philips Hue screen sync for Linux.**

[![Huenicorn on GitLab](https://img.shields.io/badge/daemon-Huenicorn%20on%20GitLab-fc6d26?logo=gitlab&logoColor=white)](https://gitlab.com/openjowelsofts/huenicorn)
[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE.txt)
[![Qt 6](https://img.shields.io/badge/Qt-6-41cd52?logo=qt&logoColor=white)](https://www.qt.io/)

</div>

<p align="center">
  <img src="docs/screenshots/sync.png" alt="Sync tab" width="220">
  <img src="docs/screenshots/screen.png" alt="Screen layout tab" width="220">
  <img src="docs/screenshots/lights.png" alt="Lights tab" width="220">
  <img src="docs/screenshots/settings.png" alt="Settings tab" width="220">
</p>

> This project is not part of Huenicorn itself. It only talks to the daemon through its REST API, so it has no build
> dependency on it. All credit for the sync engine goes to the
> [Huenicorn authors](https://gitlab.com/openjowelsofts/huenicorn).

## What is this?

[Huenicorn](https://gitlab.com/openjowelsofts/huenicorn) brings Philips Hue screen sync to Linux. It watches your
screen and streams the colors to your Hue lights in real time, so the room glows along with your games, movies and
desktop. It runs as a background daemon and is configured through a web page.

Huenicorn GUI is a more beautiful way to configure it. This native desktop app sits in your system tray, starts the
daemon for you and opens a clean window where you pick your lights, tune the effect and see at a glance whether sync is
running. The web page keeps working alongside it.

## Requirements

- Linux with a system tray (KDE Plasma, or GNOME with an AppIndicator extension)
- [Huenicorn](https://gitlab.com/openjowelsofts/huenicorn) installed and set up with your Hue bridge
- Qt 6 (Widgets, Network, Quick), CMake 3.16 or newer and a C++20 compiler to build

Install the build requirements for your distribution:

**Debian, Ubuntu**

```sh
sudo apt install build-essential cmake libgl1-mesa-dev qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtqml-workerscript
```

**Fedora, RHEL, AlmaLinux, Rocky (dnf or yum)**

```sh
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtdeclarative-devel
```

On RHEL 9 and its clones, Qt 6 comes from EPEL, so enable that repository first.

**Arch, Manjaro**

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative
```

## Build and run

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
build/huenicorn-qt
```

The daemon's REST port is read from `~/.config/huenicorn/config.json`. The app finds the `huenicorn` binary through its
settings, next to its own binary, or on your `PATH`.

## Using it

| Action | How |
| --- | --- |
| Open the configurator | Left click on the tray icon |
| Start, stop, restart the daemon | Tray menu |
| Open the browser configurator | Tray menu, or *Open Web-Configurator* in the Settings tab |
| Change autostart, daemon path, quit behavior | Tray menu, *Settings* |

## Credits

Huenicorn GUI is a companion to [Huenicorn](https://gitlab.com/openjowelsofts/huenicorn) by
[openjowelsofts](https://gitlab.com/openjowelsofts).

## License

GPLv3, see [LICENSE.txt](LICENSE.txt).
