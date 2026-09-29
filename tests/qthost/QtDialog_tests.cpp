#include "../menu/Check.h"

#include "host/HostServices.h"
#include "host/qt/QtDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

#include <cstdlib>
#include <functional>
#include <memory>
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
  setenv("RALIBRETRO_AUTO_DISMISS_BOXES", "1", 1);
  const bool answer = host::runDialog(spec);
  unsetenv("RALIBRETRO_AUTO_DISMISS_BOXES");
  qApp->removeEventFilter(&counter);
  CHECK(!answer);
  CHECK_EQ(0, counter.dialogsShown);
}

TEST(QtDialog_RunDialogOffTheGuiThreadIsRefused)
{
  auto spec = emulatorLike();
  ShowCounter counter;
  qApp->installEventFilter(&counter);
  bool answer = true;
  std::thread worker([&spec, &answer]() { answer = host::runDialog(spec); });
  worker.join();
  qApp->removeEventFilter(&counter);
  CHECK(!answer);
  CHECK_EQ(0, counter.dialogsShown);
}
