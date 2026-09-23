#!/bin/bash
# Native RK3576 build on the board. Same SHA as xiaokuang95/cosmo-edge custom.
set -euo pipefail
export LANG=C LC_ALL=C LANGUAGE=C
export PATH="/root/.cargo/bin:/usr/local/bin:/usr/bin:/bin"
export http_proxy=${COSMO_PROXY:-http://192.168.2.123:7890}
export https_proxy=$http_proxy
export HTTP_PROXY="$http_proxy" HTTPS_PROXY="$https_proxy"
export no_proxy="127.0.0.1,localhost,192.168.2.0/24,registry.npmmirror.com"
export NO_PROXY="$no_proxy"
export CARGO_BUILD_JOBS=1
export NODE_OPTIONS="--max-old-space-size=1536"
export COSMO_BUILD_JOBS="${COSMO_BUILD_JOBS:-2}"
export COSMO_TARGET_CHIP=rk3576
export COSMO_RKLLM_REQUIRED=ON
export RKNN_ROOT=/opt/sdk-src/opt/rknn
export RKLLM_ROOT=/opt/sdk-src/opt/rkllm
export ROCKCHIP_MEDIA_ROOT=/opt/sdk-src/opt/rockchip-media/rk3576

ROOT=/opt/cosmo-edge
cd "$ROOT"
echo "==== $(date) start sha=$(git rev-parse --short HEAD) jobs=$COSMO_BUILD_JOBS ===="
./scripts/build_rknn.sh -c rk3576 -r "$RKNN_ROOT" -p "$ROCKCHIP_MEDIA_ROOT"
echo "==== $(date) done ===="
ls -lh "$ROOT/build_output/rk3576" || ls -lh "$ROOT/build_rknn/packages"
