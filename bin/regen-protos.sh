#!/usr/bin/env bash
#
# Regenerate the nanopb sources this library needs.
#
# A leaf node only ever parses the Data submessage, so we generate from a
# trimmed proto/meshtastic/leafdata.proto rather than upstream mesh.proto.
# Generating from mesh.proto would drag in config, device_ui, module_config,
# telemetry and xmodem, none of which a leaf touches.
#
# Requires the protobufs submodule and a nanopb generator:
#   git submodule update --init protobufs
#   pip install grpcio-tools
#
# The nanopb generator is found on PATH, or via NANOPB_PLUGIN if it lives
# somewhere else (on Windows, point it at protoc-gen-nanopb.bat).
#
# The generator version decides the exact bytes of the output, so CI pins it.
# Use the same version, currently nanopb 0.4.9.1, or the committed files will
# churn.

set -euo pipefail

cd "$(dirname "$0")/.."

: "${NANOPB_PLUGIN:=$(command -v protoc-gen-nanopb || true)}"

# protoc is a native Windows binary under Git Bash and cannot follow a POSIX
# path, so hand it a Windows one when cygpath is available.
if [ -n "${NANOPB_PLUGIN}" ] && command -v cygpath > /dev/null; then
  NANOPB_PLUGIN=$(cygpath -w "${NANOPB_PLUGIN}")
fi

if [ -z "${NANOPB_PLUGIN}" ]; then
  echo "protoc-gen-nanopb not found. Install it with:" >&2
  echo "  pip install 'nanopb==0.4.9.1' grpcio-tools" >&2
  echo "or set NANOPB_PLUGIN to its path." >&2
  exit 1
fi

if [ ! -f protobufs/meshtastic/mesh.proto ]; then
  echo "protobufs submodule is empty; run: git submodule update --init protobufs" >&2
  exit 1
fi

# Fail if upstream Data has drifted from our trimmed copy. Compare only the
# field lines, so comment rewording upstream does not trip this.
fields_of() {
  awk '/^message Data \{/,/^\}/' "$1" |
    grep -E '^\s+(optional\s+)?[A-Za-z0-9_.]+\s+[a-z_]+\s*=\s*[0-9]+;' |
    tr -s ' '
}

if ! diff <(fields_of protobufs/meshtastic/mesh.proto) \
          <(fields_of proto/meshtastic/leafdata.proto) > /dev/null; then
  echo "ERROR: upstream Data and proto/meshtastic/leafdata.proto have drifted:" >&2
  diff <(fields_of protobufs/meshtastic/mesh.proto) \
       <(fields_of proto/meshtastic/leafdata.proto) >&2 || true
  echo "Update proto/meshtastic/leafdata.proto (and .options) to match." >&2
  exit 1
fi

# nanopb resolves the .options file relative to the working directory, so the
# generator has to run from inside proto/.
cd proto
python -m grpc_tools.protoc \
  --experimental_allow_proto3_optional \
  --plugin=protoc-gen-nanopb="${NANOPB_PLUGIN}" \
  -I. -I../protobufs \
  --nanopb_out="-S.cpp:../src/generated" \
  meshtastic/leafdata.proto ../protobufs/meshtastic/portnums.proto

echo "Regenerated src/generated/meshtastic/{leafdata,portnums}.pb.{h,cpp}"
