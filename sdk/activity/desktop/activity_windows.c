// platform/activity on Windows: a hook on the main thread's messages
// (WH_GETMESSAGE, this thread's alone), which sees each message as the
// thread takes it from its queue, before its window does, and leaves it
// as it is: a mouse's buttons (on a window's frame too), pen and touch
// contacts, keys and the characters they make, the wheel. Every window
// the app shows, its dialogs' too, takes its input on this thread. Times
// are GetTickCount64's, which goes on while the computer sleeps.
#include "asuka_windows.h"
#include "activity.asuka.h"

#include <stdlib.h>

#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif
#ifndef WM_TOUCH
#define WM_TOUCH 0x0240
#endif

typedef struct listener {
	activity_fn input;
	asuka_call *call;
	struct listener *next;
} listener;

static HHOOK hook;
static ULONGLONG last;  // when the last input was, in milliseconds
static ULONGLONG told;  // when the listeners last heard of one
static listener *listeners;

static void input(void) {
	ULONGLONG t = GetTickCount64();
	last = t;
	if (!listeners || t - told < 1000) return;
	told = t;
	for (listener *l = listeners; l; l = l->next) activity_fn_call(l->input);
}

static bool is_input(UINT m) {
	switch (m) {
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDBLCLK:
	case WM_NCLBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
	case WM_NCMBUTTONDOWN:
	case WM_NCXBUTTONDOWN:
	case WM_POINTERDOWN:
	case WM_TOUCH:
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_IME_CHAR:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		return true;
	}
	return false;
}

static LRESULT CALLBACK on_message(int code, WPARAM wp, LPARAM lp) {
	if (code == HC_ACTION && wp == PM_REMOVE && is_input(((const MSG *)lp)->message)) input();
	return CallNextHookEx(hook, code, wp, lp);
}

static int64_t m_idle(activity_Watcher *self, asuka_error *err) { return (int64_t)(GetTickCount64() - last) * 1000000; }

static void stop(void *ctx) {
	listener *l = ctx;
	for (listener **q = &listeners; *q; q = &(*q)->next)
		if (*q == l) {
			*q = l->next;
			break;
		}
	asuka_error e = {0};
	asuka_error_cancelled(&e);
	asuka_call_fail(l->call, &e);
	free(l);
}

static void m_watch(activity_Watcher *self, activity_fn in, asuka_call *call) {
	listener *l = calloc(1, sizeof *l);
	l->input = in;
	l->call = call;
	l->next = listeners;
	listeners = l;
	asuka_call_on_cancel(call, stop, l);
}

static const activity_Watcher_vtable vt = {.idle = m_idle, .watch = m_watch};

// Made on the main thread: the hook is its own, once for the process
// (every watcher, asuka dev's reloads make new ones, reads the same).
activity_Watcher *activity_make_Watcher(asuka_error *err) {
	if (!hook) {
		hook = SetWindowsHookExW(WH_GETMESSAGE, on_message, NULL, GetCurrentThreadId());
		if (!hook) {
			asuka_win_fail(err, HRESULT_FROM_WIN32(GetLastError()), "SetWindowsHookEx");
			return NULL;
		}
		last = GetTickCount64();
	}
	activity_Watcher *w = malloc(sizeof *w);
	w->vt = &vt;
	return w;
}
