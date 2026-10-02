#include "../menu/Check.h"

#include "host/HostServices.h"
#include "host/IHostEvents.h"
#include "host/qt/QtDialog.h"
#include "host/qt/QtHost.h"
// Components.h:85, the NDEBUG debug() stub, leaves its 'fmt' parameter unused
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libretro/Components.h"
#pragma GCC diagnostic pop

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
  using Kind = host::DialogControl::Kind;

  host::DialogControl control(Kind kind, unsigned id, int x, int y, int w, int h, const char* caption = "")
  {
    host::DialogControl c;
    c.kind = kind;
    c.id = id;
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    c.caption = caption;
    return c;
  }

  // Emulator Settings' shape (components/Config.cpp): a label with its combo box 2 units above it, two
  // checkboxes, then OK and Cancel.
  host::DialogSpec emulatorLike()
  {
    host::DialogSpec spec;
    spec.title = "Emulator Settings";
    spec.controls.push_back(control(Kind::Label, 51003, 0, 0, 50, 8, "Fast Forward Ratio"));
    auto combo = control(Kind::Combobox, 51001, 55, -2, 115, 12);
    combo.options = {"2x", "3x", "4x"};
    combo.selected = 1;
    spec.controls.push_back(combo);
    auto audio = control(Kind::Checkbox, 51002, 0, 15, 160, 8, "Play Audio while Fast Forwarding");
    audio.checked = true;
    spec.controls.push_back(audio);
    spec.controls.push_back(control(Kind::Checkbox, 51004, 0, 30, 160, 8, "Show Indicator when Paused or Fast Forwarding"));
    auto ok = control(Kind::Button, 1, 65, 45, 50, 14, "OK");
    ok.isDefault = true;
    spec.controls.push_back(ok);
    spec.controls.push_back(control(Kind::Button, 2, 120, 45, 50, 14, "Cancel"));
    return spec;
  }

  template <typename T>
  T* widget(const QDialog& dialog, size_t index)
  {
    return dialog.findChild<T*>(QStringLiteral("control%1").arg(index));
  }

  // Runs answer on the dialog runDialog opens, from inside its modal loop. If none is open, rejects every
  // dialog so the test fails instead of hanging.
  void answerNextModal(std::function<void(QDialog&)> answer)
  {
    QTimer::singleShot(0, [answer]() {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      CHECK(dialog != nullptr);
      if (dialog != nullptr)
      {
        answer(*dialog);
        return;
      }
      for (QWidget* top : QApplication::topLevelWidgets())
        if (auto* any = qobject_cast<QDialog*>(top))
          any->reject();
    });
  }

  // What KDE's Breeze answers, and so what a form that does not choose gets there: labels right aligned. Set on a
  // dialog before it is first shown: QFormLayout caches the style's answer on first use.
  struct RightAligningStyle : QProxyStyle
  {
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override
    {
      if (hint == QStyle::SH_FormLayoutLabelAlignment)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
  };

  // Counts dialogs shown while it is installed on the application.
  struct ShowCounter : QObject
  {
    int dialogsShown = 0;

    bool eventFilter(QObject* watched, QEvent* event) override
    {
      if (event->type() == QEvent::Show && qobject_cast<QDialog*>(watched) != nullptr)
        ++dialogsShown;
      return false;
    }
  };

  // The logger runDialog reports to once a host is up. LoggerComponent's one pure virtual is
  // log(level, line, length); QtHost_tests.cpp has the same fake, but file-local to that translation unit.
  class TestLogger : public libretro::LoggerComponent
  {
  public:
    std::vector<std::string> lines;

    void log(enum retro_log_level, const char* line, size_t length) override
    {
      lines.emplace_back(line, length);
    }
  };

  // A do-nothing IHostEvents: QtHost::create needs one, but these tests never trigger any of it.
  struct NoEvents : host::IHostEvents
  {
    void onKey(SDL_Keycode, Uint16, bool, bool) override {}
    void onMouseMove(int, int) override {}
    void onMouseButton(host::MouseButton, bool) override {}
    void onResized(int, int) override {}
    void onCloseRequested() override {}
    void onMenuCommand(size_t, int) override {}
    void onAbout() override {}
    void onExposed() override {}
  };
}

