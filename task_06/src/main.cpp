#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// =============================================================================
// Задание 6. Движение точки по M-арному дереву (Qt 6, вариант 3)
// Монте-Карло: поглощение, достижение листа, визуализация пути (QPainter)
// Сборка: CMake + Qt Widgets (Windows)
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// Параметры одного эксперимента на M-арном дереве
struct SimConfig {
    int M = 2;          // ветвление M
    int depth = 5;      // корень на уровне 0, листья на depth
    int maxVertices = 600;  // ограничение для отрисовки

    double pStopDefault = 0.0;  // если список p_stop пуст
    std::vector<double> pStopByLevel;  // по уровням 0..depth-1; на листе поглощения нет

    enum class MoveLaw { Uniform, BiasToZero };
    MoveLaw law = MoveLaw::Uniform;
    double biasAlpha = 0.7;  // для закона с перекосом в потомка 0, probability of child 0; rest split equally

    std::uint64_t trials = 20000;
    int targetLeafId = 0;  // диапазон номеров листьев 0..M^depth-1
};

// Результат одного прогона точки по дереву
struct SimResult {
    // pathDigits — индексы выбранных потомков на пути к листу
    std::vector<int> pathDigits;
    bool reachedLeaf = false;
    bool stoppedInternal = false; // поглощение во внутреннем узле

    int visitedVertices = 1; // включая корень
    int leafIdIfReached = -1;
};

std::optional<std::vector<double>> parseProbList(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        return std::vector<double>{};
    }
    std::vector<double> out;
    const auto parts = t.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    for (const auto& p : parts) {
        bool ok = false;
        const double v = p.toDouble(&ok);
        if (!ok || v < 0.0 || v > 1.0) {
            return std::nullopt;
        }
        out.push_back(v);
    }
    return out;
}

std::uint64_t ipowU64(std::uint64_t a, int e) {
    std::uint64_t r = 1;
    for (int i = 0; i < e; ++i) {
        if (a != 0 && r > (std::numeric_limits<std::uint64_t>::max() / a)) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        r *= a;
    }
    return r;
}

std::vector<int> leafIdToDigits(std::uint64_t id, int M, int depth) {
    std::vector<int> digits(depth, 0);
    for (int i = depth - 1; i >= 0; --i) {
        digits[static_cast<std::size_t>(i)] = static_cast<int>(id % static_cast<std::uint64_t>(M));
        id /= static_cast<std::uint64_t>(M);
    }
    return digits;
}

int digitsToLeafId(const std::vector<int>& digits, int M) {
    std::uint64_t id = 0;
    for (int d : digits) {
        id = id * static_cast<std::uint64_t>(M) + static_cast<std::uint64_t>(d);
        if (id > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return -1;
        }
    }
    return static_cast<int>(id);
}

double pStopAtLevel(const SimConfig& cfg, int level) {
    if (level < 0 || level >= cfg.depth) {
        return 0.0;
    }
    if (static_cast<std::size_t>(level) < cfg.pStopByLevel.size()) {
        return cfg.pStopByLevel[static_cast<std::size_t>(level)];
    }
    return cfg.pStopDefault;
}

int sampleChild(const SimConfig& cfg, std::mt19937_64& rng) {
    if (cfg.M <= 1) {
        return 0;
    }
    if (cfg.law == SimConfig::MoveLaw::Uniform) {
        std::uniform_int_distribution<int> dist(0, cfg.M - 1);
        return dist(rng);
    }
    // закон с перекосом в потомка 0
    const double alpha = std::clamp(cfg.biasAlpha, 0.0, 1.0);
    std::bernoulli_distribution pickZero(alpha);
    if (pickZero(rng)) {
        return 0;
    }
    std::uniform_int_distribution<int> dist(1, cfg.M - 1);
    return dist(rng);
}

