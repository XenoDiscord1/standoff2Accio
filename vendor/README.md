# vendor/

Third-party dependencies, pulled in as git submodules so the main repo stays small.

| Submodule | Repo | Used for |
|-----------|------|---------|
| `imgui`   | https://github.com/ocornut/imgui.git | menu + status overlay |

When you clone the main repo:

```
git clone --recursive https://github.com/<you>/standoff2-cheat.git
```

If you already cloned without `--recursive`:

```
git submodule update --init --recursive
```

GitHub Actions does `with: submodules: recursive` automatically.