TEST(QtDialog_RowsGroupALabelWithTheComboBoxAboveIt)
{
  const auto rows = host::detail::groupDialogRows(emulatorLike(), host::detail::kDialogRowTolerance);
  CHECK_EQ(size_t{4}, rows.size());
  if (rows.size() != 4)
    return;
  CHECK(rows[0] == (std::vector<size_t>{0, 1})); // the label left of its combo box
  CHECK(rows[1] == (std::vector<size_t>{2}));
  CHECK(rows[2] == (std::vector<size_t>{3}));
  CHECK(rows[3] == (std::vector<size_t>{4, 5}));
}

TEST(QtDialog_ZeroToleranceSplitsTheLabelFromItsComboBox)
{
  const auto rows = host::detail::groupDialogRows(emulatorLike(), 0);
  CHECK_EQ(size_t{5}, rows.size());
  if (rows.size() != 5)
    return;
  CHECK(rows[0] == (std::vector<size_t>{1})); // the combo box, 2 units higher, comes first
  CHECK(rows[1] == (std::vector<size_t>{0}));
}

TEST(QtDialog_BuildsOneWidgetPerControlWithTheSpecsValues)
{
  const auto spec = emulatorLike();
  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  CHECK(dialog->windowTitle() == QStringLiteral("Emulator Settings"));
  CHECK(dialog->layout() != nullptr && dialog->layout()->sizeConstraint() == QLayout::SetFixedSize);

  auto* label = widget<QLabel>(*dialog, 0);
  auto* combo = widget<QComboBox>(*dialog, 1);
  auto* audio = widget<QCheckBox>(*dialog, 2);
  auto* indicator = widget<QCheckBox>(*dialog, 3);
  auto* ok = widget<QPushButton>(*dialog, 4);
  auto* cancel = widget<QPushButton>(*dialog, 5);
  CHECK(label && combo && audio && indicator && ok && cancel);
  if (!(label && combo && audio && indicator && ok && cancel))
    return;

  CHECK(label->text() == QStringLiteral("Fast Forward Ratio"));
  CHECK_EQ(3, combo->count());
  CHECK_EQ(1, combo->currentIndex());
  CHECK(!combo->isEditable());
  CHECK(audio->isChecked());
  CHECK(!indicator->isChecked());
  CHECK(ok->isDefault());
  CHECK(!cancel->isDefault());
}

TEST(QtDialog_ALabelAndItsFieldShareAFormRow)
{
  const auto spec = emulatorLike();
  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  auto* form = qobject_cast<QFormLayout*>(dialog->layout());
  CHECK(form != nullptr);
  if (form == nullptr)
    return;

  int labelRow = -1, fieldRow = -1;
  QFormLayout::ItemRole labelRole = QFormLayout::SpanningRole, fieldRole = QFormLayout::SpanningRole;
  form->getWidgetPosition(widget<QLabel>(*dialog, 0), &labelRow, &labelRole);
  form->getWidgetPosition(widget<QComboBox>(*dialog, 1), &fieldRow, &fieldRole);
  CHECK_EQ(0, labelRow);
  CHECK_EQ(labelRow, fieldRow);
  CHECK(labelRole == QFormLayout::LabelRole);
  CHECK(fieldRole == QFormLayout::FieldRole);
}

// Windows' dialogs have their labels on the left; KDE's style would put a form's on the right.
TEST(QtDialog_LabelsAreOnTheLeftWhateverTheStyle)
{
  RightAligningStyle style;
  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(emulatorLike(), nullptr));
  dialog->setStyle(&style);
  auto* form = qobject_cast<QFormLayout*>(dialog->layout());
  CHECK(form != nullptr);
  if (form != nullptr)
    CHECK_EQ(static_cast<int>(Qt::AlignLeft), static_cast<int>(form->labelAlignment() & Qt::AlignHorizontal_Mask));
}

