#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// =============================================================================
// Задание 7. Пересечение множеств и шифр Вернам (вариант 3)
// R(k,N), порог R>1/2; демонстрация коллизий при повторном XOR-ключе
// Запуск: ./task_07 [N] [trials] [k]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

struct InputData {
    std::uint64_t N = 65536;       // Размер универсума (2^16 by default)
    std::uint64_t trials = 20000;  // Число испытаний Монте-Карло
    std::uint64_t k = 0;           // 0 — вычислить порог автоматически
};

bool parseU64(const char* text, std::uint64_t& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const auto parsed = std::stoull(s, &pos);
        if (pos != s.size()) {
            return false;
        }
        value = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

void printUsage(const char* exeName) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exeName << " [N] [trials] [k]\n\n";
    std::cerr << "Defaults:\n";
    std::cerr << "  N=65536, trials=20000, k=auto (min k with R(k,N)>1/2)\n";
}

bool parseArgs(int argc, char* argv[], InputData& in) {
    if (argc > 4) {
        return false;
    }
    if (argc >= 2 && !parseU64(argv[1], in.N)) {
        return false;
    }
    if (argc >= 3 && !parseU64(argv[2], in.trials)) {
        return false;
    }
    if (argc == 4 && !parseU64(argv[3], in.k)) {
        return false;
    }
    return true;
}

// Точная вероятность P(X∩Y=∅) для двух случайных k-подмножеств
long double exactNoIntersectionProbability(std::uint64_t k, std::uint64_t N) {
    // P(пустое пересечение двух случайных k-подмножеств N-элементного универсума):
    // P0 = C(N-k,k)/C(N,k) — произведение (N-k-i)/(N-i)
    if (k == 0) {
        return 1.0L;
    }
    if (k > N) {
        return 0.0L;
    }
    if (2 * k > N) {
        return 0.0L;  // при 2k>N два k-подмножества не могут быть непересекающимися
    }
    long double p0 = 1.0L;
    for (std::uint64_t i = 0; i < k; ++i) {
        const long double num = static_cast<long double>(N - k - i);
        const long double den = static_cast<long double>(N - i);
        p0 *= (num / den);
    }
    return p0;
}

long double exactR(std::uint64_t k, std::uint64_t N) {
    return 1.0L - exactNoIntersectionProbability(k, N);
}

long double lowerBound(std::uint64_t k, std::uint64_t N) {
    // Нижняя оценка: R(k,N) ≥ 1 - exp(-k²/N)
    const long double x = -static_cast<long double>(k) * static_cast<long double>(k) /
                          static_cast<long double>(N);
    return 1.0L - std::exp(x);
}

std::uint64_t findMinKHalfExact(std::uint64_t N) {
    for (std::uint64_t k = 1; k <= N; ++k) {
        if (exactR(k, N) > 0.5L) {
            return k;
        }
    }
    return N;
}

std::uint64_t findMinKHalfBound(std::uint64_t N) {
    for (std::uint64_t k = 1; k <= N; ++k) {
        if (lowerBound(k, N) > 0.5L) {
            return k;
        }
    }
    return N;
}

std::vector<std::uint16_t> sampleUnique16(std::uint64_t k, std::mt19937_64& rng) {
    if (k > 65536ULL) {
        throw std::runtime_error("For Vernam demo, k must be <= 65536.");
    }
    std::vector<std::uint16_t> all(65536);
    for (std::uint32_t i = 0; i < 65536; ++i) {
        all[i] = static_cast<std::uint16_t>(i);
    }
    std::shuffle(all.begin(), all.end(), rng);
    all.resize(static_cast<std::size_t>(k));
    return all;
}

long double empiricalR(std::uint64_t k, std::uint64_t N, std::uint64_t trials,
                       std::mt19937_64& rng) {
    std::vector<std::uint64_t> universe(N);
    for (std::uint64_t i = 0; i < N; ++i) {
        universe[i] = i;
    }

    std::uint64_t hit = 0;
    for (std::uint64_t t = 0; t < trials; ++t) {
        std::shuffle(universe.begin(), universe.end(), rng);
        std::unordered_set<std::uint64_t> x;
        x.reserve(static_cast<std::size_t>(k * 2 + 1));
        for (std::uint64_t i = 0; i < k; ++i) {
            x.insert(universe[static_cast<std::size_t>(i)]);
        }

        std::shuffle(universe.begin(), universe.end(), rng);
        bool inter = false;
        for (std::uint64_t i = 0; i < k; ++i) {
            if (x.count(universe[static_cast<std::size_t>(i)]) != 0U) {
                inter = true;
                break;
            }
        }
        if (inter) {
            ++hit;
        }
    }

    return static_cast<long double>(hit) / static_cast<long double>(trials);
}

