# Preamble

tmux portable is maintained from two repositories:

* `tmux` is the portable repository. It contains the portability layer,
  autotools build files, regression tests, documentation, and code needed for
  platforms outside OpenBSD.
* `tmux-openbsd-cutover` is the OpenBSD tmux import repository. Its `master`
  branch is the OpenBSD tmux history after the OpenBSD Git cutover. Portable
  merges this branch when importing OpenBSD changes.

To create the commits in the tmux-openbsd-cutover repo, the source repo is the
Github mirror of OpenBSD's src repo:

```text
https://github.com/openbsd/src.git
```

The update automation filters the OpenBSD source tree to `usr.bin/tmux/`,
publishes that filtered history as `openbsd-git` in the cutover repository,
cherry-picks new filtered commits onto cutover `master`, then merges cutover
`master` into portable `master`.

If you've never used git before, configure your identity before committing:

```sh
git config [--global] user.name "Your name"
git config [--global] user.email "you@yourdomain.com"
```

# Repository layout

The usual local layout is:

```sh
cd /some/where/useful
git clone https://github.com/tmux/tmux.git tmux-portable
git clone https://github.com/ThomasAdam/tmux-obsd.git tmux-openbsd-cutover
```

The exact directory names do not matter, but the examples below use:

```text
/path/to/tmux-portable
/path/to/tmux-openbsd-cutover
```

The cutover repository has three important branches:

* `master` is the branch portable consumes. It should contain tmux source
  only, not automation files.
* `openbsd-git` is the raw filtered OpenBSD tmux branch generated from
  OpenBSD src `master`.
* `automation` is an orphan branch containing the GitHub Actions workflow. It
  is separate so that workflow files are not merged into portable.

# Adding the OpenBSD remote to portable

In the portable repository, add the cutover repository as a remote:

```sh
cd /path/to/tmux-portable
git remote add tmux-openbsd /path/to/tmux-openbsd-cutover
git config remote.tmux-openbsd.tagOpt --no-tags
```

If the remote already exists, update it instead:

```sh
git remote set-url tmux-openbsd /path/to/tmux-openbsd-cutover
git config remote.tmux-openbsd.tagOpt --no-tags
```

Fetch the cutover master branch explicitly:

```sh
git fetch --no-tags tmux-openbsd master:refs/remotes/tmux-openbsd/master
```

# Automated syncing

The normal sync is performed by the GitHub Actions workflow in the
`automation` branch of the cutover repository. That workflow:

1. Fetches or clones OpenBSD src using a blobless clone.
2. Filters `usr.bin/tmux/` into a local `tmux-openbsd` branch.
3. Updates `tmux-openbsd-cutover/openbsd-git`.
4. Cherry-picks new `openbsd-git` commits onto cutover `master`.
5. Merges cutover `master` into portable `master`.
6. Pushes the changed repositories.

The workflow deliberately runs from the orphan `automation` branch but checks
out cutover `master` as the branch to update. This keeps `.github/` out of
cutover `master`, so portable does not import workflow files when it merges
OpenBSD changes.

# Manual portable merge

If the workflow fails while merging into portable, do the merge locally and
push the result.

Start from an up-to-date portable master:

```sh
cd /path/to/tmux-portable
git fetch origin
git checkout master
git pull --ff-only origin master
```

Fetch the cutover branch:

```sh
git remote add tmux-openbsd /path/to/tmux-openbsd-cutover 2>/dev/null || true
git config remote.tmux-openbsd.tagOpt --no-tags
git fetch --no-tags tmux-openbsd master:refs/remotes/tmux-openbsd/master
```

Merge it:

```sh
git merge --no-ff --log refs/remotes/tmux-openbsd/master
```

Resolve conflicts by deciding whether portable or OpenBSD owns the file.

Useful commands:

```sh
git checkout --ours path/to/file
git add path/to/file
```

This keeps the portable version of a conflicted file.

```sh
git checkout --theirs path/to/file
git add path/to/file
```

This takes the OpenBSD/cutover version of a conflicted file.

Before committing, inspect the result:

```sh
git status
git diff --check
git diff --cached --stat
```

Then finish and publish:

```sh
git commit
git push origin master
```

If the merge attempt is wrong, abort it:

```sh
git merge --abort
```

After pushing a manually resolved merge, rerun the workflow. It should either
no-op or continue from the now-merged portable state.

# Manual cutover update

Normally this is handled by the workflow. If it must be done manually, update
`openbsd-git` in the cutover repository from a filtered OpenBSD src checkout,
then cherry-pick the new filtered commits onto cutover `master`.

The important range is:

```text
origin/openbsd-git..openbsd-git
```

where `origin/openbsd-git` is the previously published filtered OpenBSD tmux
branch and `openbsd-git` is the newly generated filtered branch.

In the cutover repository:

```sh
cd /path/to/tmux-openbsd-cutover
git checkout master
git cherry-pick origin/openbsd-git..openbsd-git
git push origin master openbsd-git
```

Do not put workflow files on cutover `master`; keep them on the orphan
`automation` branch.

# Keeping an eye on libutil in OpenBSD

A lot of the `compat/` code in tmux comes from OpenBSD libraries, especially
imsg. Sometimes APIs change in OpenBSD in ways that require corresponding
portable changes. It is worth checking periodically for relevant OpenBSD
libutil changes and syncing those files into `compat/` as appropriate.

# Release tmux for next version

1. Update and commit README and CHANGES. The former should be checked for
   anything outdated and updated with a list of things that might break
   upgrades and the latter should mention all the major changes since the last
   version.
2. Make sure `configure.ac` has the new version number.
3. Tag with:

   ```sh
   % git tag -a 2.X
   ```

   Where `2.X` is the next version.

   Push the tag out with:

   ```sh
   % git push --tags
   ```

4. Build the tarball with `make dist`.
5. Check the tarball. If it is good, go here to select the tag just pushed:

   ```text
   https://github.com/tmux/tmux/tags
   ```

   Click "Add release notes", upload the tarball and add a link in the
   description field to the CHANGES file.
6. Clone the tmux.github.io repository, and change the RELEASE version in the
   Makefile. Commit it, and run `make` to replace `%%RELEASE%%`. Push the
   result out.
7. Change version back to master in `configure.ac`.
