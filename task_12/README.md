# task_12 — Игла Буффона (визуализация + сравнение с точной вероятностью)

## Условие

На плоскость с параллельными прямыми, расстояние между соседними прямыми — `d`,
случайно бросается игла длины `L` (`L ≤ d`).

Найти вероятность, что игла пересечёт хотя бы одну прямую, и сравнить:
- точное значение;
- результат эксперимента (моделирование).

Дано (как в условии):
- расстояние `x` от середины иглы до ближайшей прямой: `x ~ U[0, d/2]`;
- угол `φ` между иглой и направлением прямых: `φ ~ U[0, π/2]`;
- `x` и `φ` независимы.

## Теория (точная вероятность при L ≤ d)

Игла пересекает ближайшую прямую тогда и только тогда, когда проекция половины иглы на перпендикуляр к прямым больше либо равна `x`:

`x ≤ (L/2) * sin(φ)`.

Тогда:

`P = 2L / (π d)`  (для `L ≤ d`).

## Что делает приложение (Qt)

- позволяет задавать `d` и `L` (проверка `L ≤ d`);
- запускает моделирование порциями (batch);
- рисует последние `N` игл на фоне параллельных прямых:
  - красные — пересекли прямую,
  - синие — не пересекли;
- показывает:
  - `P_exact`,
  - `P_emp`,
  - разницу `|P_emp - P_exact|`.

## Сборка и запуск на Windows (Qt 6.11 MinGW)

PowerShell:

```powershell
$env:PATH = "C:\Qt\6.11.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH

cmake -S "C:\CursWorkTV\task_12" -B "C:\CursWorkTV\task_12\build" -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build "C:\CursWorkTV\task_12\build"

& "C:\CursWorkTV\task_12\build\task_12.exe"
```

Перед повторной настройкой CMake удалите `build`, если папка создавалась из WSL (`/mnt/c/...`).

## Сборка в Ubuntu (WSL)

```bash
rm -rf /mnt/c/CursWorkTV/task_12/build
sudo apt update
sudo apt install -y cmake ninja-build qt6-base-dev g++
cmake -S /mnt/c/CursWorkTV/task_12 -B /mnt/c/CursWorkTV/task_12/build -G Ninja
cmake --build /mnt/c/CursWorkTV/task_12/build
```

Для запуска GUI в WSL нужен **WSLg** или X‑server. Для защиты удобнее собирать и запускать под Windows (блок выше).

