
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

# Conflicts

You may need to blacklist the `hanwang` driver, if installed. It will
think the Parblo Coast10 is an ArtPen tablet, but then will fail to do
anything useful.
