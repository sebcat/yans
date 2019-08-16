#!/bin/sh

for YEAR in $(seq 2002 $(date '+%Y')); do
  curl -o "nvdcve-1.0-$YEAR.json.gz" "https://nvd.nist.gov/feeds/json/cve/1.0/nvdcve-1.0-$YEAR.json.gz"
  sleep 1
done
