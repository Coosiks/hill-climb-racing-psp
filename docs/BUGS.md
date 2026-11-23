# Hill Climb Racing PSP — Bug Registry

- **Baseline:** `ad51ed68ca6d1dc06eb4a35ad3836f384ecc685e`
- **Phase:** STABILIZATION (Feature Freeze)
- **Severity Levels:**
  - `P0`: Crash / Instant Death / Unplayable
  - `P1`: Gameplay / Economy / Physics regression
  - `P2`: Visual / Audio / HUD defect
  - `P3`: Polish / Docs / Inconsequential
- **Status Lifecycle:** `OPEN-CODE` -> `OPEN-REPRO` -> `NEEDS-VERIFICATION` -> `FIXED-PENDING-REVIEW` -> `FIXED-VERIFIED` (*status `FIXED-VERIFIED` is set ONLY by reviewer*)

| ID | Sev | Anchor / Substring | Description | Batch | Status | Commit |
|---|---|---|---|---|---|---|
| BUG-001 | P1 | `md.I = 2.4f;` | md.I=2.4 для всех (PlayerCar.cpp) | 3 | OPEN-CODE | - |
| BUG-002 | P1 | `md.center.Set(0.0f, -0.20f);` | md.center=(0,-0.20) для всех | 3 | OPEN-CODE | - |
| BUG-003 | P2 | `torsoLocal(-0.15f, 0.20f)` | торс захардкожен (-0.15,0.20)/0.65/0.55, голова из cfg -> разрыв до 1.55 м (train/touristbus) | 2 | OPEN-CODE | - |
| BUG-004 | P1 | `mConfig->base_spring_freq + (mSuspensionLevel * 0.40f)` | подвеска +0.40/lvl (10.6-12.8 Гц на lvl20) -> base·powf(upg_mult_suspension,lvl) | 1 | FIXED-PENDING-REVIEW | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-005 | P2 | `sprite_width`, `sprite_height` | спрайты не из коллайдера (jeep 3.40x1.45 vs 3.00x1.00; minibike 1.80x1.20 vs 1.10x0.60) | 2 | OPEN-CODE | - |
| BUG-006 | P3 | `jd.maxMotorTorque = 500.0f;` | мёртвый maxMotorTorque=500 (рантайм <=349.8) | 3 | OPEN-CODE | - |
| BUG-007 | P1 | `EndRace(); StartRace();` / `mState = STATE_GARAGE;` | потеря монет при pause->restart/garage без CommitRaceResults() + двойной коммит на game-over | 1 | FIXED-PENDING-REVIEW | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-008 | P1 | `RunHeightfieldHashBenchmark()` | boot-бенчмарк 28 трасс каждый старт PSP; теперь гейт по benchmark.txt + [BOOT] sha/garage_ready_us | 1 | FIXED-PENDING-REVIEW | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-009 | P1 | `(vConfig->fourth_upgrade_type == 2` | fuel-множитель на не-FUEL слотах; теперь только type==1 | 1 | FIXED-PENDING-REVIEW | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-010 | P2 | `Config::Physics.max_fuel` | HUD fuel делится на Config::Physics.max_fuel вместо GetMaxFuel() (геттер уже есть) | 2 | OPEN-CODE | - |
| BUG-011 | P3 | docs / perf metrics | docs: stale perf 415.51/188.08/66.23; "avg 6.870 > пик 1.925"; test name _distance vs _bonus; ROADMAP Test24 unchecked; params.csv дубли/stale | 2b | OPEN-CODE | - |
| BUG-012 | P2 | `mCar->Render(mRenderAlpha)` | колёса за кузовом (порядок Render) | 2 | OPEN-CODE | - |
| BUG-013 | P2 | `ambulance` multi-sprite | многокомпонентные спрайты (скорая): нет отпадающих частей, плавающий фрагмент | 2b | OPEN-CODE | - |
| BUG-014 | P0 | `b2Vec2 startPos(0.0f, 2.5f);` | спавн (0,2.5) без h(0)/геометрии: nuclear/rainbow/volcano мгновенная смерть -> максимум рельефа под колёсной базой + радиусы/offsets | 1 | FIXED-PENDING-REVIEW | 0c2e5bc25e2e51bf685bb4ba9a25d1685fb62119+5adb07920f4107ba8c4fc79ec4b49bee06eebd0d |
| BUG-015 | P3 | `0x000d55b0` | coin x1.25/x4 усечение (int); провенанс адресов 0x000d55b0 не подтверждён re_1300_dump; значения не менять; 500-монеты не спавнятся (P3 parity) | 2b | NEEDS-VERIFICATION | - |
| BUG-016 | P2 | `stage-icon-` | stage-icon-<id> vs APK cliffs/mud/arctic-cave; Texture::Bind silent stale-bind; audit без иконок | 2b | OPEN-CODE | - |
| BUG-017 | P2 | `NEW RECORD!` UI rect | UI-rect без измерения текста (NEW RECORD! 115x18 overflow) | 2 | OPEN-CODE | - |
| BUG-018 | P2 | `GetUpgradeCost` | GetUpgradeCost игнорирует base_upgrade_cost | 2b | OPEN-CODE | - |

**Следующие батчи:**
- Батч 2 = {003, 005, 010, 012, 017} визуал/HUD (5 позиций)
- Батч 2b = {016, 013, 018, 015, 011} ассеты/мультиспрайт/логика цен/провенанс/docs-долг (5 позиций)
- Батч 3 = {001, 002, 006} физика-калибровка (3 позиции)
