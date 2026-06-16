#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

// =============================================================================
// Задание 11. Класс дискретной СВ + GUI (Qt 6, вариант 3)
// Операции +, ×, aX; E, Var, асимметрия, эксцесс; PMF/CDF на QPainter
// Сборка: CMake + Qt Widgets (Windows)
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// ------------------------------ Ядро: дискретная СВ ------------------------------

// Класс дискретной случайной величины (закон в виде map x→p)
class DiscreteRV {
public:
    // Загрузка из пар (x,p): значения уникальны, p≥0, сумма > 0 (можно нормализовать)
    static std::optional<DiscreteRV> fromPairs(const std::vector<std::pair<long double, long double>>& pairs,
                                               bool normalize, std::string& error) {
        std::map<long double, long double> pmf;
        long double sum = 0.0L;
        for (const auto& pr : pairs) {
            const long double x = pr.first;
            const long double p = pr.second;
            if (!std::isfinite(static_cast<double>(x)) || !std::isfinite(static_cast<double>(p))) {
                error = "Non-finite value/probability.";
                return std::nullopt;
            }
            if (p < 0.0L) {
                error = "Probability must be >= 0.";
                return std::nullopt;
            }
            pmf[x] += p;  // дубликаты x суммируют вероятности
            sum += p;
        }
        if (pmf.empty()) {
            error = "Empty distribution.";
            return std::nullopt;
        }
        if (sum <= 0.0L) {
            error = "Sum of probabilities must be > 0.";
            return std::nullopt;
        }

        if (normalize) {
            for (auto& kv : pmf) {
                kv.second /= sum;
            }
        } else {
            // Требуем сумму вероятностей ≈ 1
            const long double diff = std::fabs(sum - 1.0L);
            if (diff > 1e-9L) {
                std::ostringstream ss;
                ss << "Sum of probabilities must be 1 (got " << std::setprecision(18) << static_cast<double>(sum)
                   << "). You can press Normalize.";
                error = ss.str();
                return std::nullopt;
            }
        }

        DiscreteRV rv;
        rv.pmf_ = std::move(pmf);
        rv.compact_();
        return rv;
    }

    const std::map<long double, long double>& pmf() const { return pmf_; }

    DiscreteRV scale(long double a) const {
        std::map<long double, long double> out;
        for (const auto& kv : pmf_) {
            out[kv.first * a] += kv.second;
        }
        DiscreteRV rv;
        rv.pmf_ = std::move(out);
        rv.compact_();
        return rv;
    }

    // Свертка для Z = X + Y (независимые дискретные СВ)
    DiscreteRV add(const DiscreteRV& other) const {
        std::map<long double, long double> out;
        for (const auto& a : pmf_) {
            for (const auto& b : other.pmf_) {
                out[a.first + b.first] += a.second * b.second;
            }
        }
        DiscreteRV rv;
        rv.pmf_ = std::move(out);
        rv.compact_();
        return rv;
    }

    // Распределение произведения Z = X * Y
    DiscreteRV multiply(const DiscreteRV& other) const {
        std::map<long double, long double> out;
        for (const auto& a : pmf_) {
            for (const auto& b : other.pmf_) {
                out[a.first * b.first] += a.second * b.second;
            }
        }
        DiscreteRV rv;
        rv.pmf_ = std::move(out);
        rv.compact_();
        return rv;
    }

    // Математическое ожидание E[X] = Σ x·p(x)
    long double mean() const {
        long double m = 0.0L;
        for (const auto& kv : pmf_) {
            m += kv.first * kv.second;
        }
        return m;
    }

    // Дисперсия Var(X) = Σ (x−E)²·p(x)
    long double variance() const {
        const long double m = mean();
        long double v = 0.0L;
        for (const auto& kv : pmf_) {
            const long double d = kv.first - m;
            v += d * d * kv.second;
        }
        return v;
    }

    long double skewness() const {
        const long double m = mean();
        const long double v = variance();
        if (v <= 0.0L) {
            return 0.0L;
        }
        const long double s = std::sqrt(v);
        long double mu3 = 0.0L;
        for (const auto& kv : pmf_) {
            const long double d = kv.first - m;
            mu3 += d * d * d * kv.second;
        }
        return mu3 / (s * s * s);
    }

