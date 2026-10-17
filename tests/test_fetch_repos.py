"""Fetch regressions use temporary local repositories, never the network."""
import contextlib
import io
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import fetch_repos


class FetchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = pathlib.Path(self.temp.name)
        self.source = self.base / "source"
        self.source.mkdir()
        self.git("init", cwd=self.source)
        self.git("config", "user.email", "dependency-test@example.invalid", cwd=self.source)
        self.git("config", "user.name", "Dependency Test", cwd=self.source)
        self.git("config", "commit.gpgsign", "false", cwd=self.source)
        self.git("config", "tag.gpgsign", "false", cwd=self.source)
        (self.source / "source.txt").write_text("release one\n")
        self.git("add", "source.txt", cwd=self.source)
        self.git("commit", "-m", "Initial fixture", cwd=self.source)
        self.pin = self.git("rev-parse", "HEAD", cwd=self.source)
        self.git("tag", "v1", cwd=self.source)
        self.root = self.base / "consumer"
        self.root.mkdir()
        self.repo = {"url": self.source.as_uri(), "path": "dependencies/fixture", "branch": "v1", "commit": self.pin}
        self.checkout = self.root / self.repo["path"]

    def git(self, *args, cwd):
        return subprocess.check_output(["git", *args], cwd=cwd, stderr=subprocess.PIPE, text=True).strip()

    def fetch(self, verify=False, repo=None):
        with contextlib.redirect_stdout(io.StringIO()):
            fetch_repos.fetch_repos(self.root, [repo or self.repo], verify)

    def test_clone_is_exact_detached_and_repeatable(self):
        self.fetch()
        self.assertEqual(self.git("rev-parse", "HEAD", cwd=self.checkout), self.pin)
        self.assertEqual(self.git("rev-parse", "--abbrev-ref", "HEAD", cwd=self.checkout), "HEAD")
        before = (self.checkout / "source.txt").stat().st_mtime_ns
        self.fetch()
        self.fetch(verify=True)
        self.assertEqual((self.checkout / "source.txt").stat().st_mtime_ns, before)

    def test_verify_succeeds_with_origin_unavailable(self):
        self.fetch()
        self.source.rename(self.base / "offline-origin")
        self.fetch(verify=True) # No fetch/update/contact with the configured URL.

    def test_verify_never_creates_missing_checkout(self):
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch(verify=True)
        self.assertFalse(self.checkout.exists())

    def test_dirty_tracked_file_is_preserved(self):
        self.fetch()
        target = self.checkout / "source.txt"
        target.write_text("user work\n")
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch()
        self.assertEqual(target.read_text(), "user work\n")
        self.assertEqual(self.git("rev-parse", "HEAD", cwd=self.checkout), self.pin)

    def test_untracked_file_is_preserved(self):
        self.fetch()
        target = self.checkout / "extra.cpp"
        target.write_text("user work\n")
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch(verify=True)
        self.assertEqual(target.read_text(), "user work\n")

    def test_wrong_head_is_not_checked_out_or_reset(self):
        self.fetch()
        (self.checkout / "source.txt").write_text("local commit\n")
        self.git("config", "user.email", "dependency-test@example.invalid", cwd=self.checkout)
        self.git("config", "user.name", "Dependency Test", cwd=self.checkout)
        self.git("config", "commit.gpgsign", "false", cwd=self.checkout)
        self.git("commit", "-am", "User commit", cwd=self.checkout)
        actual = self.git("rev-parse", "HEAD", cwd=self.checkout)
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch()
        self.assertEqual(self.git("rev-parse", "HEAD", cwd=self.checkout), actual)

    def test_wrong_remote_is_not_rewritten(self):
        self.fetch()
        self.git("remote", "set-url", "origin", "file:///unrelated-user-origin", cwd=self.checkout)
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch()
        self.assertEqual(self.git("remote", "get-url", "origin", cwd=self.checkout), "file:///unrelated-user-origin")

    def test_invalid_pin_and_escape_path_are_rejected(self):
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch(repo={**self.repo, "commit": "v1"})
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch(repo={**self.repo, "path": "../escape"})
        self.assertFalse((self.base / "escape").exists())

    def test_existing_non_repository_is_preserved(self):
        self.checkout.mkdir(parents=True)
        (self.checkout / "user.txt").write_text("keep")
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch()
        self.assertEqual((self.checkout / "user.txt").read_text(), "keep")

    def test_clone_failure_is_reported(self):
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch(repo={**self.repo, "url": (self.base / "missing").as_uri()})

    def test_changed_tag_never_substitutes_new_commit_for_pin(self):
        (self.source / "source.txt").write_text("release two\n")
        self.git("commit", "-am", "Changed release tag", cwd=self.source)
        self.git("tag", "-f", "v1", cwd=self.source)
        # A shallow clone cannot obtain the old pin through the moved tag. The
        # fetch must fail rather than silently accepting the replacement tip.
        with self.assertRaises(fetch_repos.DependencyError):
            self.fetch()


if __name__ == "__main__":
    unittest.main()
