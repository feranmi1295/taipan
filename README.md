# Taipan

A high-performance, general-purpose programming language with a clean syntax, entity-based OOP, and a native compiler that targets LLVM IR.

Built from scratch in C under **NextGen Inc.**

---

## Features

- **Entities** — struct-like types with methods, heap-allocated
- **Field mutation** — write back to entity fields inside methods
- **For-iter loops** — `for val in array` with runtime length via `libtaipan.a`
- **Method return values** — methods can return any type
- **Intra-method calls** — call sibling methods directly without `self.`
- **Float & integer arithmetic** — `i32`, `f32`, compound ops (`+=`, `*=`, etc.)
- **Unsafe blocks** — opt-in for low-level operations
- **vpk** — package manager (in progress)

---

## Toolchain

| Tool | Description |
|------|-------------|
| `venom` | Compiler — `.tp` → LLVM IR → native binary |
| `libtaipan.a` | Runtime library (arrays, memory, I/O) |
| `vpk` | Package manager |

---

## Building

**Requirements:** `gcc`, `clang` or `llc`, `make`

```bash
git clone https://github.com/feranmi1295/taipan.git
cd taipan
make
```

This produces the `venom` compiler and `runtime/libtaipan.a`.

---

## Usage

```bash
# Compile to LLVM IR only
./venom examples/hello.tp

# Compile and link to native binary
./venom examples/hello.tp --link

# Run
./examples/hello
```

---

## Language Tour

### Hello World

```taipan
use std.io

fn main() {
    println("Hello, Taipan!")
}
```

### Entities

```taipan
use std.io

entity Circle {
    radius: f32

    fn area() -> f32 {
        return radius * radius * 3.14159
    }

    fn describe() {
        println("radius:")
        println(radius)
        println("area:")
        println(area())
    }
}

fn main() {
    let c = Circle(radius=5.0)
    c.describe()
}
```

### Field Mutation

```taipan
entity Vec2 {
    x: f32
    y: f32

    fn scale(factor: f32) {
        x = x * factor
        y = y * factor
    }
}

fn main() {
    let v = Vec2(x=2.0, y=3.0)
    v.scale(4.0)
}
```

### Arrays and Iteration

```taipan
fn main() {
    let nums: [i32] = [10, 20, 30, 40, 50]
    let total: i32 = 0
    for n in nums {
        total = total + n
    }
    println(total)  // 150
}
```

---

## Project Structure

```
taipan/
├── lexer/          Tokenizer
├── parser/         Recursive descent parser
├── ast/            AST node definitions
├── compiler/
│   ├── analyzer.c  Semantic analysis
│   ├── codegen.c   LLVM IR code generation
│   └── main.c      CLI entry point
├── runtime/        libtaipan.a (arrays, memory)
├── vpk/            Package manager
├── examples/       Sample Taipan programs
└── Makefile
```

---

## Roadmap

- [ ] String type with length, indexing, concatenation
- [ ] Standard library (`std.math`, `std.fs`, `std.io`)
- [ ] Generics
- [ ] Pattern matching
- [ ] Module system via vpk
- [ ] Self-hosted compiler

---

## License

MIT