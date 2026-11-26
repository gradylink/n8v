# n8v

A very powerful (yet lightweight!) cross-platform UI library that uses native
widgets when available and if needed can fall back to built-in styles.

## Backends

### Full Support

- SDL2 Fallback (custom widgets using SDL2, with 4 styles)
  - Plain
  - Material 3
  - Cupertino
  - Fluent UI
- GTK4 (with optional libadwaita)
- Qt6

### Partial Support

- HTML (I'd like to add SSR/SSG)
- Terminal ([FTXUI](https://github.com/ArthurSonzogni/FTXUI), opt in with
  `N8V_BACKEND=tui`)
- WinUI 3

### Planned Support

- Cocoa

## Bindings

- Go
- [JavaScript](bindings/js/README.md)
