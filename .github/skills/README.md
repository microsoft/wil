# Copilot CLI skills

This directory holds repository-scoped [skills](https://docs.github.com/copilot) for coding agents (GitHub Copilot CLI and
compatible tools). A *skill* is a folder containing a `SKILL.md` file that gives an agent focused, reusable instructions for a
specific task in this repository. Agents discover skills here automatically and either surface them as `/`-commands or invoke them
on their own when a request matches a skill's `description`.

## Layout

    .github/skills/
      <skill-name>/
        SKILL.md          # required: YAML frontmatter + Markdown instructions
        <supporting files> # optional: scripts, templates, examples referenced by SKILL.md

## `SKILL.md` format

Each `SKILL.md` starts with a YAML frontmatter block followed by a Markdown body:

~~~markdown
---
name: my-skill
description: >-
    One or two sentences describing what the skill does and, importantly, when an agent should use it.
---

# Human-readable title

Instructions the agent should follow when the skill is active...
~~~

Recognized frontmatter fields:

| Field            | Required | Purpose                                                                              |
| ---------------- | -------- | ------------------------------------------------------------------------------------ |
| `name`           | yes      | Unique skill id; should match the folder name.                                       |
| `description`    | yes      | What the skill does and *when* to use it. Agents match against this to auto-trigger. |
| `user-invocable` | no       | Set to `false` to hide the skill from manual `/`-command invocation (default: true). |
| `allowed-tools`  | no       | Comma-separated list of tools to pre-approve while the skill is running.             |

Keep the `description` action-oriented and specific about *when* to apply the skill — that text is the primary signal an agent
uses to decide whether a skill is relevant.

## Available skills

- [`doxygen-comments`](doxygen-comments/SKILL.md) — write, update, and correct Doxygen documentation comments in WIL's headers so
  they follow the repo's conventions and generate without warnings.
