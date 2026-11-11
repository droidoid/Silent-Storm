# Porting rules (VS2022 port of the Jan03 A5 sources)

Read this before touching any source file. It is the contract every contributor (human or agent) follows.

## Layout

- `S:\` is a `subst` drive mapped to `Soft/Andy/Jan03/a5dll` (the path is otherwise too long for MSVC).
- `S:\vs2022\` holds everything new: `CMakeLists.txt`, `targets.cmake`, `build.cmd`, `compat/` (STLport
  shims), `stubs/` (stand-ins for missing commercial SDKs), `tools/` (helpers), this file, `BUILD_LOG.md`.
- The original `.sln/.vcproj` files stay untouched; CMake reads the file lists out of them.
- Build output goes outside the repo (default `C:\a5b\s`). Never write build output into the repo.
- Never modify or delete anything under `Complete/`, `Data/`, `bin/`, `Tools/`, `Versions/`.

## Building

```
set A5_BLD=C:/a5b/<yourdir>          (forward slashes; one build dir per person/agent)
python S:/vs2022/tools/b.py [-n N] [-w WIDTH] [-f REGEX] [--summary] <target> [<target>...]
```

`b.py` configures + builds the targets with `-k 0` and prints de-duplicated diagnostics with path noise
removed, then a tally by file / error code. `--nobuild` re-reads the last log. The full log is
`%A5_BLD%\last_<first target>.log`. Targets: see `targets.cmake` (libraries, `Main`, `Game`, tools, and the
object-only groups `Main_part_G`, `Main_part_A`, `Main_part_W`, `Main_part_I`).

## Editing

- Sources are **cp1251 with CRLF**. Russian comments and string literals must survive byte-for-byte.
- The Edit tool refuses non-UTF-8 files (good). For those, write a UTF-8 patch file and apply it with
  `python S:/vs2022/tools/ed.py <patchfile>` (format is documented at the top of `ed.py`).
  Pure-ASCII files may be edited directly. Never re-encode a file. Never use `sed -i`/PowerShell
  `Set-Content` on sources.
- Shell note: the Bash tool collapses `\\` into `\` even inside quoted heredocs - do not put regexes or
  Windows paths with backslashes into inline scripts; write a file with the Write tool and run it.

## What a fix may and may not do

Goal: the same program, compiled by MSVC 19.4x in permissive C++14 mode, x86. Not a modernization.

1. Smallest change that keeps the meaning. No reformatting, no renames, no "while I am here" cleanup.
2. Pure syntax fixes are made in place without ceremony: missing `typename`, `template<>`, missing return
   types (`int` was implied), `for`-scope, `friend class`, qualified member-function pointers (`&C::f`),
   two-phase lookup (`this->`), `>>` spacing, const-correctness of string literals.
3. Anything that is more than syntax gets a marker so it can be found later:
   `// VS2022 port: <why>` on the line, or `#ifdef A5_VS2022_PORT ... #else <original> #endif` when the
   original text is worth keeping. `A5_VS2022_PORT` is defined for every target.
4. Behaviour changes (stubs, disabled features, different algorithm) must be listed in
   `S:\vs2022\BUILD_LOG.md` under "Behaviour changes" by whoever makes them (append, keep entries short).
5. Never "fix" an error by deleting code, commenting a function body out, or adding a cast you do not
   understand. If something cannot be ported without a judgement call, stop on that item, describe it in
   your report, and move on to the next one.
6. Do not add third-party code or download anything. Missing SDKs (FMOD, LifeStudio, s3tc, Bink, Maya,
   Stingray) are replaced by stubs under `vs2022/stubs/` written from scratch against the calls the
   sources make - never by reproducing vendor headers from memory.

## Recurring STLport -> MSVC STL recipes

| Old code (STLport 4.5 / VC7) | Port |
|---|---|
| `c.push_back();` / `c.push_front();` (no argument) | `c.push_back( T() );` or `c.emplace_back();` |
| `it = c.insert( pos );` (one argument, default element) | `c.insert( pos, T() )` / `c.emplace( pos )` |
| `hash_map`, `hash_set`, `hash_multimap` | provided by `vs2022/compat/hash_map`, `hash_set` (aliases of `unordered_*`) |
| `hash<T>` specializations / custom hashers | keep; `std::hash` is used. A hasher must be `const`-callable |
| `vector<T>::iterator` used as `T*` (e.g. `&v[0]` written as `v.begin()`) | `&v[0]` / `v.data()`; guard empty vectors exactly as the original did |
| `c.erase( reverse_it )`, `const_iterator` -> `iterator` conversions | rewrite the one expression, same element |
| `list::size()` assumed O(1) / `splice` quirks | no change unless it fails to compile |
| `std::min/max` vs `min/max` macros with mixed types | windows.h macros stay enabled (no `NOMINMAX`); add a cast only where the template is picked |
| `string` built from `0`/`NULL`, `string += int` | make the conversion explicit, same value |
| `<iostream.h>`, `<strstream>` | `<iostream>`, `<strstream>` still exist |
| iterators of **unordered** containers used after insertion | re-check: `unordered_map` invalidates iterators on rehash, STLport `hash_map` did too - no change needed normally |

Compiler-side recurring items: missing `typename` before dependent types, default-int, `friend X;`,
taking the address of a bound member (`&obj.f`), non-const reference bound to a temporary (introduce a
named local), `enum` forward declarations, anonymous unions holding members with constructors (add an
empty user constructor under `A5_VS2022_PORT`), inline `__asm` (x86 only - keep), `__fastcall` is not the
default any more (irrelevant unless a callback type says so).

## Ownership while several people work in parallel

Each worker owns a set of translation units. Shared headers may be edited by anyone, but keep edits
minimal and local, re-read the spot right before patching (`ed.py` fails loudly if the text changed), and
rebuild afterwards - another worker may have touched the same header a minute ago.
Do not run `git add/commit/checkout/stash/reset`; the coordinator commits.
