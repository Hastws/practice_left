#include "trainer_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

// ===== TrainingItem helpers =====

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeSingleKey(
    QChar ch, Difficulty diff) {
    TrainingItem item;
    item.type = TrainingType::kSingleKey;
    item.sequence = QString(ch).toLower();
    item.label = QString(ch).toUpper();
    item.min_difficulty = diff;
    return item;
}

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeSequence(
    const QString &seq, Difficulty diff) {
    TrainingItem item;
    item.type = TrainingType::kSequence;
    item.sequence = seq.toLower();
    item.label = seq.toUpper();
    item.min_difficulty = diff;
    return item;
}

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeCombo(
    Qt::KeyboardModifiers mods, int key, const QString &label, Difficulty diff) {
    TrainingItem item;
    item.type = TrainingType::kCombo;
    item.key = key;
    item.modifiers = mods;
    item.label = label;
    item.min_difficulty = diff;
    return item;
}

TrainerWindow::TrainingItem TrainerWindow::TrainingItem::MakeSpecialKey(
    int key, const QString &label, Difficulty diff) {
    TrainingItem item;
    item.type = TrainingType::kSpecialKey;
    item.key = key;
    item.label = label;
    item.min_difficulty = diff;
    return item;
}

// ===== TrainerWindow implementation =====

TrainerWindow::TrainerWindow(QWidget *parent)
    : QMainWindow(parent),
      elapsed_(new QElapsedTimer()),
      countdown_timer_(new QTimer(this)) {
    setWindowTitle(QStringLiteral("左手快捷键训练器 - SC2风格"));
    resize(900, 700);
    setMinimumSize(700, 500);

    // Load saved settings and history
    LoadSettings();
    LoadHistory();

    // Initialize all training items
    InitAllTrainingItems();

    // Setup UI
    SetupMainUI();

    // Connect timer
    QObject::connect(countdown_timer_, &QTimer::timeout,
                     this, &TrainerWindow::OnTimerTick);

    // Apply theme
    ApplyTheme();

    // Filter items by current difficulty
    FilterItemsByDifficulty();
}

TrainerWindow::~TrainerWindow() {
    SaveSettings();
    delete elapsed_;
}

void TrainerWindow::SetupMainUI() {
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);

    stacked_widget_ = new QStackedWidget(this);

    SetupTrainingPage();
    SetupSettingsPage();
    SetupHistoryPage();

    stacked_widget_->addWidget(training_page_);
    stacked_widget_->addWidget(settings_page_);
    stacked_widget_->addWidget(history_page_);

    main_layout->addWidget(stacked_widget_);
}

void TrainerWindow::SetupTrainingPage() {
    training_page_ = new QWidget(this);
    auto *layout = new QVBoxLayout(training_page_);
    layout->setContentsMargins(10, 10, 10, 10);

    // Top bar with mode info and timer
    auto *top_bar = new QHBoxLayout();

    mode_label_ = new QLabel(QStringLiteral("模式: 无尽"), this);
    mode_label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    timer_label_ = new QLabel(QStringLiteral("--:--"), this);
    timer_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont timer_font = timer_label_->font();
    timer_font.setPointSize(16);
    timer_font.setBold(true);
    timer_label_->setFont(timer_font);

    top_bar->addWidget(mode_label_);
    top_bar->addStretch();
    top_bar->addWidget(timer_label_);

    // Floating error label
    error_label_ = new QLabel(training_page_);
    error_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    error_label_->setObjectName(QStringLiteral("errorLabel"));
    QFont error_font = error_label_->font();
    error_font.setPointSize(12);
    error_label_->setFont(error_font);
    error_label_->setAttribute(Qt::WA_TransparentForMouseEvents);
    error_label_->setWordWrap(true);
    error_label_->setText(QString());
    error_label_->raise();

    // Center big text
    target_label_ = new QLabel(QStringLiteral("点击 \"开始\" 开始训练"), this);
    target_label_->setAlignment(Qt::AlignCenter);
    QFont big_font = target_label_->font();
    big_font.setPointSize(48);
    big_font.setBold(true);
    target_label_->setFont(big_font);
    target_label_->setMinimumHeight(150);

    // Progress bar (for timed/challenge modes)
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setMinimum(0);
    progress_bar_->setMaximum(100);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(false);
    progress_bar_->setFixedHeight(8);
    progress_bar_->hide();

    // Stats at bottom
    stats_label_ = new QLabel(QStringLiteral("未开始"), this);
    stats_label_->setAlignment(Qt::AlignCenter);
    QFont stats_font = stats_label_->font();
    stats_font.setPointSize(14);
    stats_label_->setFont(stats_font);

    // Virtual keyboard
    SetupVirtualKeyboard();

    // Buttons
    auto *button_layout = new QHBoxLayout();

    start_button_ = new QPushButton(QStringLiteral("开始"), this);
    stop_button_ = new QPushButton(QStringLiteral("停止"), this);
    pause_button_ = new QPushButton(QStringLiteral("暂停"), this);
    settings_button_ = new QPushButton(QStringLiteral("⚙ 设置"), this);
    history_button_ = new QPushButton(QStringLiteral("📊 历史"), this);
    theme_button_ = new QPushButton(QStringLiteral("🌙"), this);

    stop_button_->setEnabled(false);
    pause_button_->setEnabled(false);

    // Prevent Tab from moving focus
    start_button_->setFocusPolicy(Qt::NoFocus);
    stop_button_->setFocusPolicy(Qt::NoFocus);
    pause_button_->setFocusPolicy(Qt::NoFocus);
    settings_button_->setFocusPolicy(Qt::NoFocus);
    history_button_->setFocusPolicy(Qt::NoFocus);
    theme_button_->setFocusPolicy(Qt::NoFocus);
    training_page_->setFocusPolicy(Qt::NoFocus);
    setFocusPolicy(Qt::StrongFocus);

    button_layout->addWidget(start_button_);
    button_layout->addWidget(pause_button_);
    button_layout->addWidget(stop_button_);
    button_layout->addStretch();
    button_layout->addWidget(settings_button_);
    button_layout->addWidget(history_button_);
    button_layout->addWidget(theme_button_);

    layout->addLayout(top_bar);
    layout->addWidget(target_label_, 1);
    layout->addWidget(progress_bar_);
    layout->addWidget(stats_label_);
    layout->addWidget(keyboard_widget_);
    layout->addLayout(button_layout);

    // Connect buttons
    QObject::connect(start_button_, &QPushButton::clicked,
                     this, &TrainerWindow::StartTraining);
    QObject::connect(stop_button_, &QPushButton::clicked,
                     this, &TrainerWindow::StopTraining);
    QObject::connect(pause_button_, &QPushButton::clicked,
                     this, [this]() {
                         if (paused_) {
                             ResumeTraining();
                         } else {
                             PauseTraining();
                         }
                     });
    QObject::connect(settings_button_, &QPushButton::clicked,
                     this, &TrainerWindow::ShowSettings);
    QObject::connect(history_button_, &QPushButton::clicked,
                     this, &TrainerWindow::ShowHistory);
    QObject::connect(theme_button_, &QPushButton::clicked,
                     this, &TrainerWindow::ToggleTheme);
}

