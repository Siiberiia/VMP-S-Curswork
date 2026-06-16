#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <random>
#include <vector>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// =============================================================================
// Задание 12. Игла Буффона — визуализация (Qt 6, вариант 3)
// P=2L/(πd); сравнение точной и эмпирической вероятности
// Сборка: CMake + Qt Widgets (Windows)
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

constexpr long double kPi = 3.141592653589793238462643383279502884L;

// Геометрия одной иглы на плоскости
struct Needle {
    // координаты концов иглы в мировой системе (x horizontal, y vertical), y-lines at multiples of d
    long double x1 = 0, y1 = 0;
    long double x2 = 0, y2 = 0;
    bool crosses = false;
};

struct Config {
    long double d = 1.0L;
    long double L = 0.8L;
    int needlesToDraw = 250;
    std::uint64_t batch = 2000;
};

struct Stats {
    std::uint64_t total = 0;
    std::uint64_t crosses = 0;
};

bool crossesLine(long double x, long double phi, long double L) {
    // x ∈ [0,d/2] — расстояние от центра иглы до ближайшей прямой
    // φ ∈ [0,π/2] — угол иглы с направлением прямых (i.e., with the line direction)
    // Условие пересечения: x ≤ (L/2)·sin(φ)
    return x <= (L * 0.5L) * std::sin(phi);
}

Needle sampleNeedle(long double d, long double L, std::mt19937_64& rng) {
    std::uniform_real_distribution<long double> distX(0.0L, d * 0.5L);
    std::uniform_real_distribution<long double> distPhi(0.0L, kPi * 0.5L);
    std::uniform_real_distribution<long double> distU(-6.0L * d, 6.0L * d);
    std::uniform_real_distribution<long double> distHoriz(-10.0L * d, 10.0L * d);
    std::bernoulli_distribution side(0.5);

    const long double x = distX(rng);
    const long double phi = distPhi(rng);
    const bool cross = crossesLine(x, phi, L);

    // Геометрическое построение иглы в полосе вокруг прямой y=0
    // Ближайшая прямая y=0; центр иглы на расстоянии ±x
    const long double yc = side(rng) ? x : -x;
    const long double xc = distHoriz(rng);

    // θ — угол к оси X; прямые горизонтальны, φ совпадает с углом к оси X
    const long double theta = phi;
    const long double dx = (L * 0.5L) * std::cos(theta);
    const long double dy = (L * 0.5L) * std::sin(theta);

    Needle n;
    n.x1 = xc - dx;
    n.y1 = yc - dy;
    n.x2 = xc + dx;
    n.y2 = yc + dy;
    n.crosses = cross;

    // Случайный сдвиг по вертикали (эквивалентные полосы)
    const long double vshift = distU(rng);
    n.y1 += vshift;
    n.y2 += vshift;
    return n;
}

long double exactProbability(long double d, long double L) {
    if (d <= 0.0L) return 0.0L;
    if (L < 0.0L) return 0.0L;
    if (L > d) {
        // В задании L≤d; ветка на случай нарушения ограничения.
        return 1.0L;
    }
    return (2.0L * L) / (kPi * d);
}

// Поле визуализации игл Буффона
class Canvas final : public QWidget {
public:
    void setModel(long double d, long double L, const std::vector<Needle>& needles) {
        d_ = d;
        L_ = L;
        needles_ = needles;
        update();
    }

protected:
    // Отрисовка на виджете (QPainter)
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(250, 250, 250));

        if (d_ <= 0.0L) {
            p.setPen(Qt::darkGray);
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("d должно быть > 0"));
            return;
        }

        const int margin = 20;
        const QRect plot(margin, margin, width() - 2 * margin, height() - 2 * margin);
        p.setPen(Qt::black);
        p.drawRect(plot);

        // world -> pixel scaling
        const long double viewW = 24.0L * d_;
        const long double viewH = 14.0L * d_;
        const long double xMin = -viewW * 0.5L;
        const long double yMin = -viewH * 0.5L;

        const auto xPix = [&](long double x) {
            const long double t = (x - xMin) / viewW;
            return plot.left() + static_cast<int>(t * static_cast<long double>(plot.width()));
        };
        const auto yPix = [&](long double y) {
            const long double t = (y - yMin) / viewH;
            return plot.bottom() - static_cast<int>(t * static_cast<long double>(plot.height()));
        };

        // draw parallel lines y = k*d
        p.setPen(QPen(QColor(200, 200, 200), 1));
        const int kMin = static_cast<int>(std::floor((yMin) / d_)) - 1;
        const int kMax = static_cast<int>(std::ceil((yMin + viewH) / d_)) + 1;
        for (int k = kMin; k <= kMax; ++k) {
            const long double y = static_cast<long double>(k) * d_;
            const int yy = yPix(y);
            p.drawLine(plot.left(), yy, plot.right(), yy);
        }

        // draw needles
        for (const auto& n : needles_) {
            QPen pen(n.crosses ? QColor(200, 40, 40) : QColor(30, 100, 200), n.crosses ? 3 : 2);
            p.setPen(pen);
            p.drawLine(QPointF(xPix(n.x1), yPix(n.y1)), QPointF(xPix(n.x2), yPix(n.y2)));
        }

        p.setPen(Qt::black);
        p.drawText(plot.left() + 10, plot.top() + 20,
                   QStringLiteral("d=%1, L=%2 (L<=d)")
                       .arg(QString::number(static_cast<double>(d_), 'g', 8))
                       .arg(QString::number(static_cast<double>(L_), 'g', 8)));
        p.setPen(QColor(200, 40, 40));
        p.drawText(plot.left() + 10, plot.top() + 40, QStringLiteral("красный: пересёк линию"));
        p.setPen(QColor(30, 100, 200));
        p.drawText(plot.left() + 10, plot.top() + 60, QStringLiteral("синий: не пересёк"));
    }

