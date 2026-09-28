#include "Check.h"

// ralibretro_menu_tests [substring]
int main(int argc, char* argv[])
{
  return menutests::run(argc > 1 ? argv[1] : nullptr);
}
