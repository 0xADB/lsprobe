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
git checkout tags/v5.0
cd ./security
git clone https://github.com/0xADB/lsprobe.git
cd ..
```
Add to `security/Kconfig`:
```
source security/lsprobe/Kconfig
```
after similar lines before the `choice` block.

Add to `security/Makefile`:
- after
```
subdir-$(CONFIG_SECURITY_YAMA)         += yama
```
line:
```
subdir-$(CONFIG_SECURITY_LSPROBE)      += lsprobe
```
- after
```
obj-$(CONFIG_SECURITYFS)               += inode.o
```
line
```
obj-$(CONFIG_SECURITY_LSPROBE)         += lsprobe/
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

### Building

In the `linux-stable` directory:
```
make clean
make -j $(getconf _NPROCESSORS_ONLN) deb-pkg LOCALVERSION="+lsprobed"
```
On success the .deb package will be in the directory above:
```
ls ../*.deb
```

## References

- https://blog.ptsecurity.com/2012/09/writing-linux-security-module.html
- https://www.maketecheasier.com/build-custom-kernel-ubuntu/
- https://elixir.bootlin.com
- http://dazuko.dnsalias.org