// Демонстрация Вернама: коллизии при одном ключе XOR
// Демонстрация Вернама: коллизии при одном ключе XOR
std::size_t vernamSingleCollisionCount(std::uint64_t k, std::mt19937_64& rng) {
    // Вернам для 16-битного сообщения: E_t(m) = m XOR t.
    // При повторном ключе t шифрование E_t — перестановка:
    // E_t(x1) = E_t(x2) <=> x1 = x2.
    // Коллизии шифротекстов соответствуют пересечению открытых текстов.
    const auto X1 = sampleUnique16(k, rng);
    const auto X2 = sampleUnique16(k, rng);

    std::uniform_int_distribution<std::uint32_t> keyDist(0, 65535);
    const std::uint16_t key = static_cast<std::uint16_t>(keyDist(rng));

    std::unordered_map<std::uint16_t, std::uint16_t> cToPlainX1;
    cToPlainX1.reserve(X1.size() * 2);
    for (const auto x : X1) {
        const std::uint16_t c = static_cast<std::uint16_t>(x ^ key);
        cToPlainX1[c] = x;
    }

    std::size_t collisions = 0;
    for (const auto x : X2) {
        const std::uint16_t c = static_cast<std::uint16_t>(x ^ key);
        const auto it = cToPlainX1.find(c);
        if (it != cToPlainX1.end()) {
            ++collisions;
        }
    }

    return collisions;
}

}  // конец анонимного пространства имён

// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    InputData in;
    if (!parseArgs(argc, argv, in)) {
        printUsage(argv[0]);
        return 1;
    }

    if (in.N == 0 || in.trials == 0) {
        std::cerr << "N and trials must be positive.\n";
        return 1;
    }

    const std::uint64_t kExactHalf = findMinKHalfExact(in.N);
    const std::uint64_t kBoundHalf = findMinKHalfBound(in.N);

    const std::uint64_t kUse = (in.k == 0 ? kExactHalf : in.k);
    if (kUse > in.N) {
        std::cerr << "k must satisfy k <= N.\n";
        return 1;
    }

    std::mt19937_64 rng(std::random_device{}());

    const long double rExact = exactR(kUse, in.N);
    const long double rLower = lowerBound(kUse, in.N);
    const long double rEmp = empiricalR(kUse, in.N, in.trials, rng);

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "Input: N=" << in.N << ", trials=" << in.trials << ", k=" << kUse << "\n\n";

    std::cout << "Core probability:\n";
    std::cout << "  R(k,N) = P(X ∩ Y != Ø), |X|=|Y|=k\n";
    std::cout << "  Exact R(k,N)                      = " << rExact << "\n";
    std::cout << "  Lower bound 1-exp(-k^2/N)         = " << rLower << "\n";
    std::cout << "  Empirical estimate (Monte-Carlo)  = " << rEmp << "\n";
    std::cout << "  Check exact >= bound: "
              << (rExact + 1e-15L >= rLower ? "true" : "false") << "\n\n";

    std::cout << "Threshold R(k,N) > 1/2:\n";
    std::cout << "  Minimal k (exact)   = " << kExactHalf << "\n";
    std::cout << "  Minimal k (by bound)= " << kBoundHalf
              << "  (sufficient condition)\n";
    std::cout << "  Approx sqrt(N ln 2) = "
              << std::sqrt(static_cast<long double>(in.N) * std::log(2.0L)) << "\n";

    if (in.N == 65536ULL) {
        std::cout << "\nFor Vernam (16-bit), N = 2^16 = 65536.\n";
        std::cout << "So k around sqrt(65536*ln2) is enough for probability > 1/2.\n";
    }

    // Vernam part is inherently 16-bit, keep k within range.
    const std::uint64_t kVernam = std::min<std::uint64_t>(kUse, 65536ULL);
    const std::size_t oneRunCollisions = vernamSingleCollisionCount(kVernam, rng);

    const std::uint64_t vernamTrials = 2000;
    std::uint64_t collisionEventCount = 0;
    long double avgCollisions = 0.0L;
    for (std::uint64_t t = 0; t < vernamTrials; ++t) {
        const std::size_t c = vernamSingleCollisionCount(kVernam, rng);
        avgCollisions += static_cast<long double>(c);
        if (c > 0) {
            ++collisionEventCount;
        }
    }
    avgCollisions /= static_cast<long double>(vernamTrials);
    const long double pCollisionEvent =
        static_cast<long double>(collisionEventCount) / static_cast<long double>(vernamTrials);

    std::cout << "\nVernam 16-bit demo (single reused key):\n";
    std::cout << "  one run: collisions in ciphertext sets = " << oneRunCollisions << "\n";
    std::cout << "  over " << vernamTrials
              << " runs: P(at least one collision) ~= " << pCollisionEvent << "\n";
    std::cout << "  average number of collisions ~= " << avgCollisions << "\n";
    std::cout << "  Interpretation: ciphertext collisions indicate plaintext overlaps,\n";
    std::cout << "  and this is exploitable when one key is reused.\n";

    return 0;
}
