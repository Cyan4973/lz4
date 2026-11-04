# multiconf.make

**multiconf.make** is a self-contained Makefile include that lets you build the **same targets under many different flag sets**—debug vs release, ASan vs UBSan, GCC vs Clang, etc.—without mixing the resulting object files.
Objects are stored in a **cache directory named after the compilers and flags that built them**, so objects compiled with one configuration are never reused by another. Swap flags, rebuild, swap back: the previous objects are still there, and nothing is recompiled.

To add multiconf.make to a project, or update the copy a project keeps, follow [INTEGRATION.md](INTEGRATION.md).

---

## Key Benefits

| Why it matters | What multiconf.make does |
| --- | --- |
| **Isolated configs** | Stores objects in `.cache/multiconf/<hash>/`, one directory per set of compilers and flags. |
| **Fast switching** | Switching back to a configuration already built rebuilds nothing. |
| **Per-target flags** | `app: CFLAGS += -O3` gives `app` its own objects. Targets with identical flags share theirs. |
| **Cheap link changes** | Changing `LDFLAGS` or `LDLIBS` relinks, without recompiling. |
| **Header deps** | Editing a header recompiles only the files that include it. |
| **No source lists** | Sources are discovered in the current directory and below. |
| **One-liner targets** | Macros (`c_program`, `cxx_program`, …) hide all rule boilerplate. |
| **Parallel-ready** | Safe with `make -j`. |

---

## Quick Start

### 1 · Include

```make
# Makefile, at the root of the project
include multiconf.make
```

Sources (`.c`, `.cpp`, `.cc`, `.S`) are discovered in the current directory and below.

### 2 · Declare targets

```make
$(eval $(call c_program,app,src/main.o src/util.o))
$(eval $(call cxx_program,tool,tools/tool.o src/util.o))
$(eval $(call static_library,libutil.a,src/util.o))
```

