#include "Check.h"

#include "CallSites.h"
#include "host/qt/QtDialog.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace
{
  using Kind = host::DialogControl::Kind;

  bool closes(const host::DialogControl& control)
  {
    return control.kind == Kind::Button && (control.id == 1 || control.id == 2); // IDOK, IDCANCEL
  }

  // What the design's layout check asks of a shown dialog built from spec: one line per problem.
  std::vector<std::string> layoutProblems(QDialog& dialog, const host::DialogSpec& spec)
  {
    std::vector<std::string> problems;
    const auto name = [&spec](size_t index) {
      const auto& caption = spec.controls[index].caption;
      return "control " + std::to_string(index) + (caption.empty() ? std::string() : " \"" + caption + "\"");
    };

    std::vector<QRect> rects(spec.controls.size());
    for (size_t index = 0; index < spec.controls.size(); ++index)
    {
      auto* widget = dialog.findChild<QWidget*>(QStringLiteral("control%1").arg(index));
      if (widget == nullptr || !widget->isVisible())
      {
        problems.push_back(name(index) + " has no visible widget");
        continue;
      }
      rects[index] = QRect(widget->mapTo(&dialog, QPoint(0, 0)), widget->size());
      if (!dialog.rect().contains(rects[index]))
        problems.push_back(name(index) + " is outside the dialog");
      if (widget->width() < widget->sizeHint().width())
        problems.push_back(name(index) + " is narrower than its text");
    }

    for (size_t a = 0; a < rects.size(); ++a)
      for (size_t b = a + 1; b < rects.size(); ++b)
        if (!rects[a].isNull() && !rects[b].isNull() && rects[a].intersects(rects[b]))
          problems.push_back(name(a) + " overlaps " + name(b));

    // a label followed by its combo box or edit box sits beside it, on its row
    for (size_t index = 0; index + 1 < spec.controls.size(); ++index)
    {
      const Kind next = spec.controls[index + 1].kind;
      if (spec.controls[index].kind != Kind::Label || (next != Kind::Combobox && next != Kind::Editbox))
        continue;
      const QRect& label = rects[index];
      const QRect& field = rects[index + 1];
      if (label.isNull() || field.isNull())
        continue;
      if (label.center().y() < field.top() || label.center().y() > field.bottom() || label.right() > field.left())
        problems.push_back(name(index) + " is not beside its field");
    }

    // OK and Cancel are the last row
    int lowestOther = 0;
    for (size_t index = 0; index < spec.controls.size(); ++index)
      if (!closes(spec.controls[index]) && !rects[index].isNull())
        lowestOther = std::max(lowestOther, rects[index].bottom());
    for (size_t index = 0; index < spec.controls.size(); ++index)
      if (closes(spec.controls[index]) && !rects[index].isNull() && rects[index].top() <= lowestOther)
        problems.push_back(name(index) + " is not in the last row");

    return problems;
  }

  std::unique_ptr<QDialog> shown(const host::DialogSpec& spec, int tolerance)
  {
    std::unique_ptr<QDialog> dialog(host::detail::buildDialog(spec, nullptr, tolerance));
    dialog->show();
    QApplication::processEvents();
    return dialog;
  }
}

TEST(DialogLayout_TheThreeCallSitesAreCaptured)
{
  const auto specs = dialoglayout::captureRealDialogs();
  CHECK_EQ(size_t{3}, specs.size());
  if (specs.size() != 3)
    return;
  CHECK_EQ(std::string("Video Settings"), specs[0].title);
  CHECK_EQ(std::string("Emulator Settings"), specs[1].title);
  CHECK_EQ(std::string("Saving Settings"), specs[2].title);
  // Emulator Settings' first combo box, added at y - 2 with y = 0: -2 once read as a signed short
  CHECK_EQ(-2, specs[1].controls.size() > 1 ? specs[1].controls[1].y : 0);
}

// Saving Settings' path options name the folders States builds, with the separator it builds them with: '/' here
TEST(DialogLayout_SavingPathOptionsUseForwardSlashes)
{
  const auto specs = dialoglayout::captureRealDialogs();
  CHECK_EQ(size_t{3}, specs.size());
  if (specs.size() != 3)
    return;
  std::vector<std::string> options;
  for (const auto& control : specs[2].controls)
    if (control.kind == Kind::Combobox)
      options.insert(options.end(), control.options.begin(), control.options.end());
  CHECK(!options.empty());
  for (const auto& option : options)
    if (option.find('\\') != std::string::npos)
      menutests::fail(__FILE__, __LINE__, "Saving Settings option \"" + option + "\" has a backslash");
  CHECK(std::find(options.begin(), options.end(), "Saves/[System]/[Core]") != options.end());
}

TEST(DialogLayout_TheThreeDialogsLayOutCleanly)
{
  const char* shots = std::getenv("RALIBRETRO_DIALOG_SHOTS"); // a directory for the owner to look at
  for (const auto& spec : dialoglayout::captureRealDialogs())
  {
    auto dialog = shown(spec, host::detail::kDialogRowTolerance);
    for (const auto& problem : layoutProblems(*dialog, spec))
      menutests::fail(__FILE__, __LINE__, spec.title + ": " + problem);
    if (shots != nullptr && shots[0] != '\0')
    {
      const QString path = QDir(QString::fromUtf8(shots)).filePath(QString::fromStdString(spec.title) + ".png");
      CHECK(dialog->grab().save(path));
    }
  }
}

// The check can fail: at 0 tolerance each label lands on a row of its own, apart from its combo box.
TEST(DialogLayout_ZeroToleranceFailsTheCheck)
{
  for (const auto& spec : dialoglayout::captureRealDialogs())
  {
    auto dialog = shown(spec, 0);
    bool apart = false;
    for (const auto& problem : layoutProblems(*dialog, spec))
      apart = apart || problem.find("is not beside its field") != std::string::npos;
    CHECK(apart);
  }
}
