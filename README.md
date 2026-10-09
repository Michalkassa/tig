# tig

A tiny version control system in C, modelled on git. It stores **snapshots**
of your files, tracks each file through a life cycle, and supports branches.
No dependencies — SHA-1 included.

[![CI](https://github.com/Michalkassa/tig/actions/workflows/ci.yml/badge.svg)](https://github.com/Michalkassa/tig/actions/workflows/ci.yml)

## Build

```sh
make            # builds ./tig
make test       # unit + end-to-end tests
```

Put `tig` on your `PATH` (or call it as `./tig`) and run it inside any directory.

## Quick start

```sh
tig init
echo "hello" > notes.txt
tig add notes.txt
tig commit -m "first note"

tig checkout -b idea          # new branch, switch to it
echo "wild idea" >> notes.txt
tig add notes.txt
tig commit -m "try an idea"

tig checkout main             # notes.txt is back to "hello"
tig graph
```

## File life cycle

Every file is in one of four states:

```
untracked  --tig add-->     staged
unmodified --edit file-->   modified
modified   --tig add-->     staged
staged     --tig commit-->  unmodified
unmodified --tig rm-->      untracked   (after the next commit)
```

`tig status` works these out by comparing three snapshots: the last commit
(HEAD), the staging area (index), and the files on disk.

## Commands

### `tig init`
Creates an empty repository (a `.tig/` directory) in the current directory,
on branch `main`.

### `tig add <path>...`
Stages files for the next commit. Each file is SHA-1 hashed and its content
stored under that hash.
- A **directory** (or `.`) adds every file inside it, and also stages the
  removal of tracked files that were deleted from disk.
- A tracked file you deleted can be passed by name to stage its deletion.

```sh
tig add main.c
tig add src
tig add .
```

### `tig rm <path>...`
Stops tracking files. The file **stays on disk**; after the next commit it
shows up as untracked. Works on directories too.

### `tig commit -m <message>`
Snapshots the staged files and moves the current branch to the new commit.
Refuses with `nothing to commit` if the staged snapshot equals the last one.

```
$ tig commit -m "fix parser"
[main 3fa1c02] fix parser (4 files)
```

### `tig status`
Shows the current branch and every file grouped by state:

```
On branch main

Staged (will be committed):
    new file:  b.txt

Modified (not staged, use 'tig add'):
    modified:  a.txt

Untracked (use 'tig add' to track):
    scratch.txt

Unmodified:
    README
```

Staged entries are `new file`, `modified`, or `removed`; unstaged ones are
`modified` or `deleted`.

### `tig log`
Lists commits from HEAD back to the first one, newest first.

```
commit e3044b048bbf54dcf78e1ac74aceeceb781e7ea8 (HEAD -> main)
parent bdfc64b2734e788a459e89221b57e7192e8c6409
date   2026-10-09 14:21:10

    third
```

### `tig show [commit]`
Shows one commit (default: HEAD) and its full snapshot, marking each file
relative to the parent commit. Accepts a short hash (4+ characters).

```
snapshot (2 files):
    57e72917b29f6eccdfce79d5d070659a0390acfc  modified   a.txt
    dcdc24e139db869eb059c9355c89c382de15b987  unchanged  b.txt
```

`unchanged` files point at the exact same stored blob as in the parent.

### `tig branch`
| Form | Effect |
|---|---|
| `tig branch` | list branches, `*` marks the current one |
| `tig branch <name>` | create a branch at the current commit (doesn't switch) |
| `tig branch -d <name>` | delete a branch (not the one you're on) |

Branch names may contain letters, digits, `-`, `_` and `.`.

### `tig checkout`
| Form | Effect |
|---|---|
| `tig checkout <branch>` | switch to a branch |
| `tig checkout -b <name>` | create a branch here and switch to it |
| `tig checkout <commit>` | jump to a commit (short hash ok); HEAD becomes *detached* |

Checkout rewrites your files to match the target snapshot — creating,
updating, and deleting files and directories as needed. To protect your work
it **refuses** when you have staged or unstaged changes, or when an untracked
file would be overwritten. Commit first.

In detached HEAD, new commits move HEAD itself rather than any branch; use
`tig checkout -b <name>` to keep them on a branch.

### `tig graph`
Draws every branch as a tree (coloured in a terminal; set `NO_COLOR=1` to
disable). `main` runs straight down the left; branches fork off to the right.

```
* e3044b0 (HEAD -> main) third
| * 6a7f671 (feature) more feature
| * a7347ad feature work
| | * e487047 (exp) experiment
| |/
|/
* bdfc64b second
* 78e5be4 first
```

Here `feature` and `exp` both branched off `second`.

## How it works

```
.tig/
├── HEAD              "ref: main" (on a branch) or a commit hash (detached)
├── refs/<branch>     hash of the branch's latest commit
├── index             staging area: "<hash> <path>" per file
├── objects/<sha1>    file contents, named by their SHA-1
└── commits/<sha1>    parent, time, message + the snapshot's file list
```

- **Files** are stored once per distinct content. An unchanged file in a new
  snapshot has the same hash, so it points at the blob already stored;
  only modified files add new objects.
- **Commit hash** = SHA-1 of the parent's hash plus every `<file hash> <path>`
  line. Each commit points at its parent, forming the history; two commits
  are equal exactly when their hashes are.
- **Branches** are just files holding a commit hash; committing moves the
  branch HEAD points to.

Not supported (yet): merging, `.tigignore`, symlinks, file permissions.

## Development

| Command | What it does |
|---|---|
| `make` | build `./tig` |
| `make test` | unit tests (`tests/unit/`) + end-to-end tests (`tests/e2e/`) |
| `make check` | tests again with `-Werror` and ASan/UBSan |
| `make hooks` | run `make test` before every git commit |
| `make clean` | remove build output |

CI (`.github/workflows/ci.yml`) builds and tests every push and pull request
on Linux (gcc, clang) and macOS, plus a sanitizer run. Pushing a `v*` tag
publishes Linux and macOS binaries to a GitHub release — only after all
tests pass.
