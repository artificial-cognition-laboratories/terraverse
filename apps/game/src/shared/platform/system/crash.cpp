// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/system/crash.h"

#ifndef _WIN32

#include <csignal>
#include <cstdint>
#include <initializer_list>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace crash {

namespace {

constexpr int FATAL[] = { SIGSEGV, SIGILL, SIGABRT, SIGBUS, SIGFPE, SIGTRAP };
constexpr int FRAMES = 64;

// A signal handler can't allocate, so where reports go is copied here as it's watched.
// The one mutable global in this file: a handler has no other way to be told.
char g_logs[4096];
// A stack for the handler, so a stack overflow can still report (the thread that watched).
char g_stack[64 * 1024];

void onFatal(int sig, siginfo_t *info, void *);
// Async-signal-safe writing: no stdio, no allocation.
void put(int fd, const char *text);
void putNumber(int fd, unsigned long long n, int base);
const char *signalName(int sig);

} // namespace

void watch(const std::string &logs)
{
	std::strncpy(g_logs, logs.c_str(), sizeof(g_logs) - 1);
	// backtrace() loads libgcc on its first call, which allocates: done here, never in a handler
	void *frames[1];
	backtrace(frames, 1);
	stack_t stack = {};
	stack.ss_sp = g_stack;
	stack.ss_size = sizeof(g_stack);
	sigaltstack(&stack, nullptr);
	struct sigaction act = {};
	act.sa_sigaction = onFatal;
	// once: a crash in the handler, or the raise at its end, goes to the default (a core)
	act.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
	sigemptyset(&act.sa_mask);
	for (int sig : FATAL)
		sigaction(sig, &act, nullptr);
}

void again()
{
	watch(std::string(g_logs));
}

namespace {

void onFatal(int sig, siginfo_t *info, void *)
{
	const long tid = syscall(SYS_gettid);
	char thread[32] = "?";
	{
		char path[64] = "/proc/self/task/";
		char *end = path + std::strlen(path);
		// tid, then /comm, without snprintf
		char digits[24];
		int n = 0;
		for (long t = tid; t > 0 && n < 23; t /= 10)
			digits[n++] = (char)('0' + t % 10);
		while (n > 0)
			*end++ = digits[--n];
		std::strcpy(end, "/comm");
		const int fd = open(path, O_RDONLY);
		if (fd >= 0) {
			const ssize_t got = read(fd, thread, sizeof(thread) - 1);
			thread[got > 0 ? got : 0] = '\0';
			if (got > 0 && thread[got - 1] == '\n')
				thread[got - 1] = '\0';
			close(fd);
		}
	}

	int file = -1;
	if (g_logs[0]) {
		char path[sizeof(g_logs) + 64];
		std::strcpy(path, g_logs);
		std::strcat(path, "/crash-");
		char *end = path + std::strlen(path);
		char digits[24];
		int n = 0;
		for (unsigned long long t = (unsigned long long)std::time(nullptr); t > 0 && n < 23; t /= 10)
			digits[n++] = (char)('0' + t % 10);
		while (n > 0)
			*end++ = digits[--n];
		std::strcpy(end, ".txt");
		file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	}

	void *frames[FRAMES];
	const int count = backtrace(frames, FRAMES);
	for (int fd : { STDERR_FILENO, file }) {
		if (fd < 0)
			continue;
		put(fd, "\nCRASH: ");
		put(fd, signalName(sig));
		put(fd, " (code ");
		const int code = info ? info->si_code : 0;
		if (code < 0)
			put(fd, "-");
		putNumber(fd, (unsigned long long)(code < 0 ? -(long long)code : code), 10);
		put(fd, ", address 0x");
		putNumber(fd, (unsigned long long)(info ? (uintptr_t)info->si_addr : 0), 16);
		put(fd, ") on thread ");
		putNumber(fd, (unsigned long long)tid, 10);
		put(fd, " '");
		put(fd, thread);
		put(fd, "', at ");
		putNumber(fd, (unsigned long long)std::time(nullptr), 10);
		put(fd, " (unix)\nCRASH: backtrace (addr2line -e <binary> -f -C <offset> names them):\n");
		backtrace_symbols_fd(frames, count, fd);
		put(fd, "CRASH: end\n");
	}
	if (file >= 0)
		close(file);
	// as it came: the default action now (SA_RESETHAND), so the core is the crash's own
	raise(sig);
}

void put(int fd, const char *text)
{
	const size_t n = std::strlen(text);
	if (write(fd, text, n) < 0) {
		// nowhere left to say it
	}
}

void putNumber(int fd, unsigned long long n, int base)
{
	char digits[24];
	int i = 0;
	do {
		digits[i++] = "0123456789abcdef"[n % base];
		n /= base;
	} while (n > 0 && i < 23);
	char out[24];
	int k = 0;
	while (i > 0)
		out[k++] = digits[--i];
	out[k] = '\0';
	put(fd, out);
}

const char *signalName(int sig)
{
	switch (sig) {
	case SIGSEGV: return "SIGSEGV (bad memory access)";
	case SIGILL: return "SIGILL (illegal instruction: Chromium's deliberate crash on a failed check, or a trap)";
	case SIGABRT: return "SIGABRT (aborted: an uncaught exception, or a failed assert)";
	case SIGBUS: return "SIGBUS (bus error)";
	case SIGFPE: return "SIGFPE (arithmetic)";
	case SIGTRAP: return "SIGTRAP (a breakpoint trap: Chromium's deliberate crash)";
	default: return "a fatal signal";
	}
}

} // namespace

} // namespace crash

#else

namespace crash {

// TEMP(2026-11-27): Windows has no report yet; SetUnhandledExceptionFilter is its way in
void watch(const std::string &) {}
void again() {}

} // namespace crash

#endif
