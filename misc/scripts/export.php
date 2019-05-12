#!/usr/bin/env php
<?php

#croak -- because apparently die, exit writes to stdout
function croak($msg) {
  fwrite(STDERR, "$msg\n");
  exit(1);
}

function csv_or_die($path, $skip_first_row = true) {
  $handle = fopen($path, "r")
      or croak("failed to open $path");

  $res = array();

  if ($skip_first_row) {
    $names = fgetcsv($handle);
    if (!$names) {
      fclose($handle);
      return $res;
    }
  }

  for ($i = 0; $row = fgetcsv($handle); $i++) {
    $res[] = $row;
  }

  fclose($handle);
  return $res;
}

function to_csv_or_die($path, $hdr, $rows) {
  $handle = fopen($path, "w")
    or croak("failed to open $path for writing");
  fputcsv($handle, $hdr);
  foreach ($rows as $row) {
    if (!is_array($row)) {
      $row = array($row);
    }
    fputcsv($handle, $row);
  }
  fclose($handle);
}

if ($argc != 3) {
  croak("usage: $argv[0] <basedir> <dstdir>");
}

$basedir = $argv[1];
if (!is_dir($basedir)) {
  croak("$basedir is not a valid directory");
}

$dstdir = $argv[2];
if (!is_dir($dstdir)) {
  croak("$dstdir is not a valid directory");
}

$services = array();
$svcchains = array();
$chains = array();
$sans = array();
$comps = array();
$compsvcs = array();

$rows = csv_or_die("$basedir/services.csv");
foreach ($rows as $row) {
  # ID, host, addr, transport, port, service, cert chain
  $services[$row[0]] = $row;
  # Append service => cert chain if we have a chain here
  if ($row[6]) {
    $svcchains[] = array($row[0], $row[6]);
  }
}

$rows = csv_or_die("$basedir/certs.csv");
foreach ($rows as $row) {
  $chain = $row[0];
  $depth = $row[1];
  if (!$chains[$chain]) {
    $chains[$chain] = array();
  }

  if (!$chains[$chain][$depth]) {
    $chains[$chain][$depth] = array();
  }

  # ID, depth, subject, issuer, not valid before, not valid after
  $chains[$chain][$depth] = $row;
}

$rows = csv_or_die("$basedir/sans.csv");
foreach ($rows as $row) {
  $chain = $row[0];
  $depth = $row[1];
  if (!$chains[$chain]) {
    $chains[$chain] = array();
  }

  if (!$chains[$chain][$depth]) {
    $chains[$chain][$depth] = array();
  }

  # Drop the relation to the chain, just keep unique SANs
  $sans[$row[3]] = true;
}

$sans = array_keys($sans);
sort($sans);
to_csv_or_die("$dstdir/sans.csv", array("SAN"), $sans);

$rows = csv_or_die("$basedir/comp.csv");
foreach ($rows as $row) {
  # id, name, version
  $comps[$row[0]] = $row;
}

$rows = csv_or_die("$basedir/compsvc.csv");
foreach ($rows as $row) {
  # component ID, service ID
  $compsvcs[] = array($row[0], $row[1]);
}

$out_services = array();
foreach ($services as $s) {
  # host, addr, transport, port, service
  $out_services[] = array(
    $s[1], $s[2], $s[3], $s[4], $s[5]
  );
}

usort($out_services, function($a, $b) {
  $cmp = strnatcmp($a[0], $b[0]);
  if (!$cmp) {
    $cmp = strnatcmp($a[1], $b[1]);
    if (!$cmp) {
      $cmp = strnatcmp($a[2], $b[2]);
      if (!$cmp) {
        $cmp = strnatcmp($a[3], $b[3]);
        if (!$cmp) {
          $cmp = strnatcmp($a[4], $b[4]);
          if (!$cmp) {
            $cmp = strnatcmp($a[5], $b[5]);
          }
        }
      }
    }
  }

  return $cmp;
});

to_csv_or_die("$dstdir/services.csv", array(
    "Host", "Address", "Transport", "Port", "Service"),
    $out_services);

$out_svcchains = array();
foreach ($svcchains as $s) {
  $service_id = $s[0];
  $chain_id = $s[1];
  # Service: ID, host, addr, transport, port, service, cert chain
  # Chain: ID, depth, subject, issuer, not valid before, not valid after
  $chain = $chains[$chain_id];
  $svc = $services[$service_id];
  $depth = count($chain);
  for ($i = 0; $i < $depth; $i++) {
    $out_svcchains[] = array(
      $svc[1], $svc[2], $svc[3], $svc[4], $svc[5],
      $chain[$i][1],
      $chain[$i][2],
      $chain[$i][3],
      $chain[$i][4],
      $chain[$i][5],
    );
  }
}


usort($out_svcchains, function($a, $b) {
  $len = count($a);
  for ($i = 0; $i < $len; $i++) {
    $cmp = strnatcmp($a[$i], $b[$i]);
    if ($cmp !== 0) {
      return $cmp;
    }
  }
});

to_csv_or_die("$dstdir/certificates.csv", array(
    "Host", "Address", "Transport", "Port", "Service",
    "Depth", "Subject", "Issuer", "Not Valid Before",
    "Not Valid After"),
    $out_svcchains);

$out_compsvcs = array();
foreach ($compsvcs as $cs) {
  $comp = $comps[$cs[0]];
  $svc = $services[$cs[1]];
  $out_compsvcs[] = array(
      $svc[1], $svc[2], $svc[3], $svc[4], $svc[5],
      $comp[1], $comp[2],
  );
}

usort($out_compsvcs, function($a, $b) {
  $len = count($a);
  for ($i = 0; $i < $len; $i++) {
    $cmp = strnatcmp($a[$i], $b[$i]);
    if ($cmp !== 0) {
      return $cmp;
    }
  }
});


to_csv_or_die("$dstdir/components.csv", array(
    "Host", "Address", "Transport", "Port", "Service",
    "Component", "Version"),
    $out_compsvcs);