    long double excessKurtosis() const {
        const long double m = mean();
        const long double v = variance();
        if (v <= 0.0L) {
            return 0.0L;
        }
        long double mu4 = 0.0L;
        for (const auto& kv : pmf_) {
            const long double d = kv.first - m;
            mu4 += d * d * d * d * kv.second;
        }
        const long double k = mu4 / (v * v);
        return k - 3.0L;
    }

    std::vector<std::pair<long double, long double>> cdfPoints() const {
        // step function points at each x: (x, F(x))
        std::vector<std::pair<long double, long double>> pts;
        long double acc = 0.0L;
        for (const auto& kv : pmf_) {
            acc += kv.second;
            pts.push_back({kv.first, acc});
        }
        return pts;
    }

    // Simple text format:
    // N
    // x0 p0
    // ...
    // x_{N-1} p_{N-1}
    bool saveToFile(const std::string& path, std::string& error) const {
        std::ofstream out(path);
        if (!out) {
            error = "Cannot open file for writing.";
            return false;
        }
        out << pmf_.size() << "\n";
        out << std::setprecision(18);
        for (const auto& kv : pmf_) {
            out << static_cast<double>(kv.first) << " " << static_cast<double>(kv.second) << "\n";
        }
        return true;
    }

    static std::optional<DiscreteRV> loadFromFile(const std::string& path, std::string& error) {
        std::ifstream in(path);
        if (!in) {
            error = "Cannot open file for reading.";
            return std::nullopt;
        }
        std::size_t n = 0;
        in >> n;
        if (!in || n == 0) {
            error = "Invalid header.";
            return std::nullopt;
        }
        std::vector<std::pair<long double, long double>> pairs;
        pairs.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            long double x = 0.0L, p = 0.0L;
            in >> x >> p;
            if (!in) {
                error = "Invalid line while reading pairs.";
                return std::nullopt;
            }
            pairs.push_back({x, p});
        }
        return fromPairs(pairs, true, error); // normalize on load for safety
    }

private:
    void compact_() {
        // Remove tiny probabilities and renormalize.
        long double sum = 0.0L;
        for (auto it = pmf_.begin(); it != pmf_.end();) {
            if (it->second <= 1e-18L) {
                it = pmf_.erase(it);
            } else {
                sum += it->second;
                ++it;
            }
        }
        if (sum > 0.0L) {
            for (auto& kv : pmf_) {
                kv.second /= sum;
            }
        }
    }

    std::map<long double, long double> pmf_;
};

// ------------------------------ UI helpers ------------------------------

std::vector<std::pair<long double, long double>> readPairsFromTable(QTableWidget* table, bool& ok) {
    ok = true;
    std::vector<std::pair<long double, long double>> pairs;
    for (int r = 0; r < table->rowCount(); ++r) {
        const auto* itemX = table->item(r, 0);
        const auto* itemP = table->item(r, 1);
        if (!itemX || !itemP) {
            continue;
        }
        const QString sx = itemX->text().trimmed();
        const QString sp = itemP->text().trimmed();
        if (sx.isEmpty() && sp.isEmpty()) {
            continue;
        }
        bool okX = false;
        bool okP = false;
        const double x = sx.toDouble(&okX);
        const double p = sp.toDouble(&okP);
        if (!okX || !okP) {
            ok = false;
            return {};
        }
        pairs.push_back({static_cast<long double>(x), static_cast<long double>(p)});
    }
    if (pairs.empty()) {
        ok = false;
    }
    return pairs;
}

void fillTableFromRV(QTableWidget* table, const DiscreteRV& rv) {
    table->setRowCount(0);
    int r = 0;
    for (const auto& kv : rv.pmf()) {
        table->insertRow(r);
        table->setItem(r, 0, new QTableWidgetItem(QString::number(static_cast<double>(kv.first), 'g', 12)));
        table->setItem(r, 1, new QTableWidgetItem(QString::number(static_cast<double>(kv.second), 'g', 12)));
        ++r;
    }
    // add a few empty rows for editing convenience
    for (int i = 0; i < 6; ++i) {
        table->insertRow(r + i);
        table->setItem(r + i, 0, new QTableWidgetItem(""));
        table->setItem(r + i, 1, new QTableWidgetItem(""));
    }
}

long double sumProb(const std::vector<std::pair<long double, long double>>& pairs) {
    long double s = 0.0L;
    for (const auto& pr : pairs) {
        s += pr.second;
    }
    return s;
}

// ------------------------------ Plot widget ------------------------------

