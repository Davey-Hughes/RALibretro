#pragma once

// Internal to the Qt target: what a live QtHost publishes for the host services
// (HostServices.cpp). QtHost sets and clears it on the main thread, under
// s_hostMutex; postToMainThread reads s_postTarget from any thread under the
// same lock. The dialogs and messageBox run on the main thread only.

#include <mutex>

class QObject;
class QWidget;

namespace libretro
{
  class LoggerComponent;
}

namespace host::detail
{
  inline std::mutex s_hostMutex;
  inline QObject* s_postTarget = nullptr;               // postToMainThread's target
  inline QWidget* s_dialogParent = nullptr;             // the dialogs' parent
  inline libretro::LoggerComponent* s_logger = nullptr; // the host services' warnings
}
