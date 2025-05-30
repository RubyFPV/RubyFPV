#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

echo "-----------------------------------------------------------"
echo "RubyFPV: Attempting to install RTL8814AU driver (rtw88)..."
echo "-----------------------------------------------------------"
echo "This script will try to install dependencies, clone the driver"
echo "repository, compile, and install the driver."
echo "You might be prompted for your sudo password for installation steps."
echo ""

# Dependency Checks
echo "[1/5] Checking for required dependencies..."
MISSING_DEP=0
if ! command -v git &> /dev/null; then
    echo "Error: git is not installed. Please install it (e.g., sudo apt update && sudo apt install git)."
    MISSING_DEP=1
fi
if ! command -v make &> /dev/null; then
    echo "Error: make is not installed. Please install it (e.g., sudo apt update && sudo apt install build-essential)."
    MISSING_DEP=1
fi
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc is not installed. Please install it (e.g., sudo apt update && sudo apt install build-essential)."
    MISSING_DEP=1
fi

CURRENT_KERNEL=$(uname -r)
# For Raspberry Pi, headers package is usually raspberrypi-kernel-headers
# For other Debian/Ubuntu based systems, it's often linux-headers-${CURRENT_KERNEL}
# We'll check common locations. A more robust check might be needed for wider OS support.
if [ ! -d "/lib/modules/${CURRENT_KERNEL}/build" ] && [ ! -d "/usr/src/linux-headers-${CURRENT_KERNEL}" ]; then
    echo "Error: Kernel headers for ${CURRENT_KERNEL} not found."
    echo "Please install them. For Raspberry Pi: sudo apt update && sudo apt install raspberrypi-kernel-headers"
    echo "For other Debian/Ubuntu systems: sudo apt update && sudo apt install linux-headers-${CURRENT_KERNEL}"
    MISSING_DEP=1
fi

if [ "${MISSING_DEP}" -eq 1 ]; then
    echo "-----------------------------------------------------------"
    echo "Error: Missing dependencies. Please install them and try again."
    echo "-----------------------------------------------------------"
    exit 1
fi
echo "All required build dependencies appear to be present."
echo ""

# Define Repository and Clone Directory
DRIVER_REPO="https://github.com/Kfro89/rtw88.git" # User provided this
# It's better to clone into a specific versioned/project dir if possible,
# or let the user manage where they clone it if Ruby is also cloned from git.
# For now, /tmp is a temporary build location.
# A more persistent location might be ~/ruby_drivers/rtw88
BUILD_PARENT_DIR="${HOME}/ruby_driver_builds"
CLONE_DIR="${BUILD_PARENT_DIR}/rtw88"

echo "[2/5] Preparing build directory: ${CLONE_DIR}"
mkdir -p "${CLONE_DIR}"
cd "${CLONE_DIR}"
echo "Changed directory to $(pwd)"
echo ""

# Clone or Update Repository
echo "[3/5] Cloning/Updating driver repository: ${DRIVER_REPO}"
if [ -d ".git" ]; then
    echo "Driver source directory already exists. Attempting to update..."
    if git pull; then
        echo "Repository updated successfully."
    else
        echo "Warning: Failed to update repository. Using existing files."
    fi
else
    echo "Cloning driver repository..."
    # Clone into current directory which is ${CLONE_DIR}
    if ! git clone "${DRIVER_REPO}" .; then
        echo "-----------------------------------------------------------"
        echo "Error: Failed to clone driver repository."
        echo "-----------------------------------------------------------"
        exit 1
    fi
fi
echo ""

# Compile the Driver
echo "[4/5] Compiling the rtw88 driver... This may take several minutes."
# Some drivers (like older ones for 8812au) might have specific platform options.
# The rtw88 driver generally doesn't need specific platform variables for RPi for the make command itself.
if make clean; then
    echo "Cleaned previous build artifacts."
else
    echo "Warning: 'make clean' failed, continuing build..."
fi

# Determine number of processor cores for parallel compilation
NUM_CORES=$(nproc 2>/dev/null || echo 1) # Defaults to 1 if nproc fails
echo "Using ${NUM_CORES} core(s) for compilation."

if make -j${NUM_CORES}; then
    echo "Driver compilation completed successfully."
else
    echo "-----------------------------------------------------------"
    echo "Error: Driver compilation failed."
    echo "Please check the output above for error messages."
    echo "Ensure you have enough free disk space and memory."
    echo "-----------------------------------------------------------"
    exit 1
fi
echo ""

# Install the Driver
echo "[5/5] Installing the driver modules..."
if sudo make install; then
    echo "Driver installation completed successfully."
    # depmod -a is usually run by "make install" for kernel modules.
else
    echo "-----------------------------------------------------------"
    echo "Error: Driver installation failed (sudo make install)."
    echo "Please check the output above for error messages."
    echo "-----------------------------------------------------------"
    exit 1
fi
echo ""

# The C code will attempt to modprobe after this script exits successfully.

echo "-----------------------------------------------------------"
echo "RTL8814AU (rtw88) driver installation script finished successfully."
echo "The system should now attempt to load the rtw_8814au module."
echo "-----------------------------------------------------------"
exit 0
