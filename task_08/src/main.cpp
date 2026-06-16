#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// =============================================================================
// Задание 8. Пьяный у обрыва — графики вероятностей (Qt 6, вариант 3)
// Симуляция блуждания; P(обрыв) и P(возврат в кафе) от координаты B
// Сборка: CMake + Qt Widgets (Windows)
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// Исход прогулки: обрыв (x≤0), возврат в кафе после ухода или обрезка по лимиту шагов
enum class WalkOutcome : std::uint8_t { Cliff, Return, Truncated };

// Симуляция одной прогулки из точки B: шаг +1 с вероятностью p, иначе −1
WalkOutcome simulateWalk(int B, double p, int maxSteps, std::mt19937& rng) {
    if (B < 1) {
        return WalkOutcome::Truncated;
    }
    std::bernoulli_distribution forward(p);
    int pos = B;
    bool hasLeftCafe = false;

    for (int step = 0; step < maxSteps; ++step) {
        if (forward(rng)) {
            ++pos;
        } else {
            --pos;
        }
        if (pos <= 0) {
            return WalkOutcome::Cliff;
        }
        if (pos == B && hasLeftCafe) {
            return WalkOutcome::Return;
        }
        if (pos != B) {
            hasLeftCafe = true;
        }
    }
    return WalkOutcome::Truncated;
}

// Виджет графика: P(обрыв) и P(возврат) от начальной координаты B
class PlotWidget final : public QWidget {
public:
    void setSeries(const std::vector<int>& bValues, const std::vector<double>& cliffProb,
                   const std::vector<double>& returnProb) {
        bValues_ = bValues;
        cliffProb_ = cliffProb;
        returnProb_ = returnProb;
        update();
    }

protected:
    // Отрисовка на виджете (QPainter)
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(250, 250, 250));

        if (bValues_.empty()) {
            painter.setPen(Qt::darkGray);
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Нет данных: нажмите «Считать»"));
            return;
        }

        const int left = 60;
        const int right = 20;
        const int top = 30;
        const int bottom = 50;
        const QRect plotRect(left, top, width() - left - right, height() - top - bottom);

        painter.setPen(Qt::black);
        painter.drawRect(plotRect);

        double ymin = 0.0;
        double ymax = 1.0;
        for (double v : cliffProb_) {
            ymax = std::max(ymax, v);
        }
        for (double v : returnProb_) {
            ymax = std::max(ymax, v);
        }
        ymax = std::min(1.05, ymax + 0.05);

        const int bMin = *std::min_element(bValues_.begin(), bValues_.end());
        const int bMax = *std::max_element(bValues_.begin(), bValues_.end());
        const double bSpan = std::max(1, bMax - bMin);

        const auto xPixel = [&](int b) -> int {
            const double t = (static_cast<double>(b - bMin)) / bSpan;
            return plotRect.left() + static_cast<int>(t * static_cast<double>(plotRect.width() - 1));
        };

        const auto yPixel = [&](double pVal) -> int {
            const double t = (pVal - ymin) / (ymax - ymin);
            return plotRect.bottom() - static_cast<int>(t * static_cast<double>(plotRect.height() - 1));
        };

        painter.setPen(QColor(200, 200, 200));
        for (int g = 0; g <= 10; ++g) {
            const double pGrid = ymin + (ymax - ymin) * (static_cast<double>(g) / 10.0);
            const int y = yPixel(pGrid);
            painter.drawLine(plotRect.left(), y, plotRect.right(), y);
        }

        painter.setPen(Qt::black);
        painter.drawText(plotRect.center().x() - 120, 18,
                         QStringLiteral("Вероятности от начальной координаты B"));

        painter.save();
        painter.translate(15, plotRect.center().y());
        painter.rotate(-90);
        painter.drawText(0, 0, QStringLiteral("Вероятность"));
        painter.restore();

        painter.drawText(plotRect.center().x() - 40, height() - 15, QStringLiteral("B (координата кафе)"));

        auto drawPoly = [&](const std::vector<double>& ys, const QColor& color, int widthPen) {
            if (bValues_.size() != ys.size() || bValues_.size() < 2) {
                return;
            }
            QPen pen(color, widthPen);
            painter.setPen(pen);
            for (std::size_t i = 1; i < bValues_.size(); ++i) {
                const int x0 = xPixel(bValues_[i - 1]);
                const int y0 = yPixel(ys[i - 1]);
                const int x1 = xPixel(bValues_[i]);
                const int y1 = yPixel(ys[i]);
                painter.drawLine(x0, y0, x1, y1);
            }
            painter.setBrush(color);
            const int r = 3;
            for (std::size_t i = 0; i < bValues_.size(); ++i) {
                const int x = xPixel(bValues_[i]);
                const int y = yPixel(ys[i]);
                painter.drawEllipse(QPoint(x, y), r, r);
            }
        };

        drawPoly(cliffProb_, QColor(200, 40, 40), 2);
        drawPoly(returnProb_, QColor(30, 100, 200), 2);

        painter.setPen(Qt::black);
        painter.setBrush(Qt::NoBrush);
        painter.drawText(plotRect.right() - 10, plotRect.top() + 15,
                         QStringLiteral(" ymax=%1").arg(ymax, 0, 'f', 2));

        const int lx = plotRect.left() + 8;
        int ly = plotRect.top() + 18;
        painter.setPen(QPen(QColor(200, 40, 40), 2));
        painter.drawLine(lx, ly, lx + 28, ly);
        painter.setPen(Qt::black);
        painter.drawText(lx + 34, ly + 4, QStringLiteral("обрыв (x=0)"));
        ly += 18;
        painter.setPen(QPen(QColor(30, 100, 200), 2));
        painter.drawLine(lx, ly, lx + 28, ly);
        painter.setPen(Qt::black);
        painter.drawText(lx + 34, ly + 4, QStringLiteral("возврат в кафе после ухода"));
    }

