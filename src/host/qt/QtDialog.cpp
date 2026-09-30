#include "host/qt/QtDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>

#include <algorithm>
#include <numeric>

namespace
{
  using Kind = host::DialogControl::Kind;

  // Win32's dialog result ids, as components/Dialog.h's callers pass them (native_compat.h)
  constexpr unsigned kIdOk = 1;
  constexpr unsigned kIdCancel = 2;

  QString nameOf(size_t index) { return QStringLiteral("control%1").arg(index); }

  bool closes(const host::DialogControl& control)
  {
    return control.kind == Kind::Button && (control.id == kIdOk || control.id == kIdCancel);
  }

  QDialogButtonBox::ButtonRole roleOf(const host::DialogControl& control)
  {
    if (control.id == kIdOk)
      return QDialogButtonBox::AcceptRole;
    if (control.id == kIdCancel)
      return QDialogButtonBox::RejectRole;
    return QDialogButtonBox::ActionRole;
  }

  QWidget* makeWidget(const host::DialogControl& control, size_t index, QWidget* parent)
  {
    QWidget* widget = nullptr;
    switch (control.kind)
    {
      case Kind::Label:
        widget = new QLabel(QString::fromStdString(control.caption), parent);
        break;

      case Kind::Checkbox:
      {
        auto* box = new QCheckBox(QString::fromStdString(control.caption), parent);
        box->setChecked(control.checked);
        widget = box;
        break;
      }

      case Kind::Combobox:
      {
        auto* combo = new QComboBox(parent); // not editable: Win32's CBS_DROPDOWNLIST
        for (const auto& option : control.options)
          combo->addItem(QString::fromStdString(option));
        combo->setCurrentIndex(control.selected); // -1, or out of range, selects nothing
        widget = combo;
        break;
      }

      case Kind::Editbox:
        if (control.lines > 1)
        {
          auto* edit = new QPlainTextEdit(QString::fromStdString(control.text), parent);
          edit->setReadOnly(control.readOnly);
          widget = edit;
        }
        else
        {
          auto* edit = new QLineEdit(QString::fromStdString(control.text), parent);
          edit->setReadOnly(control.readOnly);
          widget = edit;
        }
        break;

      case Kind::Button:
        widget = new QPushButton(QString::fromStdString(control.caption), parent);
        break;
    }

    widget->setObjectName(nameOf(index));
    return widget;
  }
}

std::vector<std::vector<size_t>> host::detail::groupDialogRows(const DialogSpec& spec, int tolerance)
{
  std::vector<size_t> order(spec.controls.size());
  std::iota(order.begin(), order.end(), size_t{0});
  // top to bottom; stable, so controls at the same y keep the order they were added in
  std::stable_sort(order.begin(), order.end(),
                   [&spec](size_t a, size_t b) { return spec.controls[a].y < spec.controls[b].y; });

  std::vector<std::vector<size_t>> rows;
  int rowTop = 0;
  for (size_t index : order)
  {
    const int y = spec.controls[index].y;
    if (rows.empty() || y - rowTop > tolerance)
    {
      rows.emplace_back();
      rowTop = y;
    }
    rows.back().push_back(index);
  }

  for (auto& row : rows)
    std::stable_sort(row.begin(), row.end(),
                     [&spec](size_t a, size_t b) { return spec.controls[a].x < spec.controls[b].x; });
  return rows;
}

QDialog* host::detail::buildDialog(const DialogSpec& spec, QWidget* parent, int rowTolerance)
{
  auto* dialog = new QDialog(parent);
  dialog->setWindowTitle(QString::fromStdString(spec.title));

  auto* form = new QFormLayout(dialog);
  form->setSizeConstraint(QLayout::SetFixedSize); // Windows' modal frame: the dialog is not resized
  // labels on the left, as in Windows' dialogs, whatever the style says (KDE's puts a form's on the right)
  form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  std::vector<QWidget*> widgets(spec.controls.size(), nullptr);
  for (size_t index = 0; index < spec.controls.size(); ++index)
    widgets[index] = makeWidget(spec.controls[index], index, dialog);

  for (const auto& row : groupDialogRows(spec, rowTolerance))
  {
    const auto kindAt = [&spec, &row](size_t i) { return spec.controls[row[i]].kind; };
    const bool onlyButtons = std::all_of(row.begin(), row.end(),
                                         [&spec](size_t index) { return spec.controls[index].kind == Kind::Button; });
    const bool closesTheDialog = std::any_of(row.begin(), row.end(),
                                             [&spec](size_t index) { return closes(spec.controls[index]); });

    if (row.size() == 2 && kindAt(0) == Kind::Label && (kindAt(1) == Kind::Combobox || kindAt(1) == Kind::Editbox))
    {
      form->addRow(widgets[row[0]], widgets[row[1]]);
    }
    else if (onlyButtons && closesTheDialog)
    {
      auto* box = new QDialogButtonBox(dialog);
      for (size_t index : row)
        box->addButton(static_cast<QPushButton*>(widgets[index]), roleOf(spec.controls[index]));
      form->addRow(box);
    }
    else if (row.size() == 1)
    {
      form->addRow(widgets[row[0]]);
    }
    else
    {
      auto* line = new QHBoxLayout;
      for (size_t index : row)
        line->addWidget(widgets[index]);
      form->addRow(line);
    }
  }

  // OK and Cancel close the dialog wherever they sit; any other button does nothing, as in Windows' Dialog
  for (size_t index = 0; index < spec.controls.size(); ++index)
  {
    const DialogControl& control = spec.controls[index];
    if (control.kind != Kind::Button)
      continue;

    auto* button = static_cast<QPushButton*>(widgets[index]);
    button->setDefault(control.isDefault);
    if (control.id == kIdOk)
      QObject::connect(button, &QPushButton::clicked, dialog, &QDialog::accept);
    else if (control.id == kIdCancel)
      QObject::connect(button, &QPushButton::clicked, dialog, &QDialog::reject);
  }

  return dialog;
}

void host::detail::readDialogAnswers(const QDialog& dialog, DialogSpec& spec)
{
  for (size_t index = 0; index < spec.controls.size(); ++index)
  {
    DialogControl& control = spec.controls[index];
    const QString name = nameOf(index);
    switch (control.kind)
    {
      case Kind::Checkbox:
        if (const auto* box = dialog.findChild<QCheckBox*>(name))
          control.checked = box->isChecked();
        break;

      case Kind::Combobox:
        if (const auto* combo = dialog.findChild<QComboBox*>(name))
          control.selected = combo->currentIndex();
        break;

      case Kind::Editbox:
        if (const auto* line = dialog.findChild<QLineEdit*>(name))
          control.text = line->text().toStdString();
        else if (const auto* multi = dialog.findChild<QPlainTextEdit*>(name))
          control.text = multi->toPlainText().toStdString();
        break;

      case Kind::Label:
      case Kind::Button:
        break;
    }
  }
}
