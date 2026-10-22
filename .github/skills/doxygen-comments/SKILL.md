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
- **Public API only.** `EXTRACT_ALL = NO` and `EXTRACT_PRIVATE = NO`, so private members and undocumented internals are not
  emitted. Focus documentation on the public surface; don't add Doxygen tags to private helpers expecting them to appear in the
  output.
- **Hide internal `details` namespaces.** WIL's implementation details live in namespaces named `details` (and similarly named
  variants such as `details_abi`). These must not emit documentation — wrap the entire namespace in a `/// @cond` … `/// @endcond`
  pair (note the `///` marker used for these structural tags) so Doxygen skips its contents. Put `/// @cond` on its own line
  immediately before the namespace and `/// @endcond` immediately after its closing brace. See the example below. **Keep the
  `@cond`/`@endcond` region brace-balanced:** every `{` opened inside must also close inside, and never let an *enclosing*
  scope's closing brace (such as a `} // namespace wil` wrapping the `details` block) fall inside it. Hiding a scope's closing
  brace makes Doxygen miss the close and corrupts parsing for the whole build — see the diagnosis notes below.
- **Hide implementation overloads and specializations, not just `details`.** When a public name is really an overload set, or a
  primary template plus helper specializations, that all live directly in `wil::` (SFINAE overloads, or type specializations like
  `verify_bool<bool>` / `verify_bool<int>`), document the one primary that carries the contract and wrap the remaining
  implementation overloads/specializations in `/// @cond` … `/// @endcond`. This keeps the reference page to a single clear entry
  instead of a wall of near-identical mechanical ones.
- **Add a usage example when the call pattern is non-obvious.** Include a short `~~~` fenced example for functions whose correct
  use isn't clear from the signature alone — e.g. callback or functor contracts (what the callback must do and return), paired or
  multi-step call sequences, RAII helpers whose placement or lifetime matters, round-trip or reverse operations, or subtle
  buffer/ownership conventions. Skip examples for self-explanatory helpers such as simple getters, predicates, or arithmetic.
  Doxygen fenced code blocks are delimited with `~~~` (any matching run of three or more tildes); inside `//!` banners, prefix
  each example line with `//!`.
