# Git hooks

This repo provides versioned hooks under `.githooks/`.

Enable them once:

```bash
git config core.hooksPath .githooks
```

## commit-msg

Runs `tools/commit_msg_lint.py` to enforce commit message conventions, including:

- Conventional header: `<type>(<scope>): <subject>`
- Subject length and punctuation checks
- Reject literal `\n` sequences in commit messages (must be real newlines)

