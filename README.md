# MINIcodingAgent
this application is a mini coding agentt that works with smaller ai models allowing them to perform agentic tasks. 
Recommended Tech Stack
Core Language

C++20/23

Why?

Maximum performance
Excellent multithreading
GPU support
Low memory overhead
Easy native integrations
Good for long-running agent systems

Libraries

Boost
fmt
spdlog
asio
range-v3
AI Runtime

Use multiple providers.

LM Studio

Ollama

OpenAI

Anthropic

Google

OpenRouter

vLLM

llama.cpp

TensorRT-LLM

Create one abstraction

ModelProvider

↓

Anthropic

OpenAI

Local

Google

OpenRouter

etc.

Never hardcode APIs.

GUI

For desktop

Qt 6

Reasons

Professional
Cross-platform
Fast
Native feeling
Excellent C++ support
Local Database
SQLite

Everything.

Conversation

Tasks

Memory

Embeddings

Settings

Logs

Projects

Indexes

Agents

Permissions

Events

Metrics
Vector Database

Initially

SQLite + sqlite-vec

Later

Qdrant

Milvus

Weaviate
Search Engine

Hybrid retrieval

BM25

+

Embeddings

+

AST search

+

Regex

+

Git history

+

Dependency graph
Parsing

Tree-sitter

Supports

C++

Python

Rust

Go

Java

C#

Typescript

Javascript

PHP

Lua

etc.
AST

Tree-sitter

LSP

(Language Server Protocol)

Embeddings

Local

bge-large

nomic-embed

gte-large

e5-large
Message Bus
Boost ASIO

or

ZeroMQ

or

Custom Event Bus
Thread Pool

Custom

Work-stealing scheduler

Priority queues

Fibers (optional)

JSON
nlohmann/json

or

simdjson
HTTP
CPR

or

libcurl
WebSocket

Boost Beast

Memory
SQLite

+

Embeddings

+

Knowledge Graph

+

Summaries
Code Execution

Docker

Firecracker

WSL

Linux namespaces

Sandbox

Container Runtime

Docker

Initially

Later

Kubernetes

Logging
spdlog
Testing
GoogleTest

Catch2

doctest
Dependency Injection

Simple custom DI

Avoid giant frameworks.

Build System
CMake
Package Manager
vcpkg

or

Conan
Repository Index

Store

AST

Imports

Exports

Symbols

Call Graph

References

Inheritance

File Hashes

Checksums

Dependencies

Embeddings

Documentation

Git history
Internal Architecture
Application

↓

Kernel

↓

Scheduler

↓

Planner

↓

Agents

↓

Tools

↓

Memory

↓

Execution

↓

Verification

↓

Reflection

↓

UI
Recommended Folder Structure
AIOS/

src/

kernel/

scheduler/

planner/

memory/

context/

retrieval/

prompt/

agents/

tools/

execution/

sandbox/

verification/

reflection/

planning/

repository/

parser/

ast/

lsp/

git/

filesystem/

terminal/

browser/

docker/

permissions/

security/

events/

logging/

metrics/

telemetry/

settings/

plugins/

providers/

models/

embeddings/

vector/

database/

compression/

cache/

summarization/

taskgraph/

workflow/

knowledge/

reasoning/

compiler/

ui/

network/

config/

api/

tests/

docs/
Long-Term Vision

Your system should eventually support:

Multi-agent orchestration
Parallel task graphs
Incremental repository indexing
Semantic retrieval
Automatic context compression
Self-reflection and critique
Tool/plugin marketplace
Model routing across local and cloud providers
Long-term memory and knowledge graphs
Multi-language code intelligence
Secure sandboxed execution
Workflow automation
Human approval checkpoints
Distributed execution across multiple machines
GPU-aware scheduling
Offline-first operation with optional cloud augmentation








If your goal is to build something comparable to Claude Code, Google Antigravity, Codex, or the latest agentic coding systems, you're no longer building "an AI model." You're building an **operating system around an LLM**.

