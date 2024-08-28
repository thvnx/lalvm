#ifndef LAL_H_
#define LAL_H_

#include <iostream>
#include <cstring>

#include "libadalang.h"

bool
print_exception (bool or_silent);

void
abort_on_exception (void);

void
dump(ada_node *node, int level);

#endif // LAL_H_