- **Cross-references.** Link to other entities with `@ref <name>` and `@see`, and group related members with `@ingroup <group>`
  (for example `@ingroup outparam`). Doxygen auto-links a documented name wherever it appears in text, so a bare `@ref <name>`
  (or even the bare name) usually links fine when scopes are intact — including class and function templates such as `com_ptr_t`
  or `com_agile_query`. A `@ref` that *won't* resolve is far more often a scope/structural problem (see "Diagnosing structural
  warnings") than a genuine need to qualify; qualify with `wil::` only to disambiguate a name, and note that `@ref wil::com_ptr_t`
  renders the qualified text `wil::com_ptr_t` rather than the tidier `com_ptr_t`. Doxygen strips a trailing `.` from a ref,
  but a trailing `)` or `'s` becomes part of the target and breaks it — keep punctuation off the name. A leading `::` on an
  external, undocumented name (`::Microsoft::WRL::ComPtr`) is read as an explicit link request and warns ("could not be
  resolved"); drop the `::` (or `%`-escape it) to leave it as plain text.
- **Namespaces.** Public entities live under `wil::`; STL-mirroring pieces live under `wistd::`.
- **Prefer a `PREDEFINED` macro over a per-guard escape; use `WIL_DOXYGEN` only when needed.** Doxygen evaluates `#if` guards
  against `docs/Doxyfile`'s `PREDEFINED` list, which already forces many conditions true in docs — e.g.
  `WINAPI_FAMILY_PARTITION(partition)=1`, `WIL_USE_STL=1`, `WIL_ENABLE_EXCEPTIONS`, `WIL_RESOURCE_STL`, and many `__cpp_lib_*`
  feature macros. First check whether the guard is already satisfied there; if so, no escape is needed. If a declaration is
  gated on a feature macro that is broadly useful, add that macro to `PREDEFINED` rather than sprinkling escapes. Only when the
  condition can't be satisfied that way — notably the mutually-exclusive `__WIL_*` / `__WIL_*_STL` header-wrapper guards in
  `resource.h`/`registry.h` — OR `|| defined(WIL_DOXYGEN)` into the condition (convert `#ifdef X` / `#ifndef X` to
  `#if defined(X)`; when the line wraps, continue with a trailing `\` and put `defined(WIL_DOXYGEN)` on the next line). A
  standalone `#ifdef WIL_DOXYGEN` block is for doc-only constructs with no real declaration to attach to. The **inverse**,
  `#if !defined(WIL_DOXYGEN)` around a fragment, *hides* it from docs while keeping it in real builds; use it for constructs that
  confuse Doxygen — notably a recursive template's own base (`struct priority_tag<N> : priority_tag<N - 1>` provokes a bogus
  "recursive class relation" warning), by guarding just the `: base` clause. See the example below.
- **Macros are expanded for docs.** Several macros are expanded when generating documentation (`WI_NOEXCEPT` → `noexcept`,
  `WI_NODISCARD` → `[[nodiscard]]`, and others in `docs/Doxyfile`'s `PREDEFINED`), so document the logical signature rather than
  the macro-heavy source.

## Correction checklist

When adding or fixing comments, verify:

1. **Every `@param`/`@tparam` name matches the signature** — no stale, missing, misspelled, or reordered names. This is the most
   common source of `WARN_IF_DOC_ERROR` warnings.
2. **`@return` is present and accurate** for functions that return a meaningful value; omit it for `void`.
3. **The error-handling flavor is described** where relevant. WIL entities come in neutral, exception-based, `_failfast`, and
   `_nothrow`/`NoThrow` variants — state how the function reports failure (throws, fail-fasts, or returns an `HRESULT`).
4. **Tags use `@`** (convert any `\param`, `\brief`, etc.).
5. **The brief is a real one-line summary on its own line** (the first sentence), not a restatement of the name.
6. **Cross-references resolve** — `@ref`/`@see` targets exist and are spelled correctly, with no trailing `)`/`'s` clinging to
   the name. A ref that won't resolve usually points at a scope/structural problem (see "Diagnosing structural warnings"), not a
   missing `wil::` qualifier — qualify only to disambiguate.
7. **The description still matches behavior** after any signature or behavior change.
8. **Lines wrap at 130 columns** and the comment marker style matches the surrounding code.
9. **No documentation is added to private/internal members** unless they are intentionally part of the documented surface.
10. **Internal `details`/`details_*` namespaces are wrapped in `/// @cond` … `/// @endcond`** so their contents are excluded from
    the generated documentation.
11. **Conditionally-compiled public declarations are visible in docs** — the guard is either already satisfied by `PREDEFINED`
    or ORs in `|| defined(WIL_DOXYGEN)` (reserved for conditions `PREDEFINED` can't cover, like the header-wrapper guards).
12. **Non-obvious functions carry a `~~~` usage example** — anything with a callback contract, a paired/multi-step call sequence,
    or subtle ownership/buffer semantics shows how to call it; trivial helpers do not.
13. **`@{`/`@}`, `~~~` fences, and `@cond`/`@endcond` are balanced** — and no scope-closing brace (e.g. `} // namespace wil`)
    is trapped inside a `@cond` region, which would leave the scope open and mis-attach every following member.

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

Two checks a per-file grep misses:

- **Compare the *total* warning count before and after — not just warnings that name your file.** A structural mistake in one
  header (an unbalanced `@cond`, an unclosed namespace or group) corrupts Doxygen's scope for **every** header that includes it.
  It can then mask — or, once fixed, unmask — hundreds of "undocumented" warnings elsewhere, so a large swing in the global
  count after a small edit is a signal, not noise.
- **Zero warnings is necessary but not sufficient — confirm visibility in the generated HTML.** A fully hidden entity produces no
  warning at all, so grep the emitted HTML (or open the page) to confirm the members you documented actually render and that your
  `@ref`s resolved to links.

### Diagnosing structural warnings

A few warnings point at a parsing problem, not the line they name:

- **`end of file while inside a group`** (reported at the file's last line) means a group or scope opened earlier never closed
  from Doxygen's view — a `@{`/`@name` missing its `@}`, an unbalanced `@cond` (below), or a runaway `~~~` fence that swallowed a
  `@}`.
- **A member reported "not documented" that *already has a comment*** signals an upstream structural bug — most often a
  `} // namespace …` trapped inside a `@cond` region above, leaving the namespace open so every following member is mis-scoped
  and its comment never attaches. Fix the structure; do **not** add a second, redundant comment.
- Verify `@{`/`@}`, `~~~` fences, and each `@cond`/`@endcond` are balanced and that no scope-closing brace hides inside a `@cond`.
  Fixing the root cause usually clears a whole cluster of downstream warnings at once.

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

Keeping a `@cond` region brace-balanced — the closing brace of an *outer* scope must stay outside it:

```cpp
// Wrong — `} // namespace wil` is trapped inside @cond, so Doxygen never sees namespace wil close.
namespace wil
{
/// @cond
namespace details { /* ... */ }
} // namespace wil
/// @endcond

// Right — @endcond comes before the outer namespace's closing brace.
namespace wil
{
/// @cond
namespace details { /* ... */ }
/// @endcond
} // namespace wil
```

Hiding a recursive template's base so Doxygen doesn't emit a "recursive class relation" warning:

```cpp
template <int N>
struct priority_tag
#if !defined(WIL_DOXYGEN) // hidden from docs, still compiled normally
    : priority_tag<N - 1>
#endif
{
};
```

## Status

This skill is an intentional starting point. The conventions above are grounded in the current headers and `docs/Doxyfile`, but
the checklist and examples are expected to grow as we refine it. When a rule here disagrees with real, nearby code in the file you
are editing, prefer matching the surrounding code and flag the discrepancy.
