#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Задание 1. Перестановки и теорема сложения вероятностей (вариант 3)
// Полный перебор N! перестановок; проверка P(A_i ∪ A_j) = P(A_i)+P(A_j)-P(A_i∩A_j)
// Запуск: ./task_01 N [i j]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

using Permutation = std::vector<int>;

// --- Вспомогательные функции: факториал и форматирование ---
long double factorial(int n) {
    long double result = 1.0L;
    for (int k = 2; k <= n; ++k) {
        result *= static_cast<long double>(k);
    }
    return result;
}

std::string permutationToString(const Permutation& p) {
    std::ostringstream out;
    out << "[";
    for (std::size_t idx = 0; idx < p.size(); ++idx) {
        out << p[idx];
        if (idx + 1 < p.size()) {
            out << " ";
        }
    }
    out << "]";
    return out.str();
}

// --- Входные данные и разбор аргументов командной строки ---
struct InputData {
    int n = 0;
    int i = 1;
    int j = 2;
};

bool parsePositiveInt(const char* text, int& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const int parsed = std::stoi(s, &pos);
        if (pos != s.size() || parsed <= 0) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseArgs(int argc, char* argv[], InputData& input) {
    if (argc < 2 || argc > 4) {
        return false;
    }
    if (!parsePositiveInt(argv[1], input.n)) {
        return false;
    }
    if (argc >= 3 && !parsePositiveInt(argv[2], input.i)) {
        return false;
    }
    if (argc == 4 && !parsePositiveInt(argv[3], input.j)) {
        return false;
    }
    return true;
}

void printUsage(const char* exeName) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exeName << " N [i j]\n\n";
    std::cerr << "Where:\n";
    std::cerr << "  N - number of elements in the set (N >= 1)\n";
    std::cerr << "  i, j - event indices A_i and A_j (optional)\n";
    std::cerr << "If i and j are omitted, defaults are i=1 and j=2 (for N>=2).\n";
}

}  // конец анонимного пространства имён

// --- Точка входа: перебор перестановок и проверка теоремы ---
// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    InputData in;
    if (!parseArgs(argc, argv, in)) {
        printUsage(argv[0]);
        return 1;
    }

    if (in.n == 1) {
        in.i = 1;
        in.j = 1;
    } else {
        if (argc < 3) {
            in.i = 1;
        }
        if (argc < 4) {
            in.j = 2;
        }
    }

    if (in.i > in.n || in.j > in.n) {
        std::cerr << "Error: i and j must satisfy 1 <= i, j <= N.\n";
        return 1;
    }

    std::vector<int> base(in.n);
    std::iota(base.begin(), base.end(), 1);

    std::vector<Permutation> unionPermutations;
    std::vector<Permutation> intersectionPermutations;

    long long totalCount = 0;
    long long aiCount = 0;
    long long ajCount = 0;
    long long unionCount = 0;
    long long intersectionCount = 0;

    // Перебор всех N! перестановок
    do {
        ++totalCount;
        // A_i: элемент i на i-й позиции (индексация с 1)
        const bool ai = (base[in.i - 1] == in.i);
        const bool aj = (base[in.j - 1] == in.j);
        const bool uni = ai || aj;
        const bool inter = ai && aj;

        if (ai) {
            ++aiCount;
        }
        if (aj) {
            ++ajCount;
        }
        if (uni) {
            ++unionCount;
            unionPermutations.push_back(base);
        }
        if (inter) {
            ++intersectionCount;
            intersectionPermutations.push_back(base);
        }
    } while (std::next_permutation(base.begin(), base.end()));

    const long double total = static_cast<long double>(totalCount);
    const long double pAiDirect = static_cast<long double>(aiCount) / total;
    const long double pAjDirect = static_cast<long double>(ajCount) / total;
    const long double pUnionDirect = static_cast<long double>(unionCount) / total;
    const long double pIntersectionDirect = static_cast<long double>(intersectionCount) / total;

    // Аналитические вероятности через факториалы
    const long double nFact = factorial(in.n);
    const long double pAiAnalytical = factorial(in.n - 1) / nFact;
    const long double pAjAnalytical = factorial(in.n - 1) / nFact;

    long double pIntersectionAnalytical = 0.0L;
    if (in.i == in.j) {
        pIntersectionAnalytical = pAiAnalytical;
    } else if (in.n >= 2) {
        pIntersectionAnalytical = factorial(in.n - 2) / nFact;
    }

    const long double pUnionAnalytical =
        pAiAnalytical + pAjAnalytical - pIntersectionAnalytical;

    // Проверка теоремы сложения: левая и правая части
    const long double theoremLhs = pUnionDirect;
    const long double theoremRhs = pAiDirect + pAjDirect - pIntersectionDirect;

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "Input: N=" << in.n << ", i=" << in.i << ", j=" << in.j << "\n";
    std::cout << "Total permutations: " << totalCount << "\n\n";

    std::cout << "Favorable outcomes for A_i U A_j (count = " << unionCount << "):\n";
    for (const auto& p : unionPermutations) {
        std::cout << "  " << permutationToString(p) << "\n";
    }
    std::cout << "\n";

    std::cout << "Favorable outcomes for A_i ∩ A_j (count = " << intersectionCount << "):\n";
    for (const auto& p : intersectionPermutations) {
        std::cout << "  " << permutationToString(p) << "\n";
    }
    std::cout << "\n";

    std::cout << "Direct counting probabilities:\n";
    std::cout << "  P(A_i)           = " << pAiDirect << "  (" << aiCount << "/" << totalCount
              << ")\n";
    std::cout << "  P(A_j)           = " << pAjDirect << "  (" << ajCount << "/" << totalCount
              << ")\n";
    std::cout << "  P(A_i U A_j)     = " << pUnionDirect << "  (" << unionCount << "/" << totalCount
              << ")\n";
    std::cout << "  P(A_i ∩ A_j)     = " << pIntersectionDirect << "  (" << intersectionCount
              << "/" << totalCount << ")\n\n";

    std::cout << "Analytical probabilities:\n";
    std::cout << "  P(A_i)           = " << pAiAnalytical << "\n";
    std::cout << "  P(A_j)           = " << pAjAnalytical << "\n";
    std::cout << "  P(A_i ∩ A_j)     = " << pIntersectionAnalytical << "\n";
    std::cout << "  P(A_i U A_j)     = " << pUnionAnalytical << "\n\n";

    std::cout << "Probability addition theorem check:\n";
    std::cout << "  Left side  P(A_i U A_j)                 = " << theoremLhs << "\n";
    std::cout << "  Right side P(A_i)+P(A_j)-P(A_i ∩ A_j)   = " << theoremRhs << "\n";
    std::cout << "  |LHS - RHS| = " << std::fabs(theoremLhs - theoremRhs) << "\n";

    return 0;
}
