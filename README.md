# lsprobe

A toy permissive linux security module for logging security ops.

## Building for Ubuntu Xenial

### Prerequisites

- About 5 GB of space on the hard drive
- A couple of hours for 2 x86 cores

### Dependencies

```
sudo apt-get install git build-essential kernel-package fakeroot libncurses5-dev libssl-dev ccache
```

### Sources

Get kernel sources and put module sourcess into `security` folder:
```
git clone git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
cd ./linux-stable
git checkout -b stable v5.0
cd ./security/
git clone https://github.com/0xADB/lsprobe.git && rm -rf ./lsprobe/.git
cd ..
```
Add to `security/Kconfig`:
```
source "security/integrity/Kconfig"
source "security/lsprobe/Kconfig" # <-- this line

```
Add to `security/Makefile`:
```
subdir-$(CONFIG_SECURITY_YAMA)         += yama
subdir-$(CONFIG_SECURITY_LSPROBE)      += lsprobe # <--this line

...

obj-$(CONFIG_SECURITYFS)               += inode.o
obj-$(CONFIG_SECURITY_LSPROBE)         += lsprobe/ # <-- and this line
```

### Configuration

Go to the `linux-stable` directory and clone current config and adapt it choosing default answers:
```
cp /boot/config-$(uname -r) .config
yes '' | make oldconfig
```
Customize it by
```
make menuconfig
```
selecting in the `Security options` directory the `Security probe` entry.


### Building package

In the `linux-stable` directory:
```
rm vmlinux-gdb.py # see Note below
make -j $(getconf _NPROCESSORS_ONLN) deb-pkg LOCALVERSION="-lsprobed"
```
On success the .deb package will be in the directory above:
```
ls ../*.deb
```

### Building just kernel

In the `linux-stable` directory:
```
make -j $(getconf _NPROCESSORS_ONLN) deb-pkg LOCALVERSION="-lsprobed"
sudo make modules_install install
sudo update-grub2
```

### Note

It is recommended to perform `make deb-pkg` on freshly downloaded sources. Otherwise, in `linux-stable` remove `vmlinux-gdb.py` symbolic link or dpkg-source will complain:
```
dpkg-source: error: cannot represent change to vmlinux-gdb.py:
dpkg-source: error:   new version is symlink to /mnt/shared_workspace/src/linux-stable/scripts/gdb/vmlinux-gdb.py
dpkg-source: error:   old version is nonexistent
...
dpkg-source: error: unrepresentable changes to source
dpkg-buildpackage: error: dpkg-source -i.git -b linux-stable gave error exit status 1
scripts/package/Makefile:70: recipe for target 'deb-pkg' failed
make[1]: *** [deb-pkg] Error 1
Makefile:1390: recipe for target 'deb-pkg' failed
make: *** [deb-pkg] Error 2
```

## References

- https://blog.ptsecurity.com/2012/09/writing-linux-security-module.html
- https://www.maketecheasier.com/build-custom-kernel-ubuntu/
- https://elixir.bootlin.com
- http://dazuko.dnsalias.org
- https://kernelnewbies.org/KernelBuild
