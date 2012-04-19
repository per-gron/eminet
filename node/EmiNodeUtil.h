#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiNodeUtil_h
#define emilir_EmiNodeUtil_h

static const uint64_t NSECS_PER_SEC = 1000*1000*1000;
static const uint64_t MSECS_PER_SEC = 1000;

/* Have our own assert, so we are sure it does not get optimized away in
 * a release build.
 */
#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)

#endif
