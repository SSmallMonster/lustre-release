#!/bin/sh

SRCDIR="`dirname $0`"
. $SRCDIR/common.sh

NETWORK=tcp

setup

$PTLCTL <<EOF
setup $NETWORK
mynid localhost
connect localhost 1234
add_uuid self
add_uuid mds
add_uuid ost
quit
EOF

new_fs ext2 /tmp/ost 10000
OST=$LOOPDEV
MDSFS=ext2
new_fs ${MDSFS} /tmp/mds 10000
MDS=$LOOPDEV

echo 0xffffffff > /proc/sys/portals/debug

$OBDCTL <<EOF
device 0
attach mds
setup ${MDS} ${MDSFS}
device 1
attach obdext2
setup ${OST}
device 2
attach ost
setup 1
device 3
attach osc
setup -1
quit
EOF

mount -t lustre_light -o device=3 none /mnt/obd