// Один спуск точки от корня к листу или поглощению
SimResult runSingle(const SimConfig& cfg, std::mt19937_64& rng) {
    SimResult r;
    r.pathDigits.clear();
    r.reachedLeaf = false;
    r.stoppedInternal = false;
    r.visitedVertices = 1;

    for (int level = 0; level < cfg.depth; ++level) {
        const double ps = std::clamp(pStopAtLevel(cfg, level), 0.0, 1.0);
        std::bernoulli_distribution stop(ps);
        if (stop(rng)) {
            r.stoppedInternal = true;
            return r;
        }

        const int child = sampleChild(cfg, rng);
        r.pathDigits.push_back(child);
        ++r.visitedVertices;
    }

    r.reachedLeaf = true;
    r.leafIdIfReached = digitsToLeafId(r.pathDigits, cfg.M);
    return r;
}

struct BatchStats {
    long double pHitTargetLeaf = 0.0L;
    long double pReachAnyLeaf = 0.0L;
    long double pStopInternal = 0.0L;
    std::vector<long double> pVisitedLen; // index = visitedVertices (1..depth+1)
};

BatchStats runBatch(const SimConfig& cfg, std::mt19937_64& rng) {
    BatchStats st;
    st.pVisitedLen.assign(static_cast<std::size_t>(cfg.depth + 2), 0.0L);

    std::uint64_t hitTarget = 0;
    std::uint64_t reachLeaf = 0;
    std::uint64_t stopInternal = 0;
    std::vector<std::uint64_t> lenCount(static_cast<std::size_t>(cfg.depth + 2), 0ULL);

    for (std::uint64_t t = 0; t < cfg.trials; ++t) {
        const SimResult r = runSingle(cfg, rng);
        if (r.reachedLeaf) {
            ++reachLeaf;
            if (r.leafIdIfReached == cfg.targetLeafId) {
                ++hitTarget;
            }
        }
        if (r.stoppedInternal) {
            ++stopInternal;
        }
        if (r.visitedVertices >= 1 && r.visitedVertices <= cfg.depth + 1) {
            ++lenCount[static_cast<std::size_t>(r.visitedVertices)];
        }
    }

    const long double T = static_cast<long double>(cfg.trials);
    st.pHitTargetLeaf = static_cast<long double>(hitTarget) / T;
    st.pReachAnyLeaf = static_cast<long double>(reachLeaf) / T;
    st.pStopInternal = static_cast<long double>(stopInternal) / T;
    for (int l = 1; l <= cfg.depth + 1; ++l) {
        st.pVisitedLen[static_cast<std::size_t>(l)] =
            static_cast<long double>(lenCount[static_cast<std::size_t>(l)]) / T;
    }
    return st;
}

