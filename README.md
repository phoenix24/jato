# Jato VM

[![Build Status](https://travis-ci.org/jatovm/jato.png?branch=master)](http://travis-ci.org/jatovm/jato)

Jato is an implementation of the Java virtual machine. It includes a VM and a
JIT compiler for the x86 machine architecture and supports the JNI API. Jato
uses Boehm GC as its garbage collector and relies on GNU Classpath to provide
essential Java APIs.

## Features

* JIT-only execution on x86 architecture
* Uses GNU Classpath for essential classes
* Boehm garbage collector
* Runs on Linux

## Installation

### Prerequisites

**Fedora**

```
$ sudo yum install ecj libffi-devel binutils-devel glib2-devel bison llvm-devel llvm-static
```

**Ubuntu**

```
$ sudo apt-get install ecj libffi-dev binutils-dev libiberty-dev libglib2.0-dev bison llvm-dev
```

**Archlinux**

```
$ pacman -S eclipse-ecj classpath libffi
```

### GNU Classpath

GNU Classpath needs to be built and installed from sources.

First install dependencies that are required to build GNU Classpath:

**Fedora**

```
$ sudo yum install java-1.7.0-openjdk antlr GConf2-devel gtk2-devel gettext-devel texinfo
```

**Ubuntu**

```
$ sudo apt-get install openjdk-6-jdk antlr libgconf2-dev libgtk2.0-dev ecj fastjar pccts
```

Then download the sources from:

  ftp://ftp.gnu.org/pub/gnu/classpath/classpath-0.99.tar.gz

You can then compile GNU Classpath:

```
$ ./configure --disable-Werror --disable-plugin
$ make
```

and install it to ``/usr/local``:

```
$ sudo make install
```

### Build and Install

To compile the VM and run all the tests:

```
$ make check
```

All tests should pass.

You can now install Jato with:

```
$ make install
```

The command installs an executable ``jato`` to ``$HOME/bin``.

## Usage

Jato uses the same command line options as ``java``.

To run a class:

```
$ jato <class name>
```

To specify classpath, use:

```
$ jato -cp <jar files or directories> <class name>
```

You can also execute a Jar file with:

```
$ jato -jar <jar file>
```

Jato also supports variety of command line options for debugging and tracing
purposes. See the file ``Documentation/options.txt`` for details.

## License

Copyright © 2005-2013 Pekka Enberg and contributors

Jato is distributed under the 2-clause BSD license.
