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

# The messages to lift out of mesh.proto, and the files generated as-is.
# Anything these reference from the same file is pulled in with them.
TRIM_FROM = "meshtastic/mesh.proto"
TRIM_MESSAGES = ["Data", "Routing", "User"]
TRIM_AS = "meshtastic/leafdata.proto"

# Descriptor sets for pruned imports, filled in by trim().
PRUNED_SETS = []
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


def prune_message(message, fqn, wanted):
    """Keep a message whole if it is wanted, else keep only the path to what is.

    A message reached solely because one of its nested types is referenced
    keeps that nesting and nothing else, so pulling one enum out of a large
    message does not drag the message's own dependencies along.
    """
    if fqn in wanted:
        return message

    kept = descriptor_pb2.DescriptorProto()
    kept.name = message.name
    hit = False

    for enum in message.enum_type:
        if f"{fqn}.{enum.name}" in wanted:
            kept.enum_type.add().CopyFrom(enum)
            hit = True

    for nested in message.nested_type:
        sub = prune_message(nested, f"{fqn}.{nested.name}", wanted)
        if sub is not None:
            kept.nested_type.add().CopyFrom(sub)
            hit = True

    return kept if hit else None


def prune_file(origin, wanted):
    """A copy of a file holding only the types named in wanted."""
    out = descriptor_pb2.FileDescriptorProto()
    out.name = origin.name
    out.package = origin.package
    out.syntax = origin.syntax
    out.options.CopyFrom(origin.options)

    for enum in origin.enum_type:
        if f"{origin.package}.{enum.name}" in wanted:
            out.enum_type.add().CopyFrom(enum)

    for message in origin.message_type:
        kept = prune_message(message, f"{origin.package}.{message.name}", wanted)
        if kept is not None:
            out.message_type.add().CopyFrom(kept)

    return out


def refs_of(message):
    """Every type a message names, nested messages included."""
    out = {f.type_name.lstrip(".") for f in message.field if f.type_name}
    for nested in message.nested_type:
        out |= refs_of(nested)
    return out


def declares(message, prefix):
    """Every type a message declares itself, nested ones included."""
    full = f"{prefix}.{message.name}"
    out = {full}
    for enum in message.enum_type:
        out.add(f"{full}.{enum.name}")
    for nested in message.nested_type:
        out |= declares(nested, full)
    return out


