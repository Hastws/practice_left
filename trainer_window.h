#pragma once

#include <QChar>
#include <QDateTime>
#include <QMainWindow>
#include <QString>
#include <QVector>
#include <QMap>
#include <QSet>
#include <Qt>

class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QProgressBar;
class QTimer;
class QElapsedTimer;
class QKeyEvent;
class QResizeEvent;
class QCloseEvent;
class QGridLayout;
class QStackedWidget;
class QSettings;
class QSoundEffect;
class QFrame;

// Member functions use UpperCamelCase.
// Qt virtuals keep original names: keyPressEvent / resizeEvent / closeEvent.
class TrainerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit TrainerWindow(QWidget *parent = nullptr);
    ~TrainerWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void StartTraining();
    void StopTraining();
    void PauseTraining();
    void ResumeTraining();
    void NextItem();
    void ToggleTheme();
    void ShowSettings();
    void ShowTraining();
    void ShowHistory();
    void OnTimerTick();
    void OnDifficultyChanged(int index);
    void OnModeChanged(int index);
    void ResetHistory();

private:
    // Training difficulty levels
    enum class Difficulty {
        kBeginner,      // Only single keys
        kIntermediate,  // Single keys + special keys + simple combos
        kAdvanced,      // All items including sequences
        kCustom         // User-selected types
    };

    // Training modes
    enum class TrainingMode {
        kEndless,       // No time limit
        kTimed,         // Fixed time, count rounds
        kChallenge,     // Fixed rounds, measure time
        kZen            // No stats, just practice
    };

    // One training unit:
    // - kSingleKey: single key like "q", "1"
    // - kCombo: with modifiers, e.g. Ctrl+1, Shift+Q, Alt+F4
    // - kSequence: string combo, e.g. "1a", "qwer"
    // - kSpecialKey: special key (Space, Tab, F1~F8)
    enum class TrainingType {
        kSingleKey,
        kCombo,
        kSequence,
        kSpecialKey
    };

    struct TrainingItem {
        TrainingType type;
        QString label;      // UI text, e.g. "Q", "Ctrl+1", "QWER"
        QString sequence;   // single/sequence lowercase string, e.g. "q", "qwer"
        int key = 0;        // for combo/special: Qt::Key_*
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        Difficulty min_difficulty = Difficulty::kBeginner;

        static TrainingItem MakeSingleKey(QChar ch, Difficulty diff = Difficulty::kBeginner);
        static TrainingItem MakeSequence(const QString &seq, Difficulty diff = Difficulty::kAdvanced);
        static TrainingItem MakeCombo(Qt::KeyboardModifiers mods, int key,
                                      const QString &label, Difficulty diff = Difficulty::kIntermediate);
        static TrainingItem MakeSpecialKey(int key, const QString &label,
                                           Difficulty diff = Difficulty::kIntermediate);
    };

    // History record for each session
    struct SessionRecord {
        QDateTime timestamp;
        int total_rounds = 0;
        int correct_rounds = 0;
        double duration_seconds = 0.0;
        Difficulty difficulty = Difficulty::kBeginner;
        TrainingMode mode = TrainingMode::kEndless;
    };

    // UI Setup
    void SetupMainUI();
    void SetupTrainingPage();
    void SetupSettingsPage();
    void SetupHistoryPage();
    void SetupVirtualKeyboard();
    void UpdateVirtualKeyboard(const QString &highlight_keys = QString(),
                               Qt::KeyboardModifiers mods = Qt::NoModifier);

    // Training logic
    void InitAllTrainingItems();
    void FilterItemsByDifficulty();
    void ShowCurrentItem();
    void UpdateStatsLabel();
    void UpdateTimerLabel();
    void SaveSessionRecord();
    void PlaySound(bool correct);

    // Theme and styling
    void ApplyTheme();
    void UpdateErrorLabel(const QString &text);
    void HighlightKey(const QString &key, bool pressed);

    // Persistence
    void LoadSettings();
    void SaveSettings();
    void LoadHistory();
    void SaveHistory();

    bool IsCurrentItemAltF4() const;
    QString GetKeyDisplayName(int key) const;

    // All possible training items
    QVector<TrainingItem> all_items_;
    // Filtered items based on current difficulty
    QVector<TrainingItem> items_;
    int current_index_ = -1;
    int sequence_pos_ = 0;

    bool training_ = false;
    bool paused_ = false;

    int rounds_total_ = 0;
    int rounds_correct_ = 0;
    int target_rounds_ = 50;        // for Challenge mode
    int time_limit_seconds_ = 60;   // for Timed mode
    int remaining_seconds_ = 0;

    Difficulty difficulty_ = Difficulty::kIntermediate;
    TrainingMode mode_ = TrainingMode::kEndless;
    bool dark_theme_ = true;
    bool sound_enabled_ = true;
    bool show_keyboard_ = true;

    // Custom mode type selection
    bool custom_single_keys_ = true;
    bool custom_special_keys_ = true;
    bool custom_combos_ = true;
    bool custom_sequences_ = true;

    // Timers
    QElapsedTimer *elapsed_ = nullptr;
    QTimer *countdown_timer_ = nullptr;
    qint64 paused_elapsed_ = 0;

    // Session history
    QVector<SessionRecord> history_;
    static constexpr int kMaxHistoryRecords = 100;

    // Key state tracking for virtual keyboard
    QSet<int> pressed_keys_;
    Qt::KeyboardModifiers current_modifiers_ = Qt::NoModifier;

    // UI Widgets - Main
    QStackedWidget *stacked_widget_ = nullptr;

    // UI Widgets - Training Page
    QWidget *training_page_ = nullptr;
    QLabel *error_label_ = nullptr;
    QLabel *target_label_ = nullptr;
    QLabel *stats_label_ = nullptr;
    QLabel *timer_label_ = nullptr;
    QLabel *mode_label_ = nullptr;
    QProgressBar *progress_bar_ = nullptr;
    QPushButton *start_button_ = nullptr;
    QPushButton *stop_button_ = nullptr;
    QPushButton *pause_button_ = nullptr;
    QPushButton *settings_button_ = nullptr;
    QPushButton *history_button_ = nullptr;
    QPushButton *theme_button_ = nullptr;

    // Virtual keyboard
    QWidget *keyboard_widget_ = nullptr;
    QMap<QString, QLabel*> key_labels_;

    // UI Widgets - Settings Page
    QWidget *settings_page_ = nullptr;
    QComboBox *difficulty_combo_ = nullptr;
    QComboBox *mode_combo_ = nullptr;
    QSpinBox *time_spin_ = nullptr;
    QSpinBox *rounds_spin_ = nullptr;
    QCheckBox *sound_check_ = nullptr;
    QCheckBox *keyboard_check_ = nullptr;
    QCheckBox *custom_single_check_ = nullptr;
    QCheckBox *custom_special_check_ = nullptr;
    QCheckBox *custom_combo_check_ = nullptr;
    QCheckBox *custom_sequence_check_ = nullptr;
    QWidget *custom_options_widget_ = nullptr;

    // UI Widgets - History Page
    QWidget *history_page_ = nullptr;
    QLabel *history_list_label_ = nullptr;
    QLabel *best_speed_label_ = nullptr;
    QLabel *best_accuracy_label_ = nullptr;
    QLabel *total_sessions_label_ = nullptr;

    // Sound effects
    QSoundEffect *correct_sound_ = nullptr;
    QSoundEffect *wrong_sound_ = nullptr;
};