private:
    std::vector<int> bValues_;
    std::vector<double> cliffProb_;
    std::vector<double> returnProb_;
};

// Главное окно приложения
class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QStringLiteral("Задача 8 — пьяный блуждание"));
        resize(960, 640);

        auto* central = new QWidget(this);
        setCentralWidget(central);
        auto* root = new QVBoxLayout(central);

        auto* form = new QFormLayout();
        spinP_ = new QDoubleSpinBox();
        spinP_->setRange(0.0, 1.0);
        spinP_->setSingleStep(0.01);
        spinP_->setDecimals(4);
        spinP_->setValue(0.45);
        labelQ_ = new QLabel();
        form->addRow(QStringLiteral("p (шаг от обрыва, +1):"), spinP_);
        form->addRow(QStringLiteral("q = 1 − p:"), labelQ_);
        connect(spinP_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this](double v) { updateQLabel(v); });
        updateQLabel(spinP_->value());

        spinBMin_ = new QSpinBox();
        spinBMin_->setRange(1, 500);
        spinBMin_->setValue(1);
        spinBMax_ = new QSpinBox();
        spinBMax_->setRange(1, 500);
        spinBMax_->setValue(28);
        form->addRow(QStringLiteral("B от:"), spinBMin_);
        form->addRow(QStringLiteral("B до:"), spinBMax_);

        spinTrials_ = new QSpinBox();
        spinTrials_->setRange(50, 5000000);
        spinTrials_->setValue(4000);
        spinTrials_->setSingleStep(500);
        form->addRow(QStringLiteral("испытаний на каждое B:"), spinTrials_);

        spinMaxSteps_ = new QSpinBox();
        spinMaxSteps_->setRange(1000, 20000000);
        spinMaxSteps_->setValue(800000);
        spinMaxSteps_->setSingleStep(100000);
        form->addRow(QStringLiteral("макс. шагов на прогулку:"), spinMaxSteps_);

        auto* btn = new QPushButton(QStringLiteral("Считать и построить графики"));
        connect(btn, &QPushButton::clicked, this, [this] { runSimulation(); });

        root->addLayout(form);
        root->addWidget(btn);

        plot_ = new PlotWidget();
        plot_->setMinimumHeight(420);
        root->addWidget(plot_, 1);

        status_ = new QLabel(QStringLiteral("Готово."));
        status_->setWordWrap(true);
        root->addWidget(status_);
    }

private:
    void updateQLabel(double p) {
        const double q = 1.0 - p;
        labelQ_->setText(QString::number(q, 'f', 4));
    }

    void runSimulation() {
        const double p = spinP_->value();
        const double q = 1.0 - p;
        if (p < 0.0 || p > 1.0) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("p должно быть в [0, 1]."));
            return;
        }
        if (std::fabs(p + q - 1.0) > 1e-9) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Нужно p + q = 1."));
            return;
        }

        const int bMin = spinBMin_->value();
        const int bMax = spinBMax_->value();
        if (bMin > bMax) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("B от должно быть ≤ B до."));
            return;
        }

        const int trials = spinTrials_->value();
        const int maxSteps = spinMaxSteps_->value();

        std::vector<int> bVals;
        std::vector<double> cliffProb;
        std::vector<double> returnProb;

        std::mt19937 rng(std::random_device{}());

        double truncRateSum = 0.0;
        int truncCountB = 0;

        for (int B = bMin; B <= bMax; ++B) {
            std::uint64_t cliff = 0;
            std::uint64_t ret = 0;
            std::uint64_t trunc = 0;
            for (int t = 0; t < trials; ++t) {
                const WalkOutcome o = simulateWalk(B, p, maxSteps, rng);
                if (o == WalkOutcome::Cliff) {
                    ++cliff;
                } else if (o == WalkOutcome::Return) {
                    ++ret;
                } else {
                    ++trunc;
                }
            }
            const double inv = 1.0 / static_cast<double>(trials);
            bVals.push_back(B);
            cliffProb.push_back(static_cast<double>(cliff) * inv);
            returnProb.push_back(static_cast<double>(ret) * inv);
            truncRateSum += static_cast<double>(trunc) * inv;
            ++truncCountB;
            if ((B - bMin) % 3 == 0) {
                QApplication::processEvents();
            }
        }

        plot_->setSeries(bVals, cliffProb, returnProb);

        const double avgTrunc =
            truncCountB > 0 ? (truncRateSum / static_cast<double>(truncCountB)) : 0.0;
        status_->setText(
            QStringLiteral("Модель: x=0 — обрыв; старт в точке B (кафе). Шаг +1 с вероятностью p, −1 с q=1−p. "
                           "«Возврат в кафе»: снова попасть в B после того, как уже уходили с B. "
                           "Оценки — доли исходов по %1 симуляциям на каждое B; обрезка по числу шагов — в среднем %2 на B.")
                .arg(trials)
                .arg(avgTrunc, 0, 'f', 4));
    }

    QDoubleSpinBox* spinP_ = nullptr;
    QLabel* labelQ_ = nullptr;
    QSpinBox* spinBMin_ = nullptr;
    QSpinBox* spinBMax_ = nullptr;
    QSpinBox* spinTrials_ = nullptr;
    QSpinBox* spinMaxSteps_ = nullptr;
    PlotWidget* plot_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // конец анонимного пространства имён

// Точка входа Qt-приложения
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
