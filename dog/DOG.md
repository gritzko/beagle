#   Dog API

The codebase is divided into *dogs*. Each dog has its purview,
a part of data and functionality. The dependency graph is like:
   
   beagle --> spot,graf,sniff,... --> keeper --> libdog

 1. Each dog provides a static library and an executable.
 2. CLI convention: `dog [verb] [--flags] URI*` (see dog/CLI.h).
 3. Dogs find their home and each other using dog/HOME
 4. The static lib has a `name` control struct and three uniform
    entry points:
      - `ok64 DOGOpen(u8cs branch, b8 rw)`
           open the dog's shards for `branch` in the process-wide
           home singleton.  May be called repeatedly with different
           branches to compose a multi-branch view; the FIRST call
           (across *all* dogs, process-wide) decides whether the
           session is writable.  Later calls are downgraded to ro
           regardless of `rw` — see "Branch sharding" below.
      - `ok64 DOGExec(name* state, cli* c)`
           execute a parsed CLI (verb + flags + URIs) against
           the open state; equivalent to invoking `name ...` as
           a process.  Indexing dogs (spot, graf) pull what they
           need from keeper via keeper's URI- and ULOG-based read
           APIs; nothing is pushed into them.
      - `ok64 DOGClose(name* state)`
           close every shard opened by this dog.
 5. `be` dispatches the HTTP-like verb vocabulary below to other dogs.
    Most often, by spawning processes. `be` defines the order.
    Most dogs maintain their indexes of the repo, normally as sorted
    runs in branch dirs, e.g. `.be/branch/0000000002.spot.idx`.

##  HTTP verb vocabulary

The verb vocabulary shared by all dogs is more or less HTTP-like:

    be get URI               repo → worktree retrieval
    be post URI              worktree → repo filing
    be delete URI            remove branch/tag/file
    be put URI               repo → repo, intra-repo ops
    be patch URI             transform in place (spot replace)

No verb = read-only view or search.  A verb = action with direction.

    be /path                 view file
    be '#search.ext'         search working tree (spot)

The history of commands get logged in ulog files (refs, wtlog) in
the format: timestamp verb uri.

##  URI convention

Dogs accept URIs of the form:

    [scheme:] [//authority] [path] [?ref] [#fragment]

  - `scheme:` is either the app/applet/view or transport protocol,
    either way it says which code has to process this URI
  - `//authority` — remote host or alias (`//origin`, `//host/path/repo`)
  - `path` — repo-relative file or directory,
  - `?ref` — branch, tag, SHA of the version in question,
  - `#fragment` — any value, file location or search string, or
    branch/hash or commit message.

URIs are canonicalised via `dog/DOG:DOGCanonURI` on input.
Some parts of URI can be relative or ambiguous, also resolved on input.
Dogs that construct queries internally are expected to produce 
non-ambiguous canonical form directly.

##  Indexing (post-DOGUpdate)

There is no push path into a dog's index. Each indexing dog runs
its own pass driven by the URI it was invoked with (whatever `be`
forwarded from the user — trunk, a branch, a sha, a range).

  - **spot** — indexes the *current tip versions* and the files
    in them.  Keys content by the hash of the full repo-relative
    path (no more hashlet → fn_hashlet split — spot recurses the
    tree itself).  To avoid reindexing unchanged blobs across
    runs, spot keeps a per-blob memo: 40-bit blob hashlet → 20-bit
    *path* hashlet.  On a later pass, equal path hashlet ⇒ skip;
    different ⇒ reindex (catches renames).  Same shard slot the
    old hashlet → fn_hashlet record used; format redefined.
  - **graf** — indexes the commit graph.  Walks back from each
    tip in the URI; stops at any commit already present in graf's
    own index (mention ≡ known).  Own shard family.
  - **sniff** — no index.  Updates the worktree and writes its
    attribution rows to ULOG; reads come from the worktree and
    ULOG, never from a shard graph.