class PlotWidget final : public QWidget {
public:
    enum class Mode { PMF_Bars, PMF_Polyline, CDF_Step };

    void setData(const DiscreteRV& rv) {
        pmf_.clear();
        for (const auto& kv : rv.pmf()) {
            pmf_.push_back({kv.first, kv.second});
        }
        cdf_ = rv.cdfPoints();
        update();
    }

    void setMode(Mode m) {
        mode_ = m;
        update();
    }

protected:
    // Отрисовка на виджете (QPainter)
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(250, 250, 250));

        if (pmf_.empty()) {
            p.setPen(Qt::darkGray);
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Нет данных"));
            return;
        }

        const int left = 60, right = 20, top = 30, bottom = 50;
        const QRect plot(left, top, width() - left - right, height() - top - bottom);
        p.setPen(Qt::black);
        p.drawRect(plot);

        long double xMin = pmf_.front().first;
        long double xMax = pmf_.back().first;
        if (xMin == xMax) {
            xMin -= 1.0L;
            xMax += 1.0L;
        }

        long double yMax = 0.0L;
        if (mode_ == Mode::CDF_Step) {
            yMax = 1.0L;
        } else {
            for (const auto& pr : pmf_) yMax = std::max(yMax, pr.second);
            yMax = std::max(yMax, 0.1L);
        }

        const auto xPix = [&](long double x) {
            const long double t = (x - xMin) / (xMax - xMin);
            return plot.left() + static_cast<int>(t * static_cast<long double>(plot.width() - 1));
        };
        const auto yPix = [&](long double y) {
            const long double t = y / yMax;
            return plot.bottom() - static_cast<int>(t * static_cast<long double>(plot.height() - 1));
        };

        // grid
        p.setPen(QColor(210, 210, 210));
        for (int g = 0; g <= 10; ++g) {
            const long double y = yMax * (static_cast<long double>(g) / 10.0L);
            const int yy = yPix(y);
            p.drawLine(plot.left(), yy, plot.right(), yy);
        }

        // title
        p.setPen(Qt::black);
        QString title = QStringLiteral("PMF");
        if (mode_ == Mode::PMF_Polyline) title = QStringLiteral("PMF (полилиния)");
        if (mode_ == Mode::CDF_Step) title = QStringLiteral("CDF (функция распределения)");
        p.drawText(plot.center().x() - 120, 18, title);

        // axis labels
        p.drawText(plot.center().x() - 15, height() - 15, QStringLiteral("x"));
        p.save();
        p.translate(15, plot.center().y());
        p.rotate(-90);
        p.drawText(0, 0, mode_ == Mode::CDF_Step ? QStringLiteral("F(x)") : QStringLiteral("P(X=x)"));
        p.restore();

        if (mode_ == Mode::PMF_Bars) {
            p.setPen(QPen(QColor(30, 100, 200), 2));
            p.setBrush(QColor(30, 100, 200, 80));
            for (const auto& pr : pmf_) {
                const int x = xPix(pr.first);
                const int y = yPix(pr.second);
                p.drawRect(QRect(x - 6, y, 12, plot.bottom() - y));
            }
        } else if (mode_ == Mode::PMF_Polyline) {
            p.setPen(QPen(QColor(200, 40, 40), 2));
            for (std::size_t i = 1; i < pmf_.size(); ++i) {
                p.drawLine(QPoint(xPix(pmf_[i - 1].first), yPix(pmf_[i - 1].second)),
                           QPoint(xPix(pmf_[i].first), yPix(pmf_[i].second)));
            }
            p.setBrush(QColor(200, 40, 40));
            for (const auto& pr : pmf_) {
                p.drawEllipse(QPoint(xPix(pr.first), yPix(pr.second)), 3, 3);
            }
        } else {  // CDF
            p.setPen(QPen(QColor(0, 140, 0), 2));
            // Draw step: horizontal to next x, then vertical.
            long double prevX = pmf_.front().first;
            long double prevF = 0.0L;
            // leftmost vertical at first x
            p.drawLine(QPoint(xPix(prevX), yPix(prevF)), QPoint(xPix(prevX), yPix(cdf_.front().second)));
            prevF = cdf_.front().second;
            for (std::size_t i = 1; i < cdf_.size(); ++i) {
                const long double x = cdf_[i].first;
                // horizontal from prevX to x at prevF
                p.drawLine(QPoint(xPix(prevX), yPix(prevF)), QPoint(xPix(x), yPix(prevF)));
                // vertical at x
                p.drawLine(QPoint(xPix(x), yPix(prevF)), QPoint(xPix(x), yPix(cdf_[i].second)));
                prevX = x;
                prevF = cdf_[i].second;
            }
            // extend to right border
            p.drawLine(QPoint(xPix(prevX), yPix(prevF)), QPoint(plot.right(), yPix(prevF)));
        }
    }

