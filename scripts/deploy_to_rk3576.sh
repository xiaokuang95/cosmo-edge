#!/bin/bash
# Install CosmoEdge + custom overlay onto an RK3576 board.
# Target must already SSH as Debian aarch64. Bare metal: flash Debian first.
#
# Empty board:
#   ./scripts/deploy_to_rk3576.sh root@192.168.2.16 --bootstrap --proxy=http://192.168.2.123:7890
# Already has Cosmo:
#   ./scripts/deploy_to_rk3576.sh root@192.168.2.16
set -euo pipefail
cd "$(dirname "$0")/.."

FROM="root@192.168.2.27"
TO=""
PROXY="${COSMO_PROXY:-}"
BOOTSTRAP=0
PKG="${COSMO_PKG:-$(cd ../ && pwd)/cosmo-V1.1.0-980cca8314eba775bd9ef2cab90b6f32.tar.gz}"

for a in "$@"; do
  case "$a" in
    --bootstrap) BOOTSTRAP=1 ;;
    --from=*) FROM="${a#--from=}" ;;
    --proxy=*) PROXY="${a#--proxy=}" ;;
    --pkg=*) PKG="${a#--pkg=}" ;;
    --help|-h) sed -n '2,12p' "$0"; exit 0 ;;
    *@*) TO="$a" ;;
    *) echo "usage: $0 root@NEW_IP [--bootstrap] [--from=root@SRC] [--proxy=http://ip:7890]"; exit 1 ;;
  esac
done
[ -n "$TO" ] || { echo "missing target, e.g. root@192.168.2.16"; exit 1; }

HOST="${TO#*@}"
ssh -o ConnectTimeout=8 "$TO" 'uname -m' | grep -q aarch64 \
  || { echo "$TO is not aarch64 / SSH failed"; exit 1; }

# Keep wall clock in sync so apt Release files validate.
NOW=$(date '+%Y-%m-%d %H:%M:%S %z')
ssh "$TO" "date -s '$NOW' >/dev/null || true"

pin_net_fn() {
  cat << 'PIN'
pin_live_ip() {
  local ip gw
  ip=$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.*src \([0-9.]*\).*/\1/p')
  gw=$(ip -4 route get 1.1.1.1 2>/dev/null | awk '{for(i=1;i<NF;i++) if($i=="via") print $(i+1)}')
  [ -n "$ip" ] || return 0
  [ -n "$gw" ] || gw="${ip%.*}.1"
  mkdir -p /userdata/cwaiuserdata/conf
  cat > /userdata/cwaiuserdata/conf/netCradConf.json <<EOF
{
  "dns1": "114.114.114.114",
  "dns2": "",
  "main": {"dhcp": 0, "ethName": "eth0", "gateway": "$gw", "ipAddr": "$ip", "netMask": "255.255.255.0"},
  "sub": {"dhcp": 0, "ethName": "eth1", "gateway": "", "ipAddr": "192.168.1.18", "netMask": "255.255.255.0"}
}
EOF
  echo "pinned $ip gw $gw"
}
PIN
}

if [ "$BOOTSTRAP" = 1 ]; then
  [ -f "$PKG" ] || { echo "missing official package: $PKG"; exit 1; }
  echo "bootstrap Cosmo from $PKG -> $TO"
  scp -q "$PKG" "$TO:/tmp/cosmo-stock.tar.gz"
  ssh "$TO" "PROXY='$PROXY' bash -s" << REMOTE
set -euo pipefail
export LANG=C LC_ALL=C PATH="/usr/sbin:/usr/bin:/sbin:/bin"
export DEBIAN_FRONTEND=noninteractive
$(pin_net_fn)
if [ -n "${PROXY:-}" ]; then export http_proxy="$PROXY" https_proxy="$PROXY"; fi
apt-get -o Acquire::Check-Valid-Until=false update -qq || true
# Cosmo ships its own nginx/ffmpeg; do not pull Debian ffmpeg (fragile deps).
apt-get -o Acquire::Check-Valid-Until=false install -y --no-install-recommends --fix-missing \
  tar python3 rsync ca-certificates nginx \
  libgstrtspserver-1.0-0 gstreamer1.0-rtsp gir1.2-gst-rtsp-server-1.0 python3-gi || true
