#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Iterable, List, Optional


ALLOWED_TYPES = [
    "feat",
    "fix",
    "docs",
    "style",
    "refactor",
    "perf",
    "test",
    "chore",
    "revert",
]

# Conventional-ish header:
#   <type>(<scope>): <subject>
# or:
#   <type>: <subject>
_TYPE_ALT = "|".join(ALLOWED_TYPES)
HEADER_RE = re.compile(
    rf"^(?P<type>{_TYPE_ALT})(?:\((?P<scope>[a-z0-9][a-z0-9._/-]*)\))?: (?P<subject>.+)$"
)


@dataclass(frozen=True)
class LintIssue:
    message: str
    line: Optional[int] = None


def _read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def _strip_comments(lines: Iterable[str]) -> List[str]:
    # git adds comment lines (starting with '#') to COMMIT_EDITMSG depending on config.
    # Treat them as non-message content for linting.
    out: List[str] = []
    for line in lines:
        if line.startswith("#"):
            continue
        out.append(line)
    return out


def _is_ascii(s: str) -> bool:
    try:
        s.encode("ascii")
        return True
    except UnicodeEncodeError:
        return False


def lint_message(raw_message: str) -> List[LintIssue]:
    issues: List[LintIssue] = []

    # Normalize EOLs and ignore git comment lines.
    lines = raw_message.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    lines = _strip_comments(lines)

    # Trim trailing empty lines (common when editor leaves a newline at EOF).
    while lines and lines[-1].strip() == "":
        lines.pop()

    if not lines or lines[0].strip() == "":
        return [LintIssue("empty commit message")]

    # Special check: literal "\n" should never appear as a stand-in for newlines.
    # This often happens when a wrapper script passes a single -m string with "\\n".
    joined = "\n".join(lines)
    if "\\n" in joined:
        issues.append(
            LintIssue(
                'found literal "\\n" in commit message; use real newlines instead'
            )
        )

    header = lines[0].rstrip("\n")
    if header != header.rstrip():
        issues.append(LintIssue("trailing spaces in header line", line=1))

    # Allow merge commits / default git-revert messages.
    if header.startswith("Merge "):
        return issues
    if header.startswith("Revert "):
        return issues

    if not _is_ascii(header):
        issues.append(LintIssue("header must be ASCII (commit messages are English)", line=1))

    m = HEADER_RE.match(header)
    if not m:
        issues.append(
            LintIssue(
                "invalid header format; expected "
                "<type>(<scope>): <subject> or <type>: <subject>",
                line=1,
            )
        )
        return issues

    subject = m.group("subject").strip()
    if len(subject) > 50:
        issues.append(LintIssue("subject must be <= 50 characters", line=1))

    if subject.endswith("."):
        issues.append(LintIssue("subject must not end with a period", line=1))

    if not subject:
        issues.append(LintIssue("subject must not be empty", line=1))
    else:
        first = subject[0]
        if not first.isalpha() or not first.islower():
            issues.append(
                LintIssue(
                    "subject must start with a lowercase English verb (imperative mood)",
                    line=1,
                )
            )

    # If there's a body, require a blank line after header.
    if len(lines) >= 2 and lines[1].strip() != "":
        issues.append(LintIssue("missing blank line after header", line=2))

    return issues


def _git(args: List[str]) -> str:
    return subprocess.check_output(["git", *args], text=True)


def _lint_commit_rev(rev: str) -> int:
    msg = _git(["show", "-s", "--format=%B", rev])
    issues = lint_message(msg)
    if not issues:
        return 0
    print(f"commit message lint failed for {rev}:", file=sys.stderr)
    for issue in issues:
        loc = f"line {issue.line}: " if issue.line else ""
        print(f"  - {loc}{issue.message}", file=sys.stderr)
    return 1


def _lint_commit_range(rev_range: str) -> int:
    revs = [r for r in _git(["rev-list", "--reverse", rev_range]).splitlines() if r]
    if not revs:
        print(f"no commits in range: {rev_range}", file=sys.stderr)
        return 1
    rc = 0
    for rev in revs:
        rc |= _lint_commit_rev(rev)
    return rc


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Lint git commit messages.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--file", help="Path to commit message file (for commit-msg hook).")
    group.add_argument("--rev", help="Git revision to lint (e.g. HEAD).")
    group.add_argument("--range", dest="rev_range", help="Git rev range to lint (e.g. HEAD~10..HEAD).")
    args = parser.parse_args(argv)

    if args.file:
        msg = _read_text(args.file)
        issues = lint_message(msg)
        if not issues:
            return 0
        print("commit message lint failed:", file=sys.stderr)
        for issue in issues:
            loc = f"line {issue.line}: " if issue.line else ""
            print(f"  - {loc}{issue.message}", file=sys.stderr)
        return 1

    if args.rev:
        return _lint_commit_rev(args.rev)

    return _lint_commit_range(args.rev_range)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

