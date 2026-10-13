#ifndef ALIGN_H
#define ALIGN_H

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

#endif /* ALIGN_H */