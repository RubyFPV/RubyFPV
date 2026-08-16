# AGENTS.md — Rules of Work

Single source of truth for **how to work** in this repo, for humans and for AI
coding agents (Claude Code, Codex, or anything that reads `AGENTS.md` /
`CLAUDE.md`). Architecture and code-map live in `CLAUDE.md`; this file is about
process: evidence, git hygiene, and collaboration across forks.

If you are an agent: read this file **before** your first edit, and treat the
rules in **Non-Negotiables** as hard gates, not suggestions.

Personal machine details (device IPs, credentials, local paths, bench setup)
belong in an untracked `AGENTS.local.md` — **never commit them.**

---

## 0. Non-Negotiables

These six exist because each one has already cost real debugging days.

1. **Read the diff, not the commit message.** Commit messages, release notes and
   PR titles are claims. `git diff` is evidence. Never summarize, review, merge
   or cherry-pick a change you have only read the description of.
2. **No "fixed" without an A/B repro.** Baseline reproduces the bug → patched
   build does not → revert reproduces it again. Anything less is a hypothesis.
   Say "untested" and mean it.
3. **Verify the exact code path.** Finding a matching token is not finding the
   code that runs. Check the `#ifdef`, the backend, the actual call site, and
   whether the path is boot-applied or live-applied.
4. **No speculation.** Cite the file, line, log or capture. If you do not know,
   write "I don't know" and say what you would measure. Never invent a plausible
   mechanism to fill a gap.
5. **Don't restate the user's symptom as something stronger.** Quote what was
   actually observed. Do not upgrade "sometimes stutters" into "drops the link"
   in a PR description or an upstream issue.
6. **One feature = one branch = one PR.** No exceptions for "it's small" or
   "it's related".

---

## 1. Workflow: spec → draft → simplify → verify

Every non-trivial task passes through four gates. Do not advance until the
current gate passes.

**Spec.** Read the relevant source *before* proposing anything. Write a short
plan: what changes, which files, why, and what the rollback is. Record the
design decisions and their rationale — this is what stops an agent oscillating
between two approaches halfway through. Get human sign-off on the plan.

**Draft.** Implement the plan and nothing else. Minimal focused changes; no
drive-by refactors, no extra features, no reformatting untouched lines. Build
incrementally rather than batching all verification to the end.

**Simplify.** Review your own diff before showing it. Remove dead code, orphan
headers, unnecessary abstraction, and comments that restate the code. Ask: is
every hunk in this diff part of the one feature this branch is for?

**Verify.** Build clean. Then A/B on hardware (rule 2) if the change can affect
runtime behaviour. Report what you actually ran and what you did not.

---

## 2. Git & GitHub

### Branches

- Branch off the current shared base and **say which commit** in the PR body.
- One feature per branch. Name them `feat/…`, `fix/…`, `docs/…`.
- Never rewrite history on a branch someone else may have pulled. No
  force-push to shared branches without asking first.

### Staging

- **Never `git add -A` / `git add .`** A working tree is almost always a soup of
  several half-finished things. Stage by explicit path.
- **Read the staged diff before every commit** (`git diff --cached`). You are
  looking for hunks that belong to a different feature.
- Never commit: object files (`.o`), built binaries, build logs, IDE files,
  patch/backup scratch files, or personal config (`AGENTS.local.md`).

### Commits

- Message says **what changed and why**, not just what. The "why" is the part
  nobody can reconstruct later.
- If a change is derived from another fork, PR or upstream commit, **cite the
  source commit SHA in the message.** Attribution is not optional, and it is
  what lets the next person trace a regression back.

### Pull requests

- One reviewable concern per PR. If the diff needs the word "also", split it.
- PR body must state:
  - the base commit,
  - what was tested, **on exactly which hardware**,
  - what was *not* tested.
- **Never claim verification on hardware you do not own.** "Tested on
  SSC338Q + Radxa Zero 3E" is useful. "Tested and working" is not, and if the
  reviewer runs different hardware it is actively misleading.
- Reviewing a PR means reading its diff (rule 1). A green CI run is not a
  review.

### Taking changes from another fork — the reverted-hunk trap

Before re-applying any hunk you found in another fork, another PR, or an
upstream branch:

1. Check whether the target repo **already had it and reverted it.**
   `git log --all -S'<distinctive code fragment>' -- <path>` finds both the
   commit that added it and the one that removed it.
