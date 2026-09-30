#!/bin/bash
# Linux build of WTF in WSL (Ubuntu). Run as root. Temporarily swaps /etc/resolv.conf
# (tailscale-managed and dead inside WSL) and restores it on exit.
set -euo pipefail
LOG=/root/wtf_linux_build.log
exec > >(tee "$LOG") 2>&1
cp -a /etc/resolv.conf /etc/resolv.conf.wtf_backup
restore() { cp -a /etc/resolv.conf.wtf_backup /etc/resolv.conf; rm -f /etc/resolv.conf.wtf_backup; echo "resolv.conf restored"; }
trap restore EXIT
rm -f /etc/resolv.conf
printf 'nameserver 192.168.1.254\nnameserver 8.8.8.8\n' > /etc/resolv.conf

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq || true
apt-get install -y -qq zip unzip libasound2-dev libjack-dev ladspa-sdk libcurl4-openssl-dev \
  libfreetype6-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev \
  mesa-common-dev pkg-config

REPO=/mnt/c/Users/Jason/source/repos/Glitchwave
SRC=/root/wtf_src
rm -rf "$SRC" && mkdir -p "$SRC"
git -c safe.directory='*' -C "$REPO" archive HEAD | tar -x -C "$SRC"
cd "$SRC"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8

OUT="$REPO/dist_v0.60"
mkdir -p "$OUT"
rm -rf /root/wtf_pkg && mkdir /root/wtf_pkg
cp -r build/Wtf567_artefacts/Release/. /root/wtf_pkg/
find /root/wtf_pkg \( -name '*.a' -o -name '*.lib' -o -name '*.exp' \) -delete
rm -f "$OUT/wtf-linux-x86_64.zip"
(cd /root/wtf_pkg && zip -qr "$OUT/wtf-linux-x86_64.zip" .)
cp "$LOG" "$OUT/build_linux_wsl.log"
echo "LINUX_BUILD_DONE"
ls -l "$OUT/wtf-linux-x86_64.zip"
