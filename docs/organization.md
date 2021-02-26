# Organization

Since Monza is aimed at virtualized scenarios, we classify code as either belonging to the guest or the host.
Elements of the guest end up inside the virtual machine together with the application.
Elements of the host reside the parent partition of the virtual machine, interating with the VMM, any shared memory or the outside world.

## Guest
There is a single [guest](./guest.md) implementation at the moment.

## Host
There is no special code to go on the host at the moment.