#!/usr/bin/env bash

BINARY=$1
DEP=$2
RATDEP=$3
POLY=$4

$BINARY $DEP $RATDEP $POLY 2>&1 | grep 44371162641954939938390944368
