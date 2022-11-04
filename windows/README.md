# Windows host for the application framework.

**WARNING** Code in this folder is highly experimental, it is likely that it will not work on your machine.
We apologise, but we don't have the cycles to prioritise fixing bugs in this code for now.

This folder implements a Windows host option for the application framework.
The goal is that an application developer could reuse the same source files they used to implement the Linux host and just recompile it on Windows.
Obviously, the host code needs to be written in a cross-platform way to leverage this ability.
The guest image will be the same ELF guest as on Linux and needs to be copied over.
On Windows currently use the [Host Compute System](https://learn.microsoft.com/en-us/virtualization/api/hcs/overview) to run and manage our guest VM.

## Requirements

Currently we use a `CMake` + `ninja` + `clang` build pipeline to ensure that we are as closely matched with the Linux build as possible.
* `clang`: 13.0.1 Windows release to best match the other builds.
* `MSVC`: 14.33.
* `Win11 SDK`: 10.0.22000.

## Usage

Link your host code with the new `monza-app-host` target in the local `app-framework/host` instead of the one in the original location.
No need to build the guest code as that needs to happen on Linux.
The image file to use with this host is the simple ELF guest image (not the QEMU artefact).
An example is included in `apps/example`.
