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
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if ! mkdir -p ${OUTDIR}; then
    echo "Error: Could not create directory path '${OUTDIR}'."
    exit 1
fi

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

    echo "Build the kernel"
    make mrproper
    make defconfig
    make -j$(nproc)
    cp arch/${ARCH}/boot/Image ${OUTDIR}
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,var}
mkdir -p ${OUTDIR}/rootfs/{usr/bin,usr/lib,usr/sbin,var/log}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    echo "Configure busybox"
	make distclean
	make defconfig
else
    cd busybox
fi

echo "Make and install busybox"
make -j$(nproc)
make CONFIG_PREFIX=${OUTDIR}/rootfs install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc --print-sysroot)
echo "Copying libraries from sysroot: ${SYSROOT}"
sudo cp -a ${SYSROOT}/lib/* ${OUTDIR}/rootfs/lib
sudo cp -a ${SYSROOT}/lib64/* ${OUTDIR}/rootfs/lib64

echo "Creating device nodes in rootfs/dev"
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1

echo "Clean and build the writer utility"
cd ${FINDER_APP_DIR}
make clean
make

echo "Copying finder related scripts and executables to the rootfs/home"
sudo cp finder.sh ${OUTDIR}/rootfs/home/
sudo cp writer ${OUTDIR}/rootfs/home/
sudo cp -RL conf ${OUTDIR}/rootfs/home/
sudo cp finder-test.sh ${OUTDIR}/rootfs/home/
sudo cp autorun-qemu.sh ${OUTDIR}/rootfs/home/

# Chown the root directory
echo "Changing ownership of the rootfs to root:root"
sudo chown -R root:root ${OUTDIR}/rootfs

# Create initramfs.cpio.gz
echo "Creating initramfs.cpio.gz from rootfs"
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root | gzip -9 > ${OUTDIR}/initramfs.cpio.gz

echo "Done!"
