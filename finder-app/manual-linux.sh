#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
TOOLCHAIN_LIBC=/usr/bin/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    # Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper 
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
	make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"

sha256sum ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image | awk '{ print $1 }' > Image.sha256sum
cp -p ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/
if [ ! -e "${OUTDIR}/Image" ]; then
	echo "Failed to copy image"
	exit 99
fi

sha256sum ${OUTDIR}/Image | awk '{ print $1 }' > Image_copy.sha256sum
diff Image_copy.sha256sum Image.sha256sum
if [  $? != 0 ]; then
	echo "Copy of Image was corrupted"
	exit 99
fi

echo "Creating the staging directory for the root filesystem"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

mkdir ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make -j8 CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd "${OUTDIR}"/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

echo "Adding library dependencies"
SYSROOT=$(realpath $(${CROSS_COMPILE}gcc -print-sysroot))
echo "SYSROOT is ${SYSROOT}"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter" | while read -r line; do
    INTERPRETER=$(echo "$line" | awk '{print $NF}' | sed 's/]$//')
    echo "Copying ${INTERPRETER} to ${OUTDIR}/rootfs"
    cp -L "${SYSROOT}${INTERPRETER}" "${OUTDIR}/rootfs/lib"
done

${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library" | while read -r line; do
    LIBRARY=$(echo "$line" | awk '{print $NF}' | sed 's/[][]//g')
    echo "Copying ${LIBRARY} to ${OUTDIR}/rootfs"
    cp -L "${SYSROOT}/lib64/${LIBRARY}" "${OUTDIR}/rootfs/lib64"
done


# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd "${FINDER_APP_DIR}"

make clean
make CROSS_COMPILE=aarch64-none-linux-gnu- all

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -p finder.sh  ${OUTDIR}/rootfs/home
cp -p finder-test.sh ${OUTDIR}/rootfs/home
cp -p writer ${OUTDIR}/rootfs/home
cp -p writer.sh ${OUTDIR}/rootfs/home
cp -p conf/username.txt ${OUTDIR}/rootfs/home
cp -p conf/assignment.txt ${OUTDIR}/rootfs/home
cp -p autorun-qemu.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

cd ${OUTDIR}
sha256sum initramfs.cpio | awk '{ print $1 }' > initramfs.cpio.sha256_original
if [ ! -e "initramfs.cpio" ]; then
	echo "Could not create initramfs.cpio"
	exit 99
fi

gzip -f initramfs.cpio
if [ ! -e "initramfs.cpio.gz" ]; then
	echo "Could not create initramfs.cpio.gz"
	exit 99
fi

gzip -kd initramfs.cpio.gz
sha256sum initramfs.cpio | awk '{ print $1 }' > initramfs.cpio.sha256_copy
diff initramfs.cpio.sha256_original initramfs.cpio.sha256_copy
if [ $? != 0 ]; then
	echo "initramfs was corrupt"
	exit 99
fi
