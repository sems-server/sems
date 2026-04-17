#!/usr/bin/env bash
# Build RHEL 7/8/9/10 RPMs for SEMS at a specific git ref and lay them out for
# static hosting (e.g. nginx).
#
# Usage:
#   cd /path/to/sems-sources          # a checked-out sems repository
#   ./release.sh <git-ref>            # e.g. ./release.sh v2.1.0
#
# <git-ref> is any tag, branch or commit SHA that exists in the local clone.
# The build runs in an isolated `git worktree` so the caller's working tree is
# never modified. Output goes to ../rhel relative to the current repository.
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
# The existing pkg/rpm/sems.spec + Dockerfile-rhel<N> are the single source of
# truth for the build; this script just orchestrates them and extracts the
# artifacts.

set -euo pipefail

if [[ $# -lt 1 || "$1" == "-h" || "$1" == "--help" ]]; then
    cat >&2 <<EOF
usage: $0 <git-ref>
  <git-ref>  tag, branch, or SHA to build from (e.g. v2.1.0)

Run 'git fetch --tags' first if the tag isn't in the local clone.
EOF
    exit 2
fi

REF="$1"
REPO_DIR="$(pwd)"

if ! git -C "$REPO_DIR" rev-parse --git-dir >/dev/null 2>&1; then
    echo "error: run this script from the root of a sems git checkout" >&2
    exit 1
fi

# Resolve the ref up front so we fail fast with a clear message.
if ! git -C "$REPO_DIR" rev-parse --verify --quiet "${REF}^{commit}" >/dev/null; then
    echo "error: git ref '$REF' not found in $REPO_DIR" >&2
    echo "       (try: git fetch --tags origin)" >&2
    exit 1
fi
RESOLVED_SHA="$(git -C "$REPO_DIR" rev-parse "${REF}^{commit}")"
RESOLVED_SHORT="$(git -C "$REPO_DIR" rev-parse --short "$RESOLVED_SHA")"

# Build in an isolated worktree so the caller's working tree (and any
# uncommitted changes they have) is untouched.
WORKTREE_DIR="$(mktemp -d -t sems-release-XXXXXX)"
cleanup() {
    git -C "$REPO_DIR" worktree remove --force "$WORKTREE_DIR" >/dev/null 2>&1 || true
    rm -rf "$WORKTREE_DIR"
}
trap cleanup EXIT

git -C "$REPO_DIR" worktree add --detach "$WORKTREE_DIR" "$RESOLVED_SHA" >/dev/null

SRC_DIR="$WORKTREE_DIR"
if [[ ! -f "$SRC_DIR/VERSION" || ! -f "$SRC_DIR/pkg/rpm/sems.spec" ]]; then
    echo "error: checkout at $REF does not look like a sems tree" >&2
    exit 1
fi

VERSION="$(cat "$SRC_DIR/VERSION")"
OUT_DIR="${OUT_DIR:-$(cd "$REPO_DIR/.." && pwd)/rhel}"
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

echo "git ref      : $REF ($RESOLVED_SHORT)"
echo "sems version : $VERSION"
echo "source tree  : $SRC_DIR (worktree)"
echo "output dir   : $OUT_DIR"
echo "el targets   : $EL_VERSIONS"
echo "engine       : $ENGINE"

mkdir -p "$OUT_DIR"

build_one_el() {
    local el="$1"
    local dockerfile="$SRC_DIR/Dockerfile-rhel${el}"
    local image="sems-release-el${el}:${VERSION}-${RESOLVED_SHORT}"
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

    # Snapshot the build host's installed-package inventory from the image we
    # just built, so the release folder records exactly which toolchain and
    # dependency versions produced these RPMs.
    local build_host_rpms
    build_host_rpms=$("$ENGINE" run --rm --entrypoint rpm "$image" -qa --queryformat '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' | sort)

    write_readme "$el" "$dest" "$build_host_rpms"

    if command -v createrepo_c >/dev/null 2>&1; then
        createrepo_c --quiet "$dest/x86_64"
        createrepo_c --quiet "$dest/SRPMS"
    else
        echo "note: createrepo_c not installed; skipping repodata for el${el}"
    fi

    echo "el${el} artifacts:"
    ( cd "$dest" && find . -name '*.rpm' | sort | sed 's|^\./|  |' )
}

write_readme() {
    local el="$1"
    local dest="$2"
    local build_host_rpms="$3"
    local readme="$dest/README.txt"
    local release_date
    release_date="$(date -u +'%Y-%m-%d %H:%M:%S UTC')"

    {
        echo "SEMS RHEL ${el} release"
        echo "======================="
        echo
        echo "Release date : ${release_date}"
        echo "Git ref      : ${REF} (${RESOLVED_SHORT})"
        echo "Commit SHA   : ${RESOLVED_SHA}"
        echo "SEMS version : ${VERSION}"
        echo "EL target    : ${el}"
        echo "Built with   : ${ENGINE} via Dockerfile-rhel${el}"
        echo "Built on     : $(uname -n) ($(uname -sr))"
        echo
        echo "RPMs in this directory"
        echo "----------------------"
        ( cd "$dest" && find x86_64 SRPMS -name '*.rpm' 2>/dev/null | sort )
        echo
        echo "Build host RPM inventory"
        echo "------------------------"
        echo "Full list of packages installed in the build container at the"
        echo "time these RPMs were produced (output of 'rpm -qa' inside the"
        echo "Dockerfile-rhel${el} image):"
        echo
        echo "$build_host_rpms"
    } > "$readme"
}

for el in $EL_VERSIONS; do
    build_one_el "$el"
done

echo
echo "Done. Built $REF ($RESOLVED_SHORT) as sems-$VERSION."
echo "Point nginx's document root (or an alias) at:"
echo "  $OUT_DIR"
