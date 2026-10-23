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
autoclicker toggle        # start or stop clicking (10 clicks/s on first start)
autoclicker speed-add 10  # add 10 clicks/s (works with negative values too (-10))
autoclicker speed-set 20  # set clicks/s directly
autoclicker status
```

Speed is clamped to 1-100 clicks per second.

If `notify-send` is installed, the autoclicker sends desktop notifications when it starts, stops, or its speed changes. Notifications are optional; the autoclicker works without `notify-send`.
