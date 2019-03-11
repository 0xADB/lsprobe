# lsprobe

A toy permissive linux security module for security ops researching.

## Building

### Prerequisites

- About 23 GB of the hard drive space (about 2.5 GB for sources plus 20 GB to build packages)
- 2-3 hours on 2 x86 cores with 4Gb RAM (see Notes)

### Dependencies

#### Ubuntu 16.04:
```
sudo apt-get install git build-essential kernel-package fakeroot libncurses5-dev libssl-dev 
```
#### Debian 9
```
apt-get install git build-essential fakeroot libncurses5-dev libssl-dev libelf-dev bison flex
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

obj-$(CONFIG_SECURITY_YAMA)            += yama/
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
make -j $(getconf _NPROCESSORS_ONLN) LOCALVERSION="-lsprobed"
sudo make modules_install install
sudo update-grub2
```

### Notes

- Before building `dep-pkg` target remove `vmlinux-gdb.py` symbolic link in `linux-stable` if any or dpkg-source will complain:
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
- Kernel building takes considerable time - on a debian 9 kvm guest with 2 x86-cores (and a browser playing music in a background):
```
$ time make -j $(getconf _NPROCESSORS_ONLN) LOCALVERSION="-lsprobed"
real	96m52.259s
user	137m26.076s
sys	13m59.624s
$ time make -j $(getconf _NPROCESSORS_ONLN) deb-pkg LOCALVERSION="-lsprobed"
real	130m55.841s
user	178m5.568s
sys	15m15.480s
```
Also note the hard drive space occupied after the build:
```
$ du -hs ./*
8.0K	./linux-5.0.0-lsprobed_5.0.0-lsprobed-1_amd64.buildinfo
4.0K	./linux-5.0.0-lsprobed_5.0.0-lsprobed-1_amd64.changes
632K	./linux-5.0.0-lsprobed_5.0.0-lsprobed-1.diff.gz
4.0K	./linux-5.0.0-lsprobed_5.0.0-lsprobed-1.dsc
162M	./linux-5.0.0-lsprobed_5.0.0-lsprobed.orig.tar.gz
11M	./linux-headers-5.0.0-lsprobed_5.0.0-lsprobed-1_amd64.deb
43M	./linux-image-5.0.0-lsprobed_5.0.0-lsprobed-1_amd64.deb
552M	./linux-image-5.0.0-lsprobed-dbg_5.0.0-lsprobed-1_amd64.deb
1012K	./linux-libc-dev_5.0.0-lsprobed-1_amd64.deb
21G	./linux-stable
```

## References

- https://blog.ptsecurity.com/2012/09/writing-linux-security-module.html
- https://www.maketecheasier.com/build-custom-kernel-ubuntu/
- https://elixir.bootlin.com
- http://dazuko.dnsalias.org
- https://kernelnewbies.org/KernelBuild
