#ifndef hbw_malloc_include_h
#define hbw_malloc_include_h

#ifdef __cplusplus
extern "C" {
#endif

enum {
    HBW_POLICY_BIND = 1,
    HBW_POLICY_PREFERRED = 2
};

inline int hbw_getpolicy(void);
inline void hbw_setpolicy(int mode);
inline int hbw_IsHBWAvailable(void);
inline void *hbw_malloc(size_t size);
inline void *hbw_calloc(size_t num, size_t size);
inline int hbw_posix_memalign(void **memptr, size_t alignment, size_t size);
inline void *hbw_realloc(void *ptr, size_t size);
inline void hbw_free(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
