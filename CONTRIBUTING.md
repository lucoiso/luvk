# Contributing to luvk: code style, folders, class size, front end and back end

Read before writing, moving or reviewing C++ code. The same rules apply to the Lucoiso voxel engine, which uses luvk
(its `docs/rules/code-style.md`). `.clang-format` at the root does the mechanical part; this file is the rest.

## Style

- English only. File header `// Author: Lucas Vilas-Boas` / `// Year: 2026` / `// Repo: <repository url>`, then a
  blank line.
- PascalCase for types, functions, variables and parameters; `m_` members; no `b` prefix on bools; namespace
  `luvk`; Allman braces, always braces, 4 spaces, 150 columns; CMake commands in UPPERCASE.
- Declarations in headers, definitions in `.cpp`. Inline in headers only trivial getters and templates that must be.
- Includes: the file's own header first, a blank line, then every other include sorted (clang-format does it).
  Include what you use; prefer a forward declaration in a header when a pointer or reference is enough.
- `const` by default: locals, by-value parameters, member functions that change nothing; `T* const` for a pointer
  that is never re-pointed. One declaration per line.
- `static` for file-local helpers (in a named namespace in the `.cpp`) and for member functions that use no state.
  Never `static` for a value that can change at run time (settings, per-call data).
- Early returns are fine. At most 3 levels of nested `if` / `for`; at most 2 nested calls in one expression: name the
  intermediate values with `const` locals.
- Long conditions: group with parentheses, break after `&&` / `||`, cheap checks first, null check before use; more
  than about 3 checks become named `const bool` locals before the `if`.
- Long signatures: first parameter on the name line, the others one per line aligned under it.

## Comments

- **No comments in `.cpp` bodies.** The one exception: a single short `// <why>` line when the reason is not obvious
  (driver quirk, Vulkan rule, ordering requirement). Never narration, step lists, comments that repeat the code or
  commented-out code.
- **Headers:** one short `/** ... */` line per type, function and member saying what it is; multi-line only when one
  line cannot say it. Design discussion belongs in `docs/`, not in a header.
- No item ids, dates or change history in any comment.

## Blank lines

Code is split into logical groups, never packed. One blank line after a group of local declarations, before and after
each `if` / `for` / `while` / `switch` block, and before `return` (unless it is the whole body). Never a blank line
right after `{` or right before `}`, never two in a row, one between functions.

## Folders

```
<module>/
  CMakeLists.txt
  include/luvk/<Module>/<Area>/X.hpp   public API
  src/<Module>/<Area>/X.cpp               mirrors include/ one to one
  src/<Module>/<Area>/Private/X.hpp       internal headers
```

- Each module has its own include folder (`luvk/Core/`, `luvk/ImGui/`).
- A folder over about 8 files gets subfolders by concern; a subfolder of 3 files or fewer merges with a neighbour.
- The same subfolder names everywhere: `Features/` (render features), `Private/` (internal headers).
- A file goes where most of what it includes lives. `tests/` mirrors `source/`.
- Moving files is its own commit (renames only), never mixed with a logic change or a format change.

## Front end and back end

- **Back end** works without a window: state, rules, rendering. It never links `luvk::imgui`,
  never includes `imgui.h` and never uses an `Im*` type, not even in a signature (use `glm` or engine types).
- **Front end** is ImGui code: it draws and reads input. It changes state only through the back end's public API and keeps only view state (open tab, scroll, text being typed).
- Dependencies go one way, front end to back end. A module with UI has two CMake targets in its folder: `core/` (back
  end) and `ui/` (front end, include folder `luvk/<Module>Ui/`). A module without UI has only the back end.

## Class size

- One responsibility per class. Split by composition: each part is its own class with its own state and a small
  interface, and receives only what it uses by reference in its constructor, never the whole owner. The owner only
  holds and wires its parts.
- Spreading one class's methods over many `.cpp` files is not a split.
- Split when a class passes about 30 public functions or 15 members, a `.cpp` passes 500 lines, a function passes 60
  lines, or members carry prefixes of different subjects (`m_DeviceX`, `m_SwapchainY`).
- A split is its own commit and builds and passes the tests alone.

## Checks

The engine's `tools/check-structure.py` (run over luvk too) checks sizes, folders, ImGui in back end targets, comments in `.cpp` and blank lines, with
a baseline that only goes down. The CI (GitHub workflow) checks the format with `clang-format --dry-run --Werror`. Format only the lines
you change; the whole tree was formatted once, so a whole-file format of a clean file changes nothing.
