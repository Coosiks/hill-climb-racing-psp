# hill-climb-racing-psp

[![CI Build](https://github.com/Coosiks/hill-climb-racing-psp/actions/workflows/build.yml/badge.svg)](https://github.com/Coosiks/hill-climb-racing-psp/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/Coosiks/hill-climb-racing-psp?include_prereleases&label=release)](https://github.com/Coosiks/hill-climb-racing-psp/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> [!WARNING]
> **BETA — РАННЯЯ СТАДИЯ РАЗРАБОТКИ (см. [docs/ROADMAP.md](docs/ROADMAP.md))**  
> Проект находится в активной фазе калибровки физики и архитектурной доработки.

---

## 1. О проекте

**hill-climb-racing-psp** — это открытая, фанатская **нативная C++ реимплементация** классической физической аркады *Hill Climb Racing* для консоли **Sony PlayStation Portable (PSP)** и **PC**.

> [!IMPORTANT]
> **Это НЕ .so-лоадер!** Код игры написан с нуля на чистом C++ с использованием физического движка Box2D 2.4.x и аппаратного 2D-рендера `sceGu`. Проект компилируется непосредственно под процессор MIPS Allegrex и архитектуру eDRAM PSP.

---

## 2. Текущее состояние (Beta v0.1.0-beta.1)

Актуальный статус разработки синхронизируется в файле [docs/STATUS.md](docs/STATUS.md):

### Что работает:
- Двойная сборка: нативная сборка для Linux PC (SDL2 + OpenGL) и для PSP (`EBOOT.PBP`).
- Точные 1:1 математические формулы высот оригинальной игры для 4 базовых трасс (**Countryside**, **Desert**, **Arctic**, **Moon**).
- Физика джипа: 6-вершинный полигон шасси, подвеска на двух `b2WheelJoint`, мотор с модуляцией оборотов, базовый контроль наклона в воздухе.
- Внешняя конфигурация параметров: все физические коэффициенты читаются из `config/*.cfg` без динамических аллокаций.
- Пайплайн подготовки ассетов `tools/convert_assets.py` (свиззлинг под видеопамять PSP).
- Защита от запуска без ассетов: понятный экран с подсказкой вместо падения.

### В разработке / запланировано:
- Статический пул аллокаций для Box2D (задача `T-010`).
- Чанковая система трассы на `b2ChainShape` (задача `T-020`).
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

В репозитории **нет и никогда не будет** оригинальных текстур, звуков или бинарников Fingersoft. Все ресурсы извлекаются пользователем из легально приобретённого APK оригинальной игры (поддерживаемая версия: **1.18.0** или **1.0.5**).

1. Поместите APK-файл в папку `original_apk/` (или распакуйте папку `assets/` из APK в корень проекта).
2. Запустите автоматический конвертер:
   ```bash
   python3 tools/convert_assets.py
   ```
   Скрипт сконвертирует текстуры в аппаратно-свиззленный формат PSP (POT, tiles $16\times 8$ байт) и подготовит файлы шрифтов и звука.

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
