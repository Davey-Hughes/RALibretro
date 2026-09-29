#pragma once

// Internal to the Qt target: the presenter behind host::runDialog (HostServices.cpp), in the parts its tests drive
// without a modal loop. Portable Qt: nothing here may name a Linux or POSIX API (port-roadmap.md, "Qt stays
// portable").

#include "host/HostServices.h"

#include <cstddef>
#include <vector>

class QDialog;
class QWidget;

namespace host::detail
{
  // Controls whose y lie within this many dialog units of a row's first control join that row. A combo box sits
  // 2 units above its label (components/*.cpp add it at y - 2).
  constexpr int kDialogRowTolerance = 4;

  // spec's control indices as rows: top to bottom by y, each row left to right by x.
  std::vector<std::vector<size_t>> groupDialogRows(const DialogSpec& spec, int tolerance);

  // spec's dialog, not shown: parented to parent, titled, fixed-size, its widgets holding spec's values. Each
  // control's widget is named "control<index>" (its index in spec.controls). OK accepts and Cancel rejects;
  // any other button does nothing.
  QDialog* buildDialog(const DialogSpec& spec, QWidget* parent, int rowTolerance = kDialogRowTolerance);

  // What dialog's widgets hold now, into spec's checkboxes, combo boxes and edit boxes.
  void readDialogAnswers(const QDialog& dialog, DialogSpec& spec);
}
