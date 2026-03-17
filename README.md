# cpin
A sticky note tool for your code. Attach notes to specific lines and save them locally without touching your source files.

## Editor integrations

| Editor | Repository |
|--------|-----------|
| VS Code | [jonaebel/cpin-vscode](https://github.com/jonaebel/cpin-vscode) |
| Neovim | [jonaebel/cpin.nvim](https://github.com/jonaebel/cpin.nvim) |

## Usage
```bash
# Add a note to line 42 in parser.c
cpin add src/parser.c:42 "why does this loop start at 1?"

# Add a note to your global store (~/.cpin/notes)
cpin add src/parser.c:42 "personal reminder" --global

# List all notes for a specific file
cpin list src/parser.c

# List all notes in the global store
cpin list --global

# Remove a note from line 42 in parser.c
cpin remove src/parser.c:42

# Remove a note from the global store
cpin remove src/parser.c:42 --global

# Search a note (global flag is working like above)
cpin search "personal" --global

# Export all notes as plain text
cpin export

# Export all notes as JSON
cpin export --json

# Export all notes as Markdown
cpin export --md

# Pipe Markdown export to a file
cpin export --md > NOTES.md

```

### `--json` and `--md` flags
`cpin export` prints all notes as plain text by default.
Pass `--json` for a machine-readable JSON array, or `--md` for formatted Markdown grouped by file.
Both flags work with `--global`.

### `--global` flag
By default, notes are stored in `.cpin/notes` inside your project directory.
Pass `--global` to store notes in `~/.cpin/notes` instead — useful for personal
reminders on files outside a project or notes you don't want committed.

## Installation

### Requirements
- clang (or any C compiler)
- make

### Build & Install
```bash
git clone git@github.com:jonaebel/cpin.git
cd cpin
make install       # may require sudo
```

To uninstall:
```bash
make uninstall     # may require sudo
```

## Roadmap

### v0.1 — Core (in progress)
- [x] `add`, `list`, `remove` commands
- [x] Single-file storage (`.cpin/notes`) in `file:line:content` format
- [x] `make install` target
- [x] `cpin list` with no arguments (show all notes in project)

### v0.2 — Search & Export
- [x] `cpin search <keyword>` — grep across all notes
- [x] `cpin export --json` — print notes as JSON array
- [x] `cpin export --md` — print notes as Markdown grouped by file
- [x] Per-project (`.cpin/`) vs global (`~/.cpin/`) storage via `--global` flag

### v0.3 — Editor Integration
- [ ] Git hook: warn when a noted line has moved or been deleted
- [ ] TUI interface (ncurses) for browsing notes
- [ ] Neovim plugin — show inline virtual text for noted lines
- [ ] VSCode extension

### v1.0 — Open Source Release
- [ ] `brew` formula / package manager distribution
- [ ] Stable CLI interface with man page
- [ ] CI/CD pipeline with tests
