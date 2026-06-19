#!/bin/sh

HIVELYTRACKER=`git ls-remote https://github.com/pete-gordon/hivelytracker.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$HIVELYTRACKER" != x"f393ca7c6416f00bcb574b334a7e8b57dcb19eb2"; then echo HIVELYTRACKER needs to be verified; fi

QOA=`git ls-remote https://github.com/phoboslab/qoa refs/heads/master | sed -e 's/\t.*//'`
if test "x$QOA" != x"f73b4a36f40bc022abeb32575716c80e49bdd572"; then echo QOA needs to be verified \($QOA\); fi

ADPLUGDB=`git ls-remote https://github.com/adplug/database.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$ADPLUGDB" != x"7ac0819ec55d6dd1ffe42890f82c3ada05d101b5"; then echo ADPLUGDB needs to be verified; fi

LIBBINIO=`git ls-remote https://github.com/adplug/libbinio.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$LIBBINIO" != x"79d597dde20683c8578958308b4c1746ab5859ff"; then echo LIBBINIO needs to be verified; fi

ADPLUG=`git ls-remote https://github.com/adplug/adplug.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$ADPLUG" != x"101e7503d91fc783f5152d374a5837a13082ff1b"; then echo ADPLUG needs to be verified; fi

LIBSIDPLAYFP=`git ls-remote https://github.com/libsidplayfp/libsidplayfp.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$LIBSIDPLAYFP" != x"c3298f2b20949c512ecdf7db52f850fd9df40506"; then echo LIBSIDPLAYFP needs to be verified \($LIBSIDPLAYFP\); fi

LIBRESIDFP=`git ls-remote https://github.com/libsidplayfp/libresidfp.git refs/heads/main | sed -e 's/\t.*//'`
if test "x$LIBRESIDFP" != x"8243cb5e8a04e17b742f1b11bceaa5fab098e155"; then echo LIBRESIDFP needs to be verified \($LIBRESIDFP\); fi

ANCIENT=`git ls-remote https://github.com/temisu/ancient.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$ANCIENT" != x"d52dc0c1eec35f14e0da78dd48836ac9542f2f0f"; then echo ANCIENT needs to be verified; fi

UNICODE_CASEFOLDING=`curl https://www.unicode.org/Public/latest/ 2> /dev/null|grep http|grep -i Public|head -n 1|sed -e 's/.*ublic\///' -e 's/\/.*//'`
if test "x$UNICODE_CASEFOLDING" != x"17.0.0"; then echo Unicode CaseFolding needs to be verified; fi

PSGPLAY=`git ls-remote https://github.com/frno7/psgplay.git refs/heads/main | sed -e 's/\t.*//'`
if test "x$PSGPLAY" != x"869992cbbb8488b519149d8c0dd7afafb78aae5e"; then echo PSG-play needs to be verified \($PSGPLAY\); fi

# TIMIDITY
