#!/bin/bash

set -e
set -x

mkdir -p /root/rpmbuild/SOURCES
rm -rf /root/rpmbuild/SOURCES/sems.tar.gz ./sems.tar.gz
make rpmtar
cp /root/rpmbuild/SOURCES/sems.tar.gz .
docker build --progress=plain -t sems-rhel7 -f Dockerfile-rhel7 .
docker create --name temp-sems-rhel7 sems-rhel7
docker cp temp-sems-rhel7:/root/rpmbuild/RPMS /tmp/sems-rpms
docker rm temp-sems-rhel7
ls -al /tmp/sems-rpms/RPMS/x86_64/
