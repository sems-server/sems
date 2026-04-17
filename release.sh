#!/usr/bin/env bash
# Build RHEL 7/8/9/10 RPMs for SEMS and lay them out for static hosting (e.g. nginx).
#
# Usage:
#   cd /path/to/sems-sources      # a checked-out sems repository
#   ./release.sh                  # writes RPMs to ../rhel
#
# Environment overrides:
#   EL_VERSIONS       Space-separated list of EL majors to build. Default "7 8 9 10".
#   OUT_DIR           Output directory. Default "$(dirname $PWD)/rhel".
#   CONTAINER_ENGINE  "docker" or "podman". Auto-detected.
#
# Layout produced under $OUT_DIR:
#   <el>/x86_64/*.rpm
#   <el>/SRPMS/*.src.rpm
#   <el>/{x86_64,SRPMS}/repodata/   (only if createrepo_c is installed locally)
#
# The existing pkg/rpm/sems.spec + Dockerfile-rhel<N> are the single source of truth
# for the build; this script just orchestrates them and extracts the artifacts.

set -euo pipefail

SRC_DIR="$(pwd)"
if [[ ! -f "$SRC_DIR/VERSION" || ! -f "$SRC_DIR/pkg/rpm/sems.spec" ]]; then
    echo "error: run this script from the root of a sems checkout" >&2
    exit 1
fi

VERSION="$(cat "$SRC_DIR/VERSION")"
OUT_DIR="${OUT_DIR:-$(cd "$SRC_DIR/.." && pwd)/rhel}"
EL_VERSIONS="${EL_VERSIONS:-7 8 9 10}"

ENGINE="${CONTAINER_ENGINE:-}"
if [[ -z "$ENGINE" ]]; then
    if command -v docker >/dev/null 2>&1; then
        ENGINE=docker
    elif command -v podman >/dev/null 2>&1; then
        ENGINE=podman
    else
        echo "error: neither docker nor podman found in PATH" >&2
        exit 1
    fi
fi

echo "sems version : $VERSION"
echo "source dir   : $SRC_DIR"
echo "output dir   : $OUT_DIR"
echo "el targets   : $EL_VERSIONS"
echo "engine       : $ENGINE"

mkdir -p "$OUT_DIR"

build_one_el() {
    local el="$1"
    local dockerfile="$SRC_DIR/Dockerfile-rhel${el}"
    local image="sems-release-el${el}:${VERSION}"
    local dest="$OUT_DIR/${el}"
    local staging cid

    if [[ ! -f "$dockerfile" ]]; then
        echo "skip el${el}: $dockerfile not found"
        return 0
    fi

    echo
    echo "=============================================="
    echo " Building RPMs for el${el}"
    echo "=============================================="
    "$ENGINE" build -t "$image" -f "$dockerfile" "$SRC_DIR"

    # Replace any previous artifacts for this EL but leave siblings alone.
    rm -rf "$dest"
    mkdir -p "$dest/x86_64" "$dest/SRPMS"

    cid=$("$ENGINE" create "$image")
    staging=$(mktemp -d)

    # Dockerfile-rhel<N> runs `rpmbuild -ba` + `rpmbuild -bs`, so artifacts
    # live under /root/rpmbuild/{RPMS/<arch>,SRPMS}. Copy both trees out, then
    # flatten RPMs into a single x86_64/ directory (the spec only emits x86_64
    # and noarch subpackages).
    "$ENGINE" cp "$cid:/root/rpmbuild/RPMS"  "$staging/"
    "$ENGINE" cp "$cid:/root/rpmbuild/SRPMS" "$staging/"
    "$ENGINE" rm -f "$cid" >/dev/null

    find "$staging/RPMS"  -name '*.rpm'     -exec cp {} "$dest/x86_64/" \;
    find "$staging/SRPMS" -name '*.src.rpm' -exec cp {} "$dest/SRPMS/"  \;
    rm -rf "$staging"

    if command -v createrepo_c >/dev/null 2>&1; then
        createrepo_c --quiet "$dest/x86_64"
        createrepo_c --quiet "$dest/SRPMS"
    else
        echo "note: createrepo_c not installed; skipping repodata for el${el}"
    fi

    echo "el${el} artifacts:"
    ( cd "$dest" && find . -name '*.rpm' | sort | sed 's|^\./|  |' )
}

for el in $EL_VERSIONS; do
    build_one_el "$el"
done

echo
echo "Done. Point nginx's document root (or an alias) at:"
echo "  $OUT_DIR"