Objects are named after their source file, from the current directory: `src/util.o` for `src/util.c`.
Sources outside of the current directory are added through `C_SRCDIRS` (see [Settings](#settings)), and named the same way: `../lib/util.o` for `../lib/util.c`.
The default goal is the first target defined before the include, or else after it (here `app`).

### 3 · Build any config you need

```sh
# Release with GCC
make CFLAGS="-O3"

# Debug with Clang + AddressSanitizer (new cache dir)
make CC=clang CFLAGS="-g -O0 -fsanitize=address"

# Switch back to GCC release (nothing to rebuild)
make CFLAGS="-O3"
```

Each binary is built in the cache, and `./app` is a symbolic link to the binary of the last configuration built.

---

## Per-target flags

```make
app: CFLAGS += -O3 -DNDEBUG
$(eval $(call c_program,app,src/main.o src/util.o))
```

Flags set on a target, before or after its macro, are part of its cache key: `app` gets its own objects, and editing these flags recompiles it.
Targets whose flags are identical share their objects.

A variable set on the command line (`make CFLAGS=-g`) replaces the values set by the Makefile, those of targets too: this is how GNU make works.
The flags the build needs have variables of their own, which stay.

### Flags the build needs

Flags without which the build fails, or builds something else (include paths, `-fPIC`, a soname, the defines that select the code), go to `MCM_CPPFLAGS`, `MCM_CFLAGS`, `MCM_CXXFLAGS` and `MCM_LDFLAGS`, globally or for a target.
The user does not set them: they stay when `CPPFLAGS`, `CFLAGS`, `CXXFLAGS` or `LDFLAGS` are set on the command line.

```make
MCM_CPPFLAGS += -Iinclude
libutil.so: MCM_CFLAGS += -fPIC
libutil.so: MCM_LDFLAGS += -Wl,-soname,libutil.so.1
```

Each command places them right before `CPPFLAGS`, `CFLAGS`, `CXXFLAGS` and `LDFLAGS`: the user's flags come last, and win where they conflict.
They are part of the cache keys, like the user's variables.
Defaults the user may replace, such as `-O3` or warnings, stay in `CFLAGS`.
Libraries (`-lm`) stay in `LDLIBS`, after the objects: `MCM_LDFLAGS` comes before them in the links of shared libraries and of make's built-in rules.
There is no `MCM_LDLIBS`: make's built-in rules write `$(LDLIBS)` in their recipes.

Make's built-in rules (`prog: prog.c`), and recipes written with their variables (`$(COMPILE.c)`, `$(LINK.c)`, `$(COMPILE.cc)`, `$(LINK.cc)`), get them too.
Other recipes of the Makefile add them where they use `CPPFLAGS`, `CFLAGS`, `CXXFLAGS` or `LDFLAGS`, each right before its user variable.

### Flags inherited from a parent target

```make
all32: CFLAGS += -m32
all32: app
```

make passes `-m32` to `app`, which gets its own objects, as with its own flags, with every version of GNU make.

---

## Makefile trees

Each Makefile that declares targets includes multiconf.make, e.g. `lib/Makefile` and `programs/Makefile`, run by sub-makes of the top Makefile.

- Each directory where make runs has its own cache, e.g. `lib/.cache/multiconf/`, deleted by its own `make clean`. In version control, ignore `.cache/` at any depth: no leading `/`.
- Variables set on the command line (`make CFLAGS=-O3`) reach the sub-makes: GNU make passes them on. The flags each Makefile needs stay: see [Flags the build needs](#flags-the-build-needs).
- A cache outside of the current directory (`make MCM_CACHE_ROOT=/tmp/cache`) is shared by the directories: the current directory is then part of the cache keys, so that the `src/main.o` of each directory stay apart. `make clean` in one directory deletes it whole. On Windows, see [Notes and limitations](#notes-and-limitations).
- multiconf.make can be included more than once, e.g. by two included files: only the first inclusion counts.

---

## Macros

Each macro is used as `$(eval $(call macro,parameters))`.

| Macro | Builds |
| --- | --- |
| `c_program` | an executable, linked with `$(CC)` |
| `cxx_program` | an executable, linked with `$(CXX)` |
| `static_library` | a static library, archived with `$(AR) $(ARFLAGS)`: `rv` unless the Makefile sets it |
| `c_dynamic_library` | a shared library, linked with `$(CC) -shared`. Its objects are compiled with its flags: add `-fPIC` where the platform needs it, e.g. `libutil.so: MCM_CFLAGS += -fPIC` |
| `cxx_dynamic_library` | a shared library, linked with `$(CXX) -shared`, for C++ objects: add `-fPIC` to `MCM_CXXFLAGS` |
| `program_base` | an executable, with an explicit compiler and flags variable |

`c_program_shared_o` and `cxx_program_shared_o` remain, as aliases of `c_program` and `cxx_program`.

| # | Parameter | Description |
| --- | --- | --- |
| 1 | name | The target, and the name of its link in the current directory |
| 2 | objects | Object files, named after their source from the current directory. An object with two sources (`src/util.c` and `src/util.cpp`) stops the build. |
| 3 | extra dependencies | Optional. Files built by their own rules, outside the cache, passed to the linker, except headers. `static_library` only archives object files. |
| 4 | commands | Optional. Run after the link. The built file is `$$@`. |
| 5 | extra hash | Optional, `static_library`, `c_dynamic_library`, `cxx_dynamic_library` and `program_base` only. Additional key for the cache directory. |
| 6, 7 | compiler, flags | `program_base` only: `CC` and `CFLAGS`, or `CXX` and `CXXFLAGS`. `MCM_` followed by the name of the flags (`MCM_CFLAGS`) comes before them. |

```make
$(eval $(call program_base,app,src/main.o src/util.o,$(LIBZ_A),strip $$@,$(VERSION),CC,CFLAGS))
```

### Order dependencies

```make
$(call mcm_order_deps,$(C_OBJS),deps/zstd/lib/zstd.h)
```

Makes these objects wait for `deps/zstd/lib/zstd.h`, made or updated first by its own rule (e.g. a fetched dependency, a generated header).
This matters before their first compilation, when no depfile lists their headers yet.
Its date alone does not trigger a recompilation; header tracking does, for the objects that include it.

### Objects built by your own rules

The macros only accept objects with a source.
Objects built by rules of your Makefile, e.g. from a generated source, must be declared in `MCM_EXTRA_OBJS`:

```make
MCM_EXTRA_OBJS := src/parser.o
$(MCM_CACHE_ROOT)/%/src/parser.o: src/parser.y | $(MCM_CACHE_ROOT)/%/src/.
	bison -o $(@:.o=.c) $<
	$(COMPILE.c) -MMD -MP $(@:.o=.c) -o $@
```

`-MMD -MP` writes the header dependencies of the object next to it, where multiconf.make reads them: a header change recompiles it.

For an object outside of the current directory, the rule uses its path in the cache (see [Cache layout](#cache-layout)): `$(MCM_CACHE_ROOT)/%/__/gen/parser.o` for `../gen/parser.o`.

---

## Settings

| Variable | Description |
| --- | --- |
| `MCM_CACHE_ROOT` | Cache directory, `.cache/multiconf` by default. `make clean` deletes it. |
| `MCM_EXCLUDE_DIRS` | Directories not searched for sources, besides hidden ones and the cache: paths from the current directory, or `find -path` patterns (`docs */experimental`). |
| `C_SRCDIRS`, `CXX_SRCDIRS`, `ASM_SRCDIRS` | Directories added to the discovered sources, e.g. excluded ones. Not recursive. |
| `C_SRCS`, `CPP_SRCS`, `CC_SRCS`, `ASM_SRCS` | Replace the discovered sources. So do `C_OBJS`, `CPP_OBJS`, `CC_OBJS`, `ASM_OBJS`. |
| `MCM_EXTRA_OBJS` | Objects built by rules of your Makefile. Set it before the macro calls. |
| `MCM_CPPFLAGS`, `MCM_CFLAGS`, `MCM_CXXFLAGS`, `MCM_LDFLAGS` | Flags the build needs, set by the Makefile, globally or for a target. They stay when the user sets `CPPFLAGS`, `CFLAGS`, `CXXFLAGS` or `LDFLAGS`: see [Flags the build needs](#flags-the-build-needs). |
| `MCM_STRIP` | `1`: strip programs and shared libraries after the link. Can be set for a single target (`app: MCM_STRIP := 1`). |
| `MCM_LD_RESPONSE_FILE` | `1`: pass the object list through a file, for small command line limits (Windows). The file, `app.rsp`, is next to the binary in the cache. Requires GNU make 4.0. Can be set for a single binary (`app: MCM_LD_RESPONSE_FILE := 1`). |
| `EXE` | Executable suffix, `.exe` by default on Windows (`OS=Windows_NT`, also set in MSYS2 and Cygwin). Programs are built and linked as `name$(EXE)`. Set it before the macro calls. |
| `HASH` | Hash program: `md5sum` if installed, else `md5` (macOS, BSDs). Without one, configurations are no longer kept apart, and a warning says so. |
| `V`, `VERBOSE` | `V=1` or `VERBOSE=1`: show the commands. `V=2`: also print the generated rules. |

multiconf.make provides `C_OBJS`, `CPP_OBJS`, `CC_OBJS`, `ASM_OBJS`, and `CXX_OBJS` (`CPP_OBJS` and `CC_OBJS`): the objects of all sources, e.g. `$(filter src/%,$(C_OBJS))`.

## Commands

| Command | Description |
| --- | --- |
| `make clean_cache` | Deletes the cache and the links of all binaries. `make clean` runs it too. |
| `make clean_other_configs` | Deletes the cache of the other global configurations, and keeps the current one: `make clean_other_configs CC=clang` keeps that of clang. Not for a cache outside of the current directory, which other directories may share. |
| `make V=1` | Shows the full compile and link commands. |

---

## Cache layout

```
.cache/multiconf/<global flags>/<target flags>/src/util.o       objects
.cache/multiconf/<global flags>/<target flags>/<link key>/app   binaries
./app -> .cache/multiconf/.../app                               links
```

- `<global flags>` hashes the compilers and compile flags in effect when multiconf.make is included, and the current directory if `MCM_CACHE_ROOT` is outside of it. Only the header dependencies of this configuration are read.
- `<target flags>` hashes `CC CXX CPPFLAGS CFLAGS CXXFLAGS`, and their `MCM_` variables, as seen by the target, including its own flags. All of them, for every target: changing `CXXFLAGS` also recompiles C programs.
- `<link key>` hashes the object list, the extra dependencies, the commands run after the link, the link flags and `MCM_STRIP`: changing them only relinks.
- Objects of sources outside of the current directory stay in the cache directory of their configuration: each `../` becomes `__/`, and an absolute path starts with `__root__/`. `../lib/util.o` is built as `<target flags>/__/lib/util.o`.

The cache never shrinks by itself: `make clean_other_configs` keeps only the current `<global flags>` directory, `make clean_cache` empties it.

---

## Notes and limitations

- Requires GNU make 3.81 or later (4.0 for response files), a POSIX shell, `find`, and a hash program.
- `.SECONDEXPANSION` is enabled for all rules after the include: a `$$` in a prerequisite list, or a `$` in a file name, is expanded a second time.
- The binaries built by the macros are phony targets: a rule that depends on one (`examples: liblz4.a`, or an extra dependency of a macro) runs at every build. To link a program with the code of a library, list the library's objects instead: with identical flags, they are shared, so nothing is recompiled.
- On Windows (MSYS2), links are copies of the binaries, refreshed at every build.
- On Windows (MSYS2), keep `MCM_CACHE_ROOT` relative, e.g. `../cache`: MSYS2 converts absolute paths for native compilers on their command line only, not in response files or depfiles.

## Upgrading from the previous version

This is version `2.0.0`, given by `MCM_VERSION`. The previous version had no number.
[INTEGRATION.md](INTEGRATION.md) gives the procedure, the patterns to look for in a Makefile, and how to check the result.

- Objects are named after their source from the current directory: `src/util.o`, not `util.o`. A wrong name stops the build: `app: no source for util.o: ...`.
- `C_SRCDIRS`, `CXX_SRCDIRS` and `ASM_SRCDIRS` are no longer required: sources are discovered, and these variables add directories.
- The cache moves from `cachedObjs/` to `.cache/multiconf/`, and `make clean` deletes both. `CACHE_ROOT` becomes `MCM_CACHE_ROOT`; `CACHE_ROOT` is ignored, with a warning when the Makefile or the command line sets it.
- `EXT` becomes `EXE`.
- `c_program_shared_o` and `cxx_program_shared_o` are identical to `c_program` and `cxx_program`: all targets with identical flags share their objects.
- In the commands run after the link of `c_program` and `cxx_program`, the built file is `$$@`, instead of `$$$$@`.
- Assembly sources are built with `CFLAGS` instead of `CXXFLAGS`.
- `c_dynamic_library` no longer adds `-fPIC`, and `static_library` no longer sets `ARFLAGS = rcs`: the Makefile sets them, where needed.
- `.SECONDEXPANSION` is enabled.
- `make` without a goal builds the first target of the Makefile: the previous version ran `clean_cache` when that target came after the include.
- The first build after upgrading recompiles everything.

---

## Tests

The tests are in the multiconf.make repository: projects do not copy them.

```sh
tests/run_tests.sh [make-command]
```

Builds `tests/project` in a temporary directory, and checks incremental rebuilds, the isolation of flags, every macro, and the settings.
CI runs it on Ubuntu (GNU make 4.4.1, gcc and clang, with and without response files), macOS (GNU make 3.81), FreeBSD (gmake, clang), and Windows (MSYS2 make, MinGW gcc, with response files).

```sh
tests/mutate.sh [-j jobs] [make-command]
```

Checks the test suite itself: each mutation breaks one feature of a copy of multiconf.make, and the test suite must then fail.
It reports the check that caught each mutation, and fails if one survives, or no longer applies after a change to multiconf.make.
Requires bash and perl. Runs `jobs` test suites at a time, the number of processors by default.

Before changing multiconf.make, read `AGENTS.md`, in the multiconf.make repository: the checks to run, and the rules the file follows.

## License

GPLv2 or later: see the header of `multiconf.make`.