The LLM is often only **10-30%** of the total system.

The remaining **70-90%** consists of orchestration, memory, planning, tools, execution environments, context management, verification, recovery, and hundreds of supporting systems.

---

# High Level Architecture

```
                        USER
                          │
                          ▼
                 Request Interpreter
                          │
            Intent + Constraints Parser
                          │
        ┌─────────────────┼────────────────┐
        ▼                 ▼                ▼
 Planner Agent      Context Engine    Memory Engine
        │                 │                │
        └─────────────────┼────────────────┘
                          ▼
                 Task Graph Generator
                          │
            DAG / Workflow / Dependency Graph
                          │
        ┌───────────────┬───────────────┬───────────────┐
        ▼               ▼               ▼
 Code Agent      Search Agent     Test Agent
        ▼               ▼               ▼
 Documentation    Tool Runner    Terminal Agent
        │               │               │
        └───────────────┼───────────────┘
                        ▼
               Execution Environment
                        ▼
             Observation + Verification
                        ▼
              Reflection / Self Critique
                        ▼
             Continue / Retry / Finish
```

This is vastly simplified.

Real systems are significantly more complicated.

---

# Level 1 Architecture

A modern coding agent contains roughly these systems.

```
User Interface

Conversation Manager

Context Manager

Planning Engine

Agent Scheduler

Task Queue

Memory

Tool System

Execution Engine

Verification Engine

Reflection Engine

Safety Layer

Prompt Compiler

Model Router

State Machine

Checkpoint System

Recovery System

Telemetry

Logging

Cost Controller

Caching

Permission Manager

Filesystem Layer

Git Layer

Terminal Layer

Browser Layer

Network Layer

Plugin System

Subagent System

Output Formatter
```

Already over 30 major components.

Each contains dozens of subsystems.

---

# Conversation Layer

This is not simply chat history.

Instead:

```
Conversation

├── Messages
├── Hidden reasoning state
├── Active tasks
├── Open questions
├── Constraints
├── Assumptions
├── TODO list
├── Running agents
├── Current directory
├── Current git branch
├── User preferences
├── Project summary
├── Important files
├── Recent edits
├── Failed attempts
├── Current objective
├── Confidence estimates
├── Time spent
├── Remaining budget
└── Active tool outputs
```

---

# Context Engine

One of the hardest parts.

Instead of dumping everything into the model...

A context engine builds the prompt dynamically.

```
Entire Repository
        │
        ▼
File Ranking

↓

Dependency Ranking

↓

Recent Changes

↓

Relevant Symbols

↓

AST Analysis

↓

Imports

↓

Call Graph

↓

User Request

↓

Prompt Builder
```

Only the most useful information is inserted.

---

# Prompt Compiler

The prompt itself is generated.

Example

```
System Prompt

+

Tool Descriptions

+

Repository Summary

+

Recent Conversation

+

Current Objective

+

Open TODOs

+

Relevant Files

+

API Documentation

+

Previous Attempts

+

Error Messages

+

Memory

+

Safety Constraints

+

Formatting Rules

↓

Compiled Prompt
```

---

# Planner

The planner does NOT generate code.

Instead it creates a graph.

Example

```
Build Authentication

↓

Analyze project

↓

Find auth framework

↓

Read documentation

↓

Inspect routing

↓

Locate database

↓

Generate migration

↓

Generate middleware

↓

Generate tests

↓

Run tests

↓

Fix failures

↓

Commit changes
```

This becomes a DAG.

```
Analyze

├── Read package.json
├── Read README
├── Read auth config
├── Read routes
├── Read schema
└── Read environment
```

Many can run simultaneously.

---

# Task Graph

Instead of

```
Step 1

Step 2

Step 3
```

Modern agents build

