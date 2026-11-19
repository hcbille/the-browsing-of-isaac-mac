"""mainrepo.py -- the main checkout of this repository, seen from any of its git worktrees.

A worktree has the tracked files only. The untracked inputs the Mac scripts need (the lifted game in
output/recomp, tools/lua-5.3.3, the REPENTOGON submodule's files, .scratch/game-bundle) live once, in the
main checkout. `main_checkout(repo)` finds it from git's common directory; in the main checkout itself it
is the checkout. `shared(repo, *parts)` is repo/parts when that exists, else the main checkout's.
"""
import subprocess
from pathlib import Path


def main_checkout(repo: Path) -> Path:
    try:
        common = subprocess.run(["git", "-C", str(repo), "rev-parse", "--path-format=absolute", "--git-common-dir"],
                                capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return repo
    return Path(common).parent if common else repo


def shared(repo: Path, *parts: str) -> Path:
    here = repo.joinpath(*parts)
    if here.exists() and (not here.is_dir() or any(here.iterdir())):
        return here
    return main_checkout(repo).joinpath(*parts)
