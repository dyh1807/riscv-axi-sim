# Tools Overview

## 回归脚本
- `run_regression.sh`

## Commit 规范检查
- `commit_msg_lint.py`: 提交信息 lint（支持 `--file`, `--rev`, `--range`）
- `setup_githooks.sh`: 一键启用仓库内置 hooks

启用 hooks：

```bash
bash tools/setup_githooks.sh
```

或：

```bash
git config core.hooksPath .githooks
```
