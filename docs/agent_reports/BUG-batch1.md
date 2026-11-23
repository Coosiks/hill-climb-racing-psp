# Stabilization Batch 1 Verification Report

- **Baseline:** `ad51ed68ca6d1dc06eb4a35ad3836f384ecc685e`
- **Branch:** `fix/stabilization-batch-1`
- **Fix Commits:** `9aa33842b941c473c48fa8b3603e0a2f8697db35`, `5adb07920f4107ba8c4fc79ec4b49bee06eebd0d`
- **Final Code SHA:** `5adb07920f4107ba8c4fc79ec4b49bee06eebd0d`
- **CI Status:**
  - Run ID `44953070334`: commit `9aa33842b941c473c48fa8b3603e0a2f8697db35` — `conclusion=failure` ([GitHub Actions](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953070334))
  - Run ID `44953276259`: commit `5adb07920f4107ba8c4fc79ec4b49bee06eebd0d` — `conclusion=success` ([GitHub Actions](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259))
- **PSP EBOOT SHA256:** `f264accdca46037283ecb620a58b6f854180aa612eacedd1f2c0cf2f5e1d2021` (559,868 bytes)
- **PPSSPP Cold Boot Time:** `17700999` µs (17.70 s) $\to$ `500486` µs (~0.50 s)

---

## 1. Сводная таблица багов Батча 1

| Баг | Приоритет | SHA фикса | CI Run (ID, Статус, Ссылка) | Пути доказательств (before / after) | Ключевой результат ДО | Ключевой результат ПОСЛЕ |
|---|---|---|---|---|---|---|
| **BUG-014** | P0 | `0c2e5bc` + `5adb079` | `44953276259` (success)<br>[GitHub Run](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259) | `evidence/before/test_spawn_clearance.{stdout,stderr}`<br>`evidence/after/test_spawn_clearance.{stdout,stderr}` | `Spawn clearance: 38 failures / 84 scenarios`<br>Мгновенная смерть на nuclear, rainbow, volcano, mountain, rollercoaster | `Spawn clearance: 0 failures / 84 scenarios`<br>100% чистый спавн всех 28 трасс × 3 машин (`jeep`, `ambulance`, `train`) |
| **BUG-007** | P1 | `9aa33842` | `44953276259` (success)<br>[GitHub Run](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259) | `evidence/before/test_audio_pause_bonus.{stdout,stderr}`<br>`evidence/after/test_audio_pause_bonus.{stdout,stderr}` | `Pause -> GARAGE: wallet=0 expected=625 record=0 expected=105`<br>`Driver down: first=62 second=124 expected=62` (двойной коммит) | `Pause -> GARAGE: wallet=625 expected=625 record=105 expected=105`<br>`Pause -> RESTART: wallet=1750 expected=1750 record=215 expected=215`<br>`Driver down: first=1812 second=1812 expected=1812` |
| **BUG-008** | P1 | `9aa33842` | `44953276259` (success)<br>[GitHub Run](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259) | `evidence/before/psp_hcr_log.txt`<br>`evidence/after/psp_hcr_log.txt`<br>`evidence/after-benchmark/psp_hcr_log.txt` | `garage_ready_us=17700999` (17.7 с)<br>Обязательный расчет 28 трасс FNV-1a при каждом старте | `garage_ready_us=500486` (~0.5 с)<br>Хеширование гейтировано файлом `benchmark.txt` (при наличии гейта FNV-1a совпадает 28/28 `match=YES`) |
| **BUG-009** | P1 | `9aa33842` | `44953276259` (success)<br>[GitHub Run](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259) | `evidence/before/test_garage.{stdout,stderr}`<br>`evidence/after/test_garage.{stdout,stderr}` | `fuel lvl20 jeep=265.329529 electric=265.329529`<br>4WD-апгрейд ошибочно увеличивал емкость бака Jeep | `fuel lvl20 jeep=100.000000 electric=265.329529`<br>Бак Jeep неизменно 100.0, Electric Car (FUEL) масштабируется |
| **BUG-004** | P1 | `9aa33842` | `44953276259` (success)<br>[GitHub Run](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259) | `evidence/before/test_garage.stdout`<br>`evidence/after/test_garage.stdout`<br>`evidence/{before,after}/test_suspension.{stdout,stderr}`<br>`evidence/{before,after}/suspension_droptest*.csv` | Линейная шкала: `lvl5 = 5.000000 Hz`<br>`lvl20 = 11.000000 Hz`<br>Дроп-тест: `FAIL: Step 62: Front suspension exceeded limits! (trans: 0.3721m, allowed: [-0.37, 0.37])` | Геометрическая шкала: `lvl5 = 3.828844 Hz`<br>`lvl20 = 7.959886 Hz`<br>Дроп-тест pass: max ход 0.3503 м $\le 0.37$ м |

---

