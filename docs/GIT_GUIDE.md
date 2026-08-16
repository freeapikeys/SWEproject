# Git and GitHub — commands used on the AURA project

Prepared for **SIS 2075 Software Engineering 1**, Section 6 and 7 of the brief.

This is the working reference for the version-control part of the assignment:
every command the team used, what it does, and **how we actually used it on
this project**. Commands beyond those covered in class are marked ★ and carry
a justification, since the brief awards marks for those specifically.

---

## 1. Why Git and GitHub for this project

AURA is 45 source files across four modules with three developers working in
parallel. The engine, the domain model and the AI engines are only loosely
coupled — they meet at header files — which is exactly the situation
distributed version control is built for: three people editing different files
in the same tree, with a reliable way to combine the work and a full history
if a change turns out to be wrong.

Two moments on this project made the case concretely:

1. The colour macros in `canvas.h` were changed to fix an unsigned-underflow
   bug. That header is included by nearly every file, so the change touched
   the whole build. Being able to see the single commit that did it, and to
   revert it in isolation if it had gone wrong, is the entire argument for
   version control.

2. The stand allocator went through four attempts before it produced feasible
   plans. Each attempt was a commit on a branch; `main` kept working the whole
   time, and the failed approaches are still in the history to write about.

---

## 2. Setting up

| Command | What it does | How we used it |
|---|---|---|
| `git init` | Creates a new empty repository in the current folder | Run once in `AURA/` to start tracking the project |
| `git config user.name "..."` | Sets the name attached to your commits | Each member set their own so authorship is visible in the log |
| `git config user.email "..."` | Sets the email attached to your commits | Same; GitHub links commits to accounts by email |
| `git config --list` ★ | Prints the effective configuration | Used to check a member had set their identity *before* their first commit, so their work was attributed correctly |
| `git remote add origin <url>` | Registers the GitHub repository as a remote named `origin` | Connected the local repo to `github.com/<org>/aura` |
| `git remote -v` ★ | Lists remotes and their URLs | Verifying everyone pushed to the same repository |

---

## 3. The everyday cycle

| Command | What it does | How we used it |
|---|---|---|
| `git status` | Shows changed, staged and untracked files | Run constantly, before every commit |
| `git add <file>` | Stages a specific file for the next commit | Used to keep commits focused — e.g. staging only `ai_bagscan.c` when fixing the classifier |
| `git add .` | Stages everything changed | Used for the initial import |
| `git commit -m "..."` | Records the staged snapshot with a message | The unit of work throughout |
| `git commit -am "..."` ★ | Stages all *tracked* modified files and commits in one step | Used for small follow-up fixes where nothing new was added |
| `git log --oneline --graph --decorate --all` ★ | Compact visual history including branch topology | This is the command we screenshot for the demo — it shows branches and merges in one view |
| `git diff` | Changes in the working tree not yet staged | Reviewing our own work before committing |
| `git diff --staged` ★ | Changes already staged, i.e. exactly what the commit will contain | Final check before committing |
| `git show <hash>` ★ | Full contents of one commit | Used in review to explain a specific change to the group |

---

## 4. Branching

Branching is how three people worked at once without blocking each other.
Our convention: `main` always builds and runs; all work happens on a branch
named `feature/<area>`.

| Command | What it does | How we used it |
|---|---|---|
| `git branch` | Lists local branches, marking the current one | Checking where we were before starting work |
| `git branch <name>` | Creates a branch | Creating `feature/ai-engines` |
| `git checkout -b <name>` | Creates a branch **and** switches to it | The one we used in practice — `git checkout -b feature/baggage-system` |
| `git switch <name>` ★ | Switches branch (the modern, clearer alternative to `checkout`) | Used once the team learned it; `checkout` is overloaded and does too many unrelated things |
| `git merge <branch>` | Merges another branch into the current one | Merging each finished feature branch back into `main` |
| `git merge --no-ff <branch>` ★ | Forces a merge commit even when a fast-forward is possible | Used deliberately so the history *shows* that a feature branch existed — a fast-forward merge erases that evidence, which matters for this assignment |
| `git branch -d <name>` | Deletes a branch that has been merged | Cleaning up after each merge |
| `git branch -a` ★ | Lists local *and* remote-tracking branches | Checking what teammates had pushed |

Branches used on this project:

```
main
├── feature/render-engine     canvas.c, text.c, ui.c, icons.c
├── feature/domain-model      model.c, store.c, sim.c
├── feature/ai-engines        the five engines
├── feature/screens           the seven screens
└── fix/stand-allocator       four attempts at feasible allocation
```

---

## 5. Working with GitHub

