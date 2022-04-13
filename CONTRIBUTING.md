# Contributing guide

- [Contributing guide](#contributing-guide)
  - [Submiting issues](#submiting-issues)
    - [Reporting a bug](#reporting-a-bug)
    - [Suggesting new check or existing check improvement](#suggesting-new-check-or-existing-check-improvement)
  - [Writing your first check](#writing-your-first-check)
    - [Shared and internal code](#shared-and-internal-code)
    - [General workflow](#general-workflow)
    - [Actual coding](#actual-coding)
      - [Create files](#create-files)
      - [Declare your class](#declare-your-class)
      - [Define the class](#define-the-class)
      - [Building](#building)
    - [Testing](#testing)
      - [Write the test](#write-the-test)
      - [Add to CMakeLists.txt](#add-to-cmakeliststxt)
      - [Running the test](#running-the-test)
  - [Opening your pull request](#opening-your-pull-request)
  - [Local patterns](#local-patterns)
    - [CompoundStmt stack](#compoundstmt-stack)

## Submiting issues

We use [GitHub issues](https://docs.github.com/en/issues/tracking-your-work-with-issues/about-issues) for feedback, bugreports and questions in ICA repository.

### Reporting a bug

To submit a proper bug report:

* Make sure your problem wasn't reported yet
* Check if the problem reproduces with the latest version
* Use a clear and descriptive title for your issue
* Include a [minimal reproducible example](https://stackoverflow.com/help/minimal-reproducible-example) of a code where the problem occurs
* Make sure your description includes compiler arguments, especially the plugin arguments it's run with
* If you've customly built the plugin, provide information about it
* Describe expected and actual behavior
* If a crash takes place, provide a stack trace that Clang shows

### Suggesting new check or existing check improvement

If you as a C++ developer find a problematic pattern that you believe can be caught by static analysis, or see false-negative/false-positive ICA reports, or you can suggest some other way of improving ICA checks, you may create a [GitHub issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/about-issues).

* Describe the pattern you want to catch
* Explain why the pattern is problematic
* Add a code example, comment on where do you expect a warning to occur
* Thoughts on what the analyzer should consider are appreciated

## Writing your first check

### Shared and internal code

ICA is both a tool used internally at _Itiviti_ and an open-source project. By our experience, some checks which are essential in _Itiviti_ codebase are inapplicable outside of it due to specific code layouts or design principles. Those checks and corresponding tests are not exposed on GitHub. Same goes the other way — we might have a prepared check that can be used externally, but it is not yet tested and adapted in our labs.

From the contributor's POV, this is represented as __shared__ and __internal__ subdirectories in each directory with code (__src__, __include__ or __test__):

* All code changes in __shared__ directory are eventually synced. To put it simple, for each pull request merged in _Itiviti_ internal Git repo, we create a clone PR on GitHub, and vice versa. This way, important enhancements and bugfixes reach both repositories without additional work.
* __internal__ directories are skipped while syncing, so any changes to those files are not cloned.

If you want to create new checks, please commit them into __internal__ directories. They may be moved to __shared__ afterwards. Of course, any improvements/bugfixes/test updates to existing __shared__ code is welcome!

### General workflow

We base our static analyzis on [Clang AST](https://clang.llvm.org/docs/IntroductionToTheClangAST.html), this is the key concept you are going to work with.

Most probably your workflow will look like that:

1. Write a test with the pattern you are trying to catch.
2. If it's a new file, add it to `test/(internal|shared)/CMakeLists.txt`
3. [Dump it's AST](https://stackoverflow.com/questions/18560019/how-to-view-clang-ast) to see how you can find the pattern with the analyzer. [do_dump.sh](./test/do_dump.sh) script may help you.
4. Write your check or change the existing one (more [here](#actual-coding)).
5. [Test it](CONTRIBUTING.md#Testing).
6. Think of some other cases of a similar pattern and different corner cases, go back to point 1.


### Actual coding

All ICA checks are visitors of AST nodes. Clang uses [CRTP](https://en.cppreference.com/w/cpp/language/crtp) for their visitors (see [clang::RecurisveASTVisitor](https://clang.llvm.org/doxygen/classclang_1_1RecursiveASTVisitor.html)). Thus, your check should be derived from `ica::Visitor` ([source](./include/shared/common/Visitor.h)).

Let's write a simple visitor. It will check variable names.

#### Create files

We'll call our visitor `LocalVarStyleVisitor`. So, we create `include/internal/checks/LocalVarStyleVisitor.h` and `src/internal/checks/LocalVarStyleVisitor.cpp`.

#### Declare your class

In `LocalVarStyleVisitor.h`:

```cpp
#pragma once

#include "shared/common/Visitor.h"

namespace ica {

class LocalVarStyleVisitor : public Visitor<LocalVarStyleVisitor>
{
    // this name is used in plugin arguments
    static constexpr inline auto * local_var_style = "local-var-style";

public:
    explicit LocalVarStyleVisitor(clang::CompilerInstance & ci, const Config & config);

    // you can have several checks per visitor, 'check_names' is used to register their names
    static constexpr inline auto check_names = make_check_names(local_var_style);

    bool VisitVarDecl(clang::VarDecl * var_decl);

    void printDiagnostic(clang::ASTContext & ctx)
    { /* can be used as point of reporting */ }
    void clear()
    { /* can be used as point of cleaning temporary data */ }

private:
    DiagnosticID m_warn_id = 0;
};

} // namespace ica
```

We run one big visitor for all internal checks and one for shared checks. So, you need to write your visitor into the list.

`ExclusiveUnitedVisitors.h`

```cpp
#pragma once

#include "internal/checks/BadRandVisitor.h"
#include "internal/checks/ForRangeConstVisitor.h"
#include "internal/checks/ImproperMoveVisitor.h"
#include "internal/checks/InitMembersVisitor.h"
#include "internal/checks/LocalVarStyleVisitor.h" // your include
#include "internal/checks/MiscellaneousVisitor.h"
#include "internal/checks/ReturnValueVisitor.h"

#include "shared/common/UnitedVisitor.h"

namespace ica {

// Both these names must exist;

using ExclusiveTranslationUnitVisitor = UnitedVisitor<
    MiscellaneousVisitor
>;

using ExclusiveTopLevelDeclVisitor = UnitedVisitor<
    BadRandVisitor,
    ForRangeConstVisitor,
    ImproperMoveVisitor,
    InitMembersVisitor,
    LocalVarStyleVisitor, // your visitor
    ReturnValueVisitor
>;

} // namespace ica
```

#### Define the class

In the visitor constructor we need to check if it's enabled (this is defined in `ica::Visitor` constructor from the `config`).

You can print additional values in your report and check them in your report format string. To provide the values call `AddValue` on `report`.

`Visit*` methods return `true` if the visitor should continue traversal. We just return `true` everywhere.

`LocalVarStyleVisitor.cpp`
```cpp
#include "internal/checks/LocalVarStyleVisitor.h"

namespace ica {

LocalVarStyleVisitor::LocalVarStyleVisitor(clang::CompilerInstance & ci, const Config & config)
    : Visitor(ci, config)
{
    if (!isEnabled()) return;

    m_warn_id = getCustomDiagID(local_var_style, "'%0' is bad code style for a local variable name");
}

// please, hide your local functions in anonymous namespaces
namespace {

bool isVarNameStylish(llvm::StringRef name)
{ return false; }

} // namespace anonymous

bool LocalVarStyleVisitor::VisitVarDecl(clang::VarDecl * var_decl)
{
    // almost sure you want this check in your Visit* methods
    if (!shouldProcessDecl(var_decl, getSM())) {
        return true;
    }

    if (var_decl->isLocalVarDecl()) {
        if (!isVarNameStylish(var_decl->getName())) {
            clang::SourceLocation loc = var_decl->getLocation();
            report(loc, m_warn_id)
                .AddValue(var_decl->getName());
        }
    }

    return true;
}

} // namespace ica
```

#### Building

The building process is the same as in [README](./README.md#build-guide).

### Testing

We use [ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) to run our tests. Every test is a source file where we mark expected warnings and notes (check [clang docs](https://clang.llvm.org/doxygen/classclang_1_1VerifyDiagnosticConsumer.html) for it).

Let's write test for our `LocalVarStyleVisitor` from the previous step.

#### Write the test

Let's create the source file in `test/internal/test_local_var_name.cpp`
```cpp
int foo() {
    // expected-warning@+1 {{'mYaWeSoMeVaR' is bad code style for a local variable name}}
    int mYaWeSoMeVaR = 0;
    return mYaWeSoMeVaR + 1;
}
```

#### Add to CMakeLists.txt

Add this to `test/internal/CMakeLists.txt`:

```cmake
add_ica_test(
    NAME LocalVarStyle
    CHECKS local-var-style
    FILES_PATHS test_local_var_style.cpp
)
```

* `NAME` is just name of a test, but we usually keep it in sync with the visitor name
* `CHECKS` is the same comma-separated [checks list](./README.md#checks-list) from command line arguments
* `FILES_PATHS` is space-separated list of filenames with your tests

#### Running the test

You can either call

```bash
ctest --test-dir path/to/your/build/dir/ -R LocalVarStyle
```

...or use [do_test.sh](./test/do_test.sh) script in `test` directory. The check name is optional, `all` will be used by default.

```bash
./do_test.sh internal/test_local_var_style.cpp local-var-style
```

## Opening your pull request

(I have no actual idea how it should be done in open-source github world, please help me)

* Create your branch and push all the changes there
  * Please, reduce the amount of your initial commits and use informative commit messages
* Link the issue you're trying to solve
* Leave a small description of how you solve it and why you've done it this way
* Create your pull request to the `main` branch and wait for review

## Local patterns

Some common ideas used in ICA visitors source code will be explained here. Let it help you understand existing code and give you some ideas while writing your own.

### CompoundStmt stack

Premiss:
* There are situations when we want to consider scope of the current code block.
* Code blocks within `{}` are represented by [CompoundStmt](https://clang.llvm.org/doxygen/classclang_1_1CompoundStmt.html) node in libClang.
* [RecursiveASTVisitor](https://clang.llvm.org/doxygen/classclang_1_1RecursiveASTVisitor.html) has `dataTraverseStmtPre` and `dataTraverseStmtPost` methods to overload.
  * They are called before and after traversing a `Stmt` respectively.

Idea:

Let's track current compound statements stack and overload the methods to modify it.

Application:
* Track lifetime of a variable. E.g. to check if an iterator exists and can be reused instead of another `find` call.
* To defer report till a variable gets destructed, i.e. when you've visited all the actions applied to the variable and it's lifetime ends.
* To reduce amount of tracked variables. If you know the variable's lifetime ended, you don't have to keep it memorized.
