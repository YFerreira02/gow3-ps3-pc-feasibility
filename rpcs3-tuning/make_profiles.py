"""Generate RPCS3 per-game profiles for God of War III (BCES00510).

Each profile is the user's current game config (profiles/base_BCES00510.yml)
with a small set of overrides. Overrides are addressed by their YAML path,
e.g. "Core/SPU Block Size" or "Video/Vulkan/Adapter", so keys that repeat in
different sections (such as "Renderer") are never confused.

Versions change one idea at a time so each measurement says what helped.
Run:  python3 make_profiles.py
"""
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = HERE / "profiles" / "base_BCES00510.yml"

# name -> (description, parent profile or None, overrides)
PROFILES = {
    "v0-actual": (
        "Configuracion actual del usuario, sin cambios (linea base).",
        None,
        {},
    ),
    "v1-spu-mega": (
        "SPU Block Size Safe -> Mega: bloques SPU mas grandes, menos saltos entre bloques.",
        "v0-actual",
        {"Core/SPU Block Size": "Mega"},
    ),
    "v2-spu-loop": (
        "v1 + SPU loop detection: deja de gastar CPU en bucles de espera de las SPU.",
        "v1-spu-mega",
        {"Core/SPU loop detection": "true"},
    ),
    "v3-720p": (
        "v2 + resolucion original de PS3 1280x720 (antes 720x480). Mide cuanto cuesta la calidad.",
        "v2-spu-loop",
        {"Video/Resolution": "1280x720"},
    ),
    # A partir de aqui cada perfil parte de v3 y cambia UNA cosa, para comparar cada idea por separado.
    "v4-sched-alt": (
        "v3 + planificador de hilos de RPCS3 (alternativo) en vez del de Windows.",
        "v3-720p",
        {"Core/Thread Scheduler Mode": "RPCS3 Alternative Scheduler"},
    ),
    "v5-rsx-1hilo": (
        "v3 + RSX multihilo apagado: libera un hilo de CPU para las SPU.",
        "v3-720p",
        {"Video/Multithreaded RSX": "false"},
    ),
    "v6-spurs4": (
        "v3 + maximo 4 hilos SPURS (antes 6): menos hilos SPU compitiendo por 4 nucleos.",
        "v3-720p",
        {"Core/Max SPURS Threads": "4"},
    ),
    "v7-radeon530": (
        "v3 + GPU dedicada Radeon 530 en vez de la Vega 8: la Vega comparte RAM con la CPU.",
        "v3-720p",
        {"Video/Vulkan/Adapter": "\"Radeon 530 Series\""},
    ),
}


def apply_overrides(text: str, overrides: dict) -> str:
    lines = text.splitlines()
    pending = dict(overrides)
    stack = []  # (indent, key)
    for i, line in enumerate(lines):
        stripped = line.lstrip(" ")
        if not stripped or ":" not in stripped:
            continue
        indent = len(line) - len(stripped)
        key, _, value = stripped.partition(":")
        while stack and stack[-1][0] >= indent:
            stack.pop()
        path = "/".join([k for _, k in stack] + [key])
        if path in pending:
            lines[i] = f"{' ' * indent}{key}: {pending.pop(path)}"
        if value.strip() == "":
            stack.append((indent, key))
    if pending:
        raise KeyError(f"Keys not found in base config: {sorted(pending)}")
    return "\n".join(lines) + "\n"


def resolve(name: str) -> dict:
    description, parent, overrides = PROFILES[name]
    merged = resolve(parent) if parent else {}
    merged.update(overrides)
    return merged


def main() -> None:
    base = BASE.read_text(encoding="utf-8-sig")
    for name in PROFILES:
        out = HERE / "profiles" / f"{name}.yml"
        out.write_text(apply_overrides(base, resolve(name)), encoding="utf-8", newline="\n")
        print(f"{name}: {PROFILES[name][0]}")


if __name__ == "__main__":
    main()
