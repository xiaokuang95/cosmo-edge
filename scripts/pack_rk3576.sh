#!/bin/bash
# Build a board tarball: official 1.1.0 layout + custom engine/web + OSD helpers.
# Does not include lab-only algorithms (e.g. 1111).
#
#   ./scripts/pack_rk3576.sh
#   ./scripts/pack_rk3576.sh --from=root@192.168.2.27 --out=/tmp
set -euo pipefail
cd "$(dirname "$0")/.."

FROM="root@192.168.2.27"
STOCK="${COSMO_PKG:-$(cd ../ && pwd)/cosmo-V1.1.0-980cca8314eba775bd9ef2cab90b6f32.tar.gz}"
OUT="${COSMO_PACK_OUT:-$(cd ../ && pwd)}"

for a in "$@"; do
  case "$a" in
    --from=*) FROM="${a#--from=}" ;;
    --stock=*) STOCK="${a#--stock=}" ;;
    --out=*) OUT="${a#--out=}" ;;
    -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
    *) echo "usage: $0 [--from=root@IP] [--stock=cosmo-V1.1.0-*.tar.gz] [--out=dir]"; exit 1 ;;
  esac
done
[ -f "$STOCK" ] || { echo "missing $STOCK"; exit 1; }
mkdir -p "$OUT"

SHA=$(ssh -o ConnectTimeout=8 "$FROM" 'cd /opt/cosmo-edge && git rev-parse --short HEAD')
DAY=$(date +%Y%m%d)
NAME="cosmo-custom-rk3576-${DAY}-${SHA}"
WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

echo "unpack stock $STOCK"
tar xzf "$STOCK" -C "$WORKDIR"
ROOT=$(find "$WORKDIR" -mindepth 1 -maxdepth 1 -type d | head -1)
[ -n "$ROOT" ] || { echo "empty stock tarball"; exit 1; }

echo "pull engine+web from $FROM"
ssh -o ConnectTimeout=8 "$FROM" 'cat /appfs/cosmo_wander/cwai_data/bin/cosmo-engine' > "$ROOT/bin/cosmo-engine"
chmod 775 "$ROOT/bin/cosmo-engine"
rm -rf "$ROOT/web"
ssh -o ConnectTimeout=8 "$FROM" 'tar czf - -C /appfs/cosmo_wander/cwai_data web' | tar xzf - -C "$ROOT"

mkdir -p "$ROOT/opt-cosmo-osd"
cp scripts/osd_rtsp_server.py scripts/onvif_server.py "$ROOT/opt-cosmo-osd/"
cp scripts/cosmo-osd-rtsp.service scripts/cosmo-onvif.service "$ROOT/scripts/"
cat > "$ROOT/scripts/install-osd.sh" << 'EOS'
#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p /opt/cosmo-osd /etc/netplan
cp "$ROOT/opt-cosmo-osd/"*.py /opt/cosmo-osd/
chmod 755 /opt/cosmo-osd/*.py
cp "$ROOT/scripts/cosmo-osd-rtsp.service" "$ROOT/scripts/cosmo-onvif.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now cosmo-osd-rtsp.service cosmo-onvif.service
EOS
chmod 755 "$ROOT/scripts/install-osd.sh"

# Keep stock install.sh; document two-step. Stamp version.
echo "$NAME" > "$ROOT/bin/version-custom.txt"

TAR="$OUT/${NAME}.tar.gz"
echo "pack $TAR"
# flatten to NAME/ so extract is obvious
mv "$ROOT" "$WORKDIR/$NAME"
tar czf "$TAR" -C "$WORKDIR" "$NAME"
ls -lh "$TAR"
echo "install on board:"
echo "  scp $TAR root@BOARD:/tmp/"
echo "  ssh root@BOARD 'tar xzf /tmp/${NAME}.tar.gz -C /tmp && sh /tmp/${NAME}/scripts/install.sh && sh /tmp/${NAME}/scripts/install-osd.sh'"
