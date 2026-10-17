"""Fetch exact dependency commits without changing existing user checkouts."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys


class DependencyError(RuntimeError):
    pass


def git(*args, cwd=None):
    result = subprocess.run(
        ["git", *args], cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if result.returncode:
        raise DependencyError(result.stderr.strip() or "Git command failed")
    return result.stdout.strip()


def verify_repo(repo, path):
    top = Path(git("rev-parse", "--show-toplevel", cwd=path)).resolve()
    if top != path.resolve():
        raise DependencyError(f"{path}: not a standalone dependency checkout")
    if git("remote", "get-url", "origin", cwd=path) != repo["url"]:
        raise DependencyError(f"{path}: origin differs from repos.json; checkout left unchanged")
    if git("status", "--porcelain", "--untracked-files=all", cwd=path):
        raise DependencyError(f"{path}: local changes present; checkout left unchanged")
    actual = git("rev-parse", "HEAD", cwd=path)
    if actual != repo["commit"]:
        raise DependencyError(
            f"{path}: expected {repo['commit']}, found {actual}; checkout left unchanged"
        )


def fetch_repos(root, repos, verify_only=False):
    root = Path(root).resolve()
    for repo in repos:
        pin = repo.get("commit", "")
        if not re.fullmatch(r"[0-9a-f]{40}", pin):
            raise DependencyError(f"{repo.get('path')}: full lowercase commit SHA required")
        path = root / repo["path"]
        if root not in path.resolve().parents:
            raise DependencyError(f"Dependency path must stay inside {root}")
        if path.exists():
            verify_repo(repo, path)
        elif verify_only:
            raise DependencyError(f"{path}: dependency missing; run fetch_repos.py first")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            # Retain the readable release tag for efficient shallow cloning, but
            # trust only its independently pinned commit; never substitute the tag tip.
            command = ["clone", "--no-checkout", "--depth", "1"]
            if repo.get("branch"):
                command += ["--branch", repo["branch"]]
            git(*command, repo["url"], str(path))
            git("checkout", "--detach", pin, cwd=path)
            verify_repo(repo, path)
        print(f"Verified {repo['path']} at {pin}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", action="store_true", help="Verify existing checkouts without fetching")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    try:
        repos = json.loads((root / "repos.json").read_text())
        fetch_repos(root, repos, args.verify)
    except (DependencyError, OSError, ValueError, KeyError) as error:
        print(f"Dependency error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
