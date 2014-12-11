RISC-V Proxy Kernel
=====================

About
---------

The RISC-V proxy kernel is a thin layer that services system calls generated
by code built and linked with the RISC-V newlib port.

Build Steps
---------------

We assume that the RISCV environment variable is set to the RISC-V tools
install path, and that the riscv-gcc package is installed.

    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV/riscv64-unknown-elf --host=riscv64-unknown-elf
    $ make
    $ make install
