#!/bin/sh
#
# Script used to tracking build number
#

set -e
#set -x

DIR=/var/tmp/.bundle_build_number
HASH=$(git rev-list --max-parents=0 HEAD)

mkdir -p $DIR
cd $DIR

if [ -f $HASH ]; then
    OLD=$(cat $HASH)
else
    OLD=0
fi

NEW=$(expr $OLD + 1)

printf $NEW > $HASH
printf $NEW

