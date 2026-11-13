---
name: Development Task
about: Specify a targeted feature, physics calibration, or refactoring task
title: '[TASK] T-xxx: <title>'
labels: ['task']
assignees: ''
---

## 1. Goal & Objective
<!-- High-level description of what needs to be implemented or calibrated -->

## 2. Context & Background
<!-- Reference benchmark, mathematical basis, or hardware constraint -->

## 3. Targeted Files & Components
<!-- Exact paths of files to create or modify -->
- `src/...`
- `config/...`

## 4. Acceptance Criteria
- [ ] Compiles cleanly on both PC (CMake) and PSP (PSPSDK).
- [ ] No heap memory allocations in the hot game loop.
- [ ] Tested on target hardware / PPSSPP emulator without regression.

## 5. Agent Report Deliverables
<!-- What the developer/agent must include in docs/agent_reports/T-xxx.md -->
- Summary of changes and modified files.
- Verification evidence (terminal output, build log, or performance metrics).
- Updates to `docs/STATUS.md` and `docs/ROADMAP.md`.
