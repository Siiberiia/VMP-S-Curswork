# task_11 — Класс дискретной случайной величины + GUI (Qt)

## Требования из задания

Реализовано:

- задание дискретной СВ в табличном виде (пары `значение — вероятность`);
- проверка корректности (вероятности `>= 0`, сумма `= 1`, одинаковые значения объединяются);
- умножение СВ на число `a`;
- сложение СВ (`Z = X + Y`);
- умножение СВ (`Z = X × Y`);
- вычисление `E`, `Var`, коэффициента асимметрии (skewness), коэффициента эксцесса (excess kurtosis);
- сериализация/десериализация в файл;
- отображение:
  - **закона распределения** (PMF столбиками),
  - **полилинии** PMF,
  - **функции распределения** (CDF ступеньками).

## Формулы (для отчёта)

Пусть `P(X=x_i)=p_i`, `∑ p_i = 1`.

- \( E[X] = \sum_i x_i p_i \)
- \( Var(X) = \sum_i (x_i - E[X])^2 p_i \)
- \( \gamma_1 = \\frac{\\mu_3}{\\sigma^3} \), где \(\\mu_3 = \sum_i (x_i-E[X])^3 p_i\), \(\\sigma^2 = Var(X)\)
- эксцесс: \( \\gamma_2 = \\frac{\\mu_4}{\\sigma^4} - 3 \), где \(\\mu_4 = \sum_i (x_i-E[X])^4 p_i\)

Для независимых `X` и `Y`:

- \( P(X+Y=z) = \sum_x P(X=x)P(Y=z-x) \) (свертка)
- \( P(X\\cdot Y=z) = \sum_{xy=z} P(X=x)P(Y=y) \)

## Формат файла (сериализация)

Текстовый формат:

```text
N
x0 p0
x1 p1
...
x{N-1} p{N-1}
```

При загрузке распределение нормализуется.

## Как пользоваться приложением

1. Введите распределения **X** и **Y** в таблицах (x, p).
2. Если сумма вероятностей не 1 — нажмите **Normalize**.
3. Выберите операцию:
   - `Z = X + Y`
   - `Z = X × Y`
   - `Z = a × X` (включится поле `a`)
4. Нажмите **«Вычислить Z…»**:
   - справа появится таблица `Z`;
   - появятся характеристики `E, Var, skewness, excess`;
   - ниже будет график (PMF bars / PMF polyline / CDF).
5. Можно сохранить `Z` в файл и загрузить распределение в `X` или `Y`.

## Сборка и запуск на Windows (Qt 6.11 MinGW)

PowerShell:

```powershell
$env:PATH = "C:\Qt\6.11.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH

cmake -S "C:\CursWorkTV\task_11" -B "C:\CursWorkTV\task_11\build" -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build "C:\CursWorkTV\task_11\build"

& "C:\CursWorkTV\task_11\build\task_11.exe"
```

Перед повторной настройкой CMake удалите `build`, если папка создавалась из WSL (`/mnt/c/...`).

## Сборка в Ubuntu (WSL)

```bash
rm -rf /mnt/c/CursWorkTV/task_11/build
sudo apt update
sudo apt install -y cmake ninja-build qt6-base-dev g++
cmake -S /mnt/c/CursWorkTV/task_11 -B /mnt/c/CursWorkTV/task_11/build -G Ninja
cmake --build /mnt/c/CursWorkTV/task_11/build
```

Примечание: для запуска GUI в WSL нужен **WSLg** или X‑server. Для защиты удобнее собирать и запускать под Windows (блок выше).

