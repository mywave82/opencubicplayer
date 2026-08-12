#!/bin/sh

HIVELYTRACKER=`git ls-remote https://github.com/pete-gordon/hivelytracker.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$HIVELYTRACKER" != x"f393ca7c6416f00bcb574b334a7e8b57dcb19eb2"; then echo HIVELYTRACKER needs to be verified; fi

QOA=`git ls-remote https://github.com/phoboslab/qoa refs/heads/master | sed -e 's/\t.*//'`
if test "x$QOA" != x"f73b4a36f40bc022abeb32575716c80e49bdd572"; then echo QOA needs to be verified \($QOA\); fi

ADPLUGDB=`git ls-remote https://github.com/adplug/database.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$ADPLUGDB" != x"7ac0819ec55d6dd1ffe42890f82c3ada05d101b5"; then echo ADPLUGDB needs to be verified; fi

LIBBINIO=`git ls-remote https://github.com/adplug/libbinio.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$LIBBINIO" != x"3baed731da6b44883c6820132fb1d2a44abc0939"; then echo LIBBINIO needs to be verified \($LIBBINIO\); fi

ADPLUG=`git ls-remote https://github.com/adplug/adplug.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$ADPLUG" != x"f8386a01dd6060010e625df4743f625c255abb19"; then echo ADPLUG needs to be verified \($ADPLUG\); fi

LIBSIDPLAYFP=`git ls-remote https://github.com/libsidplayfp/libsidplayfp.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$LIBSIDPLAYFP" != x"7c377133a2240995373499b50a44e1d38783da85"; then echo LIBSIDPLAYFP needs to be verified \($LIBSIDPLAYFP\); fi

LIBRESIDFP=`git ls-remote https://github.com/libsidplayfp/libresidfp.git refs/heads/main | sed -e 's/\t.*//'`
if test "x$LIBRESIDFP" != x"ccfc47936dcb5f350c62f536b0832af9b22de490"; then echo LIBRESIDFP needs to be verified \($LIBRESIDFP\); fi

ANCIENT=`git ls-remote https://github.com/temisu/ancient.git refs/heads/master | sed -e 's/\t.*//'`
if test "x$ANCIENT" != x"d52dc0c1eec35f14e0da78dd48836ac9542f2f0f"; then echo ANCIENT needs to be verified; fi

UNICODE_CASEFOLDING=`curl https://www.unicode.org/Public/latest/ 2> /dev/null|grep http|grep -i Public|head -n 1|sed -e 's/.*ublic\///' -e 's/\/.*//'`
if test "x$UNICODE_CASEFOLDING" != x"17.0.0"; then echo Unicode CaseFolding needs to be verified; fi

PSGPLAY=`git ls-remote https://github.com/frno7/psgplay.git refs/heads/main | sed -e 's/\t.*//'`
if test "x$PSGPLAY" != x"f2028e94e5f6c7b3b38c9f7b5e2e0e1939613c06"; then echo PSG-play needs to be verified \($PSGPLAY\); fi

# TIMIDITY
