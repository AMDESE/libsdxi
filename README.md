# Library SDXI

## Table of contents

1. [Build](#build)
3. [Sample Code](#sample)
4. [Install](#install)

## Build <a name="build"></a>

To build libsdxi library, use the following commands. Please make sure you have
related packages, such as automake and libtool, installed on your system.

```
> ./autogen.sh
> ./configure
> make
```

## Sample Code <a name="sample"></a>

To build SDXI sample code, use the following commands. Note it is required to
have static C libraries (e.g. glibc-static) installed on your system because
samples are built as static binaries.

```
> cd samples
> make
```

To test sample programs, execute them on a SDXI machine with proper driver loaded.
NOTE: depending on /dev/sdxi permission, you might need to run the programs with
root permission:

```
> memcopy
SDXI memory copy test ...
    memory buffer src = 0x774000
    memory buffer dst = 0x776000
Memory copy ==> SUCESS
```

## Install <a name="install"></a>

To install the library, use the following command:

```
> make install
```
