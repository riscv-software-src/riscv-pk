RISC-V Proxy Kernel and Boot Loader
=====================================

About
---------

The RISC-V Proxy Kernel, `pk`, is a lightweight application execution
environment that can host statically-linked RISC-V ELF binaries.  It is
designed to support tethered RISC-V implementations with limited I/O
capability and and thus handles I/O-related system calls by proxying them to
a host computer.

This package also contains the Berkeley Boot Loader, `bbl`, which is a
supervisor execution environment for tethered RISC-V systems.  It is
designed to host the RISC-V Linux port.

Build Steps
---------------

We assume that the RISCV environment variable is set to the RISC-V tools
install path, and that the riscv-gnu-toolchain package is installed.
Please note that building the binaries directly inside the source
directory is not supported; you need to use a separate build directory.

    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV --host=riscv64-unknown-elf \
                   --with-arch=rv64imafd \
                   --with-abi=lp64d
    $ make
    $ make install

Alternatively, the GNU/Linux toolchain may be used to build this package, by
setting `--host=riscv64-unknown-linux-gnu`. The `--with-arch` and `--with-abi`
flags are optional and can be used to specify an ISA and ABI other than the
default for the host toolchain.

The `install` step installs 64-bit build products into a directory
matching your host (e.g. `$RISCV/riscv64-unknown-elf`). 32-bit versions
are installed into a directory matching a 32-bit version of your host (e.g.
`$RISCV/riscv32-unknown-elf`).
