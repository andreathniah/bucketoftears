---
layout: post
title: "CodeQL - Exploring the Terrain #1"
author: "BoT"
tags: "writeup codeql"
excerpt_separator: <!--more-->
---

By playing **CodeQL CTF: Go and don't return** organized by GitHub Security Lab.

<!--more-->

For some reason, CodeQL simultaneously feels like it has tons of documentation and none at all. In all honestly, I am really having a hard time writing customized queries because I, for the love of God, cannot find simple examples that hints at how one could write such queries for bug hunting.

It's either I am not looking at the right set of documentation, or I am just too stupid to understand them -- the story of my life 😩 -- either way, here's my attempt at learning the basics of writing queries through [CodeQL CTF: Go and don't return](https://securitylab.github.com/ctf/go-and-dont-return/) by breaking down the [reference answer](https://securitylab.github.com/ctf/go-and-dont-return/answers/).

Note that this is an evolving post which will be updated as I work my way through the exercises. Also, a word of warning, explanations in this post may not make sense to anyone but myself. I probably got most of the terminology wrong too, oh well 💁‍♀️

## Setup Instructions

Paraphrasing from the official instructions, here's the gist of what you need to do:

1. Install Visual Studio Code
2. Install the CodeQL extension for Visual Studio Code
3. Clone [https://github.com/github/vscode-codeql-starter/](https://github.com/github/vscode-codeql-starter/) with `git clone --recursive`
4. In VS Code, click `File > Open Workspace`. Select the file `vscode-codeql-starter.code-workspace` in your checkout of this repository
5. Download [CodeQL database for MinIO](https://drive.google.com/file/d/1ZIgD5zhSg53P7vcw3aZqZKcZDRGDFTt0/view?usp=sharing) and import it into VS Code by:
   - Opening the CodeQL Databases view in the sidebar
   - Chosing to add a database from a local ZIP archive with the zip file downloaded
6. Test by running the `example.ql` query that is in the `codeql-custom-queries-go` folder

Got some results? Alright, let's start "playing" -- _ahem_, copying answers 🙈

## The Challenge

The challenge requires one to leverage on CodeQL to write a series of queries to find unsafely implemented code that mostly revolves around the below snippet which can be found in full at [this fix commit](https://github.com/minio/minio/commit/4cd6ca02c7957aeb2de3eede08b0754332a77923?diff=split):

```go
func validateAdminSignature(ctx context.Context, r *http.Request, region string) (auth.Credentials, map[string]interface{}, bool, APIErrorCode) {
      ...
      s3Err = isReqAuthenticated(ctx, r, region, serviceS3)
  }

  if s3Err != ErrNone {
      reqInfo := (&logger.ReqInfo{}).AppendTags("requestHeaders", dumpRequest(r))
      ctx := logger.SetReqInfo(ctx, reqInfo)
      logger.LogIf(ctx, errors.New(getAPIError(s3Err).Description), logger.Application)
      // missing return statement here that triggered the vulnerability
  }
      ...
}
```

### Step 1.1: Finding references to ErrNone

Find all variables named "ErrNone"

- Use `Ident` to find any variable
- Chain with `.getName()` to get name of variable

```sql
from Ident i
where i.getName() = "ErrNone"
select i
```

### Step 1.2: Finding equality tests against ErrNone

Find all operands (variable) that compares against "ErrNone"

- Use `EqualityTestExpr` to find comparision that is either `==` or `!=`
- Chain with:
  - `.getAnOperand()` to get variable that gets compared (aka `<variable_called_ErrNone>`)
  - `.(Ident)` to specify the variable "type"
  - `.getName()` to get name of variable

```sql
from EqualityTestExpr eq
where eq.getAnOperand().(Ident).getName() = "ErrNone"
select eq
```

### Step 1.3: Finding if-blocks making such a test

Find all if-statements that compares against "ErrNone"

- Use `IfStmt` to find if-statements
- Chain with:
  - `.getCond().(EqualityTestExpr)` to specify the condition being checked is either `==` or `!=`
  - `.getAnOperand().(Ident).getName() = "ErrNone"` to specify the target variable is named "ErrNone"

AKA, we are only finding `= <variable_called_ErrNone>`

```sql
from IfStmt i
where i.getCond().(EqualityTestExpr).getAnOperand().(Ident).getName() = "ErrNone"
select i
```

### Step 1.4: Finding return statements

Find all return statements

- Use `ReturnStmt` to find a return statement

```sql
from ReturnStmt r
select r
```

### Step 1.5: Finding if-blocks without return statements

Find all if-blocks that don't contain return statements in their `then` branch

- Use `IfStmt` to find if-statement
- Chain with:
  - `.getThen()` to get the "then" branch of this if-statement
  - `.getAStmt()` to get a statement in the branch block
  - `instanceof ReturnStmt` to perform a type check and ensure the variable type is a return-statement

```sql
from IfStmt i
where not i.getThen().getAStmt() instanceof ReturnStmt
select i
```

### Step 1.6: Putting it all together

Find the if-blocks testing for equality to ErrNone with no return

- Use `IfStmt` to find if-statement
- Chain with statements from:
  - [_Step 1.3_] where condition being checked is either `==` or `!=` against a target variable/operand named "ErrNone"; and
  - [_Step 1.5_] within its "then" branch, does not have a statement within that is a return-statement type

```sql
from IfStmt i
where
  i.getCond().(EqualityTestExpr).getAnOperand().(Ident).getName() = "ErrNone" and
  not i.getThen().getAStmt() instanceof ReturnStmt
select i
```

### Step 2.1: Find conditionals that are fed from calls to isReqAuthenticated

Find all equality tests of `DataFlow::EqualityTestNode` type where the operand is a sink of a data-flow configuration that tracks data flowing from **ANY call** --> into `isReqAuthenticated()` --> **ANY equality test operand**

First, create customized class to override default source and sinks in a data-flow via `DataFlow::Configuration` where:

- Source is defined as any method call of minio entity type with variable name of `isReqAuthenticated` within:
  - `any(DataFlow::CallNode cn | <filter_condition>)` to declare target type as a function/method call as `isReqAuthenticated()` is a function
  - `any(<declare_target_type> | cn.getTarget().hasQualifiedName("github.com/minio/minio/cmd", "isReqAuthenticated"))` to filter out calls involving `minio` package and has function/method name of `isReqAuthenticated`
  - `.getResult()` to get the data-flow node of the filtered call
- Sink is defined as operand of any `DataFlow::EqualityTestNode` found in the data-flow
  - `any(DataFlow::EqualityTestNode n)` to declare target type as a node performing equality test via `==` or `!=`
  - `.getAnOperand()` to get operand of the filtered operation

```java
class CustomizedAuth extends DataFlow::Configuration {
  // fix for compiler that complains "this" is not binded
  CustomizedAuth() { this = "random-string" }

  // define source as any method call of minio entity type and supplied variable name
  override predicate isSource(DataFlow::Node source) {
    source =
      any(DataFlow::CallNode cn |
        cn.getTarget().hasQualifiedName("github.com/minio/minio/cmd", "isReqAuthenticated")
      ).getResult()
  }

  // define sink as operand of any DataFlow::EqualityTestNode found in data-flow
  override predicate isSink(DataFlow::Node sink) {
    sink = any(DataFlow::EqualityTestNode n).getAnOperand()
  }
}
```

With customized class defined, to find the desired data-flow:

- Declare variables
  - `DataFlow::Configuration`: a customized class that overrides the default source and sink
  - `DataFlow::Node`: a typical data-flow node
  - `DataFlow::EqualityTestNode`: a data-flow node performing an equality test with `==` or `!=`
- Specify conditions
  - `<DataFlow::Configuration>.hasFlow(_, <DataFlow::Node>)` to find flow from given source (any type of input) to sink (any `DataFlow::Node`)
  - `<DataFlow::EqualityTestNode>.getAnOperand() = <DataFlow::Node>` to find operand that is the same declared `DataFlow::Node` variable
- Show results with `select`

AKA, we should first define the source and sinks based on requirements. After all relevant data-flow are identified, select flows (via the `sink` variable) that has similar operand as supplied node.

```sql
from CustomizedAuth config, DataFlow::Node sink, DataFlow::EqualityTestNode compare
where config.hasFlow(_, sink) and compare.getAnOperand() = sink
select compare
```

### Step 2.2: Find the true bug!

Find all if-statements:

- With equality test where the operand is a sink of a data-flow configuration that tracks data flowing from **ANY call** --> into `isReqAuthenticated()` --> **ANY equality test operand**
- Where the said equality tests against operand named "ErrNone" and does not contain return statement in their then-branch

To fulfill the first condition, we can build upon the query we have in _[Step 2.1]_ by converting it into a predicate/function (no idea what is the proper term here) that could be reused.

```sql
--  Before
from CustomizedAuth config, DataFlow::Node sink, DataFlow::EqualityTestNode compare
where config.hasFlow(_, sink) and compare.getAnOperand() = sink
select compare
```

```java
// After
EqualityTestExpr checkAuth() {
  exists(CustomizedAuth config, DataFlow::Node sink, DataFlow::EqualityTestNode compare |
    config.hasFlow(_, sink) and compare.getAnOperand() = sink
  |
    result = compare.asExpr()
  )
}
```

Combining this with query established in _[Step 1.6]_, we'll get:

```sql
from IfStmt i
where
  i.getCond() = checkAuth() and
  i.getCond().(EqualityTestExpr).getAnOperand().(Ident).getName() = "ErrNone" and
  not i.getThen().getAStmt() instanceof ReturnStmt
select i
```
