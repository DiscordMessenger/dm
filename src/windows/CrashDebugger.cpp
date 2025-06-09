#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "CrashDebugger.hpp"
#include "Main.hpp"

#ifdef _MSC_VER
#include <intrin.h>
__declspec(naked) uintptr_t GetRbp() {
	__asm {
		mov eax, ebp
		ret
	}
}
#define GetReturnAddress() _ReturnAddress()
#else
#define GetRbp() __builtin_frame_address(0)
#define GetReturnAddress() __builtin_return_address(0)
#endif

NORETURN void AbortMessage(const char* message, ...);

#ifdef ALLOW_ABORT_HIJACKING

bool PatchMemory(void* address, const void* patch, size_t size)
{
	DWORD oldProtect;
	if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		memcpy(address, patch, size);
		VirtualProtect(address, size, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), address, size);
		return true;
	}
	else
	{
		// handle error
		DbgPrintW("ERROR with PatchMemory's VirtualProtect call: %08x", GetLastError());
		return false;
	}
}

NORETURN void WINAPI HijackedAbort()
{
	AbortMessage("Oops! DiscordMessenger just called abort()! ReturnAddress=%p", GetReturnAddress());
}

#endif

struct StackFrame {
	StackFrame* next;
	void* ip;
};

struct LoadedModule {
	std::string name;
	uintptr_t base;
	size_t size;
};

std::vector<LoadedModule> g_loadedModules;

const char* TrimPathComponents(const char* path)
{
	const char* ptr1, *ptr = path;
	while ((ptr1 = strchr(ptr, '\\')) || (ptr1 = strchr(ptr, '/'))) {
		ptr = ptr1 + 1;
	}
	return ptr;
}

void AddFoundModule(const char* name)
{
	HMODULE hmod = GetModuleHandleA(name);
	if (hmod)
		g_loadedModules.push_back({ std::string(name), (uintptr_t) hmod, 0x2000000 }); // bogus size
}

// Returned pair: [dll name, offset]
std::pair<const char*, uintptr_t> ResolveName(uintptr_t fun)
{
	const char* max = "?";
	uintptr_t maxbase = 0;
	for (const auto& mod : g_loadedModules) {
		if (maxbase < mod.base && mod.base < fun && fun < mod.base + mod.size)
			maxbase = mod.base, max = TrimPathComponents(mod.name.c_str());
	}

	return { max, maxbase };
}

bool SafeMemcpy(void* dst, const void* src, size_t sz)
{
	if ((uintptr_t) src >= 0x80000000) {
		// it seems that on 9x it doesn't bother anyone that
		// we're querying in kernel space...
		return false;
	}
	
	// Probe this address with VirtualQuery
	MEMORY_BASIC_INFORMATION mbi;
	memset(&mbi, 0, sizeof mbi);
	DWORD res = VirtualQuery((void*) src, &mbi, sizeof(mbi));
	
	if (res == 0)
		return false;
	
	// is this memory free
	if (mbi.Type == MEM_FREE)
		return false;

	if (mbi.Protect & PAGE_NOACCESS)
		return false;
	
	if (mbi.Protect & PAGE_GUARD)
		return false;
	
	// highly unlikely that this stack frame wouldn't be aligned to at least 4 bytes
	if (sz == 4 && ((uintptr_t)src & 3) != 0)
		return false;
	
	// also highly unlikely that stuff below a megabyte is safe to access
	if ((uintptr_t) src < 0x100000)
		return false;

	// no it's not. therefore this is safe
	memcpy(dst, src, sz);
	return true;
}