TEST(QtDialog_EditBoxesAndOtherButtons)
{
  host::DialogSpec spec;
  spec.title = "Edits";
  auto line = control(Kind::Editbox, 10, 0, 0, 100, 12);
  line.text = "one line";
  spec.controls.push_back(line);
  auto multi = control(Kind::Editbox, 11, 0, 20, 100, 12);
  multi.text = "first\nsecond";
  multi.lines = 3;
  multi.readOnly = true;
  spec.controls.push_back(multi);
  spec.controls.push_back(control(Kind::Button, 99, 0, 80, 50, 14, "Other"));

  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  auto* one = widget<QLineEdit>(*dialog, 0);
  auto* many = widget<QPlainTextEdit>(*dialog, 1);
  auto* other = widget<QPushButton>(*dialog, 2);
  CHECK(one && many && other);
  if (!(one && many && other))
    return;

  CHECK(one->text() == QStringLiteral("one line"));
  CHECK(!one->isReadOnly());
  CHECK(many->toPlainText() == QStringLiteral("first\nsecond"));
  CHECK(many->isReadOnly());

  QSignalSpy finished(dialog.get(), &QDialog::finished);
  dialog->show();
  QTest::mouseClick(other, Qt::LeftButton);
  CHECK(dialog->isVisible()); // a button other than OK and Cancel does nothing, as in Windows' Dialog
  CHECK_EQ(0, int(finished.count()));
}

TEST(QtDialog_OkAcceptsAndTheAnswersAreRead)
{
  auto spec = emulatorLike();
  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  QSignalSpy accepted(dialog.get(), &QDialog::accepted);
  dialog->show();
  widget<QCheckBox>(*dialog, 3)->setChecked(true);
  widget<QComboBox>(*dialog, 1)->setCurrentIndex(2);
  QTest::mouseClick(widget<QPushButton>(*dialog, 4), Qt::LeftButton);
  CHECK_EQ(1, int(accepted.count()));

  host::detail::readDialogAnswers(*dialog, spec);
  CHECK(spec.controls[3].checked);
  CHECK_EQ(2, spec.controls[1].selected);
  CHECK(spec.controls[2].checked); // left as it was: still checked
}

TEST(QtDialog_CancelEscAndCloseReject)
{
  for (int way = 0; way < 3; ++way)
  {
    const auto spec = emulatorLike();
    std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
    QSignalSpy rejected(dialog.get(), &QDialog::rejected);
    dialog->show();
    if (way == 0)
      QTest::mouseClick(widget<QPushButton>(*dialog, 5), Qt::LeftButton);
    else if (way == 1)
      QTest::keyClick(dialog.get(), Qt::Key_Escape);
    else
      dialog->close();
    CHECK_EQ(1, int(rejected.count()));
    CHECK(!dialog->isVisible());
  }
}

TEST(QtDialog_RunDialogOkWritesTheAnswersIntoTheSpec)
{
  auto spec = emulatorLike();
  answerNextModal([](QDialog& dialog) {
    widget<QCheckBox>(dialog, 3)->setChecked(true);
    QTest::mouseClick(widget<QPushButton>(dialog, 4), Qt::LeftButton);
  });
  CHECK(host::runDialog(spec));
  CHECK(spec.controls[3].checked);
}

TEST(QtDialog_RunDialogEscLeavesTheSpecAlone)
{
  auto spec = emulatorLike();
  answerNextModal([](QDialog& dialog) {
    widget<QCheckBox>(dialog, 3)->setChecked(true);
    QTest::keyClick(&dialog, Qt::Key_Escape);
  });
  CHECK(!host::runDialog(spec));
  CHECK(!spec.controls[3].checked);
}

