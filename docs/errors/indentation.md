# indentation

A line's indentation matches no open block, or holds a tab.

An error the compiler reports: the program doesn't build until it's fixed.

## Why

A block may be written by indentation instead of braces: a deeper line opens it and the block ends where the indentation comes back. So a line must come back to the indentation of a block that's open, or the scanner can't tell which block it belongs to. Indentation is spaces: a tab is a different width in every editor.

## How to fix

Line the line up with the block it belongs to (the same indentation as the lines before it in that block), or write the block with braces. Indent with spaces.

## Wrong

```asuka
fn sign(a: int) -> int
  if a < 0
      return -1
    return 1
```

## Right

```asuka
fn sign(a: int) -> int
  if a < 0
    return -1
  return 1
```