void TrainerWindow::SetupVirtualKeyboard() {
    keyboard_widget_ = new QWidget(this);
    auto *keyboard_layout = new QVBoxLayout(keyboard_widget_);
    keyboard_layout->setSpacing(4);
    keyboard_layout->setContentsMargins(0, 10, 0, 0);

    // Keyboard rows for left hand
    const QVector<QStringList> rows = {
        {"`", "1", "2", "3", "4", "5", "6"},
        {"Tab", "Q", "W", "E", "R", "T"},
        {"Caps", "A", "S", "D", "F", "G"},
        {"Shift", "Z", "X", "C", "V", "B"},
        {"Ctrl", "Alt", "Space"}
    };

    // Function keys row
    const QStringList f_keys = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8"};

    // F-keys row
    auto *f_row_layout = new QHBoxLayout();
    f_row_layout->setSpacing(4);
    f_row_layout->addStretch();
    for (const QString &key : f_keys) {
        auto *label = new QLabel(key, this);
        label->setAlignment(Qt::AlignCenter);
        label->setFixedSize(40, 30);
        label->setObjectName(QStringLiteral("keyLabel"));
        label->setProperty("keyName", key);
        f_row_layout->addWidget(label);
        key_labels_[key] = label;
    }
    f_row_layout->addStretch();
    keyboard_layout->addLayout(f_row_layout);

    // Main keyboard rows
    for (const QStringList &row : rows) {
        auto *row_layout = new QHBoxLayout();
        row_layout->setSpacing(4);
        row_layout->addStretch();

        for (const QString &key : row) {
            auto *label = new QLabel(key, this);
            label->setAlignment(Qt::AlignCenter);
            label->setObjectName(QStringLiteral("keyLabel"));
            label->setProperty("keyName", key);

            // Set sizes for special keys
            if (key == "Space") {
                label->setFixedSize(200, 40);
            } else if (key == "Tab" || key == "Caps" || key == "Shift" || key == "Ctrl" || key == "Alt") {
                label->setFixedSize(60, 40);
            } else {
                label->setFixedSize(40, 40);
            }

            row_layout->addWidget(label);
            key_labels_[key] = label;
        }

        row_layout->addStretch();
        keyboard_layout->addLayout(row_layout);
    }

    keyboard_widget_->setVisible(show_keyboard_);
}

void TrainerWindow::SetupSettingsPage() {
    settings_page_ = new QWidget(this);
    auto *layout = new QVBoxLayout(settings_page_);
    layout->setContentsMargins(20, 20, 20, 20);

    // Title
    auto *title = new QLabel(QStringLiteral("⚙ 设置"), this);
    QFont title_font = title->font();
    title_font.setPointSize(24);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setAlignment(Qt::AlignCenter);

    // Difficulty selection
    auto *diff_group = new QGroupBox(QStringLiteral("难度级别"), this);
    auto *diff_layout = new QHBoxLayout(diff_group);
    auto *diff_label = new QLabel(QStringLiteral("选择难度:"), this);
    difficulty_combo_ = new QComboBox(this);
    difficulty_combo_->addItem(QStringLiteral("入门 - 仅单键"), static_cast<int>(Difficulty::kBeginner));
    difficulty_combo_->addItem(QStringLiteral("进阶 - 单键+特殊键+简单组合键"), static_cast<int>(Difficulty::kIntermediate));
    difficulty_combo_->addItem(QStringLiteral("高级 - 所有按键和序列"), static_cast<int>(Difficulty::kAdvanced));
    difficulty_combo_->addItem(QStringLiteral("自定义 - 选择练习类型"), static_cast<int>(Difficulty::kCustom));
    difficulty_combo_->setCurrentIndex(static_cast<int>(difficulty_));
    diff_layout->addWidget(diff_label);
    diff_layout->addWidget(difficulty_combo_);
    diff_layout->addStretch();

    // Custom options (initially hidden)
    custom_options_widget_ = new QWidget(this);
    auto *custom_layout = new QHBoxLayout(custom_options_widget_);
    custom_single_check_ = new QCheckBox(QStringLiteral("单键"), this);
    custom_special_check_ = new QCheckBox(QStringLiteral("特殊键"), this);
    custom_combo_check_ = new QCheckBox(QStringLiteral("组合键"), this);
    custom_sequence_check_ = new QCheckBox(QStringLiteral("序列"), this);
    custom_single_check_->setChecked(custom_single_keys_);
    custom_special_check_->setChecked(custom_special_keys_);
    custom_combo_check_->setChecked(custom_combos_);
    custom_sequence_check_->setChecked(custom_sequences_);
    custom_layout->addWidget(custom_single_check_);
    custom_layout->addWidget(custom_special_check_);
    custom_layout->addWidget(custom_combo_check_);
    custom_layout->addWidget(custom_sequence_check_);
    custom_layout->addStretch();
    custom_options_widget_->setVisible(difficulty_ == Difficulty::kCustom);

    // Training mode
    auto *mode_group = new QGroupBox(QStringLiteral("训练模式"), this);
    auto *mode_layout = new QVBoxLayout(mode_group);

    auto *mode_row = new QHBoxLayout();
    auto *mode_label = new QLabel(QStringLiteral("选择模式:"), this);
    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem(QStringLiteral("无尽模式 - 无时间限制"), static_cast<int>(TrainingMode::kEndless));
    mode_combo_->addItem(QStringLiteral("计时模式 - 固定时间"), static_cast<int>(TrainingMode::kTimed));
    mode_combo_->addItem(QStringLiteral("挑战模式 - 固定轮数"), static_cast<int>(TrainingMode::kChallenge));
    mode_combo_->addItem(QStringLiteral("禅模式 - 无统计，纯练习"), static_cast<int>(TrainingMode::kZen));
    mode_combo_->setCurrentIndex(static_cast<int>(mode_));
    mode_row->addWidget(mode_label);
    mode_row->addWidget(mode_combo_);
    mode_row->addStretch();
    mode_layout->addLayout(mode_row);

    auto *time_row = new QHBoxLayout();
    auto *time_label = new QLabel(QStringLiteral("时间限制(秒):"), this);
    time_spin_ = new QSpinBox(this);
    time_spin_->setRange(10, 600);
    time_spin_->setValue(time_limit_seconds_);
    time_spin_->setEnabled(mode_ == TrainingMode::kTimed);
    time_row->addWidget(time_label);
    time_row->addWidget(time_spin_);
    time_row->addStretch();
    mode_layout->addLayout(time_row);

    auto *rounds_row = new QHBoxLayout();
    auto *rounds_label = new QLabel(QStringLiteral("目标轮数:"), this);
    rounds_spin_ = new QSpinBox(this);
    rounds_spin_->setRange(5, 500);
    rounds_spin_->setValue(target_rounds_);
    rounds_spin_->setEnabled(mode_ == TrainingMode::kChallenge);
    rounds_row->addWidget(rounds_label);
    rounds_row->addWidget(rounds_spin_);
    rounds_row->addStretch();
    mode_layout->addLayout(rounds_row);

    // Other options
    auto *options_group = new QGroupBox(QStringLiteral("其他设置"), this);
    auto *options_layout = new QVBoxLayout(options_group);
    sound_check_ = new QCheckBox(QStringLiteral("启用声音反馈"), this);
    sound_check_->setChecked(sound_enabled_);
    keyboard_check_ = new QCheckBox(QStringLiteral("显示虚拟键盘"), this);
    keyboard_check_->setChecked(show_keyboard_);
    options_layout->addWidget(sound_check_);
    options_layout->addWidget(keyboard_check_);

    // Back button
    auto *back_button = new QPushButton(QStringLiteral("← 返回训练"), this);
    back_button->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(title);
    layout->addSpacing(20);
    layout->addWidget(diff_group);
    layout->addWidget(custom_options_widget_);
    layout->addWidget(mode_group);
    layout->addWidget(options_group);
    layout->addStretch();
    layout->addWidget(back_button);

    // Connections
    QObject::connect(difficulty_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &TrainerWindow::OnDifficultyChanged);
    QObject::connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &TrainerWindow::OnModeChanged);
    QObject::connect(back_button, &QPushButton::clicked,
                     this, &TrainerWindow::ShowTraining);

    QObject::connect(sound_check_, &QCheckBox::toggled, this, [this](bool checked) {
        sound_enabled_ = checked;
    });
    QObject::connect(keyboard_check_, &QCheckBox::toggled, this, [this](bool checked) {
        show_keyboard_ = checked;
        if (keyboard_widget_) {
            keyboard_widget_->setVisible(checked);
        }
    });

    // Custom type checkboxes
    QObject::connect(custom_single_check_, &QCheckBox::toggled, this, [this](bool checked) {
        custom_single_keys_ = checked;
        FilterItemsByDifficulty();
    });
    QObject::connect(custom_special_check_, &QCheckBox::toggled, this, [this](bool checked) {
        custom_special_keys_ = checked;
        FilterItemsByDifficulty();
    });
    QObject::connect(custom_combo_check_, &QCheckBox::toggled, this, [this](bool checked) {
        custom_combos_ = checked;
        FilterItemsByDifficulty();
    });
    QObject::connect(custom_sequence_check_, &QCheckBox::toggled, this, [this](bool checked) {
        custom_sequences_ = checked;
        FilterItemsByDifficulty();
    });

    QObject::connect(time_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        time_limit_seconds_ = value;
    });
    QObject::connect(rounds_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        target_rounds_ = value;
    });
}

