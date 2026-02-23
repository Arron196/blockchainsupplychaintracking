# Agent Development Guide

This document defines the mandatory engineering workflow for this repository.

## 1) Core Principles

- Keep changes small, reviewable, and reversible.
- Match existing project conventions before introducing new patterns.
- Prefer correctness and verifiability over speed.
- Never leave the branch in a failing build or failing test state.

## 2) Coding Standards

- Read related existing code before writing new code.
- Keep functions focused and names explicit.
- Do not hide errors; return or surface actionable failure messages.
- Do not introduce temporary hacks into committed code.
- Add comments only for non-obvious logic; avoid redundant comments.
- Keep files ASCII unless non-ASCII is explicitly required.

## 3) Branching and Worktree (Recommended Default)

Use a dedicated Git worktree for each feature/fix so tasks are isolated.

Example workflow:

```bash
# from main repository root
git fetch origin
git switch main
git pull --ff-only

# create isolated worktree for one task
git worktree add ../ws-feature -b feat/ws-events main
cd ../ws-feature
```

Rules:

- One worktree = one task branch.
- Do not mix unrelated changes in a single worktree.
- Delete completed worktrees after merge:

```bash
cd ../repo-root
git worktree remove ../ws-feature
```

## 4) Small-Feature Delivery Loop (Mandatory)

For every small feature, follow this exact cycle:

1. Implement one scoped change only.
2. Run targeted verification (build/test/lint relevant to the change).
3. If verification passes, stage only relevant files.
4. Commit immediately.

Do not batch multiple small features into one large commit.

## 5) Test-Then-Commit Policy

Before each commit:

- Run relevant tests for touched modules.
- Run project build if behavior or interfaces changed.
- Ensure no newly introduced warnings/errors remain.

Suggested commit pattern:

```bash
git add <related-files>
git commit -m "feat: add websocket telemetry event broadcast"
```

## 6) Commit Hygiene

- Keep commits atomic and focused.
- Message format: `<type>: <short purpose>`.
- Recommended types: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`.
- Explain *why* in commit body when behavior is non-trivial.

### Commit Author Identity (Mandatory)

- Commits must use the repository owner's Git identity.
- Do not override author with `GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL`,
  `GIT_COMMITTER_NAME`, or `GIT_COMMITTER_EMAIL` during normal workflow.
- If author information is wrong, stop and correct process first; do not continue creating more commits.

## 7) Pre-Push Checklist

- Branch/worktree contains only task-related changes.
- Local verification is green.
- Commit history is readable and logically ordered.
- No secrets, local database files, or build artifacts are staged.
