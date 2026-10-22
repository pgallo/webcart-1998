#!/usr/bin/env bash
# Offline check of the servlet module's pure-Java logic: compiles the
# lab.webcart.store package and its framework-free checks with javac (no
# Maven, no servlet API), runs them, then runs the HTTP/XML client against
# the real C++ gateway if it has been built.
#   scripts/check-java.sh [path/to/webcart-gateway]
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/servlet/target/check-java"
rm -rf "$out" && mkdir -p "$out"
javac -d "$out" --release 21 \
  "$root"/servlet/src/main/java/lab/webcart/store/*.java \
  "$root"/servlet/src/test/java/lab/webcart/store/{FakeGateway,CartLogicChecks,GatewayProcess,GatewayContractCheck}.java
java -cp "$out" lab.webcart.store.CartLogicChecks
gateway="${1:-$root/build/webcart-gateway}"
if [[ -x "$gateway" ]]; then
  java -cp "$out" lab.webcart.store.GatewayContractCheck "$gateway" "$root/data"
else
  echo "skipping gateway contract check: $gateway not built"
fi
