# hill-climb-racing-psp

[![CI Build](https://github.com/Coosiks/hill-climb-racing-psp/actions/workflows/build.yml/badge.svg)](https://github.com/Coosiks/hill-climb-racing-psp/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/Coosiks/hill-climb-racing-psp?include_prereleases&label=release)](https://github.com/Coosiks/hill-climb-racing-psp/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> [!WARNING]
> **BETA — РАННЯЯ СТАДИЯ РАЗРАБОТКИ (см. [docs/ROADMAP.md](docs/ROADMAP.md))**  
> Проект находится в активной фазе калибровки физики и архитектурной доработки.

---

## 1. О проекте

**hill-climb-racing-psp** — это открытая, фанатская **RE-assisted нативная C++ реимплементация** классической физической аркады *Hill Climb Racing* для консоли **Sony PlayStation Portable (PSP)** и **PC**.

> [!IMPORTANT]
> **Это НЕ .so-лоадер!** Код игры написан на чистом C++ с использованием физического движка Box2D 2.3.x (с бэкпортом лимитов подвески `b2WheelJoint` из 2.4.1) и аппаратного 2D-рендера `sceGu`. Проект компилируется непосредственно под процессор MIPS Allegrex и архитектуру eDRAM PSP.

---

## 2. Текущее состояние (Beta v0.1.0-beta.2)

Актуальный статус разработки синхронизируется в файле [docs/STATUS.md](docs/STATUS.md):

### Что работает:
- **Двойная сборка:** нативная сборка для Linux PC (SDL2 + OpenGL) и для PSP (`EBOOT.PBP`).
- **4 играбельных автомобиля (Content Parity 1.30.0):**
  - **Hill Climber (Jeep):** классический полный привод (AWD), сбалансированная физика.
  - **Motocross Bike:** задний привод (RWD), облегчённый вес, тюнинг Air Control в воздухе.
  - **Monster Truck:** полный привод (AWD), высокий крутящий момент, оверсайз-колёса ($r = 0.65$ м).
  - **Race Car:** задний привод (RWD), асимметричные колёса, аэродинамическое антикрыло Downforce ($F_y \le 150.0$ Н).
- **3-вкладочный Гараж и экономика:** карусель трасс, машин и тюнинга; квадратичная кривая цен апгрейдов $C(lvl) = 1000 \cdot (lvl+1)^2$; lazy loading текстур ($<0.2$ МБ footprint) и гвардинг старта заезда на заблокированном контенте.
- **Надёжные бинарные сейвы (`HCRS`):** 4-байтное естественное выравнивание под MIPS Allegrex, CRC32-хеширование целостности, автоматический failover на резервную копию `.bak` и миграция из старых сейвов `'HCR1'`.
- **Физика шеи водителя и Driver Down:** динамическая голова водителя на шарнире `b2RevoluteJoint` с возвращающей PD-пружиной и лимитами $[-0.60, +0.60]$ рад; ослабление мотора при аварии для ragdoll-эффекта; мгновенный Game Over при ударе головы о землю с иммунитетом к сенсорам монет.
- **Полная система трюковых бонусов:** начисление монет и показ анимированных баннеров за **Air Time** ($\ge 1.0$ с), **Wheelie** ($\ge 1.5$ с на заднем колесе), **Backflip** и **Frontflip**.
- **Точные 1:1 высоты 4 базовых трасс:** Countryside, Desert, Arctic, Moon со 100% кросс-платформенным детерминизмом хешей высот между PC и PSP.
- **Амортизированная чанковая трасса:** генерация по 8 вершин за шаг физики исключает просадки кадров на PSP-1000.
- **Свиззленный пайплайн текстур:** нулевой оверхед декодирования PNG в рантайме PSP, бинарный формат `.tex` (RGBA8888 для кузова/колёс, RGBA5551 для UI).
- **14 автоматических регрессионных тестов в CI:** 100% зелёное покрытие физики, экономики, сейвов и детерминизма.

### В разработке / запланировано:
- Статический пул аллокаций для Box2D (задача `T-010`).
- Динамический подвесной мост из досок на joints (задача `T-022`).
- Аппаратный звуковой синтез мотора через `sceSas` (задача `T-043`).
- Батчинг квадов в рендере (задача `T-040`).

---

## 3. Сборка проекта

### 3.1 Сборка под PC (Linux / WSL2)
Требуются: CMake $\ge 3.10$, GCC/Clang (C++11), `libsdl2-dev`, `libgl-dev`.

```bash
# Установка зависимостей (Ubuntu/Debian)
sudo apt-get update && sudo apt-get install -y build-essential cmake libsdl2-dev libgl-dev

# Сборка
cmake -B build-pc -S .
cmake --build build-pc -j$(nproc)

# Запуск
./build-pc/hcr_pc
```

### 3.2 Сборка под PSP (PSPSDK)
Требуется установленный тулчейн `pspdev` (`psp-g++`, `pack-pbp`, `mksfoex`). Проверенная версия: `pspdev 2024-latest`.

```bash
# Сборка с помощью скрипта
./build.sh

# Или через make
make -j$(nproc)
```
Результат сборки: `EBOOT.PBP` (готовый к запуску на CFW или в PPSSPP).

---

## 4. Подготовка ассетов («Принеси свой APK»)

В репозитории **нет и никогда не будет** оригинальных текстур, звуков или бинарников Fingersoft. Все ресурсы извлекаются пользователем из легально приобретённого APK оригинальной игры (поддерживаемые версии: **1.30.0** и **1.0.5**).

1. Получите оригинальный APK файл.
2. Запустите автоматический конвертер:
   ```bash
   python3 tools/convert_assets.py --apk <path_to_apk> --out assets
   ```
   Скрипт автоматически классифицирует текстуры (RGBA8888 для машин/колёс, RGBA5551 для UI), выполнит аппаратный свиззлинг для PSP и упакует шрифты и звуки.

---

## 5. Установка на PSP и запуск в PPSSPP

### 5.1 Реальная PSP (PSP-1000, 2000, 3000, Street, Go)
1. Подключите PSP к компьютеру через USB.
2. Откройте карту памяти: `ms0:/PSP/GAME/`.
3. Создайте каталог `HCR`: `ms0:/PSP/GAME/HCR/`.
4. Скопируйте туда файлы:
   ```
   ms0:/PSP/GAME/HCR/
   ├── EBOOT.PBP
   ├── config/
   │   ├── physics.cfg
   │   ├── vehicles.cfg
   │   └── stages.cfg
   └── assets/        <- папка, полученная после tools/convert_assets.py
   ```
5. Запустите игру из меню XMB: **Game -> Memory Stick -> Hill Climb Racing**.

### 5.2 Эмулятор PPSSPP
1. Откройте эмулятор PPSSPP.
2. Выберите **Load...** и укажите путь к `EBOOT.PBP`.

### 5.3 Управление

| Действие | Кнопка PSP | Клавиатура PC |
| :--- | :--- | :--- |
| **Газ / Наклон назад в воздухе** | `R-Trigger` или `Cross` | `Right Arrow` / `D` / `Space` |
| **Тормоз / Наклон вперёд в воздухе** | `L-Trigger` или `Square` | `Left Arrow` / `A` / `Q` |
| **Старт / Выбор в меню** | `Cross` или `Start` | `Space` / `Enter` |
| **Назад / Пауза** | `Circle` или `Start` | `Escape` / `P` |
| **Апгрейд Engine / Susp / Tires / 4WD** | `Triangle` / `Square` / `Circle` / `Select` | Клавиши `1`, `2`, `3`, `4` |

---

## 6. Участие в разработке и ревью

- Подробный план задач со статусами: [docs/ROADMAP.md](docs/ROADMAP.md).
- Архитектурное описание: [docs/architecture.md](docs/architecture.md).
- Отчёты по выполненным задачам: `docs/agent_reports/T-xxx.md`.
- Для отправки предложений используйте pull request'ы по шаблону [.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md).
- О багах сообщайте в Issues по шаблону [.github/ISSUE_TEMPLATE/bug.md](.github/ISSUE_TEMPLATE/bug.md).

---

## 7. Credits & Благодарности

- **Fingersoft Ltd.** — за создание культовой оригинальной игры Hill Climb Racing.
- **PS Vita Port (memory-hunter)** — за ценный референс адаптации ввода и структуры сейвов.
- **Erin Catto & Box2D Community** — за превосходный 2D-физический движок.
- **pspdev / PSPSDK Community** — за поддержку и развитие открытого тулчейна для PSP.

---

## 8. Legal Disclaimer

Данный проект является независимой, некоммерческой фанатской разработкой, созданной исключительно в образовательных целях и в целях сохранения платформы PlayStation Portable. Все торговые марки, названия персонажей и графические решения принадлежат **Fingersoft Ltd.** Исходный код распространяется под лицензией [MIT](LICENSE).
