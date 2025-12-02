#include "trainer_window.h"

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QVBoxLayout>

// ===== TrainingItem helpers =====

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeSingleKey(
    QChar ch) {
    TrainingItem item;
    item.type = TrainingType::kSingleKey;
    item.sequence = QString(ch).toLower();
    item.label = QString(ch).toUpper();
    return item;
}

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeSequence(
    const QString &seq) {
    TrainingItem item;
    item.type = TrainingType::kSequence;
    item.sequence = seq.toLower();
    item.label = seq.toUpper();
    return item;
}

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeCombo(
    Qt::KeyboardModifiers mods, int key, const QString &label) {
    TrainingItem item;
    item.type = TrainingType::kCombo;
    item.key = key;
    item.modifiers = mods;
    item.label = label;
    return item;
}

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeSpecialKey(
    int key, const QString &label) {
    TrainingItem item;
    item.type = TrainingType::kSpecialKey;
    item.key = key;
    item.label = label;
    return item;
}

// ===== TrainerWindow implementation =====

TrainerWindow::TrainerWindow(QWidget *parent)
    : QMainWindow(parent), elapsed_(new QElapsedTimer()) {
    setWindowTitle(QStringLiteral("Left-hand Keyboard Trainer - SC2 Style"));
    resize(600, 400);

    InitTrainingItems();

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *layout = new QVBoxLayout(central);

    // Floating error label in top-left of central widget (NOT in layout).
    error_label_ = new QLabel(central);
    error_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    error_label_->setObjectName(QStringLiteral("errorLabel"));
    QFont error_font = error_label_->font();
    error_font.setPointSize(10);
    error_label_->setFont(error_font);
    error_label_->setAttribute(Qt::WA_TransparentForMouseEvents);
    error_label_->setWordWrap(true); // allow wrapping so long messages are visible
    error_label_->setText(QString());
    error_label_->raise();

    // Center big text.
    target_label_ = new QLabel(QStringLiteral("Click \"Start\" to begin"), this);
    target_label_->setAlignment(Qt::AlignCenter);
    QFont big_font = target_label_->font();
    big_font.setPointSize(40);
    big_font.setBold(true);
    target_label_->setFont(big_font);

    // Stats at bottom.
    stats_label_ = new QLabel(QStringLiteral("Not started"), this);
    stats_label_->setAlignment(Qt::AlignCenter);

    // Buttons.
    start_button_ = new QPushButton(QStringLiteral("Start"), this);
    stop_button_ = new QPushButton(QStringLiteral("Stop"), this);
    stop_button_->setEnabled(false);

    theme_button_ = new QPushButton(QStringLiteral("Light"), this);
    theme_button_->setToolTip(QStringLiteral("Click to toggle theme"));

    // We do not want Tab to move focus to buttons.
    start_button_->setFocusPolicy(Qt::NoFocus);
    stop_button_->setFocusPolicy(Qt::NoFocus);
    theme_button_->setFocusPolicy(Qt::NoFocus);
    central->setFocusPolicy(Qt::NoFocus);
    setFocusPolicy(Qt::StrongFocus);

    auto *button_layout = new QHBoxLayout();
    button_layout->addStretch();
    button_layout->addWidget(start_button_);
    button_layout->addWidget(stop_button_);
    button_layout->addWidget(theme_button_);

    layout->addWidget(target_label_, 1);
    layout->addWidget(stats_label_);
    layout->addLayout(button_layout);

    QObject::connect(start_button_, &QPushButton::clicked,
                     this, &TrainerWindow::StartTraining);
    QObject::connect(stop_button_, &QPushButton::clicked,
                     this, &TrainerWindow::StopTraining);
    QObject::connect(theme_button_, &QPushButton::clicked,
                     this, &TrainerWindow::ToggleTheme);

    ApplyTheme();
}

