#include <stdint.h>
#include <stdarg.h>
void roam_log(const char *fmt, ...){ (void)fmt; }
const char *roam_opt(const char *name){ (void)name; return 0; }
uint32_t roam_win_mxcsr_default(void){ return 0x1f80; }
