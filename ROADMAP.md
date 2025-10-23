# Roadmap

This document serves to declare the future goals for Argo. The following table lists these goals out.

`Last Update: October 2025`

|Name|Description|Target|
|--|--|--|
|Required field|Adding in a required field for use when calling `Argo_PrintHelp()`.|2026|
|Print help widths|Allow for greater flexibility based on min-max width and terminal size.|2026|
|Print help colors|Allow for stylization based on existing, cross-platform standards.|2026|
|Improved error messages|Provide more specific issues and even suggestions to the user.|2026|
|Flag assignment|Allow for `--flag=value` format.|2026|
|Case insensitivity|Allow for case insensitivity in either/or flags and values.|2026|
|Last-in/First-in option|Allow whether the first or last value of a flag input should be used.|2026|

## Possible Additions

The following are features that may be added based on complexity and overall library intent. Bloat is to be avoided as a more streamlined library is preferred over an all-in-one solution.

|Name|Description|
|--|--|
|Multiple values|Allow for flag values such as `--flag multiple values`|
|Boolean negation|Negate boolean flags with `--no-*` syntax.|

---

**Rule of thumb:** This is meant to be an `argv` lexer, not a configuration manager.
