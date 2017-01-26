
# Warning

This driver is of alpha quality (or less.) It technically works, and
it doesn't cause *my* computer to have problems. The same cannot be
guaranteed of *your* computer.

# Building

You will need linux kernel headers to compile kernel modules. Under
Arch, you can get these with:

```
sudo pacman -S linux-headers
```

No configuration is required to build the module; simply open a shell
and run make.

```
$ make

make -C /lib/modules/[..kernel..]/build M=/home/skrylar/[..path..] modules
make[1]: Entering directory '/usr/lib/modules/[..kernel..]/build'
  CC [M]  /home/skrylar/[..path..]/parblo-coast10.o
  Building modules, stage 2.
  MODPOST 1 modules
  LD [M]  /home/skrylar/[..path..]/parblo-coast10.ko
make[1]: Leaving directory '/usr/lib/modules/[..kernel..]/build'
```

Then load the module:

```
$ sudo insmod parblo-coast10
```

NOTE: You must load the module every time you start your computer.

# Xorg setup

You will need to edit your `/etc/X11/xorg.conf` and add the following
lines:

```
Section "InputClass"
	Identifier "Parblo Coast 10"
	MatchUSBID "0b57:8534"
	MatchDevicePath "/dev/input/event*"
	Driver "wacom"
EndSection
```

This instructs Xorg to treat the tablet as a wacom device, so pressure
and pen information from the driver will be sent to Krita et all.

# Conflicts

If the `hanwang` driver is installed, you will need to uninstall or
blacklist it. ArtPen tablets have the same "name" to the kernel as the
Parblo tablet and will cause the wrong driver to be loaded.
