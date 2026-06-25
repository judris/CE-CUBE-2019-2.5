Import("env")

import os
import subprocess


def _resolve_upload_port(build_env):
    port = build_env.subst("$UPLOAD_PORT").strip()
    if (not port) or (port == "$UPLOAD_PORT"):
        return ""
    return port


def seed_minimal_protocol(target, source, env):
    upload_port = _resolve_upload_port(env)
    if not upload_port:
        print(
            "ERROR: seed_minimal_protocol requires an upload port. "
            "Pass --upload-port COMx or set upload_port in platformio.ini."
        )
        return 1

    return _run_serial_smoke_mode(env, "seed-minimal-protocol", upload_port)


def eeprom_roundtrip_test(target, source, env):
    upload_port = _resolve_upload_port(env)
    if not upload_port:
        print(
            "ERROR: eeprom_roundtrip_test requires an upload port. "
            "Pass --upload-port COMx or set upload_port in platformio.ini."
        )
        return 1

    return _run_serial_smoke_mode(env, "eeprom-roundtrip", upload_port)


def _run_serial_smoke_mode(build_env, mode, upload_port):
    project_dir = build_env["PROJECT_DIR"]
    script_path = os.path.join(project_dir, "tools", "serial_smoke.ps1")
    command = [
        "powershell",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        script_path,
        "-Mode",
        mode,
        "-Port",
        upload_port,
    ]
    return subprocess.call(command, cwd=project_dir)


env.AddCustomTarget(
    name="seed_minimal_protocol",
    dependencies=None,
    actions=[seed_minimal_protocol],
    title="Seed Minimal Protocol",
    description="Compose and upload the minimal CE-CUBE EEPROM protocol over USB serial.",
)

env.AddCustomTarget(
    name="eeprom_roundtrip_test",
    dependencies=None,
    actions=[eeprom_roundtrip_test],
    title="EEPROM Roundtrip Test",
    description="Clear, write, read back, and execute the CE-CUBE EEPROM protocol over USB serial.",
)