void TrainerWindow::SetupHistoryPage() {
    history_page_ = new QWidget(this);
    auto *layout = new QVBoxLayout(history_page_);
    layout->setContentsMargins(20, 20, 20, 20);

    // Title
    auto *title = new QLabel(QStringLiteral("📊 训练历史"), this);
    QFont title_font = title->font();
    title_font.setPointSize(24);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setAlignment(Qt::AlignCenter);

    // Stats summary
    auto *summary_group = new QGroupBox(QStringLiteral("总体统计"), this);
    auto *summary_layout = new QGridLayout(summary_group);

    total_sessions_label_ = new QLabel(QStringLiteral("总训练次数: 0"), this);
    best_speed_label_ = new QLabel(QStringLiteral("最佳速度: -- 轮/分钟"), this);
    best_accuracy_label_ = new QLabel(QStringLiteral("最佳正确率: --%"), this);

    summary_layout->addWidget(total_sessions_label_, 0, 0);
    summary_layout->addWidget(best_speed_label_, 0, 1);
    summary_layout->addWidget(best_accuracy_label_, 1, 0);

    // History list
    auto *list_group = new QGroupBox(QStringLiteral("最近训练记录"), this);
    auto *list_layout = new QVBoxLayout(list_group);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    history_list_label_ = new QLabel(this);
    history_list_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    history_list_label_->setWordWrap(true);
    scroll->setWidget(history_list_label_);

    list_layout->addWidget(scroll);

    // Buttons
    auto *button_layout = new QHBoxLayout();
    auto *reset_button = new QPushButton(QStringLiteral("清空历史"), this);
    auto *back_button = new QPushButton(QStringLiteral("← 返回训练"), this);
    reset_button->setFocusPolicy(Qt::NoFocus);
    back_button->setFocusPolicy(Qt::NoFocus);

    button_layout->addWidget(reset_button);
    button_layout->addStretch();
    button_layout->addWidget(back_button);

    layout->addWidget(title);
    layout->addSpacing(20);
    layout->addWidget(summary_group);
    layout->addWidget(list_group, 1);
    layout->addLayout(button_layout);

    QObject::connect(reset_button, &QPushButton::clicked,
                     this, &TrainerWindow::ResetHistory);
    QObject::connect(back_button, &QPushButton::clicked,
                     this, &TrainerWindow::ShowTraining);
}

