#pragma once

#include <QChar>
#include <QMainWindow>
#include <QString>
#include <QVector>
#include <Qt>

class QLabel;
class QPushButton;
class QElapsedTimer;
class QKeyEvent;
class QResizeEvent;
class QCloseEvent;

// Member functions use UpperCamelCase.
// Qt virtuals keep original names: keyPressEvent / resizeEvent / closeEvent.
class TrainerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit TrainerWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

private slots:
    void StartTraining();

    void StopTraining();

    void NextItem();

    void ToggleTheme(); // switch between light / dark

private:
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
        QString label; // UI text, e.g. "Q", "Ctrl+1", "QWER"
        QString sequence; // single/sequence lowercase string, e.g. "q", "qwer"
        int key = 0; // for combo/special: Qt::Key_*
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;

        static TrainingItem MakeSingleKey(QChar ch);

        static TrainingItem MakeSequence(const QString &seq);

        static TrainingItem MakeCombo(Qt::KeyboardModifiers mods,
                                      int key,
                                      const QString &label);

        static TrainingItem MakeSpecialKey(int key, const QString &label);
    };

    void InitTrainingItems();

    void ShowCurrentItem();

    void UpdateStatsLabel();

    void ApplyTheme(); // apply light / dark stylesheet
    void UpdateErrorLabel(const QString &text); // set text + autosize + raise

    bool IsCurrentItemAltF4() const;

    QVector<TrainingItem> items_;
    int current_index_ = -1; // index in items_
    int sequence_pos_ = 0; // position inside current sequence

    bool training_ = false;

    int rounds_total_ = 0; // number of attempts
    int rounds_correct_ = 0; // number of successful rounds

    bool dark_theme_ = true; // false = light, true = dark

    QElapsedTimer *elapsed_ = nullptr;

    QLabel *error_label_ = nullptr; // floating warning in top-left
    QLabel *target_label_ = nullptr; // center big text
    QLabel *stats_label_ = nullptr; // bottom stats
    QPushButton *start_button_ = nullptr;
    QPushButton *stop_button_ = nullptr;
    QPushButton *theme_button_ = nullptr; // theme toggle
};