TEST(QtDialog_RunDialogAutoDismissedShowsNothing)
{
  auto spec = emulatorLike();
  ShowCounter counter;
  qApp->installEventFilter(&counter);
  TestLogger logger;
  NoEvents events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));
  const size_t linesBefore = logger.lines.size();

  menutests::setEnv("RALIBRETRO_AUTO_DISMISS_BOXES", "1");
  const bool answer = host::runDialog(spec);
  menutests::unsetEnv("RALIBRETRO_AUTO_DISMISS_BOXES");
  qApp->removeEventFilter(&counter);
  CHECK(!answer);
  CHECK_EQ(0, counter.dialogsShown);

  CHECK_EQ(linesBefore + 1, logger.lines.size());
  if (logger.lines.size() == linesBefore + 1)
    CHECK_EQ(std::string("[QT] dialog auto-dismissed: Emulator Settings"), logger.lines.back());
}

TEST(QtDialog_RunDialogOffTheGuiThreadIsRefused)
{
  auto spec = emulatorLike();
  ShowCounter counter;
  qApp->installEventFilter(&counter);
  TestLogger logger;
  NoEvents events;
  host::QtHost host(&logger, events);
  CHECK(host.create("test", 64, 64));
  const size_t linesBefore = logger.lines.size();

  bool answer = true;
  std::thread worker([&spec, &answer]() { answer = host::runDialog(spec); });
  worker.join();
  qApp->removeEventFilter(&counter);
  CHECK(!answer);
  CHECK_EQ(0, counter.dialogsShown);

  CHECK_EQ(linesBefore + 1, logger.lines.size());
  if (logger.lines.size() == linesBefore + 1)
  {
    const std::string& line = logger.lines.back();
    CHECK(line.find("Emulator Settings") != std::string::npos);
    CHECK(line.find("off the GUI thread") != std::string::npos); // refusedOffGuiThread's own wording
  }
}

TEST(QtDialog_ComboBoxesWithNoSelectionReadBackMinusOne)
{
  // Win32's CB_GETCURSEL returns -1 for "nothing selected"; Qt's setCurrentIndex clamps any out-of-range
  // index (negative, or past the last option) to the same -1, so all three read back that way.
  host::DialogSpec spec;
  spec.title = "Combos";
  auto none = control(Kind::Combobox, 1, 0, 0, 100, 12);
  none.options = {"one", "two"};
  none.selected = -1;
  spec.controls.push_back(none);
  auto negative = control(Kind::Combobox, 2, 0, 20, 100, 12);
  negative.options = {"one", "two"};
  negative.selected = -2;
  spec.controls.push_back(negative);
  auto tooFar = control(Kind::Combobox, 3, 0, 40, 100, 12);
  tooFar.options = {"one", "two"};
  tooFar.selected = 99;
  spec.controls.push_back(tooFar);

  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  host::detail::readDialogAnswers(*dialog, spec);
  CHECK_EQ(-1, spec.controls[0].selected);
  CHECK_EQ(-1, spec.controls[1].selected);
  CHECK_EQ(-1, spec.controls[2].selected);
}

TEST(QtDialog_EditBoxesReadBackWhatTheUserTyped)
{
  host::DialogSpec spec;
  spec.title = "Edits";
  auto line = control(Kind::Editbox, 10, 0, 0, 100, 12);
  line.text = "one line";
  spec.controls.push_back(line);
  auto multi = control(Kind::Editbox, 11, 0, 20, 100, 12);
  multi.text = "first\nsecond";
  multi.lines = 3;
  spec.controls.push_back(multi);

  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  widget<QLineEdit>(*dialog, 0)->setText(QStringLiteral("changed"));
  widget<QPlainTextEdit>(*dialog, 1)->setPlainText(QStringLiteral("new\ntext"));

  host::detail::readDialogAnswers(*dialog, spec);
  CHECK_EQ(std::string("changed"), spec.controls[0].text);
  CHECK_EQ(std::string("new\ntext"), spec.controls[1].text);
}

TEST(QtDialog_EnterAcceptsTheDefaultButton)
{
  // Win32's Dialog: pressing Enter clicks whichever button is the default, same as clicking it.
  auto spec = emulatorLike();
  std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr));
  QSignalSpy accepted(dialog.get(), &QDialog::accepted);
  dialog->show();
  QTest::keyClick(dialog.get(), Qt::Key_Return);
  CHECK_EQ(1, int(accepted.count()));
}