private:
    Mode mode_ = Mode::PMF_Bars;
    std::vector<std::pair<long double, long double>> pmf_;
    std::vector<std::pair<long double, long double>> cdf_;
};

// ------------------------------ Main Window ------------------------------

// Главное окно приложения
class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QStringLiteral("Задача 11 — дискретная случайная величина"));
        resize(1200, 760);

        auto* central = new QWidget(this);
        setCentralWidget(central);
        auto* root = new QHBoxLayout(central);

        auto* left = new QWidget(this);
        auto* leftLayout = new QVBoxLayout(left);

        auto* boxX = new QGroupBox(QStringLiteral("X (таблица: значение — вероятность)"), this);
        auto* vX = new QVBoxLayout(boxX);
        tableX_ = makeTable_();
        vX->addWidget(tableX_);
        vX->addLayout(makeTableButtons_(tableX_, true));

        auto* boxY = new QGroupBox(QStringLiteral("Y (таблица: значение — вероятность)"), this);
        auto* vY = new QVBoxLayout(boxY);
        tableY_ = makeTable_();
        vY->addWidget(tableY_);
        vY->addLayout(makeTableButtons_(tableY_, false));

        auto* ops = new QGroupBox(QStringLiteral("Операции"), this);
        auto* opsForm = new QFormLayout(ops);

        comboOp_ = new QComboBox(this);
        comboOp_->addItem(QStringLiteral("Z = X + Y"));
        comboOp_->addItem(QStringLiteral("Z = X × Y"));
        comboOp_->addItem(QStringLiteral("Z = a × X"));
        opsForm->addRow(QStringLiteral("Операция:"), comboOp_);

        spinA_ = new QDoubleSpinBox(this);
        spinA_->setRange(-1e6, 1e6);
        spinA_->setDecimals(6);
        spinA_->setValue(2.0);
        opsForm->addRow(QStringLiteral("a (для a×X):"), spinA_);

        auto* btnCompute = new QPushButton(QStringLiteral("Вычислить Z и характеристики"), this);
        opsForm->addRow(btnCompute);

        auto* btnSave = new QPushButton(QStringLiteral("Сохранить Z в файл…"), this);
        auto* btnLoad = new QPushButton(QStringLiteral("Загрузить X или Y из файла…"), this);
        opsForm->addRow(btnSave);
        opsForm->addRow(btnLoad);

        stats_ = new QLabel(QStringLiteral("Готово."), this);
        stats_->setWordWrap(true);

        leftLayout->addWidget(boxX);
        leftLayout->addWidget(boxY);
        leftLayout->addWidget(ops);
        leftLayout->addWidget(stats_, 1);

        // right side: Z table + plot
        auto* right = new QWidget(this);
        auto* rightLayout = new QVBoxLayout(right);

        auto* boxZ = new QGroupBox(QStringLiteral("Z (результат)"), this);
        auto* vZ = new QVBoxLayout(boxZ);
        tableZ_ = makeTable_();
        tableZ_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        vZ->addWidget(tableZ_);

        auto* modeRow = new QHBoxLayout();
        modeRow->addWidget(new QLabel(QStringLiteral("Вид:"), this));
        comboPlot_ = new QComboBox(this);
        comboPlot_->addItem(QStringLiteral("Закон распределения (PMF, столбики)"));
        comboPlot_->addItem(QStringLiteral("Полилиния (PMF)"));
        comboPlot_->addItem(QStringLiteral("Функция распределения (CDF)"));
        modeRow->addWidget(comboPlot_, 1);
        vZ->addLayout(modeRow);

        plot_ = new PlotWidget();
        plot_->setMinimumHeight(320);
        vZ->addWidget(plot_);

        rightLayout->addWidget(boxZ, 1);

        root->addWidget(left, 0);
        root->addWidget(right, 1);

        seedExamples_();

        QObject::connect(btnCompute, &QPushButton::clicked, this, [this] { compute_(); });
        QObject::connect(comboPlot_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                         [this](int idx) { setPlotMode_(idx); });
        QObject::connect(comboOp_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                         [this](int idx) { spinA_->setEnabled(idx == 2); });
        QObject::connect(btnSave, &QPushButton::clicked, this, [this] { saveZ_(); });
        QObject::connect(btnLoad, &QPushButton::clicked, this, [this] { loadInto_(); });

        spinA_->setEnabled(false);
        setPlotMode_(0);
    }