void TrainerWindow::InitTrainingItems() {
    items_.clear();

    // 1. Left-hand single keys.
    const QString k_single_keys = QStringLiteral("12345qwertyasdfghzxcvzbn");
    for (QChar ch: k_single_keys) {
        items_.append(TrainingItem::MakeSingleKey(ch));
    }

    // 2. Special keys: Space, Tab, F1~F8 (camera / screen).
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_Space, QStringLiteral("Space")));
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_Tab, QStringLiteral("Tab")));
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_CapsLock, QStringLiteral("CapsLock")));
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_F1, QStringLiteral("F1")));
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_F2, QStringLiteral("F2")));
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_F3, QStringLiteral("F3")));
    items_.append(
        TrainingItem::MakeSpecialKey(Qt::Key_F4, QStringLiteral("F4")));


    // 3. Control groups: Ctrl+1~5 store.
    for (int i = 1; i <= 5; ++i) {
        int key_code = Qt::Key_0 + i; // Qt::Key_1 ... Qt::Key_5
        QString label = QStringLiteral("Ctrl+%1").arg(i);
        items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, key_code,
                                              label));
    }

    // Shift+1~5 add to group.
    // NOTE: Qt often reports Shift+digit as symbol key:
    //   Shift+1 => Qt::Key_Exclam ('!')
    //   Shift+2 => Qt::Key_At ('@')
    //   Shift+3 => Qt::Key_NumberSign ('#')
    //   Shift+4 => Qt::Key_Dollar ('$')
    //   Shift+5 => Qt::Key_Percent ('%')
    struct ShiftDigitCombo {
        int key;
        const char *label;
    };

    const ShiftDigitCombo k_shift_digit_combos[] = {
        {Qt::Key_Exclam, "Shift+1"},
        {Qt::Key_At, "Shift+2"},
        {Qt::Key_NumberSign, "Shift+3"},
        {Qt::Key_Dollar, "Shift+4"},
        {Qt::Key_Percent, "Shift+5"},
    };

    for (const ShiftDigitCombo &combo: k_shift_digit_combos) {
        items_.append(TrainingItem::MakeCombo(
            Qt::ShiftModifier, combo.key, QString::fromLatin1(combo.label)));
    }

    // Some common Ctrl combos.
    items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_Q,
                                          QStringLiteral("Ctrl+Q")));
    items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_W,
                                          QStringLiteral("Ctrl+W")));
    items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_A,
                                          QStringLiteral("Ctrl+A")));
    items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_S,
                                          QStringLiteral("Ctrl+S")));

    // Some common Shift combos.
    items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_Q,
                                          QStringLiteral("Shift+Q")));
    items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_W,
                                          QStringLiteral("Shift+W")));
    items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_A,
                                          QStringLiteral("Shift+A")));
    items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_S,
                                          QStringLiteral("Shift+S")));


    // 5. Alt+F4 training item (special case handled in closeEvent).
    items_.append(TrainingItem::MakeCombo(Qt::AltModifier,
                                          Qt::Key_F4,
                                          QStringLiteral("Alt+F4")));
    items_.append(TrainingItem::MakeCombo(Qt::AltModifier,
                                          Qt::Key_F3,
                                          QStringLiteral("Alt+F3")));
    // items_.append(TrainingItem::MakeCombo(Qt::AltModifier,
    //                                       Qt::Key_F2,
    //                                       QStringLiteral("Alt+F2")));
    // items_.append(TrainingItem::MakeCombo(Qt::AltModifier,
    //                                       Qt::Key_F1,
    //                                       QStringLiteral("Alt+F1")));

    // 6. Sequences (combos).
    const QVector<QString> k_sequences = {
        QStringLiteral("1a"),
        QStringLiteral("2a"),
        QStringLiteral("3a"),
        QStringLiteral("1s"),
        QStringLiteral("2s"),
        QStringLiteral("1aa"),
        QStringLiteral("2aa"),
        QStringLiteral("3aa"),
        QStringLiteral("1qqqq"),
        QStringLiteral("2ww"),
        QStringLiteral("qwer"),
        QStringLiteral("asdf"),
        QStringLiteral("zxcv"),
        QStringLiteral("wasd"),
        QStringLiteral("1a2a"),
        QStringLiteral("1s2s"),
        QStringLiteral("4sd"),
        QStringLiteral("5vv"),
    };

    for (const QString &seq: k_sequences) {
        items_.append(TrainingItem::MakeSequence(seq));
    }
}

void TrainerWindow::StartTraining() {
    if (items_.isEmpty()) {
        return;
    }

    rounds_total_ = 0;
    rounds_correct_ = 0;
    training_ = true;
    sequence_pos_ = 0;

    UpdateErrorLabel(QString());
    elapsed_->restart();

    start_button_->setEnabled(false);
    stop_button_->setEnabled(true);

    NextItem();
    UpdateStatsLabel();

    setFocus();
}

void TrainerWindow::StopTraining() {
    training_ = false;

    start_button_->setEnabled(true);
    stop_button_->setEnabled(false);

    target_label_->setText(QStringLiteral("Stopped"));
    UpdateErrorLabel(QString());
}

void TrainerWindow::NextItem() {
    if (items_.isEmpty()) {
        return;
    }

    int index = QRandomGenerator::global()->bounded(items_.size());
    current_index_ = index;
    sequence_pos_ = 0;

    UpdateErrorLabel(QString());
    ShowCurrentItem();
}

