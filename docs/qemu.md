# Running tests and apps using QEMU

The same machine used to [build the guest and test apps](./build.md) can be used to run the apps using QEMU once the build has finished.

# Automatically running all unit tests

From the `build` directory, you can run
```
ninja guest-verona-rt-test
```

### Manually running test app in QEMU using emulation

From the `build` directory, you can run
```
qemu-system-x86_64 -cpu IvyBridge -no-reboot -nographic -smp cores=4 -m 1G -kernel {CMAKE_BUILD_TYPE}/guests/qemu-{TestName}.img
```
Note that the `crt-malloc` test requires at least 8GB of memory.

## Debugging test app in QEMU with GDB

From the `build` directory, you can run
```
qemu-system-x86_64 -cpu IvyBridge -no-reboot -nographic -smp cores=4 -m 1G -kernel {CMAKE_BUILD_TYPE}/guests/qemu-{TestName}.img -s -S
```

From a different terminal in the the `build` directory you can connect GDB and load the symbol file with
```
MONZA_QEMU_APP={CMAKE_BUILD_TYPE}/guests/{TestName} gdb -x ../guest-verona-rt/scripts/gdb-qemu.py
```

## Using QEMU-KVM instead of emulation

In order to use KVM with QEMU the CPU descriptor on the command line needs to change from `-cpu IvyBridge` to `-enable-kvm -cpu host,+invtsc`.
Keep in mind while debugging that when using KVM hardware breakpoints (`hbreak` are needed).
