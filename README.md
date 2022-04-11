# toggler
Simple toggle with custom commands for your system tray (usually).

This little program creates a custom StatusNotifierItem - usually this is an icon inside your Linux system tray. Created StatusNotifierItem has two states that you can toggle between by activating it (usually by clicking with the left mouse button). In inactive (`off`) state it sets its status to passive, which StatusNotifier services can interpret differently. KDE Plasma, for example, hides passive icons inside the arrow menu.

**Warning**: `toggler` does not work with older system tray interface (that is used in i3wm, for example), only with DBus-based StatusNotifierItem one, like in KDE Plasma, Sway or recent Xfce. Only KDE Plasma is (kind of) tested as of now.

To see what it can do let's look at the usage:

```
Usage: toggler [options]

  -h, --help             Show help message and exit.
  -o, --on <cmd>         Command to run when the state changes to 'on'.
  -O, --off <cmd>        Command to run when the state changes to 'off'.
  -i, --icon-on <icon>   Icon name to use when the state is 'on'.
  -I, --icon-off <icon>  Icon name to use when the state is 'off'.
  -t, --title <title>    Set the title.
  -s, --state (on|off)   Set the initial state.

To close the applet, use the secondary action of your StatusNotifier service.
Usually this means - click with middle mouse button on the tray icon :)
```

Should be pretty self-explanatory. One thing to note though - only icon *names* are supported. You cannot specify a path to a custom icon.

## Compiling

Besides C standard library there's only one dependency, `libsystemd`, that is used for DBus communication. That said, your C standard library should support non-standard `getopt_long` function that is used to handle arguments parsing. Musl and glibc both support it.

So, to compile `toggler`:

```sh
# Debian: sudo apt install libsystemd-dev
# Fedora: sudo dnf install systemd-devel
# Arch: you're good to go :)
$ make
$ sudo make install # Optional, installs to /usr/local/bin by default
```

## Example

I wrote this tool for a single purpose - to toggle the state of my SOCKS-proxy. For that I use the following `toggler` command:

```sh
toggler \
        -o 'ssh -fND 1080 -S /run/user/%i/ssh-%k-ctl.sock -M socks-proxy-host' \
        -O 'ssh -S /run/user/%i/ssh-%k-ctl.sock -O exit socks-proxy-host' \
        -i network-vpn-symbolic \
        -I network-vpn-disconnected-symbolic \
        -t "Toggle Proxy"
```

Enjoy :)
