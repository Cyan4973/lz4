# Integrating multiconf.make into a project

This guide is for whoever adds multiconf.make to a project, or updates the copy a project keeps: a person, or a coding agent.
[README.md](README.md) documents each feature. This guide is the procedure: what to change, what to look for, and how to check the result.

## 1. Identify the version in place

| The project's copy of multiconf.make has | Version | Then read |
| --- | --- | --- |
| `MCM_VERSION := x.y.z` | that version | the history of this repository since that version |
| `MCM_CACHE_ROOT ?= .cache/multiconf`, no `MCM_VERSION` | the OpenZL version, before 2.0.0 | [3.2](#32-from-the-openzl-version) |
| `CACHE_ROOT ?= cachedObjs` | version 1 | [3.1](#31-from-version-1) |
| no copy | | [README.md](README.md), then [4](#4-look-for-these-patterns) |

A copy can also have local changes, which replacing it loses, often without an error: a variable that the new version does not read is ignored.
Find the version of this repository closest to the copy, then compare them (in a clone of this repository, with the path of the copy):

```sh
copy=../project/build/make/multiconf.make
for c in $(git log --format=%h -- multiconf.make); do
    printf '%s %s\n' "$(git show "${c}:multiconf.make" | diff - "$copy" | grep -c '^[<>]')" "$c"
done | sort -n | head -n 1   # lines that differ, closest commit
git show <commit>:multiconf.make | diff - "$copy"
```

Carry each change over to the Makefile (target flags, `mcm_order_deps`), or into this repository. For example:

- `OBJ_DEPS`, prerequisites of all objects (xxHash, for its generated `xxhash.h`): `$(call mcm_order_deps,$(C_OBJS),xxhash.h)`.
- No `-fPIC` on Windows (lz4): `-fPIC` is no longer added, the Makefile sets it, e.g. `liblz4.so: override CFLAGS += -fPIC`, except on Windows.

## 2. Replace the file

- Copy `multiconf.make` whole, over the project's copy.
- Do not modify it. A local change is lost at the next update, and makes the copies of the projects diverge.
  If multiconf.make needs a change, make it in this repository.
- The cache is `.cache/multiconf/` (it was `cachedObjs/` in version 1), in each directory where make runs: ignore `.cache/` in version control, at any depth (no leading `/`).
- In a tree of Makefiles, keep a single copy (e.g. `build/make/multiconf.make`), included by each Makefile that declares targets: see [Makefile trees](README.md#makefile-trees).
- The first build after an update may recompile everything: from any version before 2.0.0, it does.

## 3. Adapt the Makefile

### 3.1 From version 1

- **Object names.** Objects are named after their source from the current directory: `src/util.o` for `src/util.c`, where version 1 used `util.o`.
  A wrong name stops the build: `app: no source for util.o: ...`.
  Replace lists built with `$(notdir ...)` with paths, e.g. `$(filter src/%,$(C_OBJS))`.
- **Sources.** Sources below the current directory are discovered: remove `C_SRCDIRS`, `CXX_SRCDIRS`, `ASM_SRCDIRS` and `vpath` settings made for them.
  Keep these variables for directories outside the current directory, or excluded with `MCM_EXCLUDE_DIRS`. They are not recursive.
- **Renamed variables.** `CACHE_ROOT` becomes `MCM_CACHE_ROOT`. `EXT` becomes `EXE`, which is `.exe` by default on native Windows.
  A project that sets `EXT` itself, from its own platform detection, e.g. to cross-compile for Windows, sets `EXE = $(EXT)` before the macro calls.
- **Commands after the link** (4th parameter of `c_program` and `cxx_program`): the built file is `$$@`, where version 1 needed `$$$$@`.
- **Assembly sources** (`.S`) are built with `CFLAGS`, where version 1 used `CXXFLAGS`.
- **`c_dynamic_library`** no longer adds `-fPIC`: set it on the library, where the platform needs it, e.g. `libfoo.so: override CFLAGS += -fPIC` ([4.1](#41-target-flags-and-the-command-line)).
- **`static_library`** no longer sets `ARFLAGS = rcs`: make's default is `rv`, which lists each archived object. To keep `rcs`, set `ARFLAGS = rcs`.
- `c_program_shared_o` and `cxx_program_shared_o` are aliases of `c_program` and `cxx_program`.
- **`UNAME`** is no longer set by multiconf.make: a Makefile that reads it sets it first, e.g. `UNAME := $(shell uname)`.
  Find: `grep -n UNAME Makefile`.
- Then check every pattern of [4](#4-look-for-these-patterns): version 1 had none of these checks.

### 3.2 From the OpenZL version

- **Commands after the link** (4th parameter of `c_program` and `cxx_program`): the built file is `$$@`, where the OpenZL version needed `$$$$@`.
  Find: `grep -n -F '$$$$@' Makefile`.
- **`EXE`** is `.exe` by default on Windows, and programs are built as `name$(EXE)` in the cache too.
- **`MCM_STRIP`** is read when the binary is linked: it can be set for a single target (`app: MCM_STRIP := 1`).
- **`c_dynamic_library`** no longer adds `-fPIC`: set it on the library, where the platform needs it, e.g. `libfoo.so: override CFLAGS += -fPIC` ([4.1](#41-target-flags-and-the-command-line)).
- **`static_library`** no longer sets `ARFLAGS = rcs`: make's default is `rv`, which lists each archived object. To keep `rcs`, set `ARFLAGS = rcs`.
- **Response files** (`MCM_LD_RESPONSE_FILE=1`) are written next to their binary, in the cache. `make clean` no longer deletes `*.rsp` files of the current directory.
- **Sources outside the current directory** (`../lib/f.c`): their objects stay in the cache directory of their flags, under `__/` for each `../`. Rules or variables written for such an object must use that name ([4.3](#43-rules-and-variables-written-for-objects)).
- **Flags inherited from a parent target** (`all32: CFLAGS += -m32` and `all32: app`) used to mix objects built with different flags. They now get their own objects, with every version of GNU make: nothing to convert.
- **`UNAME`** is no longer set by multiconf.make: a Makefile that reads it sets it first, e.g. `UNAME := $(shell uname)`.
  Find: `grep -n UNAME Makefile`.

## 4. Look for these patterns

Each pattern below builds without error with some versions of GNU make, and silently does something else than intended, or fails with others.

### 4.1 Target flags and the command line

```make
app: CFLAGS += -O3
```

A variable set on the command line (`make CFLAGS=-g`, or `$(MAKE) app CFLAGS="$(CFLAGS) -m32"` in a recipe) replaces the values set on targets: `app` is then built without `-O3`. This is how GNU make works, with or without multiconf.make.

**Fix**, where the flags of a target must apply in any case: `app: override CFLAGS += -O3`.
Do it at least for the targets that a sub-make builds with flags on its command line.
Flags that a library needs, such as `-fPIC`, too: `libfoo.so: override CFLAGS += -fPIC`.

### 4.2 Rules that depend on a binary

The binaries built by the macros are phony targets, so that their link follows the current configuration.
A file target that depends on one is rebuilt at every build: e.g. `examples/demo: liblib.a`, or a binary of a macro given as extra dependency (3rd parameter) of another macro, which is then relinked at every build.

**Find**: the second build of [5](#5-verify) relinks something, or runs commands of the project.

**Fix**: link the objects of the library instead of the library. With identical flags, the objects are shared: nothing is compiled twice.

```make
$(eval $(call c_program,demo,examples/demo.o $(LIB_OBJS)))
```

A phony target that depends on a binary (`test: app`) is not affected: it runs at every build anyway.

### 4.3 Rules and variables written for objects

Objects built by the project's own rules (from a generated source, for example) must be declared in `MCM_EXTRA_OBJS`, and their rule written for any cache directory:

```make
MCM_EXTRA_OBJS := src/parser.o
$(MCM_CACHE_ROOT)/%/src/parser.o: src/parser.y | $(MCM_CACHE_ROOT)/%/src/.
	bison -o $(@:.o=.c) $<
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $(@:.o=.c) -o $@
```

`-MMD -MP` writes the header dependencies of the object next to it, where multiconf.make reads them: a header change recompiles it.

Flags for a single object are set the same way: `$(MCM_CACHE_ROOT)/%/src/util.o: CFLAGS += -Wno-unused`.
They are not part of the cache key: after editing them, touch the source, or run `make clean`.

For an object outside the current directory, these rules and variables use its name in the cache: `$(MCM_CACHE_ROOT)/%/__/gen/parser.o` for `../gen/parser.o`.

**Find**: `grep -n -F '$(MCM_CACHE_ROOT)/%' Makefile`.

### 4.4 Discovered sources

All `.c`, `.cpp`, `.cc` and `.S` files below the current directory are discovered, except in hidden directories and the cache.
Only the objects listed in macro calls are built, but `C_OBJS` and the other lists include them all.

**Find**: the build of [5](#5-verify) compiles sources it should not (tests, examples, vendored code), because the Makefile uses `$(C_OBJS)` or another whole list.
Large directories without sources (`node_modules`, build directories) only cost their traversal.

**Fix**: filter the lists (`$(filter src/%,$(C_OBJS))`), or exclude directories: `MCM_EXCLUDE_DIRS := docs build */experimental`.

### 4.5 Secondary expansion

multiconf.make enables `.SECONDEXPANSION`, for every rule after its inclusion: a `$$` in a list of prerequisites, or a `$` in a file name, is expanded a second time.

**Find**: `grep -n -E '^[^#[:space:]=]([^=:]|:[^=])*\$\$' Makefile` (rules whose prerequisites contain `$$`).

### 4.6 Internal names

The names documented in [README.md](README.md) are the interface: the macros, `mcm_order_deps`, the `MCM_*` settings, `MCM_VERSION`, `C_OBJS` and the other lists, `EXE`, `HASH`, `V`.
Other `mcm_*` and `MCM_*` names are internal, and change between versions (`MCM_LINK_KEY_*` no longer exists in 2.0.0).

**Find**: `grep -o -E '\b(MCM|mcm)_[A-Za-z_]+' Makefile | sort -u`, and compare with [README.md](README.md).

### 4.7 Silent commands

Unless `V=1` (or `VERBOSE=1`), multiconf.make sets `.SILENT`, for the whole Makefile: the commands of the project's own rules are no longer printed either.

## 5. Verify

Replace `all` with the project's usual goals, and run them from a clean state:

```sh
make clean
make -j8 all                                  # 1. builds
make all | grep -E '^(CC|CXX|AS|LD|AR) '      # 2. prints nothing: nothing recompiled, nothing relinked
make all CFLAGS=-O0 >/dev/null                # 3. builds another configuration...
make all | grep -E '^(CC|CXX|AS|LD|AR) '      #    ...then switching back prints nothing
touch src/some_header.h
make all | grep -E '^(CC|CXX) '               # 4. recompiles only the objects that include it
make clean && ls -a                           # 5. no .cache/multiconf, no links of binaries
```

Then:
- Run the project's tests, and its CI.
- On Windows (MSYS2), build with `MCM_LD_RESPONSE_FILE=1` if link lines are long.

## 6. Report

- The version replaced, and the version installed (`MCM_VERSION`).
- Each change made to the Makefile, and the pattern of [3](#3-adapt-the-makefile) or [4](#4-look-for-these-patterns) it answers.
- The results of [5](#5-verify), including anything that did not behave as described.
- Problems of multiconf.make itself, or of this guide: to fix in this repository, not in the project's copy.
