# Lispy — A Lisp Interpreter in C

A fully functional Lisp interpreter written in C, built by following [Build Your Own Lisp](https://www.buildyourownlisp.com/) — a free online book that teaches you how programming languages work by building one from scratch.

---

## What Is This?

This project is a learning exercise in language implementation. By the end, you get a working Lisp dialect that supports:

- Arithmetic and comparison operators
- Variables and environments (local and global scoping)
- First-class functions and lambdas
- Q-Expressions (quoted/unevaluated lists)
- Strings and error handling
- File loading (run `.lspy` script files)
- An interactive REPL (Read-Eval-Print Loop)

---

## How It Works

### The Big Picture

```
Input String
     │
     ▼
  Parser (mpc)          ← Tokenizes and builds an AST
     │
     ▼
  lval_read()           ← Converts AST nodes into lval (Lisp values)
     │
     ▼
  lval_eval()           ← Evaluates the lval tree
     │
     ▼
  lval_println()        ← Prints the result
```

### Core Data Structure: `lval`

Everything in the interpreter is represented as an `lval` (Lisp value). This is a tagged union struct that can hold:

| Type         | Description                              |
|--------------|------------------------------------------|
| `LVAL_NUM`   | An integer number                        |
| `LVAL_SYM`   | A symbol/identifier (e.g., `+`, `myVar`) |
| `LVAL_STR`   | A string literal                         |
| `LVAL_ERR`   | An error message                         |
| `LVAL_FUN`   | A function (builtin or user-defined lambda) |
| `LVAL_SEXPR` | S-Expression: evaluated list `(+ 1 2)`   |
| `LVAL_QEXPR` | Q-Expression: unevaluated list `{1 2 3}` |

### Environment: `lenv`

Variables and functions live in a linked chain of `lenv` (environment) structs. Each environment has:

- A parallel array of symbol names (`syms`) and their values (`vals`)
- A pointer to a parent environment (`par`) for lexical scoping

Lookups walk up the chain: local scope first, then parent scopes, up to the global environment.

### Evaluation

`lval_eval()` is the heart of the interpreter:

1. If the value is a **symbol**, look it up in the environment.
2. If it's an **S-Expression**, call `lval_eval_sexpr()`:
   - Recursively evaluate all child elements.
   - The first element must be a function (`LVAL_FUN`).
   - Call that function with the remaining elements as arguments.
3. Everything else (numbers, strings, Q-Expressions) evaluates to itself.

### Builtins

The interpreter comes with these built-in functions registered at startup:

| Category    | Functions                                      |
|-------------|------------------------------------------------|
| Arithmetic  | `+` `-` `*` `/` `%` `^` `min` `max`           |
| Comparison  | `>` `>=` `<` `<=` `==` `!=`                   |
| Logic/Control | `if`                                         |
| Lists       | `list` `head` `tail` `join` `eval`             |
| Variables   | `def` (global) `=` (local)                    |
| Functions   | `\` (lambda)                                   |
| I/O         | `print` `error` `load`                         |

### User-Defined Functions (Lambdas)

Functions are created with `\` and support **partial application** — call a function with fewer arguments than it expects and you get back a new function waiting for the rest.

---

## Building

### Prerequisites

- GCC or Clang
- `mpc` library (included as `mpc.c` / `mpc.h` in the repo)
- `libeditline` or `libreadline` (for REPL history on Linux/macOS)

### Compile

```bash
# Linux / macOS
gcc -std=c99 -Wall -o lispy lispy.c mpc.c -ledit -lm

# If using readline instead of editline
gcc -std=c99 -Wall -o lispy lispy.c mpc.c -lreadline -lm

# Windows (no readline needed — fake implementations are included)
gcc -std=c99 -Wall -o lispy lispy.c mpc.c -lm
```

---

## Usage

### Interactive REPL

```bash
./lispy
```

You'll see:

```
LISPY V-0.0.1
Crtl+C to exit
lispy>
```

Type any Lisp expression and press Enter:

```lisp
lispy> (+ 1 2)
3

lispy> (def {x} 10)
()

lispy> (* x 5)
50
```

### Run a Script File

```bash
./lispy myscript.lspy
```

You can pass multiple files; they are loaded in order:

```bash
./lispy stdlib.lspy main.lspy
```

---

## Language Guide

### Arithmetic

```lisp
(+ 1 2)          ; 3
(- 10 3)         ; 7
(* 4 5)          ; 20
(/ 10 2)         ; 5
(% 10 3)         ; 1  (modulo)
(^ 2 8)          ; 256 (power)
(min 3 7)        ; 3
(max 3 7)        ; 7
```

### Variables

```lisp
; Global definition
(def {x} 42)
(def {name} "Lispy")

; Local definition (inside a function)
(= {y} 10)
```

### Comparison and Conditionals

```lisp
(> 5 3)          ; 1 (true)
(== 5 5)         ; 1
(!= 5 3)         ; 1

; if condition {true-branch} {false-branch}
(if (> x 0)
  {print "positive"}
  {print "not positive"})
```

### Lists (Q-Expressions)

```lisp
; Create a list (not evaluated)
(list 1 2 3)           ; {1 2 3}

; Get the first element
(head {1 2 3})         ; {1}

; Get everything except the first
(tail {1 2 3})         ; {2 3}

; Join two lists
(join {1 2} {3 4})     ; {1 2 3 4}

; Evaluate a list as code
(eval {+ 1 2})         ; 3
```

### Functions

```lisp
; Define a named function using def + lambda
(def {square} (\ {x} {* x x}))

(square 5)       ; 25

; Multi-argument function
(def {add} (\ {a b} {+ a b}))

(add 3 4)        ; 7

; Partial application
(def {add5} (add 5))
(add5 10)        ; 15
```

### Strings and I/O

```lisp
(print "Hello, World!")

; Load an external file
(load "mylib.lspy")

; Trigger a custom error
(error "something went wrong")
```

### Comments

```lisp
; This is a comment — everything after ; on a line is ignored
```

---

## Project Structure

```
.
├── lispy.c       # Main interpreter source
├── mpc.c         # Micro Parser Combinators library (parser)
├── mpc.h         # mpc header
└── README.md
```

---

## Key Concepts Learned

Building this interpreter teaches:

- **Recursive descent parsing** via the `mpc` combinator library
- **Abstract Syntax Trees (AST)** — how source code becomes a tree structure
- **Tree-walking evaluation** — how an interpreter traverses and evaluates an AST
- **Environments and scoping** — how variables and closures work under the hood
- **Memory management in C** — manual `malloc`/`free`, avoiding leaks, defensive copying
- **Variadic functions** (`va_list`) for formatted error messages
- **Functional programming concepts** — first-class functions, partial application, closures

---

## Resources

- [Build Your Own Lisp](https://www.buildyourownlisp.com/) — the book this is based on (free online)
- [mpc — Micro Parser Combinators](https://github.com/orangeduck/mpc) — the parsing library used
- [Structure and Interpretation of Computer Programs](https://mitpress.mit.edu/sites/default/files/sicp/index.html) — deeper reading on interpreters

---

## License

This project follows the learning material from [buildyourownlisp.com](https://www.buildyourownlisp.com/), which is released under a Creative Commons license. The code is for educational purposes.