NORETURN void AbortMessage(const char* message, ...)
{
	static bool Aborted = false;

	// The abort has been hijacked.
	// Let's figure out where we are.
	if (Aborted)
	{
		// Already aborted - either recursive abort or screwed up so badly
		ExitThread(0);
	}

	Aborted = true;

	char stackTraceBuffer[8192];
	char smallerBuffer[128];

	va_list vl;
	va_start(vl, message);
	vsnprintf(stackTraceBuffer, 1024, message, vl);
	va_end(vl);

	// NOTE: I *assume* Windows 2000 was the first to support the "copy from message box"
	// trick.  Windows 95 does not.
	if (LOBYTE(GetVersion()) >= 5)
	{
		strcat(stackTraceBuffer, "\nSend this to iProgramInCpp right away!\n"
			"Hint: Press Ctrl-C to copy the whole message box's text.\n\n"
			"Windows Version ");
	}
	else
	{
		strcat(stackTraceBuffer, "\nSend this to iProgramInCpp right away!\n\n"
			"Windows Version ");
	}

	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof osvi;
	ri::GetVersionEx(&osvi);

	const char* csd = nullptr;
#ifdef UNICODE
	constexpr size_t CNT = sizeof(osvi.szCSDVersion) / sizeof(osvi.szCSDVersion[0]);
	char smallerBuffer2[CNT];
	for (int i = 0; i < CNT; i++) {
		WCHAR wchr = osvi.szCSDVersion[i];
		smallerBuffer2[i] = ((wchr < 0x7E && wchr >= 0x20) || wchr == 0) ? (char)wchr : '?';
	}
	csd = smallerBuffer2;
#else
	csd = osvi.szCSDVersion;
#endif

	if (strlen(csd) == 0)
		csd = "nothing";

	snprintf(
		smallerBuffer,
		sizeof smallerBuffer,
		"%d.%d.%d [%s] (%s)",
		osvi.dwMajorVersion,
		osvi.dwMinorVersion,
		osvi.dwBuildNumber,
		osvi.dwPlatformId == VER_PLATFORM_WIN32_NT ? "NT" : "9X",
		csd
	);
	strcat(stackTraceBuffer, smallerBuffer);

	// there's gotta be a better way ...
#ifdef _MSC_VER
	const char* mingwOrMsvc = "MSVC";
#else
	const char* mingwOrMsvc = "MinGW";
#endif

#ifdef UNICODE
	const char* unicodeOrAnsi = "Unicode";
#else
	const char* unicodeOrAnsi = "ANSI";
#endif

#ifdef UNICODE
	const char* debugOrRelease = "debug";
#else
	const char* debugOrRelease = "release";
#endif

#ifdef _WIN64
	const char* x64Orx86 = "x64";
#else
	const char* x64Orx86 = "x86";
#endif
	
	snprintf(
		smallerBuffer,
		sizeof smallerBuffer,
		"\nDiscordMessenger Version %.2f %s %s %s %s\n\nHere is a stack trace:\n",
		GetAppVersion(),
		mingwOrMsvc,
		unicodeOrAnsi,
		debugOrRelease,
		x64Orx86
	);

	strcat(stackTraceBuffer, smallerBuffer);

	// Figure out the current stack trace.
	StackFrame* sf = (StackFrame*)GetRbp();
	StackFrame* first = sf;

	int depth = 0;

	while (sf)
	{
		if (depth++ > 40) {
			strcat(stackTraceBuffer, "[stack trace goes deeper]");
			break;
		}
		
		//snprintf(smallerBuffer,sizeof smallerBuffer,"Depth = %d  SF=%p", depth, sf);
		//MessageBoxA(NULL, smallerBuffer, NULL, 0);
		
		void* ip;
		if (!SafeMemcpy(&ip, &sf->ip, sizeof(void*))) {
			snprintf(smallerBuffer, sizeof smallerBuffer, "[stack trace failed to fetch from %p]", &sf->ip);
			strcat(stackTraceBuffer, smallerBuffer);
			break;
		}
		
		if (ip == NULL)
			break;

		auto pr = ResolveName((uintptr_t) ip);
		snprintf(smallerBuffer, sizeof smallerBuffer, "* [F:%p] %p [%s(%p)+%X]\n", sf, sf->ip, pr.first, (void*) pr.second, ((uintptr_t)sf->ip - pr.second));
		strcat(stackTraceBuffer, smallerBuffer);
		
		if (sf == sf->next) {
			strcat(stackTraceBuffer, "[infinite loop detected]");
			break;
		}
		//if ((uintptr_t)sf->next < (uintptr_t)sf) {
		//	snprintf(smallerBuffer, sizeof smallerBuffer, "[stack trace found to go backwards  %p to %p]", sf, sf->next);
		//	strcat(stackTraceBuffer, smallerBuffer);
		//	break;
		//}
		
		sf = sf->next;
	}
	
	strcat(stackTraceBuffer, "\nAnd here's a stack dump.\n");
	uint8_t* sfu = (uint8_t*)first;
	for (int i = 0; i < 128; i += 16) {
		snprintf(smallerBuffer, sizeof smallerBuffer, "%08x: ", sfu+i);
		strcat(stackTraceBuffer, smallerBuffer);
		for (int j = 0; j < 16; j++) {
			uint8_t s;
			strcat(stackTraceBuffer, " ");
			
			if (!SafeMemcpy(&s, &sfu[i+j], 1)) {
				strcat(stackTraceBuffer, "??");
			}
			else {
				snprintf(smallerBuffer, sizeof smallerBuffer, "%02x", s);
				strcat(stackTraceBuffer, smallerBuffer);
			}
		}
		strcat(stackTraceBuffer, "\n");
	}
	
	strcat(stackTraceBuffer, "Enjoy debugging this thing!");

	const char* title;
	if (LOBYTE(GetVersion()) >= 5)
		title = "Discord Messenger fatal error! (Press CTRL-C to copy!)";
	else
		title = "Discord Messenger fatal error!";

	MessageBoxA(NULL, stackTraceBuffer, title, MB_ICONERROR);
	ExitProcess(1);
}