private:
    long double d_ = 1.0L;
    long double L_ = 0.8L;
    std::vector<Needle> needles_;
};

// Главное окно приложения
class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QStringLiteral("Задача 12 — игла Буффона (моделирование)"));
        resize(1150, 760);

        auto* central = new QWidget(this);
        setCentralWidget(central);
        auto* root = new QHBoxLayout(central);

        auto* left = new QWidget(this);
        auto* leftLayout = new QVBoxLayout(left);

        auto* box = new QGroupBox(QStringLiteral("Параметры"), this);
        auto* form = new QFormLayout(box);

        spinD_ = new QDoubleSpinBox(this);
        spinD_->setRange(0.0001, 1e6);
        spinD_->setDecimals(6);
        spinD_->setValue(1.0);
        form->addRow(QStringLiteral("d (расст. между прямыми):"), spinD_);

        spinL_ = new QDoubleSpinBox(this);
        spinL_->setRange(0.0001, 1e6);
        spinL_->setDecimals(6);
        spinL_->setValue(0.8);
        form->addRow(QStringLiteral("L (длина иглы, L ≤ d):"), spinL_);

        spinBatch_ = new QSpinBox(this);
        spinBatch_->setRange(10, 5'000'000);
        spinBatch_->setValue(2000);
        spinBatch_->setSingleStep(500);
        form->addRow(QStringLiteral("Бросков за шаг:"), spinBatch_);

        spinDraw_ = new QSpinBox(this);
        spinDraw_->setRange(0, 5000);
        spinDraw_->setValue(250);
        spinDraw_->setSingleStep(50);
        form->addRow(QStringLiteral("Рисовать последних игл:"), spinDraw_);

        auto* btnStep = new QPushButton(QStringLiteral("Сделать шаг (batch)"), this);
        auto* btnReset = new QPushButton(QStringLiteral("Сброс"), this);
        leftLayout->addWidget(box);
        leftLayout->addWidget(btnStep);
        leftLayout->addWidget(btnReset);

        info_ = new QLabel(QStringLiteral("Готово."), this);
        info_->setWordWrap(true);
        leftLayout->addWidget(info_, 1);
        leftLayout->addStretch(1);

        canvas_ = new Canvas();
        root->addWidget(left, 0);
        root->addWidget(canvas_, 1);

        QObject::connect(btnStep, &QPushButton::clicked, this, [this] { step_(); });
        QObject::connect(btnReset, &QPushButton::clicked, this, [this] { reset_(); });

        reset_();
    }

private:
    void reset_() {
        stats_ = {};
        needles_.clear();
        updateView_();
    }

    void step_() {
        const long double d = static_cast<long double>(spinD_->value());
        const long double L = static_cast<long double>(spinL_->value());
        if (d <= 0.0L) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("d должно быть > 0."));
            return;
        }
        if (L <= 0.0L) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("L должно быть > 0."));
            return;
        }
        if (L > d + 1e-15L) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Нужно L ≤ d."));
            return;
        }

        const int batch = spinBatch_->value();
        const int keep = spinDraw_->value();

        for (int i = 0; i < batch; ++i) {
            const Needle n = sampleNeedle(d, L, rng_);
            ++stats_.total;
            if (n.crosses) ++stats_.crosses;
            if (keep > 0) {
                needles_.push_back(n);
                if (static_cast<int>(needles_.size()) > keep) {
                    needles_.erase(needles_.begin(), needles_.begin() + (needles_.size() - keep));
                }
            }
        }

        updateView_();
    }

    void updateView_() {
        const long double d = static_cast<long double>(spinD_->value());
        const long double L = static_cast<long double>(spinL_->value());
        canvas_->setModel(d, L, needles_);

        const long double exact = exactProbability(d, L);
        const long double emp = stats_.total > 0
            ? static_cast<long double>(stats_.crosses) / static_cast<long double>(stats_.total)
            : 0.0L;
        const long double diff = std::fabs(emp - exact);

        info_->setText(
            QStringLiteral("Точная вероятность (при L ≤ d):  P = 2L/(πd)\n"
                           "P_exact = %1\n"
                           "P_emp   = %2   (cross=%3 / total=%4)\n"
                           "|diff|  = %5\n\n"
                           "Условие пересечения: x ≤ (L/2)·sin(φ), где x∼U[0,d/2], φ∼U[0,π/2].")
                .arg(QString::number(static_cast<double>(exact), 'f', 10))
                .arg(QString::number(static_cast<double>(emp), 'f', 10))
                .arg(QString::number(static_cast<qulonglong>(stats_.crosses)))
                .arg(QString::number(static_cast<qulonglong>(stats_.total)))
                .arg(QString::number(static_cast<double>(diff), 'f', 10)));
    }

    QDoubleSpinBox* spinD_ = nullptr;
    QDoubleSpinBox* spinL_ = nullptr;
    QSpinBox* spinBatch_ = nullptr;
    QSpinBox* spinDraw_ = nullptr;
    QLabel* info_ = nullptr;
    Canvas* canvas_ = nullptr;

    std::mt19937_64 rng_{std::random_device{}()};
    Stats stats_{};
    std::vector<Needle> needles_;
};

}  // конец анонимного пространства имён

// Точка входа Qt-приложения
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}

