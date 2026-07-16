---
name: doxygen-comments
description: >-
    Conventions and a checklist for writing, updating, and correcting Doxygen documentation comments in WIL's C++
    headers so they match the repository's style and build without warnings. Use when adding or fixing `//!` or
    `/** */` doc comments, `@param`/`@tparam`/`@return` tags, `@ref`/`@see` cross-references, or briefs, or when
    resolving warnings from the Doxygen `docs` build target.
---

# Updating and correcting Doxygen comments in WIL

WIL's public API is documented with Doxygen comments in the headers under `include/wil/`, and the docs are generated
from `docs/Doxyfile`. Use this skill to add, update, or fix those comments so they stay consistent with the codebase
and generate cleanly.

## When to use

- Writing new documentation comments for public functions, classes, templates, or macros.
- Correcting existing comments after a signature or behavior change.
- Fixing Doxygen build warnings (mismatched parameters, undocumented members, broken references, etc.).

Do **not** rewrite comments purely for style, and do not document private/internal helpers (see "Public API only"
below).

## WIL Doxygen conventions

- **Comment markers.** WIL documents public API with either `//!` line comments or `/** ... */` block comments. Match
  the style already used in the surrounding file / nearby declarations instead of mixing both. `//!` is common for
  file banners and member comments; `/** */` is common for free functions and templates.
- **Tag style is `@`, never `\`.** Use `@param`, `@tparam`, `@return`, `@brief`, `@ingroup`, `@ref`, `@see`, `@note`,
  `@file`, etc. The codebase uses zero backslash-style tags — keep it that way.
- **Brief = first sentence.** `JAVADOC_AUTOBRIEF` is enabled, so the first sentence (up to the first period) becomes
  the brief. Lead with a concise one-line summary and put details in following sentences; an explicit `@brief` is
  rarely needed.
- **Wrap at 130 columns.** This matches `.clang-format` (`ColumnLimit: 130`) and applies to comment text too.
- **Public API only.** `EXTRACT_ALL = NO` and `EXTRACT_PRIVATE = NO`, so private members and undocumented internals
  are not emitted. Focus documentation on the public surface; don't add Doxygen tags to private helpers expecting them
  to appear in the output.
- **Hide internal `details` namespaces.** WIL's implementation details live in namespaces named `details` (and similarly
  named variants such as `details_abi`). These must not emit documentation — wrap the entire namespace in a
  `/// @cond` … `/// @endcond` pair (note the `///` marker used for these structural tags) so Doxygen skips its
  contents. Put `/// @cond` on its own line immediately before the namespace and `/// @endcond` immediately after its
  closing brace. See the example below.
- **Code samples use `~~~` fences.** Doxygen fenced code blocks are delimited with `~~~` (see existing headers).
  Inside `//!` banners, examples are written as indented lines that are each prefixed with `//!`.
- **Cross-references.** Link to other entities with `@ref <name>` and `@see`, and group related members with
  `@ingroup <group>` (for example `@ingroup outparam`).
- **Namespaces.** Public entities live under `wil::`; STL-mirroring pieces live under `wistd::`.
- **Add a `WIL_DOXYGEN` escape to preprocessor guards.** Doxygen predefines `WIL_DOXYGEN` (see `docs/Doxyfile`'s
  `PREDEFINED`), so a public declaration behind a `#if`/`#ifdef` guard is invisible to the docs unless the escape is
  OR-ed into the condition. Append `|| defined(WIL_DOXYGEN)` to the guard, converting `#ifdef X` / `#ifndef X` to the
  `#if defined(X)` form as needed. When the condition wraps, continue it with a trailing `\` and put
  `defined(WIL_DOXYGEN)` on the next line. A standalone `#ifdef WIL_DOXYGEN` block is used for doc-only constructs
  that have no real declaration to attach to.
- **Macros are expanded for docs.** Several macros are expanded when generating documentation (`WI_NOEXCEPT` →
  `noexcept`, `WI_NODISCARD` → `[[nodiscard]]`, and others in `docs/Doxyfile`'s `PREDEFINED`), so document the logical
  signature rather than the macro-heavy source.

## Correction checklist

When adding or fixing comments, verify:

1. **Every `@param`/`@tparam` name matches the signature** — no stale, missing, misspelled, or reordered names. This
   is the most common source of `WARN_IF_DOC_ERROR` warnings.
2. **`@return` is present and accurate** for functions that return a meaningful value; omit it for `void`.
3. **The error-handling flavor is described** where relevant. WIL entities come in neutral, exception-based,
   `_failfast`, and `_nothrow`/`NoThrow` variants — state how the function reports failure (throws, fail-fasts, or
   returns an `HRESULT`).
4. **Tags use `@`** (convert any `\param`, `\brief`, etc.).
5. **The brief is a real one-line summary** (the first sentence), not a restatement of the name.
6. **Cross-references resolve** — `@ref`/`@see` targets exist and are spelled correctly.
7. **The description still matches behavior** after any signature or behavior change.
8. **Lines wrap at 130 columns** and the comment marker style matches the surrounding code.
9. **No documentation is added to private/internal members** unless they are intentionally part of the documented
   surface.
10. **Internal `details`/`details_*` namespaces are wrapped in `/// @cond` … `/// @endcond`** so their contents are
    excluded from the generated documentation.
11. **Conditionally-compiled public declarations carry a `WIL_DOXYGEN` escape** — the `#if`/`#ifdef` guard ORs in
    `|| defined(WIL_DOXYGEN)` so the signature is emitted into the docs.

## Validating changes

Doxygen is configured with `WARN_IF_UNDOCUMENTED = YES` and `WARN_IF_DOC_ERROR = YES`, and formats warnings as
`$file:$line: $text`, so a docs build points directly at problems. From an already-configured build directory, run the
`docs` target (it requires Doxygen on `PATH`, and `WIL_BUILD_TESTS = ON`, which is the default):

```
ninja docs      # run from the configured build directory, e.g. build/msvc
```

The `docs` target just runs Doxygen, so it is independent of the selected configuration. Review its output for new
warnings referencing the files you touched — mismatched parameters, undocumented public members, and broken
references all surface there. Generated HTML lands under the build directory. If Doxygen is not installed, at minimum
re-check each comment against the current signature using the checklist above.

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

This skill is an intentional starting point. The conventions above are grounded in the current headers and
`docs/Doxyfile`, but the checklist and examples are expected to grow as we refine it. When a rule here disagrees with
real, nearby code in the file you are editing, prefer matching the surrounding code and flag the discrepancy.