2. If it was reverted, **read the revert's message.** A revert is somebody's
   test result. Re-applying it silently reintroduces a known regression and
   throws away the evidence that produced it.
3. If you still believe the change is right, say so explicitly in the PR and
   present new evidence that beats the old evidence. Do not just re-land it.

This applies with full force to the adaptive-video, radio-rate and FEC paths,
where a change can look harmless in review and only show up as a link
regression in flight.

---

## 3. Working across forks

- The fork you branched from is the **integration point**. Open PRs there
  first; upstream curation is a separate, later decision.
- Keep your development line rebased on, or at least regularly compared
  against, that base. Long-lived divergence is what turns a 10-line fix into a
  hunk-by-hunk untangle.
- **Do not bundle a development line into a single release tag and expect it to
  be merged.** A tag is a snapshot, not a proposal. Split into per-feature
  branches before asking anyone to take the work.
- If a commit touches four unrelated subsystems, it is not one commit. Split it
  while you still remember which hunk was for what — the author can do this in
  minutes, a reviewer needs hours.
- Release notes must list **everything** in the diff. An unmentioned 500-line
  subsystem in a "bandwidth fix" release destroys the reviewer's ability to
  trust any of it.

---

## 4. Hardware & deploy safety

This is a radio system: a bad build does not throw an exception, it loses a
link on a flying aircraft. Treat runtime changes accordingly.

- **`make clean` after editing any `.h`.** The Makefile has no header
  dependency tracking. An incremental build after a header edit links objects
  with mismatched struct layouts → crash. Additive-only changes (a new
  `#define`) are safe; inserting, removing or reordering a struct member is
  not. When a fresh binary misbehaves right after a header edit, suspect an
  incomplete rebuild **first**.
- **Confirm the binary actually changed.** md5 the output. A green build with
  an unchanged hash means you shipped nothing — this happens whenever the build
  host's clock is behind the source file mtimes.
- **Deploy both sides together.** Never run a freshly-built vehicle binary
  against a stock or older ground station, or the reverse. The packet protocol
  drifts even at the same build number.
- **Fingerprint the working state the moment it works** — binary hashes, config,
  process list, a capture if relevant. Reconstructing "what was working
  yesterday" from memory does not work.
- On-device changes live in a writable overlay and are **lost on reflash or
  update**. Anything meant to persist has to go into an update package.

---

## 5. Architecture invariants

Violating these produces bugs that look like something else entirely.

- **The ground station owns video settings.** The vehicle applies them; it never
  decides them.
- **Video is vehicle → ground station only.** There is no reverse video path.
- Inter-process and air-to-ground traffic uses the standard packet format in
  `code/radio/radiopackets2.h`. Shared-memory IPC via `code/base/shared_mem.h`.
- Platform differences go through `#ifdef RUBY_BUILD_HW_PLATFORM_*`. A change
  that compiles on one platform is not a change that works on the others.
- Prefer restarting an encoder through its documented reload/restart path over
  `kill -9`. SIGKILL on the encoder is a known source of state corruption on
  resolution change.

---

## 6. Coding conventions

- **C** for low-level (base, radio, GPIO, drivers); **C++** for high-level
  (UI, processors, managers).
- Hungarian notation for variable names — project convention, follow it even if
  you dislike it.
- Config constants live in `code/base/config*.h`.
- `r_*` directories are runtime components; `ruby_*` names are executables.
- Validate input parameters; prefer graceful degradation over hard failure;
  recreate missing config files automatically rather than aborting.
- Match the surrounding code's style, comment density and naming. A diff that
  reads as foreign is harder to review than one that is merely wrong.

---

## 7. Mistakes to Avoid

- Do NOT summarize or merge a change you have only read the description of.
- Do NOT re-apply a hunk from another fork without checking whether it was
  reverted, and why.
- Do NOT claim a fix works without an A/B repro on hardware.
- Do NOT claim test coverage on hardware you do not have.
- Do NOT `git add -A`, or commit `.o` files, binaries, or personal config.
- Do NOT mix features in a branch, or ask for a release tag to be merged.
- Do NOT ship release notes that omit parts of the diff.
- Do NOT do an incremental build after editing a header.
- Do NOT deploy one side of the link against a stale other side.
- Do NOT force-push a shared branch without asking.
- Do NOT invent a mechanism to explain a symptom. Measure, or say you
  don't know.