| Command | What it does | How we used it |
|---|---|---|
| `git push -u origin main` | Uploads `main` and sets it to track the remote | First push, after `git remote add` |
| `git push` | Uploads commits on the current branch | After every working session |
| `git push origin <branch>` | Pushes a specific branch | Publishing a feature branch so others could see it |
| `git pull` | Fetches remote commits and merges them into the current branch | Start of every session, before touching anything |
| `git fetch` ★ | Downloads remote commits **without** merging | Used to inspect what a teammate had done before deciding to merge — safer than `pull` when you have uncommitted work |
| `git clone <url>` | Copies an entire repository, history included | How the second and third members got the project |

In VS Code these map to the Source Control panel: **⋯ → Pull**, the **Commit**
box, and **⋯ → Push**. The Synchronize Changes button is `pull` followed by
`push`. We used the panel day to day and the command line when we needed
something it does not expose.

---

## 6. Undoing things

This is the part that earns its keep, and the four commands differ in
important ways.

| Command | What it does | How we used it |
|---|---|---|
| `git restore <file>` ★ | Throws away uncommitted changes to a file | Recovering `theme.h` after an experiment with the palette |
| `git restore --staged <file>` ★ | Unstages a file, keeping the edit | Used after an accidental `git add .` picked up `aura.exe` before `.gitignore` existed |
| `git commit --amend` ★ | Replaces the previous commit | Fixing a typo in a commit message, and once to add a file forgotten from the commit |
| `git revert <hash>` ★ | Creates a **new** commit that undoes an earlier one | Used to back out the first stand-allocator attempt. Chosen over `reset` because the commit had already been pushed |
| `git reset --soft HEAD~1` ★ | Undoes the last commit, keeping the changes staged | Used to combine two commits that should have been one |
| `git reset --hard HEAD` ★ | Discards all uncommitted changes | Last resort, after a broken merge left the tree unbuildable |

**The rule we followed:** `reset` rewrites history and is only safe on commits
that have not been pushed. Once a commit is on GitHub and a teammate may have
pulled it, use `revert` — it undoes the change by adding to history rather
than rewriting it, so nobody else's clone breaks.

---

## 7. `.gitignore`

The file lives at the repository root and is itself tracked. Ours excludes
three categories:

1. **Build output** — `*.exe`, `*.o`, `build/`. Rebuildable from source, and
   committing a 440 KB binary on every change would bloat the repository.
2. **Run-time data** — `data/*.csv`, `data/*.log`. The *schema* matters and is
   defined in `store.c`; one developer's session data does not. A sample set
   is committed separately under `data/samples/`.
3. **Editor and OS noise** — `.vscode/`, `Thumbs.db`, OneDrive temporaries.

Useful commands:

| Command | What it does | How we used it |
|---|---|---|
| `git check-ignore -v <file>` ★ | Shows which `.gitignore` rule is excluding a file | Diagnosing why a file we wanted was not being tracked |
| `git rm --cached <file>` ★ | Stops tracking a file without deleting it | Removing `aura.exe` from tracking after it had been committed by mistake |

---

## 8. Collaboration on GitHub

For the demo, show these in the browser:

- **Insights → Network** — the branch graph, showing parallel work merging.
- **Insights → Contributors** — commits per member over time.
- **Settings → Collaborators** — how members were added to the project
  (Settings → Collaborators → Add people → their GitHub username).
- **Commits** — the history, with each commit attributed to its author.
- **Blame** on a file such as `ai_stand.c` — which member wrote which line.

Points worth making about distributed development:

- Every clone contains the **full history**, not a working copy. A member can
  commit, branch, diff and review with no network at all, then push when they
  reconnect — which matters on a campus connection.
- GitHub is the agreed meeting point, not the source of truth. If it vanished,
  any clone could restore it.
- Because history is content-addressed by SHA-1 hashes, a commit cannot be
  altered silently — the hash would change. This is what makes the record
  trustworthy as evidence of who did what.

---

## 9. The commands, in the order a session actually uses them

```bash
git pull                                  # start from what everyone else has
git checkout -b feature/baggage-system    # branch for today's work
# ... edit, build, test ...
git status                                # what changed?
git diff                                  # review it
git add src/screens/screen_baggage.c      # stage deliberately
git diff --staged                         # confirm the commit contents
git commit -m "Add sortation loop and make-up carousels"
git push origin feature/baggage-system    # publish the branch
# ... when the feature is finished and reviewed ...
git switch main
git merge --no-ff feature/baggage-system  # keep the branch visible in history
git push                                  # publish the merge
git branch -d feature/baggage-system      # tidy up
git log --oneline --graph --decorate --all
```
