---
name: doxygen-comments
description: >-
    Conventions and a checklist for writing, updating, and correcting Doxygen documentation comments in WIL's public C++ headers
    under `include/` so they match the repository's style and build without warnings. Applies only to headers under `include/` —
    not `tests/` or other code. Use when adding or fixing `//!` or `/** */` doc comments, `@param`/`@tparam`/`@return` tags,
    `@ref`/`@see` cross-references, or briefs, or when resolving warnings from the Doxygen `docs` build target.
---

# Updating and correcting Doxygen comments in WIL

WIL's public API is documented with Doxygen comments in the headers under `include/wil/`, and the docs are generated from
`docs/Doxyfile`. Use this skill to add, update, or fix those comments so they stay consistent with the codebase and generate
cleanly.

## Scope

This skill applies **only to WIL's public headers under `include/`** (primarily `include/wil/*.h`). Do **not** apply these
conventions to test code (`tests/`), documentation sources (`docs/`), packaging, or build scripts — those files are not part of
the generated API reference. If a request targets files outside `include/`, this skill does not apply.

## When to use

- Writing new documentation comments for public functions, classes, templates, or macros.
- Correcting existing comments after a signature or behavior change.
- Fixing Doxygen build warnings (mismatched parameters, undocumented members, broken references, etc.).

Do **not** rewrite comments purely for style, and do not document private/internal helpers (see "Public API only" below).

## WIL Doxygen conventions

- **Comment markers.** WIL documents public API with either `//!` line comments or `/** ... */` block comments. Match the style
  already used in the surrounding file / nearby declarations instead of mixing both. `//!` is common for file banners and member
  comments; `/** */` is common for free functions and templates.
- **Tag style is `@`, never `\`.** Use `@param`, `@tparam`, `@return`, `@brief`, `@ingroup`, `@ref`, `@see`, `@note`, `@file`,
  etc. The codebase uses zero backslash-style tags — keep it that way.
- **Brief = first sentence, on its own line.** `JAVADOC_AUTOBRIEF` makes the first sentence (up to the first period) the brief.
  Keep it on its own physical line; start the detailed description on the next line. An explicit `@brief` is rarely needed.
- **Wrap at 130 columns.** This matches `.clang-format` (`ColumnLimit: 130`) and applies to comment text too.
- **Public API only — but protected members still emit.** `EXTRACT_ALL = NO` and `EXTRACT_PRIVATE = NO`, so private members and
  undocumented internals are not emitted. Focus documentation on the public surface; don't add Doxygen tags to private helpers
  expecting them to appear in the output. Note that `EXTRACT_PROTECTED` is **not** set in `docs/Doxyfile`, so it defaults to
  `YES` — protected members *are* emitted and *do* warn when undocumented. Either document a protected member or wrap just the
  protected section in `/// @cond` … `/// @endcond`; private members need no such guard.
- **Hide internal `details` namespaces.** WIL's implementation details live in namespaces named `details` (and similarly named
  variants such as `details_abi`). These must not emit documentation — wrap the entire namespace in a `/// @cond` … `/// @endcond`
  pair (note the `///` marker used for these structural tags) so Doxygen skips its contents. Put `/// @cond` on its own line
  immediately before the namespace and `/// @endcond` immediately after its closing brace. See the example below. **Exception:**
  occasionally a `details` type is the natural host for members shared by public types (e.g. a common base whose methods the
  public types inherit). You may document that one type even though it lives in `details` — add a `@note` stating it is internal
  and not for direct use, and still `/// @cond` its own protected/private plumbing so only the shared public members show.
- **Add a usage example when the call pattern is non-obvious.** Include a short `~~~` fenced example for functions whose correct
  use isn't clear from the signature alone — e.g. callback or functor contracts (what the callback must do and return), paired or
  multi-step call sequences, RAII helpers whose placement or lifetime matters, round-trip or reverse operations, or subtle
  buffer/ownership conventions. Skip examples for self-explanatory helpers such as simple getters, predicates, or arithmetic.
  Doxygen fenced code blocks are delimited with `~~~` (any matching run of three or more tildes); inside `//!` banners, prefix
  each example line with `//!`.