# Cosmo invokes /usr/sbin/nginx itself; keep the binary, not the distro service.
systemctl disable --now nginx 2>/dev/null || true
mkdir -p /etc/netplan /opt/cosmo-data /opt/cosmo-appfs /userdata /appfs
grep -q ' /userdata ' /proc/mounts || {
  mount --bind /opt/cosmo-data /userdata
  grep -q ' /userdata ' /etc/fstab || echo '/opt/cosmo-data /userdata none bind 0 0' >> /etc/fstab
}
grep -q ' /appfs ' /proc/mounts || {
  mount --bind /opt/cosmo-appfs /appfs
  grep -q ' /appfs ' /etc/fstab || echo '/opt/cosmo-appfs /appfs none bind 0 0' >> /etc/fstab
}
rm -rf /tmp/cosmo-stock
mkdir -p /tmp/cosmo-stock
tar xzf /tmp/cosmo-stock.tar.gz -C /tmp/cosmo-stock
INSTALL=\$(find /tmp/cosmo-stock -name install.sh | head -1)
[ -n "\$INSTALL" ] || { echo "install.sh not in package"; exit 1; }
sh "\$INSTALL"
systemctl daemon-reload
pin_live_ip
timeout 25 systemctl start cosmo.service || true
sleep 3
systemctl is-active --quiet cosmo.service || { journalctl -u cosmo.service -n 30 --no-pager; exit 1; }
echo "stock Cosmo installed"
REMOTE
fi

ssh "$TO" 'test -x /appfs/cosmo_wander/cwai_data/bin/cosmo-engine' \
  || { echo "$TO still has no Cosmo. Re-run with --bootstrap."; exit 1; }

echo "overlay custom engine+web from $FROM -> $TO"
ssh -o ConnectTimeout=8 "$FROM" 'tar czf - -C /appfs/cosmo_wander/cwai_data bin/cosmo-engine web' \
  | ssh "$TO" 'tar xzf - -C /appfs/cosmo_wander/cwai_data'
ssh "$TO" 'chmod 775 /appfs/cosmo_wander/cwai_data/bin/cosmo-engine'

echo "OSD RTSP/ONVIF helpers"
scp -q scripts/osd_rtsp_server.py scripts/onvif_server.py "$TO:/tmp/"
scp -q scripts/cosmo-osd-rtsp.service scripts/cosmo-onvif.service "$TO:/tmp/"

ssh "$TO" "PROXY='$PROXY' bash -s" << REMOTE
set -euo pipefail
export LANG=C LC_ALL=C PATH="/usr/bin:/bin:/usr/local/bin"
export DEBIAN_FRONTEND=noninteractive
$(pin_net_fn)
pin_live_ip
mkdir -p /etc/netplan
systemctl disable --now nginx 2>/dev/null || true
systemctl stop cosmo.service || true
killall -9 cosmo-engine nginx srs 2>/dev/null || true
sleep 1
timeout 25 systemctl start cosmo.service || true
sleep 3
systemctl is-active --quiet cosmo.service || { journalctl -u cosmo.service -n 20 --no-pager; exit 1; }

PKGS="libgstrtspserver-1.0-0 gstreamer1.0-rtsp gir1.2-gst-rtsp-server-1.0 python3-gi"
if ! dpkg -s \$PKGS >/dev/null 2>&1; then
  if [ -n "\${PROXY:-}" ]; then export http_proxy="\$PROXY" https_proxy="\$PROXY"; fi
  apt-get -o Acquire::Check-Valid-Until=false install -y --no-install-recommends --fix-missing \$PKGS
fi
mkdir -p /opt/cosmo-osd
cp /tmp/osd_rtsp_server.py /tmp/onvif_server.py /opt/cosmo-osd/
chmod 755 /opt/cosmo-osd/*.py
cp /tmp/cosmo-osd-rtsp.service /tmp/cosmo-onvif.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now cosmo-osd-rtsp.service cosmo-onvif.service
systemctl restart cosmo-osd-rtsp.service cosmo-onvif.service
sleep 2
systemctl is-active --quiet cosmo-osd-rtsp.service
systemctl is-active --quiet cosmo-onvif.service
IP=\$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.*src \\([0-9.]*\\).*/\\1/p')
echo "deploy OK  ip=\$IP"
echo "  web   http://\$IP/     (stock login admin/admin)"
echo "  rtsp  rtsp://\$IP:554/live/<channel>_<task>"
echo "  onvif \$IP:8899"
REMOTE