bool FindLoadedDLLsAutomatically()
{
	HANDLE ths = ri::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	if (!ths)
		return false;

	MODULEENTRY32 me32;
	me32.dwSize = sizeof(MODULEENTRY32);

	if (!ri::Module32First(ths, &me32))
		return false;

	do {
		std::string str = MakeStringFromTString(me32.szModule);
		uintptr_t base = (uintptr_t) me32.modBaseAddr;
		size_t size = (size_t) me32.modBaseSize;

		g_loadedModules.push_back ({ str, base, size });
	}
	while (ri::Module32Next(ths, &me32));

	CloseHandle(ths);
	return true;
}


LONG WINAPI DMUnhandledExceptionFilter(PEXCEPTION_POINTERS ExceptionInfo)
{
	auto er = ExceptionInfo->ExceptionRecord;
	auto cr = ExceptionInfo->ContextRecord;

	AbortMessage(
		"Oops! DiscordMessenger just crashed!\n\n"
		"Exception Code: %08x\n"
		"Exception Address: %p\n"
		"Exception Parameters: %p %p\n"
		"EIP=%08x EFLAGS=%08x \n"
		"EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n"
		"ESI=%08x EDI=%08x ESP=%08x EBP=%08x\n",
		er->ExceptionCode,
		er->ExceptionAddress,
		er->NumberParameters >= 1 ? er->ExceptionInformation[0] : 0,
		er->NumberParameters >= 2 ? er->ExceptionInformation[1] : 0,
		cr->Eip, cr->EFlags,
		cr->Eax, cr->Ebx, cr->Ecx, cr->Edx,
		cr->Esi, cr->Edi, cr->Esp, cr->Ebp
	);
}

#include <csignal>

#ifdef FILE_DEBUG_HIJACKER
#define HijackDbgPrint(...) do { fprintf(something, __VA_ARGS__); fprintf(something, "\n"); } while (0)
#else
#define HijackDbgPrint(...) DbgPrintW(__VA_ARGS__)
#endif

void SignalAbort(int signum)
{
	AbortMessage("Oops! DiscordMessenger caught signal number %d! (SIGABRT = %d)", signum, SIGABRT);
}

#ifdef ALLOW_ABORT_HIJACKING
void CppTerminateAbort()
{
	AbortMessage("Oops! DiscordMessenger called std::terminate or something!");
}

void *HijackedAbortPtr = (void*) &HijackedAbort;
void *CppTerminateAbortPtr = (void*) &CppTerminateAbort;
#endif

// Hijacks the Abort function from MSVCRT.DLL or UCRTBASE.DLL,
// gets information on loaded modules, and sets up an unhandled
// exception filter.
void SetupCrashDebugging()
{
#ifdef ALLOW_ABORT_HIJACKING
#ifdef FILE_DEBUG_HIJACKER
	FILE* something = fopen("something.txt", "w");
#endif
	
	// Step 1. Hijack abort()
	if (sizeof(uintptr_t) > 4)
	{
		HijackDbgPrint("Abort hijack disabled.  The target is 64-bit.");
		return;
	}
	
	void* patchAt = (void*) &abort;
	
	// We need to find the actual address of abort() from within the loaded libraries.
	// This means that just "&abort" does not do as it links to our thunk which jumps
	// indirectly to the actual loaded abort().
	//
	// Now, import thunks are typically encoded in FF 25 XX XX XX XX. So, check for
	// those bytes' presence.
	uint8_t* abortPtr = (uint8_t*) &abort;
	if (abortPtr[0] != 0xFF || abortPtr[1] != 0x25)
	{
		// Wow! It looks like this ain't an import thunk of a known kind.
		auto x = abortPtr;
		HijackDbgPrint("Abort hijack may be ineffective.  The import thunk looks weird.  Here is a dump:");
		HijackDbgPrint("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7],
			x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15]);
	}
	else
	{
		// It is.  This means that the address we're looking for is after the FF 25 bytes.
		void*** ptr = (void***)(abortPtr + 2);
		
		// ptr - The address of the offset of the FF 25 XX XX XX XX instruction
		// *ptr - The address where the JMP loads the address to jump to
		// **ptr = The address to jump to
		
		HijackDbgPrint("Found import thunk at %p, jumping to indirection via %p", ptr, *ptr);
		HijackDbgPrint("The indirection leads to %p", **ptr);
		
		patchAt = **ptr;
	}
	
	uintptr_t Patch[2];
	Patch[0] = 0x25FF9090;                     // NOP; NOP; JMP [modrm]
	Patch[1] = (uintptr_t) &HijackedAbortPtr;  // [absolute indirect 32-bit address]
	
	if (!PatchMemory((void*) patchAt, Patch, sizeof Patch))
	{
		HijackDbgPrint("Abort hijack disabled.  Could not write 8 bytes to %p.", &abort);
		return;
	}
	
	HijackDbgPrint("Abort hijack engaged.  abort() = %p, patched at %p, hijacked to %p", &abort, patchAt, &HijackedAbort);
	
	// also register a signal handler, because as it turns out, MSVCRT from Win98
	// *does*, indeed, emit a SIGABRT, and as it also turns out, our hijack method
	// isn't 100% reliable.
	signal(SIGABRT, SignalAbort);
	
	// Step 1.5.  <sigh> I have to debug this separately don't I?