- **Cross-references.** Link to other entities with `@ref <name>` and `@see`, and group related members with `@ingroup <group>`
  (for example `@ingroup outparam`). Prefer `@ref func()` (with the parentheses) when linking a function — the `()` delimits the
  name, so a trailing comma, period, or closing paren won't break the link, unlike a bare `@ref name`. Backticked code such as
  `get()` never becomes a link; use `@ref get()` when you want one. A `@ref` on a derived type resolves members it inherits from
  a base.
- **Namespaces.** Public entities live under `wil::`; STL-mirroring pieces live under `wistd::`.
- **Prefer a `PREDEFINED` macro over a per-guard escape; use `WIL_DOXYGEN` only when needed.** Doxygen evaluates `#if` guards
  against `docs/Doxyfile`'s `PREDEFINED` list, which already forces many conditions true in docs — e.g.
  `WINAPI_FAMILY_PARTITION(partition)=1`, `WIL_USE_STL=1`, `WIL_ENABLE_EXCEPTIONS`, `WIL_RESOURCE_STL`, and many `__cpp_lib_*`
  feature macros. First check whether the guard is already satisfied there; if so, no escape is needed. If a declaration is
  gated on a feature macro that is broadly useful, add that macro to `PREDEFINED` rather than sprinkling escapes. Only when the
  condition can't be satisfied that way — notably the mutually-exclusive `__WIL_*` / `__WIL_*_STL` header-wrapper guards in
  `resource.h`/`registry.h` — OR `|| defined(WIL_DOXYGEN)` into the condition (convert `#ifdef X` / `#ifndef X` to
  `#if defined(X)`; when the line wraps, continue with a trailing `\` and put `defined(WIL_DOXYGEN)` on the next line). A
  standalone `#ifdef WIL_DOXYGEN` block is for doc-only constructs with no real declaration to attach to.
- **Macros are expanded for docs.** Several macros are expanded when generating documentation (`WI_NOEXCEPT` → `noexcept`,
  `WI_NODISCARD` → `[[nodiscard]]`, and others in `docs/Doxyfile`'s `PREDEFINED`), so document the logical signature rather than
  the macro-heavy source.
- **Deleted members render but never warn.** Doxygen emits `= delete`d member functions (they show as a row in the output) but
  does *not* warn when they are undocumented — so "deleted means hidden" is false. Document a deleted overload when the deletion
  itself is the contract (e.g. an `operator co_await() &` deleted to reject awaiting an lvalue); otherwise it appears as a bare
  `= delete` with no explanation.
- **Keep every comment self-contained.** Doxygen reorders members (alphabetically) in the output, so a comment must stand on its
  own: never refer to declaration order ("the previous overload", "as above") or write "behaves like X, except…". Restate the
  relevant behavior on each entity even if it repeats a sibling.
- **Backtick `#include`s and header names in comment text.** A bare `#include` in a comment triggers an "explicit link request to
  'include' could not be resolved" warning, and a bare `<foo.h>` is parsed as an HTML tag and dropped from the output. Write the
  whole thing as code: `#include <ole2.h>` and `<wil/coroutine.h>`.
- **Keep comment text ASCII.** Don't paste em dashes (—), ellipses (…), smart quotes, or arrows into `.h` comment text; use `-`,
  `;`, `:`, `...`, or plain quotes. (This applies to the header comments only — Markdown docs like this file may use them.)

## Correction checklist

When adding or fixing comments, verify:

1. **Every `@param`/`@tparam` name matches the signature** — no stale, missing, misspelled, or reordered names. This is the most
   common source of `WARN_IF_DOC_ERROR` warnings.
2. **`@return` is present and accurate** for functions that return a meaningful value; omit it for `void`.
3. **The error-handling flavor is described** where relevant. WIL entities come in neutral, exception-based, `_failfast`, and
   `_nothrow`/`NoThrow` variants — state how the function reports failure (throws, fail-fasts, or returns an `HRESULT`).
4. **Tags use `@`** (convert any `\param`, `\brief`, etc.).
5. **The brief is a real one-line summary on its own line** (the first sentence), not a restatement of the name.
6. **Cross-references resolve** — `@ref`/`@see` targets exist and are spelled correctly.
7. **The description still matches behavior** after any signature or behavior change.
8. **Lines wrap at 130 columns** and the comment marker style matches the surrounding code.
9. **No documentation is added to private members**, but **protected members are documented or `@cond`-hidden** (they emit and
   warn by default), unless intentionally part of the documented surface.
10. **Internal `details`/`details_*` namespaces are wrapped in `/// @cond` … `/// @endcond`** so their contents are excluded from
    the generated documentation.
11. **Conditionally-compiled public declarations are visible in docs** — the guard is either already satisfied by `PREDEFINED`
    or ORs in `|| defined(WIL_DOXYGEN)` (reserved for conditions `PREDEFINED` can't cover, like the header-wrapper guards).
12. **Non-obvious functions carry a `~~~` usage example** — anything with a callback contract, a paired/multi-step call sequence,
    or subtle ownership/buffer semantics shows how to call it; trivial helpers do not.
13. **Comment text is plain ASCII and `#include`s/header names are backticked** — no em dashes, ellipses, or smart quotes in
    header comments, and `#include <foo.h>` / `<foo.h>` are written as code spans so Doxygen doesn't mangle them.
14. **Each comment stands on its own** — no references to declaration order or "same as the previous overload", since Doxygen
    reorders members in the output.

## Validating changes

Doxygen is configured with `WARN_IF_UNDOCUMENTED = YES` and `WARN_IF_DOC_ERROR = YES`, and formats warnings as `$file:$line:
$text`, so a docs build points directly at problems. From an already-configured build directory, run the `docs` target (it
requires Doxygen on `PATH`, and `WIL_BUILD_TESTS = ON`, which is the default):

```
ninja docs      # run from the configured build directory, e.g. build/msvc
```

The `docs` target just runs Doxygen, so it is independent of the selected configuration. Review its output for new warnings
referencing the files you touched — mismatched parameters, undocumented public members, and broken references all surface there.
Generated HTML lands under the build directory. If Doxygen is not installed, at minimum re-check each comment against the current
signature using the checklist above.

## Example

Correcting stale parameter names and backslash tags:

```cpp
// Before — backslash tags, @param name doesn't match the signature, missing @return.
/** Opens the widget.
\param widgetName  The name to open.
*/
HRESULT open_widget_nothrow(PCWSTR name, wil::unique_hwidget& widget);

// After — @-style tags, names match the signature, failure mode and return documented, wrapped at 130 columns.
/** Opens the named widget.
Returns an error code rather than throwing, so it is safe for callers built without exceptions.
@param name    Name of the widget to open.
@param widget  Receives the opened widget on success.
@return `S_OK` on success, or a failure `HRESULT` from the underlying `OpenWidget` call. */
HRESULT open_widget_nothrow(PCWSTR name, wil::unique_hwidget& widget);
```

Hiding an internal `details` namespace so Doxygen excludes it from the output:

```cpp
/// @cond
namespace details
{
    // Internal implementation — intentionally undocumented.
    template <typename T>
    using ensure_trivially_destructible_t = typename ensure_trivially_destructible<T>::type;
} // namespace details
/// @endcond
```

Adding a `WIL_DOXYGEN` escape so a guarded public declaration still appears in the docs:

```cpp
// Before — only compiled when <objbase.h> has been included, so Doxygen never sees this signature.
#if defined(__WIL_OBJBASE_H_)
template <typename T>
com_ptr<T> make_com_ptr(T* ptr);
#endif

// After — OR in defined(WIL_DOXYGEN) (which Doxygen predefines) so the signature is documented.
#if defined(__WIL_OBJBASE_H_) || defined(WIL_DOXYGEN)
template <typename T>
com_ptr<T> make_com_ptr(T* ptr);
#endif
```

## Status

This skill is an intentional starting point. The conventions above are grounded in the current headers and `docs/Doxyfile`, but
the checklist and examples are expected to grow as we refine it. When a rule here disagrees with real, nearby code in the file you
are editing, prefer matching the surrounding code and flag the discrepancy.
