# Bootloader for QEMU

When starting up Application Processors in QEMU there is no way to put them immediately into 64-bit mode with a predefined state.
To avoid needing to introduce 16-bit assembly into the Monza 64-bit binary, we use a simple bootloader to handle the transition to 64-bit for all our cores.

We chose the [Pure64](https://github.com/ReturnInfinity/Pure64) bootloader as it is very small in size and brings all cores into 64-bit mode before transitioning to the payload with a simple mechanism to assign work to the cores.

There is a simple wrapper to process the memory map extracted by Pure64 to match the format expected by Monza.

The bootloader and wrapper are prepended to the 64-bit Monza ELF images to form the QEMU-compatible images.
On hypervisor/VMM combinations that allow directly starting APs into a given 64-bit state the bootloader is not necessary.