```
          Root Goal

       /      |      \

Analyze  Search Docs  Read Code

    \       |        /

     Build Understanding

          /      \

Generate Tests Generate Code

        \       /

      Execute

         │

      Verify

         │

Reflect

         │

Retry
```

---

# Scheduler

The scheduler decides

```
Which agent runs

How many

Parallel?

Sequential?

Priority

Timeout

Memory budget

Context budget

Cost budget

Retry count

Cancellation
```

---

# Subagents

Claude Code is famous for these.

Instead of one LLM

```
Main Agent

↓

Spawn

↓

Research Agent

↓

Documentation Agent

↓

Bug Finder

↓

Test Writer

↓

Refactor Agent

↓

Security Agent

↓

Performance Agent

↓

Architecture Agent
```

Each receives different prompts.

---

# Example

Main agent

```
Implement OAuth.
```

Research agent

```
Read latest OAuth docs.
```

Code agent

```
Modify source.
```

Test agent

```
Write integration tests.
```

Security agent

```
Check vulnerabilities.
```

Reflection agent

```
Critique result.
```

All run independently.

---

# Tool Calling

Modern agents expose tools as functions.

Example

```
ReadFile()

WriteFile()

Search()

FindSymbol()

RunTests()

RunTerminal()

GitDiff()

GitCommit()

OpenBrowser()

HTTP()

DatabaseQuery()

Lint()

Format()

ASTQuery()

ReplaceSymbol()

RenameClass()

RunPython()

RunDocker()

GenerateImage()

OCR()

Transcribe()

Compile()

Benchmark()

MemoryLookup()
```

Some systems expose 100+ tools.

---

# Tool Selection

The LLM decides

```
Need file?

↓

ReadFile()

Need command?

↓

Terminal()

Need documentation?

↓

Search()

Need git?

↓

Git()

Need browser?

↓

Browser()

Need AST?

↓

Parser()
```

---

# Execution Loop

This is the heart.

```
Observe

↓

Think

↓

Plan

↓

Tool Call

↓

Observe

↓

Think

↓

Another Tool

↓

Observe

↓

Generate

↓

Verify

↓

Reflect

↓

Repeat
```

This may execute hundreds of iterations.

---

# Reflection Loop

One of the biggest improvements.

```
Write code

↓

Run tests

↓

Failure

↓

Analyze failure

↓

Identify mistake

↓

Rewrite

↓

Retest

↓

Repeat
```

Sometimes

20+

50+

100+

iterations.

---

# Memory System

Usually several layers.

```
Conversation Memory

↓

Session Memory

↓

Repository Memory

↓

Long-Term Memory

↓

Embedding Memory

↓

Knowledge Graph

↓

Project Summary

↓

User Preferences
```

---

# Repository Understanding

Instead of reading everything

The agent builds

```
AST

Imports

Exports

Call Graph

Dependency Graph

Type Graph

Inheritance Graph

API Graph

Folder Graph

Symbol Graph

Git Graph

Ownership Graph
```

---

# Code Index

Example

```
Symbol

↓

File

↓

Class

↓

Method

↓

References

↓

Callers

↓

Implementations

↓

Tests

↓

Documentation
```

This allows

```
Find every usage of UserRepository
```

instantly.

---

# AST Layer

Rather than regex.

```
Parser

↓

AST

↓

Semantic Analysis

↓

Type Resolution

↓

Reference Resolution

↓

Transformation

↓

Pretty Printer
```

---

# Verification

Generated code is not trusted.

Pipeline

```
Compile

↓

Lint

↓

Format

↓

Tests

↓

Static Analysis

↓

Security Scan

↓

Type Check

↓

Coverage

↓

Benchmarks
```

Failures go back into planning.

---

# Self Critique

Large agents often run another LLM.

```
Generated Solution

↓

Critic

↓

Problems Found

↓

Revision

↓

Critic

↓

Revision
```

---

# Context Compression

Instead of

100 MB repository

↓