private:
    QTableWidget* makeTable_() {
        auto* t = new QTableWidget(this);
        t->setColumnCount(2);
        t->setHorizontalHeaderLabels({QStringLiteral("x"), QStringLiteral("p")});
        t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        t->setRowCount(8);
        for (int r = 0; r < t->rowCount(); ++r) {
            t->setItem(r, 0, new QTableWidgetItem(""));
            t->setItem(r, 1, new QTableWidgetItem(""));
        }
        return t;
    }

    QHBoxLayout* makeTableButtons_(QTableWidget* table, bool isX) {
        auto* row = new QHBoxLayout();
        auto* btnNorm = new QPushButton(QStringLiteral("Normalize"), this);
        auto* btnClear = new QPushButton(QStringLiteral("Clear"), this);
        auto* btnExample = new QPushButton(isX ? QStringLiteral("Example X") : QStringLiteral("Example Y"), this);

        row->addWidget(btnNorm);
        row->addWidget(btnClear);
        row->addWidget(btnExample);
        row->addStretch(1);

        QObject::connect(btnNorm, &QPushButton::clicked, this, [this, table] { normalizeTable_(table); });
        QObject::connect(btnClear, &QPushButton::clicked, this, [table] {
            for (int r = 0; r < table->rowCount(); ++r) {
                table->item(r, 0)->setText("");
                table->item(r, 1)->setText("");
            }
        });
        QObject::connect(btnExample, &QPushButton::clicked, this, [this, isX] {
            if (isX) seedX_(); else seedY_();
        });

        return row;
    }

    void seedExamples_() {
        seedX_();
        seedY_();
    }

    void seedX_() {
        // X: {-1,0,2} with probs {0.2,0.5,0.3}
        tableX_->item(0, 0)->setText("-1");
        tableX_->item(0, 1)->setText("0.2");
        tableX_->item(1, 0)->setText("0");
        tableX_->item(1, 1)->setText("0.5");
        tableX_->item(2, 0)->setText("2");
        tableX_->item(2, 1)->setText("0.3");
        for (int r = 3; r < tableX_->rowCount(); ++r) {
            tableX_->item(r, 0)->setText("");
            tableX_->item(r, 1)->setText("");
        }
    }

    void seedY_() {
        // Y: {1,3} with probs {0.6,0.4}
        tableY_->item(0, 0)->setText("1");
        tableY_->item(0, 1)->setText("0.6");
        tableY_->item(1, 0)->setText("3");
        tableY_->item(1, 1)->setText("0.4");
        for (int r = 2; r < tableY_->rowCount(); ++r) {
            tableY_->item(r, 0)->setText("");
            tableY_->item(r, 1)->setText("");
        }
    }

    void normalizeTable_(QTableWidget* table) {
        bool ok = true;
        const auto pairs = readPairsFromTable(table, ok);
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Неверные числа в таблице."));
            return;
        }
        const long double s = sumProb(pairs);
        if (s <= 0.0L) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Сумма вероятностей должна быть > 0."));
            return;
        }
        // write back normalized probabilities aligned with rows that are non-empty
        long double acc = 0.0L;
        for (int r = 0; r < table->rowCount(); ++r) {
            const QString sx = table->item(r, 0)->text().trimmed();
            const QString sp = table->item(r, 1)->text().trimmed();
            if (sx.isEmpty() && sp.isEmpty()) continue;
            bool okP = false;
            const double p = sp.toDouble(&okP);
            if (!okP) continue;
            const long double pn = static_cast<long double>(p) / s;
            table->item(r, 1)->setText(QString::number(static_cast<double>(pn), 'g', 12));
            acc += pn;
        }
    }

    std::optional<DiscreteRV> buildRV_(QTableWidget* table, bool allowNormalize, QString& error) {
        bool ok = true;
        const auto pairs = readPairsFromTable(table, ok);
        if (!ok) {
            error = QStringLiteral("Неверные данные таблицы (пусто или не числа).");
            return std::nullopt;
        }
        std::string err;
        const auto rvOpt = DiscreteRV::fromPairs(pairs, allowNormalize, err);
        if (!rvOpt.has_value()) {
            error = QString::fromStdString(err);
            return std::nullopt;
        }
        return rvOpt.value();
    }

    void setPlotMode_(int idx) {
        if (idx == 0) plot_->setMode(PlotWidget::Mode::PMF_Bars);
        else if (idx == 1) plot_->setMode(PlotWidget::Mode::PMF_Polyline);
        else plot_->setMode(PlotWidget::Mode::CDF_Step);
    }

    void compute_() {
        QString err;
        auto xOpt = buildRV_(tableX_, false, err);
        if (!xOpt.has_value()) {
            QMessageBox::warning(this, QStringLiteral("Ошибка X"), err);
            return;
        }
        auto yOpt = buildRV_(tableY_, false, err);
        if (!yOpt.has_value()) {
            QMessageBox::warning(this, QStringLiteral("Ошибка Y"), err);
            return;
        }

        const DiscreteRV X = xOpt.value();
        const DiscreteRV Y = yOpt.value();

        const int op = comboOp_->currentIndex();
        if (op == 0) {
            z_ = X.add(Y);
        } else if (op == 1) {
            z_ = X.multiply(Y);
        } else {
            z_ = X.scale(static_cast<long double>(spinA_->value()));
        }

        fillTableFromRV(tableZ_, z_.value());
        plot_->setData(z_.value());

        const long double m = z_->mean();
        const long double v = z_->variance();
        const long double s = z_->skewness();
        const long double e = z_->excessKurtosis();

        stats_->setText(
            QStringLiteral("Z построена. Характеристики:\n"
                           "E[Z] = %1\nVar[Z] = %2\nSkewness = %3\nExcess kurtosis = %4\n"
                           "Размер поддержки |Z| = %5")
                .arg(QString::number(static_cast<double>(m), 'g', 12))
                .arg(QString::number(static_cast<double>(v), 'g', 12))
                .arg(QString::number(static_cast<double>(s), 'g', 12))
                .arg(QString::number(static_cast<double>(e), 'g', 12))
                .arg(QString::number(static_cast<int>(z_->pmf().size()))));
    }

    void saveZ_() {
        if (!z_.has_value()) {
            QMessageBox::information(this, QStringLiteral("Нет данных"), QStringLiteral("Сначала вычисли Z."));
            return;
        }
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить Z"),
                                                          QString(), QStringLiteral("Text (*.txt)"));
        if (path.isEmpty()) return;
        std::string error;
        if (!z_->saveToFile(path.toStdString(), error)) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QString::fromStdString(error));
        }
    }

    void loadInto_() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Загрузить распределение"),
                                                          QString(), QStringLiteral("Text (*.txt)"));
        if (path.isEmpty()) return;
        std::string error;
        const auto rvOpt = DiscreteRV::loadFromFile(path.toStdString(), error);
        if (!rvOpt.has_value()) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QString::fromStdString(error));
            return;
        }

        QMessageBox mb(this);
        mb.setWindowTitle(QStringLiteral("Куда загрузить?"));
        mb.setText(QStringLiteral("Загрузить в X или в Y?"));
        auto* btnX = mb.addButton(QStringLiteral("В X"), QMessageBox::AcceptRole);
        auto* btnY = mb.addButton(QStringLiteral("В Y"), QMessageBox::DestructiveRole);
        mb.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
        mb.exec();
        if (mb.clickedButton() == btnX) {
            fillTableFromRV(tableX_, rvOpt.value());
        } else if (mb.clickedButton() == btnY) {
            fillTableFromRV(tableY_, rvOpt.value());
        }
    }

    QTableWidget* tableX_ = nullptr;
    QTableWidget* tableY_ = nullptr;
    QTableWidget* tableZ_ = nullptr;
    QComboBox* comboOp_ = nullptr;
    QDoubleSpinBox* spinA_ = nullptr;
    QLabel* stats_ = nullptr;
    QComboBox* comboPlot_ = nullptr;
    PlotWidget* plot_ = nullptr;

    std::optional<DiscreteRV> z_;
};

}  // конец анонимного пространства имён

// Точка входа Qt-приложения
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
