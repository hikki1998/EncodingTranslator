# Repository Guidelines

## Project Structure & Module Organization

This is a Qt5/CMake C++17 Windows desktop application for scanning and converting source file encodings.

- `src/` contains application code: `main.cpp`, UI logic in `MainWindow.*`, workers in `SearchWorker.*` and `ConvertWorker.*`, and encoding/filter logic in `EncodingUtils.*`.
- `tests/` contains executable-based tests, currently `test_search.cpp` for search and exclusion behavior.
- `resources/` contains Qt and Windows resources, including `app.qrc` and `app.rc`.
- `build/` and `out/` are generated outputs; do not commit generated binaries or deployment artifacts.

## Build, Test, and Development Commands

Run commands from the repository root in PowerShell.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --build build --config Release --target test_search
./build/Release/test_search.exe
```

- Configure with CMake using Qt 5.15+ and MSVC.
- Build `EncodingTranslator.exe` into `build/Release/`.
- Build and run `test_search` before changing search, filtering, or encoding behavior.

## Coding Style & Naming Conventions

- Use C++17, Qt5 idioms, and the existing header/source split.
- Keep classes in `PascalCase` and functions/variables in `camelCase`, matching current files.
- Prefer small, single-purpose functions; avoid adding abstractions until duplication or complexity justifies them.
- Preserve MSVC warning cleanliness under `/W4`, `/utf-8`, and `/permissive-`.
- Keep UI coordination in `MainWindow.*`; keep filesystem scanning and conversion logic in worker or utility classes.

## Testing Guidelines

- Add focused executable tests under `tests/` when changing non-trivial logic.
- Name test files by behavior, for example `test_search.cpp` or `test_encoding.cpp`.
- Keep tests deterministic and avoid modifying real project files; use temporary directories/files where needed.
- Run the relevant test target, then the executable, before opening a pull request.

## Commit & Pull Request Guidelines

- Existing history uses short imperative messages, for example `Add application icon resources` and `Improve main window UI styling`.
- Keep commits focused on one logical change.
- Pull requests should include a concise summary, test results, and screenshots or GIFs for visible UI changes.
- Link related issues when applicable and call out any encoding-conversion risks or manual verification steps.

## Security & Configuration Tips

- Encoding conversion overwrites selected files; verify selections carefully and work from a clean Git state or backup.
- Do not hard-code local Qt, MSVC, or user-specific paths in CMake files.
- Avoid expanding default scans to generated or dependency directories such as `.git`, `build`, `.vs`, and `node_modules`.