void TrainerWindow::ShowCurrentItem() {
    if (current_index_ < 0 || current_index_ >= items_.size()) {
        target_label_->setText(QStringLiteral("No training item"));
        return;
    }

    const TrainingItem &item = items_.at(current_index_);

    if (item.type == TrainingType::kSequence) {
        int total = item.sequence.size();
        QString display =
                QStringLiteral("%1\n(%2/%3)")
                .arg(item.label)
                .arg(0)
                .arg(total);
        target_label_->setText(display);
    } else {
        target_label_->setText(item.label);
    }
}

void TrainerWindow::UpdateStatsLabel() {
    qint64 ms = elapsed_->isValid() ? elapsed_->elapsed() : 0;
    double seconds = static_cast<double>(ms) / 1000.0;
    if (seconds <= 0.0) {
        seconds = 1.0;
    }

    double rounds_per_min =
            60.0 * static_cast<double>(rounds_total_) / seconds;
    double accuracy =
            (rounds_total_ > 0)
                ? (100.0 * static_cast<double>(rounds_correct_) /
                   static_cast<double>(rounds_total_))
                : 0.0;

    QString text =
            QStringLiteral("Done: %1 / %2   Accuracy: %3%%   Speed: %4 rounds/min")
            .arg(rounds_correct_)
            .arg(rounds_total_)
            .arg(QString::number(accuracy, 'f', 1))
            .arg(QString::number(rounds_per_min, 'f', 1));

    stats_label_->setText(text);
}

void TrainerWindow::ApplyTheme() {
    if (dark_theme_) {
        // Dark theme
        setStyleSheet(
            "QWidget {"
            "  background-color: #202124;"
            "  color: #e8eaed;"
            "}"
            "QPushButton {"
            "  background-color: #303134;"
            "  color: #e8eaed;"
            "  border: 1px solid #5f6368;"
            "  border-radius: 4px;"
            "  padding: 4px 10px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #3c4043;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #292a2d;"
            "  color: #80868b;"
            "}"
            "QLabel#errorLabel {"
            "  color: #f28b82;"
            "}"
        );
        if (theme_button_ != nullptr) {
            theme_button_->setText(QStringLiteral("Dark"));
        }
    } else {
        // Light theme
        setStyleSheet(
            "QWidget {"
            "  background-color: #ffffff;"
            "  color: #202124;"
            "}"
            "QPushButton {"
            "  background-color: #f1f3f4;"
            "  color: #202124;"
            "  border: 1px solid #dadce0;"
            "  border-radius: 4px;"
            "  padding: 4px 10px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #e8eaed;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #f1f3f4;"
            "  color: #9aa0a6;"
            "}"
            "QLabel#errorLabel {"
            "  color: #d93025;"
            "}"
        );
        if (theme_button_ != nullptr) {
            theme_button_->setText(QStringLiteral("Light"));
        }
    }

    // 主题变更后保证 error label 在最上面
    if (error_label_ != nullptr) {
        error_label_->raise();
    }
}

void TrainerWindow::UpdateErrorLabel(const QString &text) {
    if (!error_label_) {
        return;
    }
    error_label_->setText(text);
    // 宽度由 resizeEvent 控制，这里只根据当前文本调整高度。
    error_label_->adjustSize();
    error_label_->raise();
}

bool TrainerWindow::IsCurrentItemAltF4() const {
    if (!training_ || current_index_ < 0 || current_index_ >= items_.size()) {
        return false;
    }
    const TrainingItem &item = items_.at(current_index_);
    return (item.type == TrainingType::kCombo &&
            item.key == Qt::Key_F4 &&
            (item.modifiers & Qt::AltModifier));
}

void TrainerWindow::ToggleTheme() {
    dark_theme_ = !dark_theme_;
    ApplyTheme();
}

