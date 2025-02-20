# Project guidelines

## Languages and tools

The core Dialog tools (`dgdebug` and `dialogc`) are implemented in C and built
with GCC and Make. We'd like to keep this source as portable as possible.

Additional tooling, such as for testing, is implemented in dependency-free
Python 3. Users shouldn't need to run Python code to build the core tools, but
contributors may need them to run tests etc.

The documentation is in Asciidoc. The public-facing docs are currently built
using Antora, and that build process is not part of this repository, and the
docs themselves do not use Antora-specific syntax or extensions.

## Versioning

There are a few different versions at play here:

- **Language** - The language version is a two-character string, like `1a`. This
  version changes when the Dialog language itself changes. (As a rule of thumb,
  it's a language change when existing code might break, or when we need to
  orchestrate changes across both the compiler and library.)
- **Compiler** - The compiler version is a combination of the language version
  and a two-digit suffix, like `1a/00`. This version applies to both `dialogc`
  and `dgdebug`, and increments whenever a new version of these tools is
  released.
- **Library** - An old-fashioned version string like `1.0.0`. The first two
  parts correspond to the two digits of the language version, so the first
  release of the library for language version `1z` would be `1.25.0`. (This
  allows users to determine whether a particular library and compiler are
  compatible... do they share the same language version?)
- **Documentation** - Any particular version of the documentation documents a
  single version of both library and compiler. We do not currently version the
  documentation separately, though that might change in the future.

A `-dev` suffix or similar indicates versions that are not (yet) an official
release.

## Contributing

Contributors are understood to license their contributions under the
[project's overall license](./license.txt).

In general, we prefer contributions to include any necessary documentation
changes, to pass existing tests, and to include new tests as appropriate.
Contributions without this may take longer to review or be rejected or reverted.

This is a community project, but we generally strive to follow the precedent or
vision set by Linus (the original creator of the language and tools) where we
can.