// Виджет отрисовки M-арного дерева и пути точки
class TreeView final : public QWidget {
public:
    void setModel(int M, int depth, const std::vector<int>& targetDigits,
                  const std::vector<int>& currentPath) {
        M_ = M;
        depth_ = depth;
        targetDigits_ = targetDigits;
        currentPath_ = currentPath;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        // Масштаб виджета: ширина ~ M^depth, высота ~ число уровней
        const int levels = depth_ + 1;
        const int h = 120 + levels * 90;
        std::uint64_t nodesLast = ipowU64(static_cast<std::uint64_t>(std::max(1, M_)), depth_);
        nodesLast = std::min<std::uint64_t>(nodesLast, 512ULL);
        const int w = 160 + static_cast<int>(nodesLast) * 30;
        return QSize(std::clamp(w, 500, 2400), std::clamp(h, 500, 1800));
    }

protected:
    // Отрисовка на виджете (QPainter)
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(250, 250, 250));

        if (M_ < 1 || depth_ < 0) {
            return;
        }

        const int margin = 40;
        const int levelH = 85;
        const int nodeR = 10;

        // Координаты узлов по уровням (с ограничением для больших деревьев)
        std::vector<std::vector<QPointF>> pos(static_cast<std::size_t>(depth_ + 1));
        std::vector<std::vector<std::uint64_t>> ids(static_cast<std::size_t>(depth_ + 1));

        for (int lvl = 0; lvl <= depth_; ++lvl) {
            std::uint64_t count = ipowU64(static_cast<std::uint64_t>(std::max(1, M_)), lvl);
            if (count > static_cast<std::uint64_t>(maxNodesPerLevel_)) {
                count = maxNodesPerLevel_;
            }
            pos[static_cast<std::size_t>(lvl)].resize(static_cast<std::size_t>(count));
            ids[static_cast<std::size_t>(lvl)].resize(static_cast<std::size_t>(count));
            for (std::uint64_t i = 0; i < count; ++i) {
                ids[static_cast<std::size_t>(lvl)][static_cast<std::size_t>(i)] = i;
            }
        }

        const int plotW = width() - 2 * margin;
        for (int lvl = 0; lvl <= depth_; ++lvl) {
            const auto& level = pos[static_cast<std::size_t>(lvl)];
            const int y = margin + lvl * levelH;
            const int n = static_cast<int>(level.size());
            for (int i = 0; i < n; ++i) {
                const double t = (n == 1) ? 0.5 : (static_cast<double>(i) / static_cast<double>(n - 1));
                const double x = margin + t * static_cast<double>(plotW);
                pos[static_cast<std::size_t>(lvl)][static_cast<std::size_t>(i)] = QPointF(x, y);
            }
        }

        auto isOnPath = [&](const std::vector<int>& digits, int lvl, std::uint64_t idx) -> bool {
            if (lvl == 0) {
                return idx == 0;
            }
            if (static_cast<int>(digits.size()) < lvl) {
                return false;
            }
            std::uint64_t cur = 0;
            for (int i = 0; i < lvl; ++i) {
                cur = cur * static_cast<std::uint64_t>(M_) + static_cast<std::uint64_t>(digits[static_cast<std::size_t>(i)]);
            }
            return cur == idx;
        };

        // Рёбра дерева (если размер допустим)
        for (int lvl = 0; lvl < depth_; ++lvl) {
            const auto& levelPos = pos[static_cast<std::size_t>(lvl)];
            const auto& nextPos = pos[static_cast<std::size_t>(lvl + 1)];
            const std::uint64_t count = levelPos.size();
            const std::uint64_t nextCount = nextPos.size();
            if (count == 0 || nextCount == 0) {
                continue;
            }

            for (std::uint64_t i = 0; i < count; ++i) {
                for (int c = 0; c < M_; ++c) {
                    std::uint64_t childIdx = i * static_cast<std::uint64_t>(M_) + static_cast<std::uint64_t>(c);
                    if (childIdx >= nextCount) {
                        continue;
                    }
                    const bool onTarget = isOnPath(targetDigits_, lvl, i) &&
                                          isOnPath(targetDigits_, lvl + 1, childIdx);
                    const bool onCurrent = isOnPath(currentPath_, lvl, i) &&
                                           isOnPath(currentPath_, lvl + 1, childIdx);

                    QPen pen(QColor(180, 180, 180), 1);
                    if (onTarget) {
                        pen = QPen(QColor(0, 140, 0), 2);
                    }
                    if (onCurrent) {
                        pen = QPen(QColor(30, 100, 200), 3);
                    }
                    p.setPen(pen);
                    p.drawLine(levelPos[static_cast<std::size_t>(i)],
                               nextPos[static_cast<std::size_t>(childIdx)]);
                }
            }
        }

        // Отрисовка узлов
        for (int lvl = 0; lvl <= depth_; ++lvl) {
            const auto& levelPos = pos[static_cast<std::size_t>(lvl)];
            for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(levelPos.size()); ++i) {
                const bool onTarget = isOnPath(targetDigits_, lvl, i);
                const bool onCurrent = isOnPath(currentPath_, lvl, i);

                QColor fill(255, 255, 255);
                QPen pen(QColor(120, 120, 120), 1);
                if (onTarget) {
                    fill = QColor(230, 255, 230);
                    pen = QPen(QColor(0, 140, 0), 2);
                }
                if (onCurrent) {
                    fill = QColor(230, 240, 255);
                    pen = QPen(QColor(30, 100, 200), 3);
                }
                p.setPen(pen);
                p.setBrush(fill);
                p.drawEllipse(levelPos[static_cast<std::size_t>(i)], nodeR, nodeR);
            }
        }

        p.setPen(Qt::black);
        p.drawText(10, 20, QString("Идеальное M-арное дерево: M=%1, глубина=%2").arg(M_).arg(depth_));
        p.setPen(QColor(0, 140, 0));
        p.drawText(10, 40, QString("Зелёный: целевой лист (leafId)"));
        p.setPen(QColor(30, 100, 200));
        p.drawText(10, 60, QString("Синий: путь одного эксперимента"));
    }

