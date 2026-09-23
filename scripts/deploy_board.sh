#!/bin/bash
# One-shot board deploy: build (optional) -> install engine+web -> install OSD RTSP/ONVIF services.
# Run ON THE BOARD from repo root:  ./scripts/deploy_board.sh [--no-build] [--proxy http://IP:7890]
set -euo pipefail
cd "$(dirname "$0")/.."

PROXY="${COSMO_PROXY:-}"
NO_BUILD=0
for a in "$@"; do
  case "$a" in
    --no-build) NO_BUILD=1 ;;
    --proxy=*)  PROXY="${a#--proxy=}" ;;
    *) echo "usage: $0 [--no-build] [--proxy http://ip:7890]"; exit 1 ;;
  esac
done
[ -n "$PROXY" ] && export COSMO_PROXY="$PROXY"

# 1. build
if [ "$NO_BUILD" = 0 ]; then
  ./scripts/board-native-build.sh
fi
ENGINE=build_rknn/cosmo-engine
[ -f "$ENGINE" ] || { echo "missing $ENGINE (build first, drop --no-build)"; exit 1; }

# 2. install engine + web
systemctl stop cosmo.service
killall -9 cosmo-engine nginx srs 2>/dev/null || true
cp -a "$ENGINE" /appfs/cosmo_wander/cwai_data/bin/cosmo-engine
chmod 775 /appfs/cosmo_wander/cwai_data/bin/cosmo-engine
if [ -d build_rknn/install/web ]; then
  rsync -a --delete build_rknn/install/web/ /appfs/cosmo_wander/cwai_data/web/
  mkdir -p /appfs/cosmo_wander/cwai_data/files/Interface
  cp -a build_rknn/install/web/staticfile/. /appfs/cosmo_wander/cwai_data/files/Interface/ 2>/dev/null || true
fi
systemctl start cosmo.service
for i in $(seq 1 30); do systemctl is-active --quiet cosmo.service && break; sleep 1; done
systemctl is-active --quiet cosmo.service || { journalctl -u cosmo.service -n 30 --no-pager; exit 1; }
echo "engine+web deployed"

# 3. OSD RTSP relay + ONVIF service (skip apt when already installed)
export DEBIAN_FRONTEND=noninteractive
PKGS="libgstrtspserver-1.0-0 gstreamer1.0-rtsp gir1.2-gst-rtsp-server-1.0 python3-gi"
if ! dpkg -s $PKGS >/dev/null 2>&1; then
  [ -n "$PROXY" ] && { export http_proxy="$PROXY" https_proxy="$PROXY"; }
  apt-get install -y --no-install-recommends $PKGS
fi
mkdir -p /opt/cosmo-osd
cp scripts/osd_rtsp_server.py scripts/onvif_server.py /opt/cosmo-osd/
cp scripts/cosmo-osd-rtsp.service scripts/cosmo-onvif.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now cosmo-osd-rtsp.service cosmo-onvif.service
systemctl restart cosmo-osd-rtsp.service cosmo-onvif.service
sleep 3
systemctl is-active --quiet cosmo-osd-rtsp.service && systemctl is-active --quiet cosmo-onvif.service

IP=$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n "s/.*src \([0-9.]*\).*/\1/p")
echo "deploy OK  ip=$IP"
echo "  web:    http://$IP/"
echo "  rtsp:   rtsp://$IP:554/live/<channel>_<task>"
echo "  onvif:  port 8899 + WS-Discovery 3702 (NVR: ONVIF, any user/pass)"
