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
| BUG-001 | P1 | `md.I = 2.4f;` | `md.I` из полигона (через `I_cm`, мин. `1.2f`, параллельный перенос) вместо фиксированных `2.4f` (`PlayerCar.cpp`) | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-002 | P1 | `md.center.Set(0.0f, -0.20f);` | `md.center` от центроида полигона со смещением `-0.15f` по Y вместо `(0, -0.20)` (`PlayerCar.cpp`) | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-003 | P2 | `torsoLocal(-0.15f, 0.20f)` | торс и масштаб головы/торса привязаны к `driver_head_offset` и `driver_head_radius` (`PlayerCar.cpp`) | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-004 | P1 | `mConfig->base_spring_freq + (mSuspensionLevel * 0.40f)` | подвеска +0.40/lvl (10.6-12.8 Гц на lvl20) -> base·powf(upg_mult_suspension,lvl) | 1 | FIXED-VERIFIED | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-005 | P2 | `sprite_width`, `sprite_height` | выровнены размеры спрайтов с коллайдером/PNG (`jeep`, `hippievan`, `onewheeler`, `policecar`, `truck`, `minibike`, `hotrod`) | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-006 | P3 | `jd.maxMotorTorque = 500.0f;` | инициализация от `base_motor_torque * 2.0f` вместо мёртвых `500.0f` (`PlayerCar.cpp`) | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-007 | P1 | `EndRace(); StartRace();` / `mState = STATE_GARAGE;` | потеря монет при pause->restart/garage без CommitRaceResults() + двойной коммит на game-over | 1 | FIXED-VERIFIED | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-008 | P1 | `RunHeightfieldHashBenchmark()` | boot-бенчмарк 28 трасс каждый старт PSP; теперь гейт по benchmark.txt + [BOOT] sha/garage_ready_us | 1 | FIXED-VERIFIED | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-009 | P1 | `(vConfig->fourth_upgrade_type == 2` | fuel-множитель на не-FUEL слотах; теперь только type==1 | 1 | FIXED-VERIFIED | 9aa33842b941c473c48fa8b3603e0a2f8697db35 |
| BUG-010 | P2 | `Config::Physics.max_fuel` | HUD fuel делится на `mCar->GetMaxFuel()` (`Game::RenderHUD`) | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-011 | P3 | docs / perf metrics | обновлены метрики `STATUS.md`, галочка `Test 24` и секция стабилизации в `ROADMAP.md`, дедупликация/правка `params.csv` | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-012 | P2 | `mCar->Render(mRenderAlpha)` | порядок `Render()`: фоновые слои -> водитель -> кузов -> передние/отделённые детали -> колёса | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-013 | P2 | `ambulance` multi-sprite | многокомпонентные слои кузова (`ambulance`, `policecar`, `electric`, `hotrod`, `superoffroad`) и отпадающие при аварии части (`PlayerCar.cpp`) | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-014 | P0 | `b2Vec2 startPos(0.0f, 2.5f);` | спавн по максимуму рельефа и минимуму потолка над колёсной базой | 1 | FIXED-VERIFIED | 0c2e5bc25e2e51bf685bb4ba9a25d1685fb62119+5adb07920f4107ba8c4fc79ec4b49bee06eebd0d |
| BUG-015 | P3 | `0x000d55b0` | coin x1.25/x4 усечение (int); провенанс адресов 0x000d55b0 не подтверждён re_1300_dump; значения не менять; 500-монеты не спавнятся (P3 parity) | 2b | NEEDS-VERIFICATION | - |
| BUG-016 | P2 | `stage-icon-` | маппинг `mountain->cliffs`, `mudpool->mud`, `arcticcave->arctic-cave` и фоллбэк на `mTexStageLocked` при отсутствии ширины текстуры | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-017 | P2 | `NEW RECORD!` UI rect | измерение ширины через `GetTextWidth` и выверенный межстрочный интервал в `RenderGameOver()` | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-018 | P2 | `GetUpgradeCost` | `GetUpgradeCost(curLevel, baseCost)` учитывает `base_upgrade_cost` в `UpdateGarage` и `RenderGarage` | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-019 | P1 | `special_physics_mode == 2` | отключение моторов колёсных шарниров на Hovercraft, демпфер тангажа на воздушной подушке, апгрейд `hover_thrust` | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-020 | P1 | `thruster_fuel_drain` / `totalMult > 3.0f` | перевод расхода трастера на `1/FIXED_TIMESTEP` (единицы в секунду) и поднятие потолка `totalMult` до `6.0f` | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-021 | P1 | `Terrain::CreateBridge` | ширина дощечки и угол по хорде для крутых оврагов (`mountain`/`rooftops`/`construction`/`factory`), лут над пролётом моста | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-022 | P2 | `Game::OnFuelCollected` | пополнение бака на `mCar->GetMaxFuel()` вместо базовых `Config::Physics.max_fuel` | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-023 | P2 | `Game::UpdateGarage` | заблокированные трассы/машины не перезаписывают `PlayerInfo` при пролистывании и заблокирована прокачка в Tab 2 | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-024 | P2 | `AudioEngine` / `UpdateGameOver` | `UpdateMusicStream()` на Game Over, `SDL_LockAudioDevice` в `PlayMusic`/`StopMusic`, `memcpy` вместо невыровненных указателей и выравнивание нечётных чанков RIFF | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-025 | P2 | `special_physics_mode == 1` | масштабирование трения передней лыжи Snowmobile/Sleigh с уровнем апгрейда `TIRES` | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-026 | P2 | `Game::RenderPlaying` (Night Mode) | конус фар использует интерполированные `GetInterpolatedPosition` и `GetInterpolatedAngle` | 2 | FIXED-PENDING-REVIEW | local-head |
| BUG-027 | P2 | `Texture::LoadPNG_PC` | даунскейл >512px PNG на PC без обрезки с `mMaxU > 1.0f` | 2b | FIXED-PENDING-REVIEW | local-head |
| BUG-028 | P3 | `sTotalStepTicks` / `SFX_UPGRADE` | сброс счётчиков телеметрии в `StartRace()` и фоллбэк `wrench.wav` для `SFX_UPGRADE` | 3 | FIXED-PENDING-REVIEW | local-head |
| BUG-029 | P1 | `[onewheeler]` / `[minibike]` (`vehicles.cfg`) | Одно колесо на `onewheeler` (`0.00, -0.42`, `r=0.50`) + гиростабилизация и баланс `minibike` (`base_motor_torque=7.2`, база `-0.52..0.54`): сток `lvl 0` едет стабильно (`PPSSPP: fix_02_onewheeler.png`, `fix_03_minibike.png`) | 4 | FIXED-PENDING-REVIEW | local-head |
| BUG-030 | P1 | `pfd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL;` (`Terrain.cpp:806`) | Добавлен флаг `CAT_DRIVER` в `maskBits` досок подвесного моста | 4 | FIXED-PENDING-REVIEW | local-head |
| BUG-031 | P2 | `sParticleTexture.LoadPNG("assets/Tire.png")` (`ParticleSystem.cpp:20`) | Переключено на `assets/smoke.png` (`PPSSPP: fix_01_jeep.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-032 | P2 | `0xFF352518` / `0xFF181512` / `0xFF453020` (`Game.cpp:646..844,1407`) | Все цветовые константы гаража и GameOver приведены к формату ABGR `0xAABBGGRR` (`PPSSPP: fix_00_garage.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-033 | P2 | `if (HasCeiling())` (`Terrain.cpp:1297-1299`) | Добавлена верхняя заливка `DrawTerrainFill(..., cameraY + 14.0f)` для потолков `cave`/`arcticcave` (`PPSSPP: fix_06_cave.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-034 | P2 | `terrain-rainbow-surface.tex` (`orig=8x8, pot=16x8`) | Горизонтальный тайлинг NPOT repeat-текстур в `Texture::LoadTex()` и `tools/convert_assets.py` с `mMaxU=1.0` (`PPSSPP: fix_07_rainbow.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-035 | P2 | `groundTex->Bind(GU_REPEAT)` / `surfaceTex->Bind(GU_REPEAT)` (`Renderer2D.cpp:320,362,611,640`) | Передача `GU_LINEAR` / `GL_LINEAR` в `Bind()` на PSP и PC (`PPSSPP: fix_05_snowmobile.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-036 | P2 | `car-snowmobile-ski.png` (`vehicles.cfg:1277-1290`, `PlayerCar.cpp:913-931`) | Для `skiMode` лыжи рисуются невращающимися в пропорции 2:1 вдоль угла шасси (`PPSSPP: fix_05_snowmobile.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-037 | P2 | `[train]` `driver_head_offset = 1.40, 0.50` (`vehicles.cfg:1257`) | Водитель перемещён в открытую кабину (`-0.30, 0.52`) (`PPSSPP: fix_04_train.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-038 | P2 | `OnDriverDown()` / `UpdateGameOver()` (`Game.cpp:402-409,1325-1358`) | Кратковременный шаг пост-аварийной физики (`2.5s`) в `UpdateGameOver(frameTime)` и учёт `mWorld->GetGravity().y` для отлетевших деталей | 4 | FIXED-PENDING-REVIEW | local-head |
| BUG-039 | P2 | `DrawFilledRect(pX, 170.0f, ...)` (`Game.cpp:1131-1134`) | Заменено плавным градиентом атмосферного горизонта без ступеньки и вертикального шва (`PPSSPP: fix_01_jeep.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-040 | P2 | `RenderGarage()` / `RenderHUD()` (`Game.cpp:666,1190-1198`) | Уменьшен масштаб/выравнен текст табов (`0.31f, y=38`), HUD метров (`0.38f, y=11`) и правое выравнивание монет (`PPSSPP: fix_00_garage.png`, `fix_01_jeep.png`) | 5 | FIXED-PENDING-REVIEW | local-head |
| BUG-041 | P2 | `Particle Emissions` (`Game.cpp:986-1006`) | Блокировка частиц земли при `hide_wheels == 1` и поворот позиции выхлопа на угол шасси | 4 | FIXED-PENDING-REVIEW | local-head |
| BUG-042 | P2 | `AudioEngine::StopMusic()` / `PlayMusic()` (`AudioEngine.cpp:280-314,729-742,837-854`) | Синхронизация `sAudioInMusicMix` в `StopMusic()` на PSP и стриминг `.ogg` через `stb_vorbis` на PC | 6 | FIXED-PENDING-REVIEW | local-head |
| BUG-043 | P3 | `strncpy(...)` (`Config.cpp:200..254,338..393`) | Безопасный хелпер `CopyCfgStr` (`strnlen`+`memcpy`+`'\0'`): `0` предупреждений в `src/` при чистой сборке `psp-g++` | 6 | FIXED-PENDING-REVIEW | local-head |
| BUG-044 | P3 | Полировка ассетов/звука/UI (`05_race_benchmark_mid.png`, `v2_snowmobile_mid.png`, `AudioEngine.cpp:524`, `Game.cpp:212,555`) | Выровнены арки `jeep` (`3.50x1.48`), устранена пунктирная линия `Arctic` (`GU_LINEAR`), добавлен `LoadVehicleEngine()` для уникальных двигателей, звук отказа при покупке, удалена неиспользуемая `mTexLogo` | 6 | FIXED-PENDING-REVIEW | local-head |

**Закрытые/актуальные батчи:**
- Батч 1 = {004, 007, 008, 009, 014} — `FIXED-VERIFIED`
- Батч 2 = {003, 005, 010, 012, 017, 022, 026} визуал/HUD — `FIXED-PENDING-REVIEW`
- Батч 2b = {011, 013, 016, 018, 023, 024, 027} ассеты/мультиспрайт/гараж/аудио/docs — `FIXED-PENDING-REVIEW` (кроме BUG-015 `NEEDS-VERIFICATION` parity)
- Батч 3 = {001, 002, 006, 019, 020, 021, 025, 028} физика-калибровка и мосты — `FIXED-PENDING-REVIEW`
- Батчи 4–6 (по результатам PPSSPP v1.20.4 и 29×28 аудита) = {029..044} — `FIXED-PENDING-REVIEW` (`Батч 4`: {029, 030, 038, 041}; `Батч 5`: {031..037, 039, 040}; `Батч 6`: {042, 043, 044})

### Заметка по тесту lvl 20 (наблюдение при полном газе)
В безголовом 8-сек тесте на Countryside (газ в пол без отпускания) сток `lvl 0` стабилен для 27/29 машин (кроме `BUG-029`: `onewheeler`, `minibike`). На максимуме апгрейдов `lvl 20` (`torque * 1.10^20 = 6.73x`, с `BOOST` до `13.45x`) зажатый газ в пол приводит к перевороту у многих машин (`jeep`, `monster_truck`, `race_car`, `electric`, `dieseltruck`, `rallycar`, `dragster`, `tank`, `superoffroad`, `truck`, `snowmobile`, `hotrod`, `policecar`, `hippievan`, `moonlander`, `sleigh`), что частично ожидаемо для высокой мощности HCR и может потребовать отдельной настройки кривой момента/анти-вилли только по решению после согласования базовых фиксов.