def trim(full, out):
    """Write a descriptor set holding just TRIM_MESSAGES and what they reference."""
    fds = descriptor_pb2.FileDescriptorSet()
    fds.ParseFromString(full.read_bytes())
    by_name = {f.name: f for f in fds.file}

    origin = by_name[TRIM_FROM]
    at = {m.name: i for i, m in enumerate(origin.message_type)}
    local_enums = {e.name for e in origin.enum_type}

    # Follow references inside the file, so a message that names another type
    # brings it along rather than dangling.
    wanted, queue, refs = [], list(TRIM_MESSAGES), set()
    while queue:
        name = queue.pop(0)
        if name in wanted:
            continue
        if name not in at:
            raise SystemExit(f"{TRIM_FROM} has no message {name}")
        wanted.append(name)
        for ref in refs_of(origin.message_type[at[name]]):
            local = ref[len(origin.package) + 1 :] if ref.startswith(origin.package + ".") else None
            if local is not None and local.split(".")[0] in at:
                queue.append(local.split(".")[0])
            refs.add(ref)

    provided = set()
    for name in wanted:
        provided |= declares(origin.message_type[at[name]], origin.package)

    # Top level enums of the same file, such as a hardware model.
    keep_enums = [
        e for e in origin.enum_type if f"{origin.package}.{e.name}" in refs - provided
    ]
    provided |= {f"{origin.package}.{e.name}" for e in keep_enums}

    external = refs - provided

    trimmed = descriptor_pb2.FileDescriptorProto()
    trimmed.name = TRIM_AS
    trimmed.package = origin.package
    trimmed.syntax = origin.syntax
    trimmed.options.CopyFrom(origin.options)
    for enum in keep_enums:
        trimmed.enum_type.add().CopyFrom(enum)
    for name in wanted:
        trimmed.message_type.add().CopyFrom(origin.message_type[at[name]])

    # Imports are pruned to the types actually reached, so referencing one
    # nested enum does not pull a whole configuration schema in behind it.
    pruned = []
    for dep in origin.dependency:
        depfile = by_name[dep]
        reached = {r for r in external if r.startswith(depfile.package + ".")}
        if not reached:
            continue
        candidate = prune_file(depfile, reached)
        if candidate.message_type or candidate.enum_type:
            pruned.append(candidate)
            trimmed.dependency.append(dep)

    satisfied = set()
    for f in pruned:
        for enum in f.enum_type:
            satisfied.add(f"{f.package}.{enum.name}")
        for message in f.message_type:
            satisfied |= declares(message, f.package)

    missing = external - satisfied
    if missing:
        raise SystemExit(f"referenced types come from no known import: {sorted(missing)}")

    # Source info paths encode the index of each top level type, so remap the
    # kept ones. Without this the generated header loses the comments.
    for loc in origin.source_code_info.location:
        path = list(loc.path)
        if len(path) >= 2 and path[0] == 4:
            name = next((n for n, i in at.items() if i == path[1]), None)
            if name is None or name not in wanted:
                continue
            new = trimmed.source_code_info.location.add()
            new.CopyFrom(loc)
            del new.path[:]
            new.path.extend([4, wanted.index(name), *path[2:]])

    result = descriptor_pb2.FileDescriptorSet()
    for f in pruned:
        result.file.add().CopyFrom(f)
    result.file.add().CopyFrom(trimmed)
    out.write_bytes(result.SerializeToString())

    # A pruned import still has to be generated, since the trimmed file
    # includes its header. Each is written as its own single file set, which
    # is how nanopb decides what to emit.
    for i, f in enumerate(pruned):
        if f.name in [w for w in WHOLE]:
            continue
        one = descriptor_pb2.FileDescriptorSet()
        one.file.add().CopyFrom(f)
        side = out.parent / f"import{i}.pb"
        side.write_bytes(one.SerializeToString())
        PRUNED_SETS.append(side)

    enums = [e.name for e in keep_enums]
    imports = [f"{f.name} -> {sorted(reached_names(f))}" for f in pruned]
    print(f"keeping {wanted}, enums {enums or 'none'}")
    for line in imports:
        print(f"  import {line}")
    return wanted


def reached_names(f):
    out = set()
    for enum in f.enum_type:
        out.add(enum.name)
    for message in f.message_type:
        out |= {n.split(".", 1)[-1] for n in declares(message, f.package)} - {message.name}
        out.add(message.name)
    return out


def options_for(messages, source, out):
    """Lift the nanopb options these messages need out of an upstream file.

    Two shapes matter. A qualified line names its message, "*Data.payload".
    An unqualified one applies to a field of that name in any message,
    "*id max_size:16", and those are kept wholesale: dropping them silently
    turns a sized field into a callback.
    """
    if not source.exists():
        out.write_text("", encoding="utf-8")
        return

    names = "|".join(re.escape(m) for m in messages)
    qualified = re.compile(r"^\*(" + names + r")\.")
    unqualified = re.compile(r"^\*[A-Za-z_][A-Za-z0-9_]*\s")

    lines = [
        ln
        for ln in source.read_text(encoding="utf-8").splitlines()
        if qualified.match(ln) or unqualified.match(ln)
    ]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"options: {lines or 'none'}")


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
        kept = trim(full, leaf)

        options = tmp / "leaf.options"
        options_for(kept, PROTOBUFS / "meshtastic" / "mesh.options", options)

        for side in PRUNED_SETS:
            nanopb(generator, side)

        nanopb(generator, leaf, options)

        for proto in WHOLE:
            whole = tmp / (pathlib.Path(proto).stem + ".pb")
            descriptor_set(proto, whole, source_info=True)
            nanopb(generator, whole)

    print(f"Regenerated {OUT.relative_to(ROOT).as_posix()}")


if __name__ == "__main__":
    main()