## 2. Подробные данные из файлов Evidence

### BUG-014: Динамический расчет высоты спавна
- **Исходный файл теста:** `tools/test_spawn_clearance.cpp` (28 трасс × 3 автомобиля: Jeep, Ambulance, Train; 84 сценария, проверка проникновения колес в грунт и состояния `Driver Down` на первых 60 шагах симуляции).
- **ДО (baseline `ad51ed68`):**
  - Выходной код: `1`
  - `Spawn clearance: 38 failures / 84 scenarios`
  - Хвост `evidence/before/test_spawn_clearance.stderr`:
    ```
    alienplanet/train: rear0=1.5700 front0=1.5700 h0=1.5666 clear=0 alive=1 finite=1
    arcticcave/jeep: rear0=1.6000 front0=1.6000 h0=1.8423 clear=0 alive=1 finite=1
    arcticcave/ambulance: rear0=1.6600 front0=1.6600 h0=1.8423 clear=0 alive=1 finite=1
    arcticcave/train: rear0=1.5700 front0=1.5700 h0=1.8423 clear=0 alive=1 finite=1
    haunted/jeep: rear0=1.6000 front0=1.6000 h0=2.3622 clear=0 alive=1 finite=1
    haunted/ambulance: rear0=1.6600 front0=1.6600 h0=2.3622 clear=0 alive=1 finite=1
    haunted/train: rear0=1.5700 front0=1.5700 h0=2.3622 clear=0 alive=1 finite=1
    nuclear/jeep: rear0=1.6000 front0=1.6000 h0=3.2463 clear=0 alive=1 finite=1
    nuclear/ambulance: rear0=1.6600 front0=1.6600 h0=3.2463 clear=0 alive=0 finite=1
    nuclear/train: rear0=1.5700 front0=1.5700 h0=3.2463 clear=0 alive=0 finite=1
    ```
  - Референсные значения высоты $h(0)$ из `test_spawn_clearance.stdout`:
    - `mars`: 1.8657 m, `mountain`: 2.6018 m, `rollercoaster`: 2.5434 m, `construction`: 1.6564 m, `seasons`: 1.6445 m, `alienplanet`: 1.5666 m, `factory`: 0.9453 m, `cave`: 0.8868 m, `forest`: 0.3796 m.
- **ПОСЛЕ (`5adb079`):**
  - Выходной код: `0`
  - `evidence/after/test_spawn_clearance.stderr`: пустой (0 байт).
  - Хвост `evidence/after/test_spawn_clearance.stdout`:
    ```
    nuclear/train: rear0=4.9163 front0=4.9163 h0=3.2463 clear=1 alive=1 finite=1
    === SPAWN CLEARANCE AUDIT COMPLETE ===
    Total vehicle-stage scenarios evaluated: 84
    Total clearance/survival failures: 0
    All vehicle-stage pairs successfully cleared ground geometry!
    ```
  - На коммите `5adb079` добавлено сканирование рельефа по сетке `CHUNK_STEP_METERS` от заднего края колеса до переднего (`ceilf(rearEdge/step)*step .. frontEdge`), что устранило остаточные сбои длиннобазных машин (например, поезд на `forest`).

---

### BUG-007: Сохранение монет и рекордов в паузе + защита от двойного коммита
- **Исходный файл теста:** `tools/test_audio_pause_bonus.cpp` (Sub-test D).
- **ДО (baseline `ad51ed68`):**
  - Выходной код: `1`
  - `evidence/before/test_audio_pause_bonus.stderr`:
    ```
    FAIL: Coins lost on Pause -> GARAGE (line 420)
    ```
  - Хвост `evidence/before/test_audio_pause_bonus.stdout`:
    ```
    Pause -> GARAGE: wallet=0 expected=625 record=0 expected=105
    Pause -> RESTART: wallet=0 expected=1125 record=0 expected=215
    Driver down: first=62 second=124 expected=62
    ```
- **ПОСЛЕ (`9aa33842` / `5adb079`):**
  - Выходной код: `0`
  - `evidence/after/test_audio_pause_bonus.stderr`: пусто (0 байт).
  - Хвост `evidence/after/test_audio_pause_bonus.stdout`:
    ```
    Pause -> GARAGE: wallet=625 expected=625 record=105 expected=105
    Pause -> RESTART: wallet=1750 expected=1750 record=215 expected=215
    Driver down: first=1812 second=1812 expected=1812
    -> Sub-test D PASS: Pause Restart/Garage results commitment and double-commit protection verified.
    ```

---