private:
    int M_ = 2;
    int depth_ = 5;
    int maxNodesPerLevel_ = 256;
    std::vector<int> targetDigits_;
    std::vector<int> currentPath_;
};

// Главное окно приложения
class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QStringLiteral("Задача 6 — движение по M-арному дереву"));
        resize(1100, 760);

        auto* central = new QWidget(this);
        setCentralWidget(central);
        auto* root = new QHBoxLayout(central);

        auto* left = new QWidget(this);
        auto* leftLayout = new QVBoxLayout(left);

        auto* paramsBox = new QGroupBox(QStringLiteral("Параметры"), this);
        auto* form = new QFormLayout(paramsBox);

        spinM_ = new QSpinBox(this);
        spinM_->setRange(1, 8);
        spinM_->setValue(2);
        form->addRow(QStringLiteral("M (потомков у узла):"), spinM_);

        spinDepth_ = new QSpinBox(this);
        spinDepth_->setRange(1, 10);
        spinDepth_->setValue(5);
        form->addRow(QStringLiteral("Глубина дерева:"), spinDepth_);

        spinLeafId_ = new QSpinBox(this);
        spinLeafId_->setRange(0, 1000000);
        spinLeafId_->setValue(0);
        form->addRow(QStringLiteral("Целевой лист (leafId):"), spinLeafId_);

        editLeafDigits_ = new QLineEdit(this);
        editLeafDigits_->setReadOnly(true);
        form->addRow(QStringLiteral("Лист в base-M (цифры):"), editLeafDigits_);

        editPStop_ = new QLineEdit(this);
        editPStop_->setPlaceholderText(QStringLiteral("например: 0.1 0.05 0.02 (по уровням 0..depth-1)"));
        form->addRow(QStringLiteral("p(остаться) по уровням:"), editPStop_);

        spinPStopDefault_ = new QDoubleSpinBox(this);
        spinPStopDefault_->setRange(0.0, 1.0);
        spinPStopDefault_->setDecimals(4);
        spinPStopDefault_->setSingleStep(0.01);
        spinPStopDefault_->setValue(0.0);
        form->addRow(QStringLiteral("p(остаться) по умолчанию:"), spinPStopDefault_);

        comboLaw_ = new QComboBox(this);
        comboLaw_->addItem(QStringLiteral("Равномерно по потомкам"));
        comboLaw_->addItem(QStringLiteral("С перекосом в 0-го потомка"));
        form->addRow(QStringLiteral("Закон движения:"), comboLaw_);

        spinAlpha_ = new QDoubleSpinBox(this);
        spinAlpha_->setRange(0.0, 1.0);
        spinAlpha_->setDecimals(4);
        spinAlpha_->setSingleStep(0.05);
        spinAlpha_->setValue(0.7);
        form->addRow(QStringLiteral("alpha (для перекоса):"), spinAlpha_);

        spinTrials_ = new QSpinBox(this);
        spinTrials_->setRange(100, 5000000);
        spinTrials_->setValue(20000);
        spinTrials_->setSingleStep(1000);
        form->addRow(QStringLiteral("Экспериментов (trials):"), spinTrials_);

        btnOne_ = new QPushButton(QStringLiteral("Один эксперимент (показать путь)"), this);
        btnBatch_ = new QPushButton(QStringLiteral("Серия (оценить вероятности)"), this);

        leftLayout->addWidget(paramsBox);
        leftLayout->addWidget(btnOne_);
        leftLayout->addWidget(btnBatch_);

        resultLabel_ = new QLabel(QStringLiteral("Готово."), this);
        resultLabel_->setWordWrap(true);
        leftLayout->addWidget(resultLabel_, 1);

        leftLayout->addStretch(1);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        treeView_ = new TreeView();
        scroll->setWidget(treeView_);

        root->addWidget(left, 0);
        root->addWidget(scroll, 1);

        connect(spinM_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncLeafUI(); });
        connect(spinDepth_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncLeafUI(); });
        connect(spinLeafId_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncLeafUI(); });
        connect(comboLaw_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { syncLawUI(); });
        connect(btnOne_, &QPushButton::clicked, this, [this] { runOne(); });
        connect(btnBatch_, &QPushButton::clicked, this, [this] { runBatchUI(); });

        syncLawUI();
        syncLeafUI();
    }

