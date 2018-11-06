#!/bin/sh

I=0
NJOBS=100
JOBTYPE=hej
while [ $I -lt $NJOBS ]; do
  ID=$(printf '%d\n' $I | store put iteration.txt)
  kneg queue --name "fulhax-${I}" "$ID" "$JOBTYPE" > /dev/null 2>&1
  printf '%s\n' "$ID"
  I=$((I + 1))
done
