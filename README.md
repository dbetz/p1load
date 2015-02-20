# p1load

p1load is a tiny, nimble cross-platform loader library for the Propeller microcontroller.

## Building

`p1load` can be built in multiple ways.

### GNU Make

Building using plain old `make` is straight-forward. To build for the various platforms,
specify the target operating system using the `OS` variable.

```
OS=macosx make
```

Possible options:

* Mac OS X: `OS=macosx`
* Linux: `OS=linux`
* Raspberry Pi: `OS=raspberrypi`
* Windows MinGW: `OS=msys`

If no platform is specified, the default `macosx` is used.

### qmake

`qmake` project files can be found in the `qmake/` directory.

```
cd qmake
qmake
make
```

`qmake` will automatically build in platform-specific features except
for Raspberry Pi. Pass `CPU=armhf` to `qmake` to enable GPIO support for Raspberry Pi.

```
qmake CPU=armhf
```
