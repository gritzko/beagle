#   Repo layout and branches

Beagle refactors many aspects of git to make things more usable. That applies
to long-standing `git` issues as well as new difficulties caused by agentic
parallel-worktree workflows.

In particular, Beagle employs HTTP verb/URI vocabulary to systematize most of
revision control maneuvers and make remembering a dictionary of commands and
flags unnecessary. That benefits LLMs no less than us, mere humans.

Beagle employs multi-project repos. Having your own little GitHub in `~/.be`
is the most idiomatic usage. That is made in part to make submodules usable,
in part to facilitate parallel-worktrees workflows as the default mode. We
all know from experience a hundred ways to screw up a worktree, while a repo
as such is almost impossible to break. So why copy the repo?

Also, be usee file tree like branches - refinement of the git model where
branch nesting is sort of available, but not really. Every project has its
default trunk branch, arbitrarily nested branch dirs and tags attached to
particular branches. `be` steers the user towards ff-only branches, so
the tree structure allows for all the messiness to be put deeper in the
tree while the trunk and adjacent branches stay clean and linear. Staging
and stash is unnecessary then, being replaced by short dirty branchlets.

The state of a work tree is described by an URI that maps to a SHA1 hash,
as one might logically expect. One can check out local and remote branches
alike, clone (get), push (post), fetch (head), and pull (patch) at will.
The correspondence between git and be verbs is not 1:1, but close enough. 

Technically, our coordinates in the Beagle world are defined by
two pieces:

 1. the URI specifying a branch, which is backwards-compatible with git,
    `ssh://git@github.com:gritzko/beagle.git?v0.0.3`
 2. the hash URI resolved to, e.g. b8caa674c56d8d7fd483d20ed67850f0a4eebeb5

These are relayed to each be worker as one command-line argument:

    --at ssh://git@github.com:gritzko/libabc.git#ed4a385d2e47fa821c42df7a4003f6a9c850a94b

`be` prints the current state URI on every invocation.

##  URI structure

 1. scheme
      - projection schemes: `log:`, `diff:`, `commit:` etc are effectively
        read-only views/projections of the repo (non-verb commands).
      - network schemes that imply network connection: `ssh:`, `be:`, `http:`
 2. authority
      - check out remote branches, push to remotes, etc
      - no scheme: use the cached version
      - may not fully specify the host, e.g. "github" will find the full
        URI in the ref log (e.g. git@github.com:gritzko/beagle.git)
 3. path
      - for remotes: repo path,
      - for all other cases: relative path within a project,
 4. query - specifies the project, branch, tag and/or hash.
    User input takes any of these shapes:
      - `?/project/branch/v0.1` absolute path, project, branch, tag;
      - `?branch/` project-relative path, branch only;
      - `?./relative` path relative to the current branch;
      - `?..` relative;
      - `?26609afb` hashlet;
      - `?static scratch` commit message fragment;
      - `?null`, `?back` magic refs (no-branch, previous-checkout).
 5. fragment - opaque, verb-specific payload (commit message body,
    line jump, cherry-pick sha, projector count, etc.).  Never
    reinterpreted by the resolver, never amended in flight.

Every input query shape resolves to a single canonical form:

    ?/<project>/<branch-path>/<sha-or-tag>

Project segment is an opaque label.  Cross-clone identity (the
"is `abc` the same project as `libabc`?" question) is handled
structurally: a `be patch` whose source and target tips share no
common ancestor is refused as unrelated-history (graf computes
the merge base; empty intersection ⇒ refuse).  Genuine merges of
mirrors-with-different-labels go through the same LCA check and
pass naturally.

##  Repo dir layout

Git objects (blobs, trees, commits, tags) get appended to pack logs
and indexed by the workdogs (spot for search, graf for version
history, etc).  Each project is a single flat object shard: one dir
holds the objects for *all* local branches, all tags, and all
remote-tracking refs.  There are no per-branch object subdirectories
and no separate remotes store.

    .be/                                    # repo root
    .be/wtlog                               # default worktree log
    .be/<project>/                          # project shard (one dir)
    .be/<project>/refs                      # single ref log (ULOG)
    .be/<project>/wtlog                     # colocated worktree log
    .be/<project>/NNNNN.keeper              # pack log files (RON64 seqno)
    .be/<project>/NNNNN.keeper.idx          # keeper LSM index runs
    .be/<project>/NNNNN.graf.idx            # graf history index runs
    .be/<project>/NNNNN.spot.idx            # spot trigram index runs

Branches and tags are not directories; they are pure ref rows in the
single `<project>/refs` reflog.  A branch tip is `?heads/<name>` → sha;
creating a branch writes one ref row (no mkdir), dropping a branch
writes a ref tombstone (no dir or object deletion).  Remotes fold into
the same shard: fetched remote objects land in the project's pack logs
and remote-tracking refs (`//host?heads/...`) live in the same
`<project>/refs`.  Object retrieval consults exactly one dir — there is
no fan-out across a trunk → leaf dir chain.

Garbage collection is epoch-based and out of band.  A dropped branch's
objects simply linger in the shard; there is no per-branch GC and no
live object deletion.  At a major release the repo is recompacted by
copying the reachable closure into a fresh project id (e.g. `beagle` →
`beagle2`), leaving the dead objects behind in the old shard.

For secondary worktrees, `.be` is a wtlog file with its very
first record pointing at the repo root.

As an example of a workflow in this system, rebase on top of
remote may look like:

    # check out beagle remote (git can't have /project/branch)
    be get ssh://other.host?/project/branch 
    # apply local branch changes
    be patch ?my-local-branch     
    # ff commit to remote
    be post ssh://other.host      
    # reset my branch to rebased
    be put ?my-local-branch       

In this case, `post` will only be successfull (registered in the
log) if the rebased commit successfully fast-forwards the 
branch in the remote repo, while `put` does not verify ff-ness.

Another example, stashing:  

    be post '?null#belay'            # commit to no branch
    be get ?something-else
    ...
    be get ?back                     # return to the branch
    be patch #belay                  # apply the stash
    
Here, be makes some assumptions from the context. E.g. `be put`
may explicitly specify which branch to set to which hash 
`be put ?/project/branch#31dd71cc74f6be7b9a7ab8ea9633e567f552bb95`
or say `be put ?branch` and the rest will be derived from the
context (current project, current commit hash). `be put ?v1.2.3`
would create a tag while `be put ?feature/` insists we create a
branch (note the trailing slash). Note `null` (no branch) and
`back` (previous checked out branch) special names.

A normal commit-and-tag would look like:

    be post '#highly informative commit message'
    be put ?v1.2.3

A cautious worktree reset can be achieved by:

    be post '?null'

...while a careless reset is

    be get --force \?