```
Summaries

↓

Semantic Compression

↓

Symbol Extraction

↓

Dependency Extraction

↓

Relevant Chunks

↓

Prompt
```

---

# Retrieval Pipeline

```
Question

↓

Embedding

↓

Vector Search

↓

BM25

↓

AST Search

↓

Regex

↓

Symbol Search

↓

Git Search

↓

Merge Results

↓

Rank

↓

Prompt
```

---

# Model Routing

One model is inefficient.

Instead

```
Small Model

Classification

↓

Medium Model

Planning

↓

Large Model

Complex reasoning

↓

Code Model

Generation

↓

Vision Model

Images

↓

Embedding Model

Retrieval
```

---

# Parallelism

Instead of

```
One request
```

Modern agents might execute

```
15 searches

12 file reads

8 subagents

6 documentation lookups

3 terminal commands

2 test suites

simultaneously
```

Then merge the results.

---

# State Machine

Everything is tracked explicitly.

```
Idle

↓

Planning

↓

Researching

↓

Executing

↓

Waiting

↓

Testing

↓

Reflecting

↓

Retrying

↓

Finished

↓

Cancelled
```

---

# Checkpoints

Every major action is saved.

```
Checkpoint

↓

Modify files

↓

Compile

↓

Failure

↓

Rollback

↓

Retry
```

---

# Cost Manager

Tracks

```
Tokens

$

Latency

GPU Time

API Calls

Tool Calls

Context Size

Retries
```

---

# Caching

Caches

```
LLM Responses

Embeddings

AST

Repository Index

Documentation

Search Results

File Hashes

Tokenization

Prompt Pieces
```

---

# Permission Layer

Before executing

```
Delete?

Modify?

Shell?

Network?

Git Push?

Docker?

SSH?

Database?
```

The agent checks policies or requests approval.

---

# Event Bus

Large systems are typically event-driven rather than linear.

```
FileChanged
ToolFinished
AgentSpawned
AgentCompleted
TestsFailed
TestsPassed
PlanUpdated
MemoryUpdated
ContextRebuilt
UserInterrupted
RetryRequested
CheckpointCreated
```

Different components subscribe to these events.

---

# Putting It Together

A sophisticated coding agent resembles a distributed operating system:

```
                    User
                      │
                Intent Parser
                      │
              Context Builder
                      │
                Master Planner
                      │
          Dependency / Task Graph
                      │
     ┌───────────── Scheduler ─────────────┐
     │             │            │          │
Research Agent  Code Agent  Test Agent  Review Agent
     │             │            │          │
     └─────── Tool & Execution Layer ──────┘
                      │
      Filesystem • Git • Terminal • Browser
                      │
             Verification & Reflection
                      │
             Memory & Context Updates
                      │
                 Continue or Finish
```

### If you want to build a system at the level of Claude Code or Google Antigravity, expect roughly these implementation scales:

* **Core orchestration engine:** 50–150 modules
* **Tool framework:** 100–300 tools and adapters
* **Memory and retrieval:** vector search, symbolic indexing, project summaries, caches, and long-term memory
* **Repository intelligence:** parsers, ASTs, dependency graphs, call graphs, semantic search, and incremental indexing
* **Planning and execution:** task graphs, schedulers, retries, checkpoints, and rollback
* **Agent ecosystem:** planner, researcher, coder, tester, reviewer, debugger, security reviewer, documentation writer, and specialized subagents
* **Infrastructure:** sandboxing, permissions, telemetry, logging, cost tracking, model routing, streaming, caching, and plugin systems

At the frontier, these systems easily exceed **500–1,000 distinct subsystems** and **millions of lines of code** when counting orchestration frameworks, IDE integrations, language servers, indexing pipelines, execution sandboxes, observability, testing infrastructure, and cloud services. The LLM is only one component; the real engineering challenge is designing the architecture that allows the model to reliably perceive, plan, act, verify, recover from failures, and collaborate with many specialized agents and tools.
