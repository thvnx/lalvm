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

void
dump_image(ada_node *node, int level);

class AdaNode {
public:
    ada_node node;
    AdaNode (ada_node n) {node = n;}
};

AdaNode*
convert(ada_node *node);

#endif // LAL_H_
