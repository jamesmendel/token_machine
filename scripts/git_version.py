from pathlib import Path
import subprocess

Import("env")


def _git(args: list[str]) -> str:
    """Run a git command from the project dir, return stdout stripped or empty on failure."""
    try:
        repo = Path(env["PROJECT_DIR"])
        return subprocess.check_output(["git"] + args, cwd=repo, stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return ""


commit = _git(["rev-parse", "--short", "HEAD"])
branch = _git(["symbolic-ref", "--short", "HEAD"])
tag = _git(["describe", "--exact-match", "--tags"])

# Check for uncommitted changes
porcelain = _git(["status", "--porcelain"])
dirty = bool(porcelain)

if not commit:
    commit = "unknown"
if not branch:
    branch = "unknown"

# Generate version header
version_h = Path(env["PROJECT_DIR"]) / "src" / "version.h"
lines = [
    "#pragma once",
    "",
    f'#define GIT_COMMIT "{commit}"',
    f'#define GIT_BRANCH "{branch}"',
]
if tag:
    lines.append(f'#define GIT_TAG "{tag}"')
if dirty:
    lines.append("#define GIT_DIRTY")
lines.append("")

version_h.write_text("\n".join(lines))

parts = [f"branch={branch}", f"commit={commit}"]
if tag:
    parts.append(f"tag={tag}")
if dirty:
    parts.append("dirty")
print("Git version: " + ", ".join(parts))