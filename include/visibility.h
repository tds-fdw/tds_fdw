#ifndef VISIBILITY_H
#define VISIBILITY_H


#if __GNUC__ >= 4
#define PGDLLEXPORT __attribute__((visibility("default")))
#endif

#endif
