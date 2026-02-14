---
name: git-commit
description: "Git commit message 规范与本仓库硬性检查流程（commit-msg hook + lint 脚本）"
---

# Git Commit 规范技能

适用场景：
- 准备提交代码前，检查 commit message 是否符合规范
- 在本地开启并使用仓库内置的 commit-msg 硬性检查

## 1. 规范要求

提交标题必须符合：

- `<type>(<scope>): <subject>`
- 或 `<type>: <subject>`

`type` 允许值：
- `feat` `fix` `docs` `style` `refactor` `perf` `test` `chore` `revert`

`subject` 约束：
- 英文
- 建议祈使句
- 首字母小写
- 不超过 50 字符
- 结尾不加句号

## 2. 本仓库硬性约束

本仓库提供：
- `tools/commit_msg_lint.py`
- `.githooks/commit-msg`

`commit-msg` hook 会强制检查：
- 头部格式
- subject 规则
- body 空行规则
- 禁止字面量 `"\\n"`（必须使用真实换行）

## 3. 启用方式

一次性启用版本化 hooks：

```bash
git config core.hooksPath .githooks
```

或执行：

```bash
bash tools/setup_githooks.sh
```

## 4. 手动检查命令

- 检查某个提交：

```bash
python3 tools/commit_msg_lint.py --rev HEAD
```

- 检查一段提交：

```bash
python3 tools/commit_msg_lint.py --range HEAD~10..HEAD
```

## 5. 推荐提交流程

1. `git add ...`
2. `python3 tools/commit_msg_lint.py --rev HEAD`（可选，提交前自检）
3. `git commit`
4. 由 hook 自动执行最终校验
