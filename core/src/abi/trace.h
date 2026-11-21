// trace.h: where an error was thrown, for a report of it (a program's
// error, asuka dev's overlay): the VM's stack as the last error thrown on
// the main thread left it, and the frames of functions compiled to C
// (lang/cgen) it unwound through. A line per frame, innermost first:
// "name (file:line)".
#ifndef AK_ABI_TRACE_H
#define AK_ABI_TRACE_H

#include "base/base.h"

// ak_trace_start traces the errors thrown on this thread from now on;
// ak_trace_stop stops, and lets the last one go.
void ak_trace_start(void);
void ak_trace_stop(void);
// ak_trace_forget lets the last error thrown go and keeps tracing: its
// module is about to go (a reload), and an error of the module's own
// types can't outlive it, since freeing one reads its shape.
void ak_trace_forget(void);
// ak_trace_of is err's trace: its compiled frames' lines (*c) and the
// VM's (*vm), both "" when err isn't the last error thrown. Valid until
// the next error is thrown.
void ak_trace_of(const void *err, const char **c, const char **vm);

#endif
