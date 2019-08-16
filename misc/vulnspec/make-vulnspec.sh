#!/bin/sh

OUTFILE=components.vsi

rm -f $(OUTFILE)
for YEAR in $(seq 2002 $(date '+%Y')); do
  ./load.py "nvdcve-1.0-$YEAR.json.gz" >> $(OUTFILE)
done
