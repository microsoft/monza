Monza is a research unikernel aimed to explore the design space for a non-POSIX environment for running micro-service type workloads in the cloud.
It offers a minimal C/C++ development environment focused on in-memory computation with no filesystem, registry or other locally persistent state.

With Monza we are exploring a number of different research directions:
* New system-level components and abstractions to enable memory-safe and high-performance applications in cloud environments.
* Minimal environment for application deployment without a traditional operating system onto isolated VMs (for example using [AMD SEV-SNP](https://www.amd.com/system/files/TechDocs/SEV-SNP-strengthening-vm-isolation-with-integrity-protection-and-more.pdf)).

This research project is at an early stage and is open sourced to facilitate academic collaborations. We are keen to engage in research collaborations on this project, please do reach out to discuss this.

The project is not ready to be used outside of research. It has no connection to any production services running in the cloud today and there are currently no plans to make it into a product.

# Supported hypervisors and VMMs
Since Monza is targeting virtualized workloads in the cloud, it assumes an underlying virtualization environment to avoid some legacy x86 code requirements.
The currently supported platforms:
* QEMU with emulation.
* QEMU with KVM.

# Development Documents
## [Organization](docs/organization.md)
## [Building](docs/build.md)
## [Contributing](CONTRIBUTING.md)
## [Running tests and apps using QEMU](docs/qemu.md)
## [Developing new apps](docs/apps.md)