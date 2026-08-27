#!/usr/bin/env python3
"""Regenerate the nanopb sources this library needs.

A leaf node only ever parses the Data submessage of mesh.proto. Generating
mesh.proto wholesale would drag in config, device_ui, module_config, telemetry
and xmodem, none of which a leaf touches, so instead this trims Data out at the
descriptor level and generates from that.

The upstream protobufs submodule stays the single source of truth. Nothing in
this repository restates a .proto or a nanopb option; the trimmed descriptor is
derived on every run and never committed. If upstream adds a field to Data, or
makes it reference a new message, the next run picks it up and CI notices that
the committed output has moved.

Requires the protobufs submodule and the generator:

    git submodule update --init protobufs
    pip install 'nanopb==0.4.9.1' grpcio-tools

The generator version decides the exact bytes of the output, so use the pinned
one or the committed files will churn.
"""

import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

from google.protobuf import descriptor_pb2

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROTOBUFS = ROOT / "protobufs"
OUT = ROOT / "src" / "generated"

# The message to lift out of mesh.proto, and the files generated as-is.
TRIM_FROM = "meshtastic/mesh.proto"
TRIM_MESSAGE = "Data"
TRIM_AS = "meshtastic/leafdata.proto"
WHOLE = ["meshtastic/portnums.proto"]


def protoc(args):
    subprocess.run(
        [sys.executable, "-m", "grpc_tools.protoc", "--experimental_allow_proto3_optional", *args],
        check=True,
    )


def descriptor_set(proto, out, source_info=False):
    args = [f"-I{PROTOBUFS}", "--include_imports", f"--descriptor_set_out={out}"]
    if source_info:
        args.append("--include_source_info")
    protoc([*args, proto])


def trim(full, out):
    """Write a descriptor set holding just TRIM_MESSAGE and the imports it uses."""
    fds = descriptor_pb2.FileDescriptorSet()
    fds.ParseFromString(full.read_bytes())
    by_name = {f.name: f for f in fds.file}

    origin = by_name[TRIM_FROM]
    index, message = next(
        (i, m) for i, m in enumerate(origin.message_type) if m.name == TRIM_MESSAGE
    )

    trimmed = descriptor_pb2.FileDescriptorProto()
    trimmed.name = TRIM_AS
    trimmed.package = origin.package
    trimmed.syntax = origin.syntax
    trimmed.options.CopyFrom(origin.options)
    trimmed.message_type.add().CopyFrom(message)

    # Carry only the imports the message actually refers to, so a new field
    # pointing at a new message brings its import along by itself.
    refs = {f.type_name.lstrip(".") for f in message.field if f.type_name}
    kept = []
    for dep in origin.dependency:
        depfile = by_name[dep]
        provides = {f"{depfile.package}.{m.name}" for m in depfile.message_type}
        provides |= {f"{depfile.package}.{e.name}" for e in depfile.enum_type}
        if refs & provides:
            kept.append(dep)
    trimmed.dependency.extend(kept)

    missing = refs - {
        f"{by_name[d].package}.{n.name}"
        for d in kept
        for n in list(by_name[d].message_type) + list(by_name[d].enum_type)
    }
    if missing:
        raise SystemExit(f"{TRIM_MESSAGE} refers to types from no known import: {sorted(missing)}")

    # Source info paths encode the message's index, so remap it to 0. Without
    # this the generated header loses the upstream comments.
    for loc in origin.source_code_info.location:
        path = list(loc.path)
        if len(path) >= 2 and path[0] == 4 and path[1] == index:
            new = trimmed.source_code_info.location.add()
            new.CopyFrom(loc)
            del new.path[:]
            new.path.extend([4, 0, *path[2:]])

    result = descriptor_pb2.FileDescriptorSet()
    for dep in kept:
        result.file.add().CopyFrom(by_name[dep])
    result.file.add().CopyFrom(trimmed)
    out.write_bytes(result.SerializeToString())

    print(f"{TRIM_MESSAGE} refers to {sorted(refs) or 'scalars only'}, keeping imports {kept}")


def options_for(message, source, out):
    """Lift the nanopb options for one message out of an upstream .options file."""
    if not source.exists():
        out.write_text("", encoding="utf-8")
        return
    wanted = re.compile(rf"^\*{re.escape(message)}\.")
    lines = [ln for ln in source.read_text(encoding="utf-8").splitlines() if wanted.match(ln)]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"options for {message}: {lines or 'none'}")


def nanopb(generator, descriptors, options=None):
    args = [generator, "-S.cpp", "-D", str(OUT)]
    if options:
        args += ["-f", str(options)]
    subprocess.run([*args, str(descriptors)], check=True)


def main():
    if not (PROTOBUFS / TRIM_FROM).exists():
        raise SystemExit("protobufs submodule is empty; run: git submodule update --init protobufs")

    generator = shutil.which("nanopb_generator")
    if not generator:
        raise SystemExit(
            "nanopb_generator not found. Install it with:\n"
            "  pip install 'nanopb==0.4.9.1' grpcio-tools"
        )

    OUT.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = pathlib.Path(tmpdir)

        full = tmp / "all.pb"
        descriptor_set(TRIM_FROM, full, source_info=True)

        leaf = tmp / "leaf.pb"
        trim(full, leaf)

        options = tmp / "leaf.options"
        options_for(TRIM_MESSAGE, PROTOBUFS / "meshtastic" / "mesh.options", options)

        nanopb(generator, leaf, options)

        for proto in WHOLE:
            whole = tmp / (pathlib.Path(proto).stem + ".pb")
            descriptor_set(proto, whole, source_info=True)
            nanopb(generator, whole)

    print(f"Regenerated {OUT.relative_to(ROOT).as_posix()}")


if __name__ == "__main__":
    main()
