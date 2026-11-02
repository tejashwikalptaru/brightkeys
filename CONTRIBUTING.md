# Contributing to BrightKeys

Thanks for helping out! Bug reports, monitor/keyboard compatibility notes and pull requests are all welcome.

## Reporting a problem

Please use the issue templates. For anything brightness-related, the most useful details are:

- the monitor model and how it is connected (HDMI, DisplayPort, USB-C, dock, adapter...)
- whether DDC/CI is enabled in the monitor's on-screen menu
- the keyboard model, and what the *Add brightness up key...* window shows when you press the key

## Building

See [Build from source](README.md#build-from-source). `build.bat` sets up the MSVC environment by itself;
from a Developer Command Prompt it simply uses the active one.

## Code style

- C17, Win32 API only. Please don't add third-party dependencies or a C runtime-heavy approach.
- The build uses `/W4 /WX`: warnings are errors.
- One module per concern; shared declarations go in `src/brightkeys.h`. Module-private state is `static`
  and prefixed with `s_`, shared globals with `g_`.
- 4-space indentation, braces on the same line (see `.editorconfig`).
- Keep the hook procedure (`input.c`) fast: it runs for every key press system-wide.

## Commit messages and releases

Every merge to `main` publishes a release, and the version bump comes from the commit messages
([Conventional Commits](https://www.conventionalcommits.org/)):

- `fix: handle monitors that report max brightness 0` → patch
- `feat: per-monitor brightness offsets` → minor
- `feat!: new settings format` or a `BREAKING CHANGE:` footer → major

Squash-merging a PR uses the PR title as the commit message, so please give PRs a conventional title.

## Pull requests

1. Fork and create a branch.
2. Make the change and check that `build.bat` succeeds.
3. Test it on a real DDC/CI monitor if you can, and say in the PR what you tested.
4. Open the PR. CI builds it and attaches the installer and portable zip as an artifact.
