# Hierarchical Agent Workflow — Quick Reference

A system for implementing software features using coordinated sub-agents that keep context clean and enable parallel execution.

## Core Concept

```
┌─────────────────────────────────────────────────────────────────┐
│                         YOU                                     │
│                          │                                      │
│                          ▼                                      │
│  ┌─────────────────────────────────────────┐                   │
│  │       PLANNING COORDINATOR              │ ◄── Lightweight   │
│  │   (creates structure, delegates detail) │     context       │
│  └──────────────┬──────────────────────────┘                   │
│                 │                                               │
│        ┌────────┼────────┬────────┐                            │
│        ▼        ▼        ▼        ▼                            │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                  │
│   │Phase 1 │ │Phase 2 │ │Phase 3 │ │Phase N │  ◄── Parallel    │
│   │  Doc   │ │  Doc   │ │  Doc   │ │  Doc   │      where       │
│   └────────┘ └────────┘ └────────┘ └────────┘      possible    │
│                                                                 │
│                          │                                      │
│                          ▼                                      │
│  ┌─────────────────────────────────────────┐                   │
│  │     IMPLEMENTATION ORCHESTRATOR         │ ◄── Fresh         │
│  │   (coordinates impl & review agents)    │     conversation  │
│  └──────────────┬──────────────────────────┘                   │
│                 │                                               │
│        ┌────────┴────────┐                                     │
│        ▼                 ▼                                      │
│   ┌─────────┐       ┌─────────┐                                │
│   │  Impl   │ ◄───► │ Review  │  ◄── Iterate until ACCEPT     │
│   │  Agent  │       │  Agent  │      (max 3 cycles)            │
│   └─────────┘       └─────────┘                                │
│                                                                 │
│                          │                                      │
│                          ▼                                      │
│              ┌───────────┴───────────┐                         │
│              ▼           ▼           ▼                          │
│         Architecture  Quality   Requirements                    │
│           Review      Review      Review                        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## File Structure

Place these files in your project:

```
your-project/
├── .claude/
│   └── prompts/
│       ├── planning-coordinator.md      # Orchestrates planning phase
│       ├── phase-documenter.md          # Template for phase sub-agents
│       ├── implementation-orchestrator.md # Orchestrates implementation
│       ├── implementation-agent.md      # Template for coding sub-agents
│       └── review-agent.md              # Template for review sub-agents
├── docs/
│   └── implementation-plan/             # Generated during planning
│       ├── 00-overview.md
│       ├── 01-phase-one.md
│       └── ...
└── src/
    └── ...
```

## File Relationships

```
planning-coordinator.md ──references──► phase-documenter.md
                                        (spawns sub-agents using this)

implementation-orchestrator.md ──references──► implementation-agent.md
                               ──references──► review-agent.md
                                               (spawns sub-agents using these)
```

## Workflow

### Step 1: Planning Phase

Start a Claude Code conversation and run:

```
Follow the instructions in .claude/prompts/planning-coordinator.md

Feature specification:
[Describe what you want to build, or reference a spec file]
```

**What happens:**
1. Coordinator analyzes codebase for patterns
2. Creates `docs/implementation-plan/00-overview.md` with phase structure
3. Spawns sub-agents to document each phase in detail
4. Produces complete implementation plan

**Output:** `docs/implementation-plan/` folder with overview + one file per phase

### Step 2: Implementation Phase

Start a **fresh conversation** (clean context) and run:

```
Follow the instructions in .claude/prompts/implementation-orchestrator.md

The implementation plan is in docs/implementation-plan/
```

**What happens:**
1. Orchestrator reads plan, identifies parallel tracks
2. Spawns implementation agents for each track
3. Spawns review agents to verify each implementation
4. Routes feedback until all phases pass review (max 3 iterations)
5. Spawns final review agents (architecture, quality, requirements)
6. Produces completion report

**Output:** Working code + final review summary

## Key Principles

| Principle | Why It Matters |
|-----------|----------------|
| **Coordinators don't write code** | Keeps their context focused on orchestration |
| **Sub-agents get full context** | They have fresh context windows to work with |
| **Parallel where possible** | Independent phases/tracks run simultaneously |
| **Bounded iteration** | Max 3 review cycles prevents infinite loops |
| **Explicit handoffs** | Each agent receives complete instructions |

## Customization Points

### Adjusting Review Strictness

Edit `review-agent.md` section "When to ACCEPT" / "When to REJECT"

### Changing Iteration Limits

Edit `implementation-orchestrator.md`:
```
- If iteration < 3: Route feedback...
- If iteration >= 3: Mark as "Escalated"...
```

### Adding Domain-Specific Patterns

Add a section to `phase-documenter.md` or `implementation-agent.md`:
```markdown
## Project-Specific Patterns

- Always use [your pattern] for [situation]
- Reference `path/to/canonical/example.cpp` for [pattern type]
```

### Specialized Final Reviews

Edit the "Final Review Phase" section in `implementation-orchestrator.md` to add or modify the three review perspectives.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Agent going off-track | Check that spawn instructions include full context |
| Infinite revision loops | Verify iteration counter is being checked |
| Context exhaustion | Ensure coordinator isn't reading full source files |
| Inconsistent phases | Add cross-reference check in planning coordinator |
| Missed requirements | Strengthen requirements coverage final review |

## Quick Commands

**Start planning:**
```
Follow .claude/prompts/planning-coordinator.md
Feature: [description]
```

**Start implementation:**
```
Follow .claude/prompts/implementation-orchestrator.md
Plan: docs/implementation-plan/
```

**Resume stuck implementation:**
```
Continue as implementation orchestrator per .claude/prompts/implementation-orchestrator.md
Current status: [paste status table]
Resume from: [track/phase]
```

## Version

Last updated: January 2025
Compatible with: Claude Code with sub-agent/Task capabilities
