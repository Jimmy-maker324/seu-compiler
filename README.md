# Seu 编译器

东南大学编译原理专题实践项目：自研词法分析器生成器（**SeuLex**）、语法分析器生成器（**SeuYacc**），以及基于二者构建的 C 语言子集前端 **compiler.exe**，完成词法分析、语法分析、AST 构建、语义检查、中间代码生成与语法树可视化。

源码中含较完整的中文注释（含算法设计、流程与数据结构说明），并支持 Doxygen 生成 API 文档。

---

## 目录

1. [环境要求](#环境要求)
2. [快速开始](#快速开始)
3. [整体架构](#整体架构)
4. [编译流程说明](#编译流程说明)
5. [运行编译器](#运行编译器)
6. [输出说明](#输出说明)
7. [语法树可视化（Graphviz）](#语法树可视化graphviz)
8. [生成 API 文档（Doxygen）](#生成-api-文档doxygen)
9. [支持的源语言](#支持的源语言)
10. [工具单独使用](#工具单独使用)
11. [项目结构](#项目结构)
12. [代码阅读顺序](#代码阅读顺序)
13. [常见问题](#常见问题)
14. [参考命令汇总](#参考命令汇总)

---

## 环境要求

| 组件 | 说明 |
|------|------|
| **g++** | 支持 C++11，需已加入系统 PATH |
| **Windows** | 推荐使用 `build.bat` 一键构建；也可在 MinGW / MSYS2 终端中手动执行其中命令 |
| **Graphviz**（可选） | 将 `ast.dot` 渲染为 PNG/SVG；[下载地址](https://graphviz.org/download/)，安装后将 `bin` 目录加入 PATH |
| **Doxygen**（可选） | 生成 HTML API 文档；[下载地址](https://www.doxygen.nl/download.html)，安装后加入 PATH |

> 控制台仅输出各阶段简要进度；词法分析、语法分析、语法树的详细内容默认写入 **`output/out.txt`**（可用 `-o` 指定路径）。

---

## 快速开始

在项目根目录打开命令行：

```bat
build.bat
build\compiler.exe examples\test.c
test.bat
```

`test.bat` 会批量运行 `examples/` 下回归用例并校验退出码（见 [回归测试](#回归测试)）。

构建成功后得到 `build\compiler.exe`。对 `examples\test.c` 编译后，`output/` 目录下通常会有：

| 产物 | 说明 |
|------|------|
| `output/out.txt` | 词法/语法/语义详细报告 |
| `output/output.ir` | 四元式中间代码 |
| `output/ast.dot` | 语法树 Graphviz 描述（可渲染为 `ast.png`） |

可选：

```bat
dot -Tpng output\ast.dot -o output\ast.png
gen_docs.bat
```

---

## 整体架构

```
                    ┌─────────────────────────────────────────┐
                    │           构建期（build.bat）            │
                    └─────────────────────────────────────────┘
   grammar/lex.l ──SeuLex──► generated/lex.yy.cpp/h   grammar/yacc.y ──SeuYacc──► generated/yyparse.cpp
        正则→NFA→DFA→代码生成                    First→LR(1)→LALR→分析表→yyparse

                    ┌─────────────────────────────────────────┐
                    │         运行期（compiler.exe）           │
                    └─────────────────────────────────────────┘
   源文件.c ──► 词法(yylex) ──► 语法(yyparse/AST) ──► 语义 ──► IR
                      │              │                  │        │
                      │              ├─ AST 文本打印     │        └─ output/output.ir
                      │              └─ output/ast.dot  │
                      └─ Token 列表（演示输出）          └─ 符号表/类型检查
```

| 模块 | 职责 | 核心算法 |
|------|------|----------|
| **SeuLex** | 读 `grammar/lex.l`，生成 `generated/lex.yy.cpp` | 正则解析、Thompson NFA、子集构造、DFA 最小化、表驱动最长匹配 |
| **SeuYacc** | 读 `grammar/yacc.y`，生成 `generated/yyparse.cpp` | First 不动点、LR(1) DFA、LALR 合并、Action/Goto 填表、冲突消解 |
| **前端** | `main_front` 驱动各阶段 | AST 遍历、作用域符号表、语法制导四元式生成 |

---

## 编译流程说明

`build.bat` 按顺序完成：

```
┌─────────┐  grammar/lex.l  ┌─────────────────────┐
│ SeuLex  │ ──────────────►  │ generated/lex.yy.*  │
└─────────┘                  └──────────┬──────────┘
                                        │
┌─────────┐ grammar/yacc.y              │     ┌─────────────────────┐
│ SeuYacc │ ────────────────────────────┼──►  │ generated/yyparse.cpp│
└─────────┘  (--lalr)                   │     └──────────┬──────────┘
                                        │                │
                                        ▼                ▼
                                   ┌─────────────────────┐
                                   │  build\compiler.exe │
                                   │  (main_front 等)    │
                                   └─────────────────────┘
```

| 步骤 | 命令 / 动作 | 产物 |
|------|-------------|------|
| 1 | 编译并运行 `build\seulex.exe grammar\lex.l` | `generated/lex.yy.cpp`、`generated/lex.yy.h` |
| 2 | 编译并运行 `build\seuyacc.exe --lalr grammar\yacc.y generated/yyparse.cpp` | `generated/yyparse.cpp`（LALR 分析表 + 归约代码） |
| 3 | 编译各前端模块并链接 | `build\compiler.exe` |

`build.bat` 编译选项含 **`-Wall`**，便于开发期暴露未使用变量等问题。

修改 **`grammar/lex.l`** 或 **`grammar/yacc.y`** 后，须重新运行 `build.bat`（会重新生成分析器并链接）。

### 声明文法（`var_decl` / `func_def`）

变量与函数定义共用 **`type_spec` + `declarator`** 链（见 `grammar/yacc.y`），不再为每种类型/修饰符单独写产生式：

| 非终结符 | 形式 | 说明 |
|----------|------|------|
| `var_decl` | `type_spec init_declarator ';'` | `init_declarator` = `declarator` 或带 `=` 初值 |
| `func_def` | `type_spec declarator compound_stmt` | 形参表在 `declarator` 的 `(...)` 中 |
| `param_decl` | `type_spec declarator` | 与变量声明共用 `DeclaratorInfo::buildType` |

语义动作集中在 `finishVarDecl` / `finishFuncDef`；`for_init` 中的局部声明复用 `init_declarator`。

> **注意**：`grammar/lex.l` 规则区仅支持**整行** `/* ... */` 注释；勿在 pattern 或 `{ action }` 行尾加注释，否则 SeuLex 解析会失败。

---

## 运行编译器

### 用法

```text
build\compiler.exe <源文件> [-o 报告.txt] [--no-opt]
```

| 选项 | 说明 |
|------|------|
| `-o path` | 词法/语法/语义详情报告路径（默认 `output/out.txt`） |
| `--no-opt` | 跳过 IR 优化；`output/output.ir` 与 `output/output_raw.ir` 内容相同 |

示例：

```bat
build\compiler.exe examples\test.c
build\compiler.exe examples\test.c -o output\out.txt
build\compiler.exe examples\block_scope.c --no-opt
```

### 处理阶段

| 序号 | 阶段 | 控制台 | 详情输出 |
|------|------|--------|----------|
| 1 | 词法分析 | Token 数量摘要 | `output/out.txt` 中逐 Token 列表 |
| 2 | 语法分析 | 成功/失败 | `output/out.txt` |
| 3 | 语法树（文本） | 已写入提示 | `output/out.txt` |
| 4 | 语法树（Graphviz） | 已导出提示 | `output/out.txt` + `output/ast.dot` |
| 5 | 语义分析 | 通过 / 失败（含错误数） | `output/out.txt` 中符号表与类型检查 |
| 6 | 中间代码 | 优化统计（默认）或 `--no-opt` 提示 | `output/output.ir`、`output/output_raw.ir` |

语法分析失败时以非零退出码结束（**1** = 语法错误，`yyparse` 失败或 `parseErrorCount > 0`），不生成 AST/IR。  
语义分析失败（含符号重定义、类型错误）时退出码为 **2**，**不生成 IR**。

---

## 输出说明

### 控制台

- 词法 / 语法 / 语法树阶段的一行摘要
- 语义分析、中间代码完成提示

### 文件

| 文件 | 说明 |
|------|------|
| `output/out.txt` | 词法 Token 列表、语法分析结果、AST 文本树（默认，`-o` 可改路径） |
| `output/output.ir` | 四元式中间代码（经优化） |
| `output/output_raw.ir` | 优化前原始四元式 |
| `output/ast.dot` | Graphviz DOT 格式 AST |
| `output/ast.png` | AST 可视化（安装 Graphviz 后由编译器自动尝试生成） |

### 中间代码优化

生成 IR 后默认运行 **IROptimizer**（`ir_opt.cpp`）；加 **`--no-opt`** 可跳过优化，直接输出与 `output_raw.ir` 相同的 `output.ir`。

| Pass | 说明 |
|------|------|
| 常量传播 / 折叠 | 基本块内替换已知常量，编译期求值纯算术/逻辑运算 |
| 公共子表达式消除（CSE） | 基本块内复用相同 `(op, arg1, arg2)` 的纯计算结果 |
| 死代码消除 | 删除结果临时变量未被使用的纯计算四元式（含 `copy`） |
| 循环不变式外提 | 将循环内仅依赖循环外变量的纯计算移至循环头之前 |

控制台会输出各 pass 的优化计数；可对比 `output/output_raw.ir` 与 `output/output.ir`。

新增终结符时只需编辑 **`include/tokens.def`**，`common_defs.h`、`token_names.cpp` 与 `SeuYacc/token_registry.inl` 会自动同步。

### 中间代码常见操作符

| 类别 | 操作符 |
|------|--------|
| 算术 | `+`、`-`、`*`、`/`、`neg` |
| 关系与逻辑 | `==`、`!=`、`<`、`<=`、`>`、`>=`、`&&`、`||`、`not`（result 为行号/`L*` 时表示 relop 成立则跳转） |
| 赋值 | `=`、`*=`（指针解引用赋值） |
| 数组/指针 | `[]`（读）、`[]=`（写）、`&`（取址）、`*`（解引用） |
| 结构体/联合体 | `.`、`.=`（成员读/写）、`->`、`->=`（指针成员读/写） |
| 控制流 | `goto`（无条件/为假出口）、比较条件跳转（`relop, a, b, 行号`：成立则跳转，否则顺序执行下一行）、`label` |
| 其它 | `copy`（一元拷贝，可被 DCE 消除） |
| 调用 | `param`（传实参/接形参）、`call`（含函数指针间接调用） |
| 字符串 | `str`（定义只读字符串常量，结果为 `strN`，表示 `char*`） |

**局部变量命名**：`Symbol::irName` 在 `addSymbol` 时分配；语义分析与 IR 生成共用 `enterScope` / `leaveScope` / `getSymbol`（全局/函数名保留源名，其它局部为 `name_N`）。

**字符串示例**（`examples/string_test.c` 编译成功后，`output/output.ir` 片段）：

```text
(str, "hello", , str0)
(=, str0, , msg_1)
(param, msg_1, , )
(call, echo, 1, t0)
```

### AST 语义值栈约定

`YYSTYPE` 为 union（`sval` / `ptr` 等字段共享存储）。为避免归约后仍读取已失效的 `sval` 导致 AST 损坏，当前实现采用：

| 位置 | 约定 |
|------|------|
| **移进**（`SeuYacc/code_gen.cpp`） | `T_IDENTIFIER` / `T_STRING_LITERAL` 移进时立即构造 `IdentifierNode` / `StringNode` 写入 `ptr` |
| **声明**（`grammar/yacc.y`） | 通过 `takeIdentName()` 从临时 `IdentifierNode` 取名字，不再 `free(sval)` |
| **表达式** | `primary_expr` 对标识符/字符串仅 `adopt($1.ptr)` 透传 |

修改 `grammar/yacc.y` 或 `SeuYacc/code_gen.cpp` 后须完整运行 `build.bat` 重新生成 `generated/yyparse.cpp`。

---

## 语法树可视化（Graphviz）

语法分析成功后会自动生成 **`output/ast.dot`**（由 `ast_dot.cpp` 导出）。

### 本地渲染

```bat
dot -Tpng output\ast.dot -o output\ast.png
dot -Tsvg output\ast.dot -o output\ast.svg
```

### 在线预览

将 `output/ast.dot` 复制到 [Graphviz Online](https://dreampuf.github.io/GraphvizOnline/)。

### 节点含义

- 方框：AST 结点类型（`Program`、`FuncDef`、`BinaryOp` 等）  
- 有向边：父子关系  
- 标签可含标识符名、常量值、行号等  

---

## 生成 API 文档（Doxygen）

项目已配置 `config/Doxyfile` 与 `gen_docs.bat`（在项目根目录执行），可扫描手写源码（自动排除 `generated/`）生成 HTML 文档。

### 安装

1. 安装 [Doxygen](https://www.doxygen.nl/download.html) 并加入 PATH  
2. （可选）安装 Graphviz，用于类图  

### 生成与查看

```bat
gen_docs.bat
```

成功后打开 **`docs/html/index.html`**。源码中的 `@file`、`@class`、`@brief` 等注释会出现在文档中。

---

## 支持的源语言

C 语言 **MiniC 子集**，文法见 `grammar/yacc.y`，词法见 `grammar/lex.l`；终结符定义与 `include/common_defs.h` 一致。

### 主要特性

| 类别 | 支持内容 |
|------|----------|
| 类型 | `int`、`char`、`float`、`double`、`void` |
| 聚合类型 | `struct`、`union` 定义与成员访问（`.` / `->`） |
| 指针 | 声明、取址 `&`、解引用 `*`、指针赋值、`*=` |
| 数组 | 一维数组、指针/数组下标 |
| 函数 | 多参数、递归、**函数指针**与间接调用 |
| 语句 | 复合语句、`if`/`else`、`while`、`for`、`switch`/`case`/`default`、`return`、`break`、`continue` |
| 声明 | 块内/全局变量声明，支持 `type id = expr` 带初值形式 |
| 表达式 | 赋值、逻辑与/或、相等/关系、加减乘除、一元 `!`/`-`/`&`/`*`、调用、下标、成员访问 |
| 字面量 | 整型、浮点、**字符串**（类型 `char*`，IR 为 `str` 四元式） |

### 示例

| 文件 | 说明 |
|------|------|
| `examples/test.c` | 结构体/联合体/指针/函数指针/`switch`/`for` 等综合示例 |
| `examples/block_scope.c` | 块作用域与变量遮蔽（语义通过，exit 0） |
| `examples/redef.c` | 符号重定义（语义失败，exit 2） |
| `examples/bad_break.c` | 循环外 `break`（语义失败，exit 2） |
| `examples/bad_continue.c` | 循环外 `continue`（语义失败，exit 2） |
| `examples/dup_case.c` | `switch` 重复 `case`（语义失败，exit 2） |
| `examples/bad_void_return.c` | `void` 函数返回值（语义失败，exit 2） |
| `examples/bad_empty_return.c` | 非 `void` 函数空 `return`（语义失败，exit 2） |
| `examples/bad_return_type.c` | 返回值类型不匹配（语义失败，exit 2） |
| `examples/string_test.c` | 字符串字面量与 `char*` 传参（exit 0） |
| `examples/bad_string_int.c` | 整型变量赋字符串（语义失败，exit 2） |
| `examples/syntax_err.c` | 语法错误（exit 1） |

### 回归测试

在项目根目录执行：

```bat
test.bat
```

若 `build\compiler.exe` 不存在，脚本会先调用 `build.bat`。已有编译器时跳过构建：

```bat
test.bat --no-build
```

| 用例 | 期望退出码 | 说明 |
|------|------------|------|
| `test.c` | 0 | 综合示例，应生成 `output/output.ir` |
| `block_scope.c` | 0 | 块作用域与 IR 变量遮蔽 |
| `syntax_err.c` | 1 | 语法错误 |
| `redef.c` | 2 | 符号重定义 |
| `bad_break.c` | 2 | 循环外 `break` |
| `bad_continue.c` | 2 | 循环外 `continue` |
| `dup_case.c` | 2 | 重复 `case` |
| `bad_void_return.c` | 2 | `void` 函数带返回值 |
| `bad_empty_return.c` | 2 | 非 `void` 函数空 `return` |
| `bad_return_type.c` | 2 | 返回值类型不匹配 |
| `string_test.c` | 0 | 字符串字面量与 `char*` |
| `bad_string_int.c` | 2 | 整型赋字符串 |

全部通过时 `test.bat` 退出码为 **0**；任一失败为 **1**。

### 限制

- 词法器仅保留 MiniC 所需关键字与运算符（见 `common_defs.h`）；未支持的关键字（如 `goto`）会当作 `IDENTIFIER`  
- 不支持预处理、typedef、多维数组、变参、枚举等完整 C 特性  
- 语义规则见 `typecheck.cpp`（类型检查与隐式转换策略以当前实现为准）  

### 语义检查（节选）

| 规则 | 说明 |
|------|------|
| 块作用域 | 复合语句 `{ ... }` 进入新作用域；内层变量可遮蔽外层同名符号 |
| 函数体 | 形参与函数体最外层块共享同一作用域（与 C 一致） |
| 重定义 | 同一作用域内重复声明同一标识符报错 |
| `break` / `continue` | 分别须在循环/`switch` 内、循环内使用 |
| `switch` | `case` 标签值不可重复 |
| `return` | `void` 函数不得带返回值；非 `void` 函数 `return` 须带兼容类型的表达式 |
| 赋值 | 数值类型间允许隐式转换；指针仅接受同类型指针或函数指针兼容；**字符串字面量**（`char*`）可赋给 `char*` 及带初值的 `char*` 声明 |

---

## 工具单独使用

需先通过 `build.bat` 前段生成 `build\seulex.exe` / `build\seuyacc.exe`。

### SeuLex

```bat
build\seulex.exe grammar\lex.l
```

| 项目 | 说明 |
|------|------|
| 输入 | Flex 风格 `grammar/lex.l`（`%%` 分隔） |
| 输出 | `generated/lex.yy.cpp`、`generated/lex.yy.h` |
| 流水线 | 正则 AST → Thompson NFA → 子集构造 DFA → 最小化 → 代码生成 |

### SeuYacc

```bat
seuyacc.exe [选项] <输入.y> <输出.cpp>
```

| 选项 | 说明 |
|------|------|
| `-h`, `--help` | 帮助 |
| `-v`, `--verbose` | 调试输出 |
| `--lalr` | LALR(1) 合并（`build.bat` 默认） |
| `--print-first` | 打印 First 集 |
| `--print-dfa` | 打印 LR 状态机 |
| `--print-table` | 打印分析表 |

示例：

```bat
build\seuyacc.exe --lalr grammar\yacc.y generated/yyparse.cpp
build\seuyacc.exe --lalr --print-dfa grammar\yacc.y generated/yyparse.cpp
```

---

## 项目结构

```
seu-compiler/
├── README.md              # 本说明
├── build.bat              # 一键构建
├── test.bat               # 回归测试（校验 examples 退出码）
├── gen_docs.bat           # Doxygen 生成文档
│
├── grammar/               # 文法定义（生成器输入）
│   ├── lex.l              # 词法规则（SeuLex 输入）
│   └── yacc.y             # 语法与 AST 动作（SeuYacc 输入）
│
├── examples/              # 示例源程序
│   ├── test.c             # 综合特性示例
│   ├── block_scope.c      # 块作用域示例
│   ├── redef.c            # 语义错误：符号重定义
│   ├── bad_break.c        # 语义错误：循环外 break
│   ├── bad_continue.c     # 语义错误：循环外 continue
│   ├── dup_case.c         # 语义错误：重复 case
│   ├── bad_void_return.c  # 语义错误：void 函数返回值
│   ├── bad_empty_return.c # 语义错误：非 void 空 return
│   ├── bad_return_type.c  # 语义错误：返回值类型不匹配
│   ├── string_test.c      # 字符串字面量与 char*
│   ├── bad_string_int.c   # 语义错误：整型赋字符串
│   └── syntax_err.c       # 语法错误示例
│
├── config/                # 工具配置
│   └── Doxyfile
│
├── output/                # 编译器运行产物（.gitignore 忽略内容，保留 .gitkeep）
│
├── include/               # 跨模块公共头文件
│   ├── common_defs.h      # TokenType、YYSTYPE、宏
│   ├── tokens.def         # 终结符主表（enum / 显示名 / SeuYacc 共用）
│   ├── token_names.h      # tokenDisplayName（词法 dump）
│   ├── seulex.h
│   └── ast.h              # AST 节点定义
│
├── SeuLex/                # 词法生成器
├── SeuYacc/               # 语法生成器
├── Frontend/              # 编译器前端
│   ├── main_front.cpp     # 驱动词法/语法/语义/IR
│   ├── ast_format.*       # AST 共享格式化（文本树 / DOT 共用）
│   ├── ast_printer.*      # AST 文本树
│   ├── ast_dot.*          # AST Graphviz 导出
│   ├── token_names.cpp    # 终结符显示名
│   └── typecheck.* / irgen.* / ir_opt.* / symbol.* / type.*
├── generated/             # 【自动生成，已 .gitignore，build.bat 重建】
├── build/                 # 可执行文件与 .o（seulex、seuyacc、compiler）
└── docs/html/             # Doxygen 输出（运行 gen_docs.bat 后）
```

---

## 代码阅读顺序

建议**按数据流**阅读，而非按文件名排序。各核心 `.cpp` 文件头有「算法设计 / 流程 / 数据结构」说明。

### 0. 全局（约 30 分钟）

1. `README.md`（本文）  
2. `build.bat`  
3. `examples/test.c`  

### 1. 公共约定（约 20 分钟）

4. `common_defs.h`  
5. `seulex.h`  
6. `yacc_common.h`  

### 2. SeuLex 流水线（约 2–3 小时）

7. `lex_common.h` → 8. `grammar/lex.l` → 9. `lex_main.cpp`  
10. `LexFileParser.cpp` → 11. `RegExpParser.cpp` → 12. `NFAConstructor.cpp`  
13. `DFAConverter.cpp` → 14. `DFAMinimizer.cpp` → 15. `CodeGenerator.cpp`  

可选对照：`generated/lex.yy.cpp` 中的 `nextState`、`yylex`。

### 3. SeuYacc 流水线（约 3–4 小时）

16. `yacc_main.cpp` → 17. `grammar.cpp` → 18. `first_set.cpp`  
19. `lr1_dfa.cpp` → 20. `lalr.cpp` → 21. `parsing_table.cpp` → 22. `code_gen.cpp`  
23. `common.cpp`  

可选对照：`generated/yyparse.cpp` 中的 `action_table`、`yyparse` 主循环。

### 4. 文法与 AST（约 2 小时）

24. `grammar/yacc.y`（含 `takeIdentName`、`adopt`/`grab` 约定）→ 25. `ast.h` → 26. `type.h`、`type.cpp` → 27. `seulex.h`  
   对照 `SeuYacc/code_gen.cpp` 中移进时对 `T_IDENTIFIER` / `T_STRING_LITERAL` 的 AST 构造逻辑。

### 5. 编译器前端（约 2 小时）

28. `main_front.cpp` → 29. `token_names.h/cpp` → 30. `symbol.h/cpp` → 31. `typecheck.h/cpp`  
32. `irgen.h/cpp` → 33. `ir_opt.h/cpp` → 34. `ast_format.h/cpp` → 35. `ast_printer.cpp` → 36. `ast_dot.cpp`  

边读边运行：`build\compiler.exe examples\test.c`，对照 `output/output.ir`、`output/ast.dot` / `output/ast.png`。

### 时间紧时的最短路径（12 个文件）

`README` → `build.bat` → `common_defs.h` → `yacc_common.h` → `LexFileParser.cpp`（`processRules`）→ `DFAConverter.cpp` → `CodeGenerator.cpp` → `yacc_main.cpp` → `first_set.cpp` + `lr1_dfa.cpp`（文件头）→ `parsing_table.cpp` + `code_gen.cpp`（文件头）→ `yacc.y` + `ast.h` → `main_front.cpp` + `typecheck.cpp` + `irgen.cpp`。

---

## 常见问题

### `build.bat` 找不到 g++

安装 MinGW-w64 或 MSYS2，将 `g++.exe` 所在目录加入 `PATH`。

### SeuLex 报错 `invalid action code`

检查 `grammar/lex.l` 规则区是否在 pattern/action **行尾**写了注释；仅允许**整行** `/* ... */` 注释。

### 语法分析失败

- 确认源程序在 `grammar/yacc.y` 子集内  
- 查看 `yylineno` 报错行  
- 检查括号、分号与声明顺序  

### 没有 `output/ast.dot` 或 `output/output.ir`

仅语法分析成功且存在 `astRoot` 时生成。

### `dot` 不是内部或外部命令

未安装 Graphviz；可用在线工具打开 `output/ast.dot`。

### 修改 `grammar/yacc.y` 后链接错误

重新运行完整 `build.bat`，确保 `generated/yyparse.cpp` 与 `build/yyparse.o` 已更新。

### `doxygen` 不是内部或外部命令

安装 Doxygen 并配置 PATH，或仅用 IDE 阅读源码注释。

### 中文乱码

使用 Windows Terminal，或 `chcp 65001`。`main_front.o` 编译时已指定 UTF-8。

---

## 参考命令汇总

```bat
:: 构建完整编译器
build.bat

:: 回归测试（examples 退出码 + 成功用例 IR 产物）
test.bat
test.bat --no-build

:: 编译示例并查看各阶段输出
build\compiler.exe examples\test.c

:: 语法树渲染（需 Graphviz）
dot -Tpng output\ast.dot -o output\ast.png

:: 生成 API 文档（需 Doxygen）
gen_docs.bat

:: 仅重新生成词法/语法分析器
build\seulex.exe grammar\lex.l
build\seuyacc.exe --lalr grammar\yacc.y generated\yyparse.cpp
```

---

*东南大学编译原理专题实践 — Seu 编译器*