void TrainerWindow::keyPressEvent(QKeyEvent *event) {
    if (!training_) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (items_.isEmpty() ||
        current_index_ < 0 ||
        current_index_ >= items_.size()) {
        return;
    }

    // Eat pure modifier keys: Shift / Ctrl / Alt / Meta
    // so they do NOT count as attempts or errors.
    int raw_key = event->key();
    if (raw_key == Qt::Key_Shift ||
        raw_key == Qt::Key_Control ||
        raw_key == Qt::Key_Alt ||
        raw_key == Qt::Key_Meta) {
        return;
    }

    // We want to eat Tab / Shift+Tab ourselves, never let Qt move focus.
    if (event->key() == Qt::Key_Tab ||
        event->key() == Qt::Key_Backtab) {
        // fall through to our own logic, do NOT call base.
    }

    // ESC stops training.
    if (event->key() == Qt::Key_Escape) {
        StopTraining();
        return;
    }

    const TrainingItem &item = items_.at(current_index_);

    switch (item.type) {
        case TrainingType::kSingleKey: {
            QString text = event->text().toLower();
            if (text.isEmpty()) {
                return;
            }

            QChar ch = text.at(0);
            QChar expected = item.sequence.isEmpty()
                                 ? QChar()
                                 : item.sequence.at(0);

            rounds_total_++;

            if (!item.sequence.isEmpty() && ch == expected) {
                // Correct: this round completed.
                rounds_correct_++;
                UpdateErrorLabel(QString());
                NextItem();
            } else {
                // Wrong: show error, stay on this key.
                QString error_text =
                        QStringLiteral("Error: expected '%1', got '%2'")
                        .arg(QString(expected))
                        .arg(QString(ch));
                UpdateErrorLabel(error_text);
            }

            UpdateStatsLabel();
            return;
        }

        case TrainingType::kCombo: {
            // Alt+F4 is a special case: many window managers do not send
            // a QKeyEvent for it, only a closeEvent. We handle it in closeEvent.
            if (IsCurrentItemAltF4()) {
                // Do nothing here; wait for closeEvent to mark success.
                return;
            }

            Qt::KeyboardModifiers mods =
                    event->modifiers() &
                    (Qt::ControlModifier |
                     Qt::ShiftModifier |
                     Qt::AltModifier |
                     Qt::MetaModifier);
            int key = event->key();

            rounds_total_++;

            if (key == item.key && mods == item.modifiers) {
                rounds_correct_++;
                UpdateErrorLabel(QString());
                NextItem();
            } else {
                QString error_text =
                        QStringLiteral("Error: press %1").arg(item.label);
                UpdateErrorLabel(error_text);
            }

            UpdateStatsLabel();
            return;
        }

        case TrainingType::kSpecialKey: {
            int key = event->key();

            rounds_total_++;

            if (key == item.key) {
                rounds_correct_++;
                UpdateErrorLabel(QString());
                NextItem();
            } else {
                QString error_text =
                        QStringLiteral("Error: press %1").arg(item.label);
                UpdateErrorLabel(error_text);
            }

            UpdateStatsLabel();
            return;
        }

        case TrainingType::kSequence: {
            QString text = event->text().toLower();
            if (text.isEmpty()) {
                return;
            }

            const QString &seq = item.sequence;
            if (seq.isEmpty()) {
                return;
            }

            if (sequence_pos_ < 0 || sequence_pos_ >= seq.size()) {
                sequence_pos_ = 0;
            }

            QChar expected = seq.at(sequence_pos_);
            QChar ch = text.at(0);

            if (ch == expected) {
                // Current char correct.
                sequence_pos_++;
                UpdateErrorLabel(QString());

                if (sequence_pos_ >= seq.size()) {
                    // Whole sequence done correctly in one go.
                    rounds_total_++;
                    rounds_correct_++;
                    NextItem();
                } else {
                    // Still in progress.
                    QString display =
                            QStringLiteral("%1\n(%2/%3)")
                            .arg(item.label)
                            .arg(sequence_pos_)
                            .arg(seq.size());
                    target_label_->setText(display);
                }
            } else {
                // Wrong during sequence:
                // 1. Count as failed attempt.
                // 2. Show error.
                // 3. Reset sequence and stay on same item.
                rounds_total_++;

                QString error_text =
                        QStringLiteral("Error: expected '%1', got '%2'")
                        .arg(QString(expected))
                        .arg(QString(ch));
                UpdateErrorLabel(error_text);

                sequence_pos_ = 0;
                QString display =
                        QStringLiteral("%1\n(%2/%3)")
                        .arg(item.label)
                        .arg(0)
                        .arg(seq.size());
                target_label_->setText(display);
            }

            UpdateStatsLabel();
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

void TrainerWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

    if (!error_label_) {
        return;
    }

    QWidget *cw = centralWidget();
    if (!cw) {
        return;
    }

    // Place floating error label in top-left corner of central widget.
    const int margin_x = 8;
    const int margin_y = 4;
    int max_width = cw->width() - margin_x * 2;
    if (max_width < 80) {
        max_width = 80;
    }

    error_label_->move(margin_x, margin_y);
    error_label_->setMaximumWidth(max_width);
    error_label_->adjustSize();
    error_label_->raise();
}

void TrainerWindow::closeEvent(QCloseEvent *event) {
    // When training Alt+F4, interpret the close request as a correct Alt+F4.
    if (training_) {
        if (IsCurrentItemAltF4()) {
            rounds_total_++;
            rounds_correct_++;
            error_label_->setText(QString());
            NextItem();
            UpdateStatsLabel();
            event->ignore(); // keep training
            return;
        }

        // Otherwise, block system close while training.
        error_label_->setText(
            QStringLiteral("Warning: system close is disabled while training."));
        event->ignore();
        return;
    }

    QMainWindow::closeEvent(event);
}
