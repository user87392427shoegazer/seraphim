# seraphim

a concept wayland window manager with window-management logic written in angelscript.

## the gimmick

- **infinite canvas**
- **magnet snapping**
- **double borders**
- **dragspawn**
- **layouts** grid, columns, rows, master and cascade, or you can make it yourself
- **camera bookmarks**
- **window rules** automatically size or position windows by title or app id
- **hot reload**
- **control socket** manage windows, layouts and the camera with `seraphimctl`

## build

install the required dependencies using your package manager. wlroots 0.20 and its development files are required.

`make` generates the required wayland protocol headers automatically. angelscript is fetched and built by the makefile.

```sh
make deps
make
sudo make install
```

### freebsd

use gnu make (`gmake`), not the base `make`:

```sh
pkg install gmake git pkgconf wlroots020 wayland wayland-protocols \
    libxkbcommon libinput libdrm pixman evdev-proto seatd
gmake deps
gmake
sudo gmake install
```

you also need `seatd` running (`service seatd enable && service seatd start`), your user in the `video` group, and `drm-kmod` for intel/amd graphics.

to run the tests:

```sh
make test
```

## start

```sh
seraphim
```

to use a custom angelscript configuration, copy `scripts/seraphim.as` to `~/.config/seraphim/seraphim.as`.

## binds

| keys | action |
| --- | --- |
| super + left drag | move window |
| super + right drag | resize window |
| left drag on empty canvas | pan canvas |
| right drag on empty canvas | create spawn region |
| right click on empty canvas | open window |
| scroll on empty canvas | zoom |
| super + arrows | pan canvas |
| super + = / - | zoom in / out |
| super + 0 / home | reset camera |
| super + 1–9 | restore camera bookmark |
| super + ctrl + 1–9 | save camera bookmark |
| super + l | apply current layout |
| super + ctrl + l | switch layout |
| super + ctrl + r | reload script |
| super + t | spawn window |
| super + w / a / s / d | focus nearest window |
| super + tab | centre view on focused window |
| super + z | fit all windows in view |
| super + q | close focused window |
| super + escape | quit |

## configuration

- `config_appearance.h` background, borders, colours and spawn-region style
- `config_animation.h` zoom limits, zoom step and pan distance
- `config_input.h` snapping, resizing and default window size
- `config_keys.h` modifier keys and keybindings
- `config.h` general config

## control

`seraphimctl` lets you control the compositor from a terminal:

```sh
seraphimctl windows
seraphimctl layouts
seraphimctl layout columns
seraphimctl zoom 0.5
seraphimctl reload
seraphimctl help
```

## sum side nots

- no workspaces, for me, infinite canvas is enough
- xwayland is not yet enabled, so x11-only applications will not work

## tests

```sh
make test
```

the tests run without wlroots or a display and cover the window-management logic, layouts, snapping, camera controls, keybindings and script reloads.

## license

gpl-3.0 i think

## credit

this wouldnt exist without:
[vxwm](https://codeberg.org/wh1tepearl/vxwm)
[leeloo](https://git.ohmu.pw/ohmu/leeloo/)
some recent changes to seraphim are taken from [lilith](https://github.com/ohto121/lilith)
