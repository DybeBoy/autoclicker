# autoclicker

Minimal left-click autoclicker for Linux/Wayland. Written in C with libc only. It clicks at the current cursor position through `/dev/uinput`, so it works on any compositor.

## Build and install

```sh
make
make install   # copies to ~/.local/bin (override with PREFIX=...)
```

## Permissions (once)

As root:

```sh
echo 'KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"' > /etc/udev/rules.d/99-autoclicker.rules
usermod -aG input $USER
modprobe uinput
udevadm control --reload && udevadm trigger
```

Then log in again.

## Usage

```sh
autoclicker start         # start clicking (10 clicks/s)
autoclicker end           # stop
autoclicker speed-add 10  # add 10 clicks/s (works with negative values too (-10))
autoclicker speed-set 20  # set clicks/s directly
autoclicker status
```

Speed is clamped to 1-100 clicks per second.