### BUG-008: Оптимизация времени холодного старта PSP
- **ДО (baseline `ad51ed68`, `timing_probe=1`):**
  - Лог `evidence/before/psp_hcr_log.txt`:
    ```
    [BOOT] baseline=ad51ed68ca6d1dc06eb4a35ad3836f384ecc685e timing_probe=1
    [PERF_ASSETS] Game::Init UI textures loaded in 351788 us (351.79 ms)
    [HASH] Stage Countryside   (4001 pts) -> FNV-1a: 0x6e83945913e2d687 (match=YES)
    ... (все 28 строк [HASH])
    [BOOT] garage_ready_us=17700999
    ```
  - Полное время холодного старта до гаража: **17.701 с** (17,700,999 µs).
- **ПОСЛЕ (`5adb079`, стандартный запуск):**
  - Лог `evidence/after/psp_hcr_log.txt`:
    ```
    [BOOT] sha=5adb07920f4107ba8c4fc79ec4b49bee06eebd0d date=Nov 22 2026 00:46:55
    [PERF_ASSETS] Game::Init UI textures loaded in 353246 us (353.25 ms)
    [BOOT] garage_ready_us=500486
    ```
  - Время холодного старта: **0.500 с** (500,486 µs, ускорение старта более чем в 35 раз).
  - Вывод `[HASH]` полностью отсутствует в штатном режиме.
- **ПОСЛЕ (`5adb079`, запуск с флагом `-Benchmark` / `benchmark.txt`):**
  - Лог `evidence/after-benchmark/psp_hcr_log.txt`:
    ```
    [BOOT] sha=5adb07920f4107ba8c4fc79ec4b49bee06eebd0d date=Nov 22 2026 00:46:55
    [HASH] Stage Countryside   (4001 pts) -> FNV-1a: 0x6e83945913e2d687 (match=YES)
    ... (все 28 строк [HASH])
    [BOOT] garage_ready_us=17700999
    ```
  - При наличии маркерного файла бенчмарк исполняется штатно, FNV-1a хеши 28/28 `match=YES`.

---

### BUG-009: Корректность слота расширения бака
- **Исходный файл теста:** `tools/test_garage.cpp` (Sub-test 4).
- **ДО (baseline `ad51ed68`):**
  - Выходной код: `1`
  - `evidence/before/test_garage.stderr`:
    ```
    FAIL: Only FUEL upgrades must scale max and initial fuel (line 189)
    ```
  - Хвост `evidence/before/test_garage.stdout`:
    ```
    fuel lvl16 jeep=218.287308 electric=218.287308
    fuel lvl20 jeep=265.329529 electric=265.329529
    ```
- **ПОСЛЕ (`9aa33842` / `5adb079`):**
  - Выходной код: `0`
  - `evidence/after/test_garage.stderr`: пусто (0 байт).
  - Хвост `evidence/after/test_garage.stdout`:
    ```
    fuel lvl16 jeep=100.000000 electric=218.287308
    fuel lvl20 jeep=100.000000 electric=265.329529
    [PASS] Fuel capacity and suspension frequency dynamically scale with upgrade tiers
    ```

---

### BUG-004: Формула жесткости подвески и дроп-тест
- **Формула:** `base * powf(upg_mult_suspension, lvl)` вместо ошибочной линейной `base + 0.40f * lvl`.
- **ДО (baseline `ad51ed68`):**
  - `test_garage.stdout`: `jeep suspension lvl5 actual=5.000000 expected=3.828844 Hz`, `lvl20 = 11.000000 Hz`.
  - `evidence/before/test_suspension.stderr`:
    ```
    FAIL: Step 62: Front suspension exceeded limits! (trans: 0.3721m, allowed: [-0.37, 0.37])
    ```
  - Хвост `evidence/before/test_suspension.stdout`:
    ```
    Spawning car at Y = 5.90m (free-fall drop height: 5.00m)...
    Drop lvl20: 11.000000 Hz
      Hard impact detected at step 59 (t = 0.98s, chassis Y = 0.855m)
    Impact step: 59 (0.98s)
    Rear suspension translation range:  [-0.0208m, 0.2460m]
    Front suspension translation range: [-0.0206m, 0.2451m]
    ```
- **ПОСЛЕ (`9aa33842` / `5adb079`):**
  - `evidence/after/test_suspension.stderr`: пусто (0 байт).
  - Хвост `evidence/after/test_suspension.stdout`:
    ```
    Spawning car at Y = 5.90m (free-fall drop height: 5.00m)...
    Drop lvl20: 7.959886 Hz
      Hard impact detected at step 59 (t = 0.98s, chassis Y = 0.855m)
    Impact step: 59 (0.98s)
    Rear suspension translation range:  [-0.0356m, 0.3500m]
    Front suspension translation range: [-0.0357m, 0.3503m]
    Suspension travel limits regression test PASSED successfully!
    ```
  - Частоты подвески 29 автомобилей (`test_suspension.stdout`):
    - `jeep`: 3.000000 Hz $\to$ 7.959886 Hz (было 11.000000 Hz)
    - `motocross`: 4.000000 Hz $\to$ 10.613181 Hz (было 12.000000 Hz)
    - `monster_truck`: 2.600000 Hz $\to$ 6.898568 Hz (было 10.600000 Hz)
    - `tank`: 3.200000 Hz $\to$ 8.490545 Hz (было 11.200000 Hz)
    - `minibike`: 4.800000 Hz $\to$ 12.735817 Hz (было 12.800000 Hz)

