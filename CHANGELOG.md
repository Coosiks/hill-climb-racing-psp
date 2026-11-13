# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- **T-010**: Static zero-allocation Box2D memory pool.
- **T-011**: Calibration of chassis inertia and center-of-mass.
- **T-012**: Wheel joint spring frequency and suspension travel tuning.
- **T-020**: Recycled terrain chunks (`b2ChainShape`).

---

## [0.1.0-beta.1] - 2026-11-13

### Added
- **Clean-room reimplementation** of Hill Climb Racing for Sony PlayStation Portable (PSP) and PC.
- **Dual-Build Framework:** Native PC target (SDL2 + OpenGL) and PSP target (PSPSDK + `sceGu` + `sceAudio`).
- **Exact mathematical curves** decompiled from `libgame.so` for Countryside, Desert, Arctic, and Moon stages.
- **Kinematic Box2D vehicle model**: 6-vertex chassis polygon, front and rear wheel joints, suspension damping, and basic air pitch control.
- **Data-driven configuration engine**: Extracted all tunable constants into `config/physics.cfg`, `config/vehicles.cfg`, and `config/stages.cfg`.
- **Missing assets detection**: Friendly on-screen instruction screen when running without converted game assets.
- **Offline Asset Converter**: Idempotent pipeline (`tools/convert_assets.py`) for unpacking user APKs and swizzling textures into PSP hardware tile format.
- **Automated CI/CD**: GitHub Actions workflow for building both PSP `EBOOT.PBP` and PC Linux binaries.
