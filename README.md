# Курсовая работа — общий указатель заданий

Корневая папка проектов: `C:\CursWorkTV`.

Ниже — краткое описание каждого задания, где лежит код и как его собирать/запускать.

- **Консольные задания (C++17)**: удобно собирать в **Ubuntu (WSL)** из пути `/mnt/c/CursWorkTV/...`.
- **Задания на Qt 6**: собирать в **Windows** (у вас установлен Qt **6.11.0**, комплект **mingw_64** и инструменты MinGW/CMake/Ninja из `C:\Qt\Tools\...`).

---

## Оглавление

| Папка | Тема | Тип |
|------|------|-----|
| `task_01` | Перестановки, теорема сложения вероятностей | консоль |
| `task_02` | Монета до двух одинаковых подряд | консоль |
| `task_03` | Завод: сборка/переработка/качество | консоль |
| `task_04` | Подбор ключа из связки | консоль |
| `task_05` | Случайное блуждание, JSON-конфиг | консоль |
| `task_06` | M-арное дерево, движение точки | Qt |
| `task_07` | Пересечение множеств, Вернам | консоль |
| `task_08` | Пьяный у обрыва, графики | Qt |
| `task_09` | Монеты в кошельке, условная вероятность | консоль |
| `task_10` | Дети в семье (Пуассон), условие «нет девочек» | консоль |
| `task_11` | Класс дискретной СВ + графики | Qt |
| `task_12` | Игла Буффона | Qt |
| `task_13` | Случайный граф, оценки характеристик | консоль |

Подробности по каждому заданию — в `README.md` внутри соответствующей папки.

---

## WSL: общий префикс пути

В Ubuntu:

```bash
cd /mnt/c/CursWorkTV/<task_XX>
```

---

## Шаблон сборки консольных задач (g++)

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o <имя_бинарника>
```

Имя бинарника в каждом задании совпадает с именем папки (`task_01`, `task_02`, …), если не указано иное.

---

## Шаблон сборки и запуска Qt-задач (Windows, PowerShell)

Собирать и запускать **только в Windows** (не смешивать с WSL в одной папке `build`).

```powershell
$env:PATH = "C:\Qt\6.11.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH

cmake -S "C:\CursWorkTV\<task_XX>" -B "C:\CursWorkTV\<task_XX>\build" -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build "C:\CursWorkTV\<task_XX>\build"

& "C:\CursWorkTV\<task_XX>\build\<task_XX>.exe"
```

`C:\Qt\6.11.0\mingw_64\bin` нужен при **запуске**: без него `.exe` сразу завершается (не находятся DLL Qt). Сообщение `Could NOT find WrapVulkanHeaders` при `cmake` можно игнорировать.

Если раньше запускали `cmake` из WSL — удалите папку `build` и настройте заново в PowerShell.

---

## `task_01` — перестановки и теорема сложения вероятностей

- **Папка**: `C:\CursWorkTV\task_01`
- **Сборка/запуск (WSL)**:

```bash
cd /mnt/c/CursWorkTV/task_01
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_01
./task_01 5 2 4
```

---

## `task_02` — монета до двух одинаковых подряд

- **Папка**: `C:\CursWorkTV\task_02`

```bash
cd /mnt/c/CursWorkTV/task_02
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_02
./task_02 5 300000
```

---

## `task_03` — завод (сборка/переработка/качество)

- **Папка**: `C:\CursWorkTV\task_03`
- **Выход**: файлы `times_L3.txt`, `times_L4.txt`, `times_L5.txt` в **текущей директории запуска**

```bash
cd /mnt/c/CursWorkTV/task_03
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_03
./task_03 2 1 2 2 2 1 1 10 10 1 1
```

---

## `task_04` — подбор ключа

- **Папка**: `C:\CursWorkTV\task_04`

```bash
cd /mnt/c/CursWorkTV/task_04
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_04
./task_04 5 20
```

---

## `task_05` — блуждание, параметры в JSON

- **Папка**: `C:\CursWorkTV\task_05`
- **Файл**: `config.json` рядом с бинарником (или путь аргументом)

```bash
cd /mnt/c/CursWorkTV/task_05
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_05
./task_05
```

---

## `task_06` — M-арное дерево (Qt)

- **Папка**: `C:\CursWorkTV\task_06`
- **Сборка**: см. раздел «Шаблон сборки Qt-задач», подставьте `task_06`.

---

## `task_07` — пересечение множеств и Вернам

- **Папка**: `C:\CursWorkTV\task_07`

```bash
cd /mnt/c/CursWorkTV/task_07
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_07
./task_07
```

---

## `task_08` — пьяный у обрыва (Qt)

- **Папка**: `C:\CursWorkTV\task_08`
- **Сборка**: Qt-шаблон, исполняемый файл `task_08.exe`.

---

## `task_09` — монеты, условная вероятность

- **Папка**: `C:\CursWorkTV\task_09`

```bash
cd /mnt/c/CursWorkTV/task_09
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_09
./task_09 10 2 500000
```

---

## `task_10` — Пуассон детей, «нет девочек»

- **Папка**: `C:\CursWorkTV\task_10`

```bash
cd /mnt/c/CursWorkTV/task_10
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_10
./task_10 2 800000
```

---

## `task_11` — дискретная СВ: операции и графики (Qt)

- **Папка**: `C:\CursWorkTV\task_11`
- **Сборка**: Qt-шаблон, `task_11.exe`.

---

## `task_12` — игла Буффона (Qt)

- **Папка**: `C:\CursWorkTV\task_12`
- **Сборка**: Qt-шаблон, `task_12.exe`.

---

## `task_13` — случайный граф, оценки характеристик

- **Папка**: `C:\CursWorkTV\task_13`

```bash
cd /mnt/c/CursWorkTV/task_13
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o task_13
./task_13 6 5000 0.35
```

---

## Примечания для защиты

- Для **Qt**-задач удобно иметь скриншоты интерфейса и 1–2 примера параметров (они же в локальных `README.md`).
- Для **консольных** задач сохраняйте в отчёт команды запуска и фрагменты вывода/сгенерированных файлов (например, `task_03` → `times_L5.txt`).
