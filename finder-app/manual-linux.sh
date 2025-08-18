#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

SOURCEDIR=$(pwd)
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_36_1
BUSYBOX_REPO=https://github.com/mirror/busybox.git
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_TARGET=aarch64-none-linux-gnu
CROSS_COMPILE=${CROSS_TARGET}-
CROSS_LIBRARIES=$(dirname $(which ${CROSS_COMPILE}gcc))/../${CROSS_TARGET}/libc

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Cleaning (mrproper) kernel for ${ARCH}"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    echo "Generating default configs (defconfig) for ${ARCH}"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    echo "Compiling the kernel for ${ARCH}"
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

    echo "Compiling the modules for ${ARCH}"
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

    echo "Building the devicetree for ${ARCH}"
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
mkdir -p ${OUTDIR}/rootfs/{bin,dev,home,lib,lib64,proc,sbin,sys,tmp,usr,var}
mkdir -p ${OUTDIR}/rootfs/usr/{bin,lib,sbin}
mkdir -p ${OUTDIR}/rootfs/var/log
mkdir -p ${OUTDIR}/rootfs/home/conf

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
#git clone git://busybox.net/busybox.git
git clone ${BUSYBOX_REPO} --depth 1 --single-branch --branch ${BUSYBOX_VERSION} busybox
    cd busybox
    #git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
echo "Configuring busybox"
make distclean

echo "Configuring busybox"
make defconfig

echo "Compiling busybox for ${ARCH} with cross-compiler ${CROSS_COMPILE}"
make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

echo "Installing busybox to ${OUTDIR}/rootfs"
make -j$(nproc) CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp ${CROSS_LIBRARIES}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp ${CROSS_LIBRARIES}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp ${CROSS_LIBRARIES}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp ${CROSS_LIBRARIES}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/tty c 5 0
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/tty0 c 4 0
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/tty1 c 4 1
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/tty2 c 4 2
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/tty3 c 4 3
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/tty4 c 4 4
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/ttyS0 c 4 64
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/ttyS1 c 4 65
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/ttyS2 c 4 66
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/ttyS3 c 4 67
sudo mknod -m 660 ${OUTDIR}/rootfs/dev/ttyS4 c 4 68

# TODO: Clean and build the writer utility
cd ${SOURCEDIR}
make clean
CROSS_COMPILE=${CROSS_COMPILE} make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp *.sh ${OUTDIR}/rootfs/home
cp writer ${OUTDIR}/rootfs/home
cp ../conf/*.txt ${OUTDIR}/rootfs/home/conf/

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
