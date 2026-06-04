# Troubleshooting

## The item does not appear in “Add New Items”

Install system-wide and restart the panel:

```sh
make clean
make all PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
sudo make install PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
xfce4-panel -r
```

Verify files exist:

```sh
ls /usr/share/xfce4/panel/plugins/darkmode-switcher.desktop
ls /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/libdarkmode-switcher.so
```

## Panel changes but Thunar does not

GTK apps such as Thunar need `xfsettingsd` to receive Xfce theme changes.
The plugin starts it automatically if missing, but you can test manually:

```sh
pgrep -a xfsettingsd || xfsettingsd --replace &
```

Then restart Thunar if it still did not refresh:

```sh
thunar -q
thunar &
```

## Chrome, Firefox, or other apps do not change immediately

Some apps do not live-refresh system theme hints. Restart the app after toggling.
Also ensure the app is set to follow the system theme.

The plugin sets:

```sh
gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
gsettings set org.gnome.desktop.interface gtk-theme 'Adwaita-dark'
```

or switches them back for light mode.

## Check current state

```sh
xfconf-query -c xsettings -p /Net/ThemeName
xfconf-query -c xfce4-panel -p /panels/dark-mode
gsettings get org.gnome.desktop.interface color-scheme
gsettings get org.gnome.desktop.interface gtk-theme
pgrep -a xfsettingsd
```

## The panel theme only updates after restart

This is expected on some Xfce 4.18 setups. The plugin runs `xfce4-panel -r` after toggling so the panel repaints.
