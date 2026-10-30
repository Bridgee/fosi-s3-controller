"""Shared bounded, staged operations for the three offline font generators."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def require(condition, message):
    # Unlike assert, validation stays active under python -O.
    if not condition:
        raise ValueError(message)


def run_converter(command, timeout=120):
    try:
        return subprocess.run(command, check=True, capture_output=True, text=True,
                              timeout=timeout).stdout
    except subprocess.TimeoutExpired as error:
        raise ValueError("Font converter timed out; committed assets were not changed") from error
    except subprocess.CalledProcessError as error:
        raise ValueError("Font converter failed; committed assets were not changed: " +
                         (error.stderr or "")[-2000:]) from error


def check_inputs(font, converter, node, sha256, version):
    require(hashlib.sha256(font.read_bytes()).hexdigest() == sha256,
            "Unexpected font source checksum")
    actual = run_converter([node, str(converter), "--version"], timeout=10).strip()
    require(actual == version, "Unexpected font converter version; use " + version)


def publish_assets(outputs):
    """Validate/convert first; stage all outputs, replace files atomically, roll back errors.

    All font outputs share a directory. A process killed between separate renames
    is not a multi-file filesystem transaction; normal converter/validation/write
    exceptions retain the prior assets. CI equality checks detect incomplete sets.
    """
    require(bool(outputs), "No generated assets")
    parents = {path.parent.resolve() for path in outputs}
    require(len(parents) == 1, "Font outputs must share a directory")
    parent = next(iter(parents))
    require(all(not p.is_symlink() for p in outputs), "Refusing symlink font output")
    with tempfile.TemporaryDirectory(prefix=".font-stage-", dir=parent) as directory:
        stage = Path(directory)
        backups = {}
        for index, (path, text) in enumerate(outputs.items()):
            (stage / str(index)).write_text(text, encoding="utf-8")
            if path.exists():
                backup = stage / (str(index) + ".old")
                shutil.copyfile(path, backup)
                backups[path] = backup
            else:
                backups[path] = None
        committed = []
        try:
            for index, path in enumerate(outputs):
                os.replace(stage / str(index), path)
                committed.append(path)
        except OSError:
            for path in reversed(committed):
                if backups[path] is None:
                    path.unlink()
                else:
                    os.replace(backups[path], path)
            raise


def c_array(text, name):
    match = re.search(r"\b" + re.escape(name) + r"\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    require(match is not None, "Missing generated array: " + name)
    return match[1]


def integers(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return [int(v, 0) for v in re.findall(r"-?0x[0-9a-fA-F]+|-?\d+", text)]


def field(text, name):
    found = re.search(r"\." + re.escape(name) + r"\s*=\s*([^,}\n]+)", text)
    require(found is not None, "Missing generated field: " + name)
    return found[1].strip()