#ifdef ABORT_ON_EXCEPTION
	HMODULE hmod = GetModuleHandleA("libstdc++-6.dll");
	if (!hmod) {
		hmod = GetModuleHandleA("lstdcpp.dll");
	}
	
	if (!hmod) {
		HijackDbgPrint("C++ terminate hijack failed.  Libstdc++-6.dll not loaded.");
	}
	else {
		const char* fname = "__cxa_throw";
		FARPROC fp = GetProcAddress(hmod, fname);
		if (!fp) {
			HijackDbgPrint("C++ terminate hijack failed. Cannot find %s.", fname);
		}
		else {
			// Patch that thing as well
			// 
			// It starts with push ebp; push edi; push esi; push ebx; sub esp, 0x2C (55 57 56 53 83 EC 2C)
			// I was going to allow restoration but for debugging I won't.
			uint8_t* patch = (uint8_t*) fp;
			void* destptr = &CppTerminateAbortPtr;
			uint8_t patch_data[6];
			patch_data[0] = 0xFF; // jmp
			patch_data[1] = 0x25; // mod_rm
			memcpy(&patch_data[2], &destptr, sizeof(void*));
			
			// now we can patch
			if (!PatchMemory((void*) fp, patch_data, 6)) {
				HijackDbgPrint("C++ terminate hijack failed. Cannot patch 6 bytes.");
			}
			else {
				HijackDbgPrint("C++ terminate hijack engaged.  Function = %p, hijacked to %p", patch, &CppTerminateAbort);
			}
		}
	}
#endif
	
#ifdef FILE_DEBUG_HIJACKER
	fclose(something);
#endif
#endif
	
	// Step 2. Find all loaded modules.
	// We either import toolhelp32 functions from Kernel32.dll and find
	// them that way, or just come with our own hardcoded list.
	if (!FindLoadedDLLsAutomatically())
	{
		// our own module
		char moduleName[1024];
		strcpy(moduleName, "dummy.exe");
		GetModuleFileNameA(g_hInstance, moduleName, sizeof moduleName);

		// maybe just cut out all the path components though
		AddFoundModule(TrimPathComponents(moduleName));

		// base system DLLs
		AddFoundModule("ntdll.dll");
		AddFoundModule("kernel32.dll");
		AddFoundModule("user32.dll");
		AddFoundModule("gdi32.dll");
		AddFoundModule("shell32.dll");
		AddFoundModule("msimg32.dll");
		AddFoundModule("shlwapi.dll");
		AddFoundModule("crypt32.dll");
		AddFoundModule("ws2_32.dll");
		AddFoundModule("ole32.dll");
		AddFoundModule("comctl32.dll");
		AddFoundModule("msvcrt.dll");
		AddFoundModule("ucrtbase.dll");

		// long file name versions of dependencies
		AddFoundModule("libcrypto-3.dll");
		AddFoundModule("libssl-3.dll");
		AddFoundModule("libgcc_s_dw2-1.dll");
		AddFoundModule("libstdc++-6.dll");

		// short file name versions of dependencies
		AddFoundModule("libcrypt.dll");
		AddFoundModule("libssl.dll");
		AddFoundModule("libgcc.dll");
		AddFoundModule("lstdcpp.dll");
	}

	// Step 3. Set the unhandled exception filter.
	SetUnhandledExceptionFilter(DMUnhandledExceptionFilter);
}
