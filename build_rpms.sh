#!/bin/bash

set -e
set -x

rm -rf /tmp/sems-rpms
mkdir -p /tmp/sems-rpms
#podman system prune -a
podman build --progress=plain -t sems-build -f Dockerfile-rhel9 .
podman create --name temp-sems-build sems-build
podman cp temp-sems-build:/root/rpmbuild/RPMS /tmp/sems-rpms
podman rm temp-sems-build
ls -al /tmp/sems-rpms/RPMS/x86_64/
