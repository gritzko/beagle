#include "GRAF.h"

#include <stdio.h>
#include <string.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/HUNK.h"
#include "graf/TDIFF.h"

// --- Helper: read file via mmap ---
static ok64 graf_read(u8cs *data, u8bp *mapped, u8cs path_arg) {
    sane(data != NULL && mapped != NULL);
    a_path(path, path_arg);
    call(FILEMapRO, mapped, $path(path));
    (*data)[0] = u8bDataHead(*mapped);
    (*data)[1] = u8bIdleHead(*mapped);
    done;
}

// --- Mode-change-only hunk: emit a single one-liner ---
static ok64 emit_mode_change(u8cs name, u8cs old_mode, u8cs new_mode) {
    sane(1);
    char body[256];
    int bl = snprintf(body, sizeof(body),
                      "old mode %.*s\nnew mode %.*s\n",
                      (int)$len(old_mode), (char *)old_mode[0],
                      (int)$len(new_mode), (char *)new_mode[0]);
    if (bl <= 0) return OK;
    hunk hk = {};
    $mv(hk.uri, name);
    hk.text[0] = (u8cp)body;
    hk.text[1] = (u8cp)body + bl;
    return GRAFHunkEmit(&hk, NULL);
}

ok64 GRAFDiff(u8cs old_path, u8cs new_path, u8cs name,
              u8cs old_mode, u8cs new_mode) {
    sane($ok(old_path) && $ok(new_path));

    // Detect extension (no leading dot) from logical name
    u8cs ext_nodot = {};
    PATHu8sExt(ext_nodot, name);

    // Read both sides (either may be empty for new/deleted files)
    u8bp map_old = NULL, map_new = NULL;
    u8cs old_data = {}, new_data = {};
    ok64 oro = graf_read(&old_data, &map_old, old_path);
    ok64 nro = graf_read(&new_data, &map_new, new_path);
    if (oro != OK && nro != OK) return oro;
    if (oro != OK) { old_data[0] = NULL; old_data[1] = NULL; }
    if (nro != OK) { new_data[0] = NULL; new_data[1] = NULL; }

    // Byte-identical content: emit mode-change hunk if modes differ
    if (oro == OK && nro == OK && u8csEq(old_data, new_data)) {
        b8 mode_diff = !u8csEmpty(old_mode) && !u8csEmpty(new_mode) &&
                       !u8csEq(old_mode, new_mode);
        if (mode_diff) {
            (void)GRAFArenaInit();
            emit_mode_change(name, old_mode, new_mode);
        }
        if (map_old) FILEUnMap(map_old);
        if (map_new) FILEUnMap(map_new);
        done;
    }

    if (GRAFArenaInit() != OK) {
        if (map_old) FILEUnMap(map_old);
        if (map_new) FILEUnMap(map_new);
        return NOROOM;
    }

    // Display name (NUL-terminated) for hunk titles
    char dispname[FILE_PATH_MAX_LEN];
    size_t dlen = (size_t)$len(name);
    if (dlen >= sizeof(dispname)) dlen = sizeof(dispname) - 1;
    memcpy(dispname, name[0], dlen);
    dispname[dlen] = 0;

    ok64 o = DIFFu8cs(graf_arena, old_data, new_data, ext_nodot,
                      dispname, GRAFHunkEmit, NULL);

    GRAFArenaCleanup();
    if (map_old) FILEUnMap(map_old);
    if (map_new) FILEUnMap(map_new);
    return o;
}