void TrainerWindow::InitAllTrainingItems() {
    all_items_.clear();

    // ===== 1. Single Keys (Beginner) =====
    const QString basic_keys = QStringLiteral("12345qwertasdfgzxcvb");
    for (QChar ch : basic_keys) {
        all_items_.append(TrainingItem::MakeSingleKey(ch, Difficulty::kBeginner));
    }

    // Additional single keys (Intermediate)
    const QString extra_keys = QStringLiteral("67yhun");
    for (QChar ch : extra_keys) {
        all_items_.append(TrainingItem::MakeSingleKey(ch, Difficulty::kIntermediate));
    }

    // ===== 2. Special Keys =====
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_Space, QStringLiteral("Space"), Difficulty::kBeginner));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_Tab, QStringLiteral("Tab"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_CapsLock, QStringLiteral("Caps"), Difficulty::kIntermediate));

    // F keys
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F1, QStringLiteral("F1"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F2, QStringLiteral("F2"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F3, QStringLiteral("F3"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F4, QStringLiteral("F4"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F5, QStringLiteral("F5"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F6, QStringLiteral("F6"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F7, QStringLiteral("F7"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeSpecialKey(Qt::Key_F8, QStringLiteral("F8"), Difficulty::kAdvanced));

    // ===== 3. Ctrl Combos (Control Groups) =====
    for (int i = 1; i <= 5; ++i) {
        int key_code = Qt::Key_0 + i;
        QString label = QStringLiteral("Ctrl+%1").arg(i);
        all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, key_code, label, Difficulty::kIntermediate));
    }
    for (int i = 6; i <= 9; ++i) {
        int key_code = Qt::Key_0 + i;
        QString label = QStringLiteral("Ctrl+%1").arg(i);
        all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, key_code, label, Difficulty::kAdvanced));
    }
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_0, QStringLiteral("Ctrl+0"), Difficulty::kAdvanced));

    // ===== 4. Shift Combos (Add to Group) =====
    struct ShiftDigitCombo {
        int key;
        const char *label;
        Difficulty diff;
    };
    const ShiftDigitCombo shift_combos[] = {
        {Qt::Key_Exclam, "Shift+1", Difficulty::kIntermediate},
        {Qt::Key_At, "Shift+2", Difficulty::kIntermediate},
        {Qt::Key_NumberSign, "Shift+3", Difficulty::kIntermediate},
        {Qt::Key_Dollar, "Shift+4", Difficulty::kIntermediate},
        {Qt::Key_Percent, "Shift+5", Difficulty::kIntermediate},
    };
    for (const auto &combo : shift_combos) {
        all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, combo.key,
                                                   QString::fromLatin1(combo.label), combo.diff));
    }

    // Common Ctrl letter combos
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_Q, QStringLiteral("Ctrl+Q"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_W, QStringLiteral("Ctrl+W"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_E, QStringLiteral("Ctrl+E"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_R, QStringLiteral("Ctrl+R"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_A, QStringLiteral("Ctrl+A"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_S, QStringLiteral("Ctrl+S"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_D, QStringLiteral("Ctrl+D"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_F, QStringLiteral("Ctrl+F"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_Z, QStringLiteral("Ctrl+Z"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_X, QStringLiteral("Ctrl+X"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_C, QStringLiteral("Ctrl+C"), Difficulty::kIntermediate));
    all_items_.append(TrainingItem::MakeCombo(Qt::ControlModifier, Qt::Key_V, QStringLiteral("Ctrl+V"), Difficulty::kIntermediate));

    // Common Shift letter combos
    all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_Q, QStringLiteral("Shift+Q"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_W, QStringLiteral("Shift+W"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_E, QStringLiteral("Shift+E"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_R, QStringLiteral("Shift+R"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_A, QStringLiteral("Shift+A"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::ShiftModifier, Qt::Key_S, QStringLiteral("Shift+S"), Difficulty::kAdvanced));

    // Alt combos
    all_items_.append(TrainingItem::MakeCombo(Qt::AltModifier, Qt::Key_F1, QStringLiteral("Alt+F1"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::AltModifier, Qt::Key_F2, QStringLiteral("Alt+F2"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::AltModifier, Qt::Key_F3, QStringLiteral("Alt+F3"), Difficulty::kAdvanced));
    all_items_.append(TrainingItem::MakeCombo(Qt::AltModifier, Qt::Key_F4, QStringLiteral("Alt+F4"), Difficulty::kAdvanced));

    // ===== 5. Sequences (Advanced) =====
    const QVector<QString> basic_sequences = {
        QStringLiteral("1a"), QStringLiteral("2a"), QStringLiteral("3a"),
        QStringLiteral("1s"), QStringLiteral("2s"), QStringLiteral("3s"),
        QStringLiteral("1d"), QStringLiteral("2d"), QStringLiteral("3d"),
        QStringLiteral("1q"), QStringLiteral("2q"), QStringLiteral("3q"),
    };
    for (const QString &seq : basic_sequences) {
        all_items_.append(TrainingItem::MakeSequence(seq, Difficulty::kIntermediate));
    }

    const QVector<QString> advanced_sequences = {
        QStringLiteral("1aa"), QStringLiteral("2aa"), QStringLiteral("3aa"),
        QStringLiteral("1ss"), QStringLiteral("2ss"), QStringLiteral("3ss"),
        QStringLiteral("1qqqq"), QStringLiteral("2ww"), QStringLiteral("3ee"),
        QStringLiteral("qwer"), QStringLiteral("asdf"), QStringLiteral("zxcv"),
        QStringLiteral("wasd"), QStringLiteral("1a2a"), QStringLiteral("1s2s"),
        QStringLiteral("4sd"), QStringLiteral("5vv"), QStringLiteral("1a2a3a"),
        QStringLiteral("qqqq"), QStringLiteral("aaaa"), QStringLiteral("ssss"),
        QStringLiteral("1234"), QStringLiteral("5432"), QStringLiteral("qwert"),
        QStringLiteral("asdfg"), QStringLiteral("zxcvb"),
    };
    for (const QString &seq : advanced_sequences) {
        all_items_.append(TrainingItem::MakeSequence(seq, Difficulty::kAdvanced));
    }
}

void TrainerWindow::FilterItemsByDifficulty() {
    items_.clear();

    for (const TrainingItem &item : all_items_) {
        bool include = false;

        switch (difficulty_) {
            case Difficulty::kBeginner:
                include = (item.min_difficulty == Difficulty::kBeginner);
                break;
            case Difficulty::kIntermediate:
                include = (item.min_difficulty == Difficulty::kBeginner ||
                          item.min_difficulty == Difficulty::kIntermediate);
                break;
            case Difficulty::kAdvanced:
                include = true;
                break;
            case Difficulty::kCustom:
                switch (item.type) {
                    case TrainingType::kSingleKey:
                        include = custom_single_keys_;
                        break;
                    case TrainingType::kSpecialKey:
                        include = custom_special_keys_;
                        break;
                    case TrainingType::kCombo:
                        include = custom_combos_;
                        break;
                    case TrainingType::kSequence:
                        include = custom_sequences_;
                        break;
                }
                break;
        }

        if (include) {
            items_.append(item);
        }
    }

    // Ensure at least some items
    if (items_.isEmpty()) {
        items_ = all_items_;
    }
}

void TrainerWindow::UpdateVirtualKeyboard(const QString &highlight_keys,
                                          Qt::KeyboardModifiers mods) {
    // Reset all keys
    for (auto it = key_labels_.begin(); it != key_labels_.end(); ++it) {
        it.value()->setProperty("highlighted", false);
        it.value()->setProperty("modifier", false);
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }

    // Highlight modifier keys
    if (mods & Qt::ControlModifier) {
        if (key_labels_.contains("Ctrl")) {
            key_labels_["Ctrl"]->setProperty("modifier", true);
            key_labels_["Ctrl"]->style()->unpolish(key_labels_["Ctrl"]);
            key_labels_["Ctrl"]->style()->polish(key_labels_["Ctrl"]);
        }
    }
    if (mods & Qt::ShiftModifier) {
        if (key_labels_.contains("Shift")) {
            key_labels_["Shift"]->setProperty("modifier", true);
            key_labels_["Shift"]->style()->unpolish(key_labels_["Shift"]);
            key_labels_["Shift"]->style()->polish(key_labels_["Shift"]);
        }
    }
    if (mods & Qt::AltModifier) {
        if (key_labels_.contains("Alt")) {
            key_labels_["Alt"]->setProperty("modifier", true);
            key_labels_["Alt"]->style()->unpolish(key_labels_["Alt"]);
            key_labels_["Alt"]->style()->polish(key_labels_["Alt"]);
        }
    }

    // Highlight target keys
    for (QChar ch : highlight_keys) {
        QString key = ch.toUpper();
        if (key_labels_.contains(key)) {
            key_labels_[key]->setProperty("highlighted", true);
            key_labels_[key]->style()->unpolish(key_labels_[key]);
            key_labels_[key]->style()->polish(key_labels_[key]);
        }
    }
}

void TrainerWindow::StartTraining() {
    if (items_.isEmpty()) {
        FilterItemsByDifficulty();
        if (items_.isEmpty()) {
            return;
        }
    }

    rounds_total_ = 0;
    rounds_correct_ = 0;
    training_ = true;
    paused_ = false;
    sequence_pos_ = 0;
    paused_elapsed_ = 0;

    UpdateErrorLabel(QString());
    elapsed_->restart();

    start_button_->setEnabled(false);
    stop_button_->setEnabled(true);
    pause_button_->setEnabled(true);
    pause_button_->setText(QStringLiteral("暂停"));
    settings_button_->setEnabled(false);
    history_button_->setEnabled(false);

    // Update mode label
    QString mode_text;
    switch (mode_) {
        case TrainingMode::kEndless:
            mode_text = QStringLiteral("模式: 无尽");
            progress_bar_->hide();
            break;
        case TrainingMode::kTimed:
            mode_text = QStringLiteral("模式: 计时 (%1秒)").arg(time_limit_seconds_);
            remaining_seconds_ = time_limit_seconds_;
            progress_bar_->setMaximum(time_limit_seconds_);
            progress_bar_->setValue(time_limit_seconds_);
            progress_bar_->show();
            countdown_timer_->start(1000);
            break;
        case TrainingMode::kChallenge:
            mode_text = QStringLiteral("模式: 挑战 (%1轮)").arg(target_rounds_);
            progress_bar_->setMaximum(target_rounds_);
            progress_bar_->setValue(0);
            progress_bar_->show();
            break;
        case TrainingMode::kZen:
            mode_text = QStringLiteral("模式: 禅");
            progress_bar_->hide();
            break;
    }
    mode_label_->setText(mode_text);

    NextItem();
    UpdateStatsLabel();
    UpdateTimerLabel();

    setFocus();
}

void TrainerWindow::StopTraining() {
    countdown_timer_->stop();

    // Save session record if meaningful
    if (rounds_total_ > 0 && mode_ != TrainingMode::kZen) {
        SaveSessionRecord();
    }

    training_ = false;
    paused_ = false;

    start_button_->setEnabled(true);
    stop_button_->setEnabled(false);
    pause_button_->setEnabled(false);
    settings_button_->setEnabled(true);
    history_button_->setEnabled(true);

    target_label_->setText(QStringLiteral("训练结束"));
    UpdateErrorLabel(QString());
    UpdateVirtualKeyboard();
    progress_bar_->hide();
}

void TrainerWindow::PauseTraining() {
    if (!training_ || paused_) return;

    paused_ = true;
    paused_elapsed_ = elapsed_->elapsed();
    countdown_timer_->stop();

    pause_button_->setText(QStringLiteral("继续"));
    target_label_->setText(QStringLiteral("已暂停\n按 继续 或 空格键 继续"));
}

void TrainerWindow::ResumeTraining() {
    if (!training_ || !paused_) return;

    paused_ = false;
    elapsed_->restart();

    if (mode_ == TrainingMode::kTimed) {
        countdown_timer_->start(1000);
    }

    pause_button_->setText(QStringLiteral("暂停"));
    ShowCurrentItem();
    setFocus();
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
        target_label_->setText(QStringLiteral("无训练项目"));
        return;
    }

    const TrainingItem &item = items_.at(current_index_);
    QString highlight_keys;
    Qt::KeyboardModifiers mods = Qt::NoModifier;

    if (item.type == TrainingType::kSequence) {
        int total = item.sequence.size();
        QString display = QStringLiteral("%1\n(%2/%3)")
                              .arg(item.label)
                              .arg(0)
                              .arg(total);
        target_label_->setText(display);

        // Highlight first key
        if (!item.sequence.isEmpty()) {
            highlight_keys = item.sequence.at(0);
        }
    } else if (item.type == TrainingType::kCombo) {
        target_label_->setText(item.label);
        mods = item.modifiers;
        highlight_keys = GetKeyDisplayName(item.key);
    } else if (item.type == TrainingType::kSpecialKey) {
        target_label_->setText(item.label);
        highlight_keys = item.label;
    } else {
        target_label_->setText(item.label);
        highlight_keys = item.label;
    }

    UpdateVirtualKeyboard(highlight_keys, mods);
}

void TrainerWindow::UpdateStatsLabel() {
    if (mode_ == TrainingMode::kZen) {
        stats_label_->setText(QStringLiteral("禅模式 - 专注练习"));
        return;
    }

    qint64 ms = elapsed_->isValid() ? (paused_ ? paused_elapsed_ : elapsed_->elapsed() + paused_elapsed_) : 0;
    double seconds = static_cast<double>(ms) / 1000.0;
    if (seconds <= 0.0) {
        seconds = 1.0;
    }

    double rounds_per_min = 60.0 * static_cast<double>(rounds_total_) / seconds;
    double accuracy = (rounds_total_ > 0)
                          ? (100.0 * static_cast<double>(rounds_correct_) /
                             static_cast<double>(rounds_total_))
                          : 0.0;

    QString text = QStringLiteral("完成: %1/%2   正确率: %3%   速度: %4 轮/分钟")
                       .arg(rounds_correct_)
                       .arg(rounds_total_)
                       .arg(QString::number(accuracy, 'f', 1))
                       .arg(QString::number(rounds_per_min, 'f', 1));

    stats_label_->setText(text);

    // Update progress bar for challenge mode
    if (mode_ == TrainingMode::kChallenge) {
        progress_bar_->setValue(rounds_correct_);
    }
}

void TrainerWindow::UpdateTimerLabel() {
    qint64 ms = elapsed_->isValid() ? (paused_ ? paused_elapsed_ : elapsed_->elapsed() + paused_elapsed_) : 0;

    if (mode_ == TrainingMode::kTimed) {
        int remaining = remaining_seconds_;
        int mins = remaining / 60;
        int secs = remaining % 60;
        timer_label_->setText(QStringLiteral("%1:%2")
                                  .arg(mins, 2, 10, QChar('0'))
                                  .arg(secs, 2, 10, QChar('0')));
    } else {
        int total_secs = static_cast<int>(ms / 1000);
        int mins = total_secs / 60;
        int secs = total_secs % 60;
        timer_label_->setText(QStringLiteral("%1:%2")
                                  .arg(mins, 2, 10, QChar('0'))
                                  .arg(secs, 2, 10, QChar('0')));
    }
}

void TrainerWindow::OnTimerTick() {
    if (!training_ || paused_) return;

    UpdateTimerLabel();

    if (mode_ == TrainingMode::kTimed) {
        remaining_seconds_--;
        progress_bar_->setValue(remaining_seconds_);

        if (remaining_seconds_ <= 0) {
            StopTraining();
            target_label_->setText(QStringLiteral("时间到!"));
        }
    }
}

void TrainerWindow::SaveSessionRecord() {
    SessionRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.total_rounds = rounds_total_;
    record.correct_rounds = rounds_correct_;
    record.duration_seconds = static_cast<double>(elapsed_->elapsed() + paused_elapsed_) / 1000.0;
    record.difficulty = difficulty_;
    record.mode = mode_;

    history_.prepend(record);

    // Keep only recent records
    while (history_.size() > kMaxHistoryRecords) {
        history_.removeLast();
    }

    SaveHistory();
}

void TrainerWindow::ApplyTheme() {
    QString base_style;
    QString key_style;

    if (dark_theme_) {
        base_style = QStringLiteral(
            "QWidget {"
            "  background-color: #1a1a2e;"
            "  color: #eaeaea;"
            "}"
            "QPushButton {"
            "  background-color: #16213e;"
            "  color: #eaeaea;"
            "  border: 1px solid #0f3460;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #0f3460;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #0d1b2a;"
            "  color: #5c5c5c;"
            "}"
            "QLabel#errorLabel {"
            "  color: #e94560;"
            "  font-weight: bold;"
            "}"
            "QGroupBox {"
            "  border: 1px solid #0f3460;"
            "  border-radius: 6px;"
            "  margin-top: 10px;"
            "  padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"
            "  padding: 0 5px;"
            "}"
            "QComboBox, QSpinBox {"
            "  background-color: #16213e;"
            "  color: #eaeaea;"
            "  border: 1px solid #0f3460;"
            "  border-radius: 4px;"
            "  padding: 4px 8px;"
            "}"
            "QCheckBox {"
            "  color: #eaeaea;"
            "}"
            "QProgressBar {"
            "  background-color: #16213e;"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #e94560;"
            "  border-radius: 4px;"
            "}"
            "QScrollArea {"
            "  border: none;"
            "}"
        );
        key_style = QStringLiteral(
            "QLabel#keyLabel {"
            "  background-color: #16213e;"
            "  color: #eaeaea;"
            "  border: 1px solid #0f3460;"
            "  border-radius: 4px;"
            "  font-size: 12px;"
            "  font-weight: bold;"
            "}"
            "QLabel#keyLabel[highlighted=\"true\"] {"
            "  background-color: #e94560;"
            "  color: white;"
            "  border: 2px solid #ff6b6b;"
            "}"
            "QLabel#keyLabel[modifier=\"true\"] {"
            "  background-color: #0f3460;"
            "  color: #00d9ff;"
            "  border: 2px solid #00d9ff;"
            "}"
        );
        theme_button_->setText(QStringLiteral("🌙"));
    } else {
        base_style = QStringLiteral(
            "QWidget {"
            "  background-color: #f5f5f5;"
            "  color: #333333;"
            "}"
            "QPushButton {"
            "  background-color: #ffffff;"
            "  color: #333333;"
            "  border: 1px solid #cccccc;"
            "  border-radius: 6px;"
            "  padding: 8px 16px;"
            "  font-size: 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #e8e8e8;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #f0f0f0;"
            "  color: #999999;"
            "}"
            "QLabel#errorLabel {"
            "  color: #d32f2f;"
            "  font-weight: bold;"
            "}"
            "QGroupBox {"
            "  border: 1px solid #cccccc;"
            "  border-radius: 6px;"
            "  margin-top: 10px;"
            "  padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"
            "  padding: 0 5px;"
            "}"
            "QComboBox, QSpinBox {"
            "  background-color: #ffffff;"
            "  color: #333333;"
            "  border: 1px solid #cccccc;"
            "  border-radius: 4px;"
            "  padding: 4px 8px;"
            "}"
            "QCheckBox {"
            "  color: #333333;"
            "}"
            "QProgressBar {"
            "  background-color: #e0e0e0;"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #1976d2;"
            "  border-radius: 4px;"
            "}"
            "QScrollArea {"
            "  border: none;"
            "}"
        );
        key_style = QStringLiteral(
            "QLabel#keyLabel {"
            "  background-color: #ffffff;"
            "  color: #333333;"
            "  border: 1px solid #cccccc;"
            "  border-radius: 4px;"
            "  font-size: 12px;"
            "  font-weight: bold;"
            "}"
            "QLabel#keyLabel[highlighted=\"true\"] {"
            "  background-color: #1976d2;"
            "  color: white;"
            "  border: 2px solid #1565c0;"
            "}"
            "QLabel#keyLabel[modifier=\"true\"] {"
            "  background-color: #e3f2fd;"
            "  color: #1976d2;"
            "  border: 2px solid #1976d2;"
            "}"
        );
        theme_button_->setText(QStringLiteral("☀️"));
    }

    setStyleSheet(base_style + key_style);

    if (error_label_) {
        error_label_->raise();
    }
}

void TrainerWindow::UpdateErrorLabel(const QString &text) {
    if (!error_label_) return;
    error_label_->setText(text);
    error_label_->adjustSize();
    error_label_->raise();
}

void TrainerWindow::ToggleTheme() {
    dark_theme_ = !dark_theme_;
    ApplyTheme();
}

void TrainerWindow::ShowSettings() {
    stacked_widget_->setCurrentWidget(settings_page_);
}

void TrainerWindow::ShowTraining() {
    FilterItemsByDifficulty();
    stacked_widget_->setCurrentWidget(training_page_);
    setFocus();
}

void TrainerWindow::ShowHistory() {
    // Update history display
    int total_sessions = history_.size();
    double best_speed = 0.0;
    double best_accuracy = 0.0;

    for (const SessionRecord &record : history_) {
        if (record.duration_seconds > 0) {
            double speed = 60.0 * record.total_rounds / record.duration_seconds;
            if (speed > best_speed) {
                best_speed = speed;
            }
        }
        if (record.total_rounds > 0) {
            double accuracy = 100.0 * record.correct_rounds / record.total_rounds;
            if (accuracy > best_accuracy) {
                best_accuracy = accuracy;
            }
        }
    }

    total_sessions_label_->setText(QStringLiteral("总训练次数: %1").arg(total_sessions));
    best_speed_label_->setText(QStringLiteral("最佳速度: %1 轮/分钟").arg(QString::number(best_speed, 'f', 1)));
    best_accuracy_label_->setText(QStringLiteral("最佳正确率: %1%").arg(QString::number(best_accuracy, 'f', 1)));

    // Build history list
    QString list_html;
    const QStringList diff_names = {QStringLiteral("入门"), QStringLiteral("进阶"),
                                     QStringLiteral("高级"), QStringLiteral("自定义")};
    const QStringList mode_names = {QStringLiteral("无尽"), QStringLiteral("计时"),
                                     QStringLiteral("挑战"), QStringLiteral("禅")};

    for (int i = 0; i < qMin(20, history_.size()); ++i) {
        const SessionRecord &record = history_.at(i);
        double speed = (record.duration_seconds > 0)
                           ? (60.0 * record.total_rounds / record.duration_seconds)
                           : 0.0;
        double accuracy = (record.total_rounds > 0)
                              ? (100.0 * record.correct_rounds / record.total_rounds)
                              : 0.0;

        list_html += QStringLiteral("<p><b>%1</b><br/>")
                         .arg(record.timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm")));
        list_html += QStringLiteral("难度: %1 | 模式: %2<br/>")
                         .arg(diff_names.value(static_cast<int>(record.difficulty)))
                         .arg(mode_names.value(static_cast<int>(record.mode)));
        list_html += QStringLiteral("正确: %1/%2 | 正确率: %3% | 速度: %4 轮/分钟</p>")
                         .arg(record.correct_rounds)
                         .arg(record.total_rounds)
                         .arg(QString::number(accuracy, 'f', 1))
                         .arg(QString::number(speed, 'f', 1));
    }

    if (history_.isEmpty()) {
        list_html = QStringLiteral("<p style='color: gray;'>暂无训练记录</p>");
    }

    history_list_label_->setText(list_html);

    stacked_widget_->setCurrentWidget(history_page_);
}

void TrainerWindow::OnDifficultyChanged(int index) {
    difficulty_ = static_cast<Difficulty>(difficulty_combo_->itemData(index).toInt());
    custom_options_widget_->setVisible(difficulty_ == Difficulty::kCustom);
    FilterItemsByDifficulty();
}

void TrainerWindow::OnModeChanged(int index) {
    mode_ = static_cast<TrainingMode>(mode_combo_->itemData(index).toInt());
    time_spin_->setEnabled(mode_ == TrainingMode::kTimed);
    rounds_spin_->setEnabled(mode_ == TrainingMode::kChallenge);
}

void TrainerWindow::ResetHistory() {
    history_.clear();
    SaveHistory();
    ShowHistory();
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

QString TrainerWindow::GetKeyDisplayName(int key) const {
    switch (key) {
        case Qt::Key_Space: return QStringLiteral("Space");
        case Qt::Key_Tab: return QStringLiteral("Tab");
        case Qt::Key_CapsLock: return QStringLiteral("Caps");
        case Qt::Key_F1: return QStringLiteral("F1");
        case Qt::Key_F2: return QStringLiteral("F2");
        case Qt::Key_F3: return QStringLiteral("F3");
        case Qt::Key_F4: return QStringLiteral("F4");
        case Qt::Key_F5: return QStringLiteral("F5");
        case Qt::Key_F6: return QStringLiteral("F6");
        case Qt::Key_F7: return QStringLiteral("F7");
        case Qt::Key_F8: return QStringLiteral("F8");
        case Qt::Key_Exclam: return QStringLiteral("1");
        case Qt::Key_At: return QStringLiteral("2");
        case Qt::Key_NumberSign: return QStringLiteral("3");
        case Qt::Key_Dollar: return QStringLiteral("4");
        case Qt::Key_Percent: return QStringLiteral("5");
        default:
            if (key >= Qt::Key_0 && key <= Qt::Key_9) {
                return QString::number(key - Qt::Key_0);
            }
            if (key >= Qt::Key_A && key <= Qt::Key_Z) {
                return QChar('A' + (key - Qt::Key_A));
            }
            return QString();
    }
}

void TrainerWindow::LoadSettings() {
    QSettings settings(QStringLiteral("LeftHandTrainer"), QStringLiteral("Settings"));

    difficulty_ = static_cast<Difficulty>(settings.value(QStringLiteral("difficulty"), 1).toInt());
    mode_ = static_cast<TrainingMode>(settings.value(QStringLiteral("mode"), 0).toInt());
    time_limit_seconds_ = settings.value(QStringLiteral("time_limit"), 60).toInt();
    target_rounds_ = settings.value(QStringLiteral("target_rounds"), 50).toInt();
    dark_theme_ = settings.value(QStringLiteral("dark_theme"), true).toBool();
    sound_enabled_ = settings.value(QStringLiteral("sound"), true).toBool();
    show_keyboard_ = settings.value(QStringLiteral("keyboard"), true).toBool();

    custom_single_keys_ = settings.value(QStringLiteral("custom_single"), true).toBool();
    custom_special_keys_ = settings.value(QStringLiteral("custom_special"), true).toBool();
    custom_combos_ = settings.value(QStringLiteral("custom_combo"), true).toBool();
    custom_sequences_ = settings.value(QStringLiteral("custom_sequence"), true).toBool();
}

void TrainerWindow::SaveSettings() {
    QSettings settings(QStringLiteral("LeftHandTrainer"), QStringLiteral("Settings"));

    settings.setValue(QStringLiteral("difficulty"), static_cast<int>(difficulty_));
    settings.setValue(QStringLiteral("mode"), static_cast<int>(mode_));
    settings.setValue(QStringLiteral("time_limit"), time_limit_seconds_);
    settings.setValue(QStringLiteral("target_rounds"), target_rounds_);
    settings.setValue(QStringLiteral("dark_theme"), dark_theme_);
    settings.setValue(QStringLiteral("sound"), sound_enabled_);
    settings.setValue(QStringLiteral("keyboard"), show_keyboard_);

    settings.setValue(QStringLiteral("custom_single"), custom_single_keys_);
    settings.setValue(QStringLiteral("custom_special"), custom_special_keys_);
    settings.setValue(QStringLiteral("custom_combo"), custom_combos_);
    settings.setValue(QStringLiteral("custom_sequence"), custom_sequences_);
}

void TrainerWindow::LoadHistory() {
    QSettings settings(QStringLiteral("LeftHandTrainer"), QStringLiteral("History"));

    int count = settings.beginReadArray(QStringLiteral("sessions"));
    history_.clear();

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        SessionRecord record;
        record.timestamp = settings.value(QStringLiteral("timestamp")).toDateTime();
        record.total_rounds = settings.value(QStringLiteral("total")).toInt();
        record.correct_rounds = settings.value(QStringLiteral("correct")).toInt();
        record.duration_seconds = settings.value(QStringLiteral("duration")).toDouble();
        record.difficulty = static_cast<Difficulty>(settings.value(QStringLiteral("difficulty")).toInt());
        record.mode = static_cast<TrainingMode>(settings.value(QStringLiteral("mode")).toInt());
        history_.append(record);
    }

    settings.endArray();
}

void TrainerWindow::SaveHistory() {
    QSettings settings(QStringLiteral("LeftHandTrainer"), QStringLiteral("History"));

    settings.beginWriteArray(QStringLiteral("sessions"));

    for (int i = 0; i < history_.size(); ++i) {
        settings.setArrayIndex(i);
        const SessionRecord &record = history_.at(i);
        settings.setValue(QStringLiteral("timestamp"), record.timestamp);
        settings.setValue(QStringLiteral("total"), record.total_rounds);
        settings.setValue(QStringLiteral("correct"), record.correct_rounds);
        settings.setValue(QStringLiteral("duration"), record.duration_seconds);
        settings.setValue(QStringLiteral("difficulty"), static_cast<int>(record.difficulty));
        settings.setValue(QStringLiteral("mode"), static_cast<int>(record.mode));
    }

    settings.endArray();
}

void TrainerWindow::PlaySound(bool correct) {
    // Sound implementation placeholder
    // In a full implementation, this would play audio feedback
    Q_UNUSED(correct);
}

void TrainerWindow::keyPressEvent(QKeyEvent *event) {
    // Handle resume from pause with Space
    if (paused_ && event->key() == Qt::Key_Space) {
        ResumeTraining();
        return;
    }

    if (!training_ || paused_) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (items_.isEmpty() ||
        current_index_ < 0 ||
        current_index_ >= items_.size()) {
        return;
    }

    // Track pressed keys for keyboard display
    pressed_keys_.insert(event->key());
    current_modifiers_ = event->modifiers();

    // Eat pure modifier keys
    int raw_key = event->key();
    if (raw_key == Qt::Key_Shift ||
        raw_key == Qt::Key_Control ||
        raw_key == Qt::Key_Alt ||
        raw_key == Qt::Key_Meta) {
        return;
    }

    // Handle Tab ourselves
    if (event->key() == Qt::Key_Tab ||
        event->key() == Qt::Key_Backtab) {
        // Fall through to our logic
    }

    // ESC stops training
    if (event->key() == Qt::Key_Escape) {
        StopTraining();
        return;
    }

    const TrainingItem &item = items_.at(current_index_);

    switch (item.type) {
        case TrainingType::kSingleKey: {
            QString text = event->text().toLower();
            if (text.isEmpty()) return;

            QChar ch = text.at(0);
            QChar expected = item.sequence.isEmpty() ? QChar() : item.sequence.at(0);

            rounds_total_++;

            if (!item.sequence.isEmpty() && ch == expected) {
                rounds_correct_++;
                UpdateErrorLabel(QString());
                PlaySound(true);
                NextItem();
            } else {
                QString error_text = QStringLiteral("错误: 期望 '%1', 输入 '%2'")
                                         .arg(QString(expected))
                                         .arg(QString(ch));
                UpdateErrorLabel(error_text);
                PlaySound(false);
            }

            UpdateStatsLabel();

            // Check challenge mode completion
            if (mode_ == TrainingMode::kChallenge && rounds_correct_ >= target_rounds_) {
                StopTraining();
                target_label_->setText(QStringLiteral("挑战完成!"));
            }
            return;
        }

        case TrainingType::kCombo: {
            if (IsCurrentItemAltF4()) {
                return;
            }

            Qt::KeyboardModifiers mods =
                event->modifiers() &
                (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
            int key = event->key();

            rounds_total_++;

            if (key == item.key && mods == item.modifiers) {
                rounds_correct_++;
                UpdateErrorLabel(QString());
                PlaySound(true);
                NextItem();
            } else {
                QString error_text = QStringLiteral("错误: 请按 %1").arg(item.label);
                UpdateErrorLabel(error_text);
                PlaySound(false);
            }

            UpdateStatsLabel();

            if (mode_ == TrainingMode::kChallenge && rounds_correct_ >= target_rounds_) {
                StopTraining();
                target_label_->setText(QStringLiteral("挑战完成!"));
            }
            return;
        }

        case TrainingType::kSpecialKey: {
            int key = event->key();

            rounds_total_++;

            if (key == item.key) {
                rounds_correct_++;
                UpdateErrorLabel(QString());
                PlaySound(true);
                NextItem();
            } else {
                QString error_text = QStringLiteral("错误: 请按 %1").arg(item.label);
                UpdateErrorLabel(error_text);
                PlaySound(false);
            }

            UpdateStatsLabel();

            if (mode_ == TrainingMode::kChallenge && rounds_correct_ >= target_rounds_) {
                StopTraining();
                target_label_->setText(QStringLiteral("挑战完成!"));
            }
            return;
        }

        case TrainingType::kSequence: {
            QString text = event->text().toLower();
            if (text.isEmpty()) return;

            const QString &seq = item.sequence;
            if (seq.isEmpty()) return;

            if (sequence_pos_ < 0 || sequence_pos_ >= seq.size()) {
                sequence_pos_ = 0;
            }

            QChar expected = seq.at(sequence_pos_);
            QChar ch = text.at(0);

            if (ch == expected) {
                sequence_pos_++;
                UpdateErrorLabel(QString());

                if (sequence_pos_ >= seq.size()) {
                    rounds_total_++;
                    rounds_correct_++;
                    PlaySound(true);
                    NextItem();
                } else {
                    QString display = QStringLiteral("%1\n(%2/%3)")
                                          .arg(item.label)
                                          .arg(sequence_pos_)
                                          .arg(seq.size());
                    target_label_->setText(display);

                    // Highlight next key
                    UpdateVirtualKeyboard(QString(seq.at(sequence_pos_)));
                }
            } else {
                rounds_total_++;
                PlaySound(false);

                QString error_text = QStringLiteral("错误: 期望 '%1', 输入 '%2'")
                                         .arg(QString(expected))
                                         .arg(QString(ch));
                UpdateErrorLabel(error_text);

                sequence_pos_ = 0;
                QString display = QStringLiteral("%1\n(%2/%3)")
                                      .arg(item.label)
                                      .arg(0)
                                      .arg(seq.size());
                target_label_->setText(display);

                UpdateVirtualKeyboard(QString(seq.at(0)));
            }

            UpdateStatsLabel();

            if (mode_ == TrainingMode::kChallenge && rounds_correct_ >= target_rounds_) {
                StopTraining();
                target_label_->setText(QStringLiteral("挑战完成!"));
            }
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

void TrainerWindow::keyReleaseEvent(QKeyEvent *event) {
    pressed_keys_.remove(event->key());
    current_modifiers_ = event->modifiers();
    QMainWindow::keyReleaseEvent(event);
}

void TrainerWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

    if (!error_label_) return;

    QWidget *cw = centralWidget();
    if (!cw) return;

    const int margin_x = 8;
    const int margin_y = 50;  // Below top bar
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
    if (training_) {
        if (IsCurrentItemAltF4()) {
            rounds_total_++;
            rounds_correct_++;
            UpdateErrorLabel(QString());
            PlaySound(true);
            NextItem();
            UpdateStatsLabel();

            if (mode_ == TrainingMode::kChallenge && rounds_correct_ >= target_rounds_) {
                StopTraining();
                target_label_->setText(QStringLiteral("挑战完成!"));
            }

            event->ignore();
            return;
        }

        UpdateErrorLabel(QStringLiteral("警告: 训练中无法关闭窗口"));
        event->ignore();
        return;
    }

    SaveSettings();
    QMainWindow::closeEvent(event);
}