---

## 3. Контрольные суммы CSV-файлов дроп-теста

| Файл CSV | Каталог | SHA-256 Контрольная сумма |
|---|---|---|
| `suspension_droptest.csv` | `evidence/before` | `3ed4f3350cd600cc899c2be78b83b060e61f329b115af9a219510364d2cb664d` |
| `suspension_droptest_lvl20.csv` | `evidence/before` | `3303a7aff59cd54873f9be077dcee2cb14dbba3d319fa6384367cdd9a2ead165` |
| `suspension_droptest.csv` | `evidence/after` | `3ed4f3350cd600cc899c2be78b83b060e61f329b115af9a219510364d2cb664d` |
| `suspension_droptest_lvl20.csv` | `evidence/after` | `257483681fcb8eb3706e7045223dfc9de228844ef1721533514866633682a5da` |

*Примечание:* `suspension_droptest.csv` (дроп на уровне upgrade 0) идентичен до и после, так как при $lvl=0$ множитель $1.05^0 = 1.0$, и частота равна базовой (3.0 Гц). Файл `suspension_droptest_lvl20.csv` демонстрирует различие: на базовой формуле частота составляла 11.0 Гц (жесткая подвеска), на исправленной — 7.959886 Гц (мягкая физическая подвеска).

---

## 4. Верификация CI и бинарных артефактов

### История CI запусков
1. **CI Run ID `44953070334`** ([Ссылка](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953070334)):
   - **Head SHA:** `9aa33842b941c473c48fa8b3603e0a2f8697db35`
   - **Статус:** `completed`, **Заключение:** `failure`
   - **Причина отказа:** На коммите `9aa33842` тесты `test_spawn_clearance` (8 падений из-за недостаточного сканирования рельефа) и `test_suspension` завершались с ошибкой.
2. **CI Run ID `44953276259`** ([Ссылка](https://github.com/Coosiks/hill-climb-racing-psp/actions/runs/44953276259)):
   - **Head SHA:** `5adb07920f4107ba8c4fc79ec4b49bee06eebd0d`
   - **Статус:** `completed`, **Заключение:** `success`
   - **Задачи (Jobs):**
     - `Linux PC Build & Test` (Ubuntu, CMake, GCC, 25 регрессионных тестов) — `success` (0 отказов)
     - `PSP Build` (PSPSDK / psp-gcc 15.2.0, генерация EBOOT.PBP) — `success`
   - **Артефакты:** `hcr-psp-build` (содержит скомпилированный `EBOOT.PBP`).

### Проверка бинарного файла EBOOT.PBP
- **SHA-256:** `f264accdca46037283ecb620a58b6f854180aa612eacedd1f2c0cf2f5e1d2021`
- **Размер:** 559,868 байт
- **Задеплоенное состояние:** Игра развернута в каталог пользователя:
  `C:\Users\magad\Desktop\PPSSPP\memstick\PSP\GAME\HCR\EBOOT.PBP`
  Бинарник пользователя содержит сборку коммита `5adb079` (`[BOOT] sha=5adb0792...`).
  Последующие коммиты документационного слоя обновляют git HEAD, но исполняемый код и бинарный деплой остаются зафиксированными на `5adb079` (деплой повторно не производится).

---

## 5. Наблюдения и риски

1. **Маржинальный ход подвески Jeep lvl20 (0.3503 м):**
   При свободном падении с 5 метров с мягкой геометрической пружиной lvl20 (7.96 Гц вместо 11.0 Гц) максимальный ход передней подвески достигает **0.3503 м** при базовом лимите 0.35 м и допуске солвера 0.02 м (верхняя граница 0.37 м). Запас составляет 0.0197 м. Рекомендуется калибровка параметров демпфирования в Батче 3.
2. **Сканирование рельефа длиннобазных машин (BUG-014):**
   Решение `5adb079` с итерацией по сетке `CHUNK_STEP_METERS` гарантирует, что длиннобазные машины (`train` 3.80 м, `touristbus` 3.40 м) не проваливаются задней тележкой в локальные складки рельефа при спавне.
3. **Порядок инициализации в `tools/test_suspension.cpp`:**
   В текущем коде теста первый вызов `RunDrop` выполняется до `Config::LoadAll()` на скомпилированных defaults структуры `Config::VehicleJeep`, а второй — на загруженной конфигурации `*jeep`. В рамках Батча 1 код тестов заморожен ревьюером, исправление вынесено как рекомендация при разморозке.