private:
    void syncLawUI() {
        const bool bias = (comboLaw_->currentIndex() == 1);
        spinAlpha_->setEnabled(bias);
    }

    void syncLeafUI() {
        const int M = spinM_->value();
        const int depth = spinDepth_->value();
        const std::uint64_t leafCount = ipowU64(static_cast<std::uint64_t>(M), depth);
        const int maxId = (leafCount > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                              ? std::numeric_limits<int>::max()
                              : static_cast<int>(leafCount - 1);
        spinLeafId_->setRange(0, std::max(0, maxId));
        const int leafId = std::min(spinLeafId_->value(), maxId);
        spinLeafId_->setValue(leafId);

        const auto digits = leafIdToDigits(static_cast<std::uint64_t>(leafId), M, depth);
        QString s;
        for (int d : digits) {
            s += QString::number(d);
            s += " ";
        }
        editLeafDigits_->setText(s.trimmed());

        treeView_->setModel(M, depth, digits, {});
    }

    std::optional<SimConfig> buildConfig() const {
        SimConfig cfg;
        cfg.M = spinM_->value();
        cfg.depth = spinDepth_->value();
        cfg.pStopDefault = spinPStopDefault_->value();
        cfg.biasAlpha = spinAlpha_->value();
        cfg.trials = static_cast<std::uint64_t>(spinTrials_->value());
        cfg.targetLeafId = spinLeafId_->value();
        cfg.law = (comboLaw_->currentIndex() == 1) ? SimConfig::MoveLaw::BiasToZero
                                                   : SimConfig::MoveLaw::Uniform;

        auto pListOpt = parseProbList(editPStop_->text());
        if (!pListOpt.has_value()) {
            return std::nullopt;
        }
        cfg.pStopByLevel = std::move(pListOpt.value());

        // Ограничение параметров для отрисовки
        const std::uint64_t totalNodesUpper = (cfg.M == 1)
            ? static_cast<std::uint64_t>(cfg.depth + 1)
            : (ipowU64(static_cast<std::uint64_t>(cfg.M), cfg.depth + 1) - 1) /
              static_cast<std::uint64_t>(cfg.M - 1);
        if (totalNodesUpper > 8000ULL) {
            QMessageBox::warning(
                const_cast<MainWindow*>(this),
                QStringLiteral("Слишком большое дерево"),
                QStringLiteral("Для визуализации выбери меньше M и/или глубину (сейчас узлов порядка %1).")
                    .arg(QString::number(static_cast<qulonglong>(totalNodesUpper))));
        }

        return cfg;
    }

    void runOne() {
        auto cfgOpt = buildConfig();
        if (!cfgOpt.has_value()) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Неверный список p(остаться)."));
            return;
        }
        SimConfig cfg = cfgOpt.value();

        std::mt19937_64 rng(std::random_device{}());
        const SimResult r = runSingle(cfg, rng);
        const auto targetDigits = leafIdToDigits(static_cast<std::uint64_t>(cfg.targetLeafId), cfg.M, cfg.depth);
        treeView_->setModel(cfg.M, cfg.depth, targetDigits, r.pathDigits);

        QString outcome;
        if (r.reachedLeaf) {
            outcome = QStringLiteral("Дошёл до листа. leafId=%1.").arg(r.leafIdIfReached);
        } else if (r.stoppedInternal) {
            outcome = QStringLiteral("Остановился во внутреннем узле (поглощение).");
        } else {
            outcome = QStringLiteral("Неожиданный исход.");
        }

        QString pathStr;
        for (int d : r.pathDigits) {
            pathStr += QString::number(d) + " ";
        }
        pathStr = pathStr.trimmed();

        resultLabel_->setText(
            QStringLiteral("Один эксперимент:\n- %1\n- Посещено вершин: %2 (включая корень)\n- Путь (цифры 0..M-1): %3")
                .arg(outcome)
                .arg(r.visitedVertices)
                .arg(pathStr.isEmpty() ? QStringLiteral("(пусто)") : pathStr));
    }

    void runBatchUI() {
        auto cfgOpt = buildConfig();
        if (!cfgOpt.has_value()) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Неверный список p(остаться)."));
            return;
        }
        SimConfig cfg = cfgOpt.value();

        std::mt19937_64 rng(std::random_device{}());
        const BatchStats st = runBatch(cfg, rng);
        const auto targetDigits = leafIdToDigits(static_cast<std::uint64_t>(cfg.targetLeafId), cfg.M, cfg.depth);
        treeView_->setModel(cfg.M, cfg.depth, targetDigits, {});

        QString lens;
        for (int l = 1; l <= cfg.depth + 1; ++l) {
            const long double p = st.pVisitedLen[static_cast<std::size_t>(l)];
            if (p > 0.00005L) {
                lens += QStringLiteral("  P(L=%1)≈%2\n").arg(l).arg(QString::number(static_cast<double>(p), 'f', 6));
            }
        }
        if (lens.isEmpty()) {
            lens = QStringLiteral("  (нет данных)\n");
        }

        resultLabel_->setText(
            QStringLiteral("Серия: trials=%1\n"
                           "- P(дойти до любого листа) ≈ %2\n"
                           "- P(остановиться во внутреннем узле) ≈ %3\n"
                           "- P(попасть в заданный лист leafId=%4) ≈ %5\n\n"
                           "Распределение L = число посещённых вершин (включая корень):\n%6")
                .arg(QString::number(static_cast<qulonglong>(cfg.trials)))
                .arg(QString::number(static_cast<double>(st.pReachAnyLeaf), 'f', 6))
                .arg(QString::number(static_cast<double>(st.pStopInternal), 'f', 6))
                .arg(cfg.targetLeafId)
                .arg(QString::number(static_cast<double>(st.pHitTargetLeaf), 'f', 6))
                .arg(lens));
    }

    QSpinBox* spinM_ = nullptr;
    QSpinBox* spinDepth_ = nullptr;
    QSpinBox* spinLeafId_ = nullptr;
    QLineEdit* editLeafDigits_ = nullptr;
    QLineEdit* editPStop_ = nullptr;
    QDoubleSpinBox* spinPStopDefault_ = nullptr;
    QComboBox* comboLaw_ = nullptr;
    QDoubleSpinBox* spinAlpha_ = nullptr;
    QSpinBox* spinTrials_ = nullptr;
    QPushButton* btnOne_ = nullptr;
    QPushButton* btnBatch_ = nullptr;
    QLabel* resultLabel_ = nullptr;
    TreeView* treeView_ = nullptr;
};

}  // конец анонимного пространства имён

// Точка входа Qt-приложения
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
