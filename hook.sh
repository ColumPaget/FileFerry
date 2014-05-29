#!/bin/sh

DATE=`date +%Y_%m_%d`
echo "THIS IS A HOOK! $*"

mv -v $1 $1.$DATE
