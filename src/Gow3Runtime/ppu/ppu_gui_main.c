#include "ppu_core.h"
#include "ppu_elf.h"
#include <stdint.h>

typedef void *HANDLE;
typedef void *HWND;
typedef void *HINSTANCE;
typedef void *HICON;
typedef void *HCURSOR;
typedef void *HBRUSH;
typedef void *HMENU;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef int32_t BOOL;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef uint16_t WORD;
typedef uint16_t WCHAR16;
typedef LRESULT (__stdcall *WindowProc)(HWND, UINT, WPARAM, LPARAM);
typedef struct Point { int32_t x, y; } Point;
typedef struct Message { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; Point pt; DWORD private_value; } Message;
typedef struct WindowClass {
    UINT style; WindowProc lpfnWndProc; int32_t cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    const WCHAR16 *lpszMenuName; const WCHAR16 *lpszClassName;
} WindowClass;
typedef struct OpenFileName {
    DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance;
    const WCHAR16 *lpstrFilter; WCHAR16 *lpstrCustomFilter;
    DWORD nMaxCustFilter; DWORD nFilterIndex; WCHAR16 *lpstrFile; DWORD nMaxFile;
    WCHAR16 *lpstrFileTitle; DWORD nMaxFileTitle; const WCHAR16 *lpstrInitialDir;
    const WCHAR16 *lpstrTitle; DWORD Flags; WORD nFileOffset; WORD nFileExtension;
    const WCHAR16 *lpstrDefExt; LPARAM lCustData; void *lpfnHook;
    const WCHAR16 *lpTemplateName; void *pvReserved; DWORD dwReserved; DWORD FlagsEx;
} OpenFileName;

__declspec(dllimport) HINSTANCE __stdcall GetModuleHandleW(const WCHAR16 *);
__declspec(dllimport) HCURSOR __stdcall LoadCursorW(HINSTANCE, const WCHAR16 *);
__declspec(dllimport) uint16_t __stdcall RegisterClassW(const WindowClass *);
__declspec(dllimport) HWND __stdcall CreateWindowExW(DWORD, const WCHAR16 *, const WCHAR16 *, DWORD,
    int32_t, int32_t, int32_t, int32_t, HWND, HMENU, HINSTANCE, void *);
__declspec(dllimport) LRESULT __stdcall DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) BOOL __stdcall ShowWindow(HWND, int32_t);
__declspec(dllimport) BOOL __stdcall UpdateWindow(HWND);
__declspec(dllimport) BOOL __stdcall GetMessageW(Message *, HWND, UINT, UINT);
__declspec(dllimport) BOOL __stdcall TranslateMessage(const Message *);
__declspec(dllimport) LRESULT __stdcall DispatchMessageW(const Message *);
__declspec(dllimport) void __stdcall PostQuitMessage(int32_t);
__declspec(dllimport) int32_t __stdcall GetWindowTextW(HWND, WCHAR16 *, int32_t);
__declspec(dllimport) BOOL __stdcall SetWindowTextW(HWND, const WCHAR16 *);
__declspec(dllimport) BOOL __stdcall GetOpenFileNameW(OpenFileName *);
__declspec(dllimport) HANDLE __stdcall CreateFileW(const WCHAR16 *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL __stdcall GetFileSizeEx(HANDLE, int64_t *);
__declspec(dllimport) BOOL __stdcall ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) void *__stdcall VirtualAlloc(void *, size_t, DWORD, DWORD);
__declspec(dllimport) BOOL __stdcall VirtualFree(void *, size_t, DWORD);
__declspec(dllimport) void __stdcall ExitProcess(uint32_t);

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_READ 0x80000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define MEM_RELEASE 0x8000u
#define PAGE_READWRITE 4u
#define WS_CHILD 0x40000000u
#define WS_VISIBLE 0x10000000u
#define WS_BORDER 0x00800000u
#define WS_VSCROLL 0x00200000u
#define WS_OVERLAPPEDWINDOW 0x00cf0000u
#define ES_AUTOHSCROLL 0x0080u
#define ES_MULTILINE 0x0004u
#define ES_READONLY 0x0800u
#define ES_AUTOVSCROLL 0x0040u
#define ES_NUMBER 0x2000u
#define BS_PUSHBUTTON 0u
#define BS_DEFPUSHBUTTON 1u
#define WM_CREATE 1u
#define WM_DESTROY 2u
#define WM_COMMAND 0x0111u
#define CW_USEDEFAULT ((int32_t)0x80000000u)
#define SW_SHOW 5
#define COLOR_WINDOW 5
#define IDC_ARROW ((const WCHAR16 *)(uintptr_t)32512u)
#define OFN_HIDEREADONLY 0x00000004u
#define OFN_PATHMUSTEXIST 0x00000800u
#define OFN_FILEMUSTEXIST 0x00001000u
#define CALLBACK __stdcall
#define LOWORD(value) ((uint16_t)((uintptr_t)(value) & 0xffffu))

static void zero_memory(void *memory, size_t size)
{
    uint8_t *bytes = (uint8_t *)memory;
    for (size_t i = 0; i < size; ++i) bytes[i] = 0;
}

void *memcpy(void *destination, const void *source, size_t size)
{
    uint8_t *to = (uint8_t *)destination;
    const uint8_t *from = (const uint8_t *)source;
    for (size_t i = 0; i < size; ++i) to[i] = from[i];
    return destination;
}

#define ID_PATH 1001
#define ID_BROWSE 1002
#define ID_BUDGET 1003
#define ID_RUN 1004
#define ID_STATUS 1005
#define DEFAULT_BUDGET 10000000ull
#define MAX_FILE_SIZE (1024ull * 1024ull * 1024ull)
#define MAX_GUEST_SPAN (512ull * 1024ull * 1024ull)

static HWND g_path_edit;
static HWND g_budget_edit;
static HWND g_status_edit;
static HINSTANCE g_instance;
static WCHAR16 g_selected_path[32768];

static void set_status(const WCHAR16 *message)
{
    if (g_status_edit)
        SetWindowTextW(g_status_edit, message);
}

static int path_has_extension(const WCHAR16 *path, const WCHAR16 *extension)
{
    uint32_t path_length = 0, extension_length = 0;
    while (path[path_length] != 0) ++path_length;
    while (extension[extension_length] != 0) ++extension_length;
    if (path_length < extension_length)
        return 0;
    const WCHAR16 *tail = path + path_length - extension_length;
    for (uint32_t i = 0; i < extension_length; ++i) {
        WCHAR16 left = tail[i], right = extension[i];
        if (left >= 'A' && left <= 'Z') left = (WCHAR16)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (WCHAR16)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

static int parse_budget_text(const WCHAR16 *text, uint64_t *value)
{
    uint64_t parsed = 0;
    if (!text || !*text) return 0;
    for (uint32_t i = 0; text[i] != 0; ++i) {
        if (text[i] < '0' || text[i] > '9') return 0;
        const uint32_t digit = text[i] - '0';
        if (parsed > (UINT64_MAX - digit) / 10u) return 0;
        parsed = parsed * 10u + digit;
    }
    if (parsed == 0) return 0;
    *value = parsed;
    return 1;
}

static int load_file(const WCHAR16 *path, uint8_t **bytes, size_t *length)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return 0;
    int64_t file_size = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size <= 0 ||
        (uint64_t)file_size > MAX_FILE_SIZE) {
        CloseHandle(file);
        return 0;
    }
    const size_t size = (size_t)file_size;
    uint8_t *buffer = (uint8_t *)VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) { CloseHandle(file); return 0; }
    size_t offset = 0;
    while (offset < size) {
        DWORD request = (DWORD)(size - offset);
        if (request > 1024u * 1024u) request = 1024u * 1024u;
        DWORD received = 0;
        if (!ReadFile(file, buffer + offset, request, &received, 0) || received == 0) {
            VirtualFree(buffer, 0, MEM_RELEASE);
            CloseHandle(file);
            return 0;
        }
        offset += received;
    }
    CloseHandle(file);
    *bytes = buffer;
    *length = size;
    return 1;
}

static void append_text(WCHAR16 *buffer, uint32_t capacity, const WCHAR16 *text)
{
    uint32_t used = 0;
    while (used < capacity && buffer[used] != 0) ++used;
    while (used + 1 < capacity && *text != 0)
        buffer[used++] = *text++;
    if (used < capacity) buffer[used] = 0;
}

static void append_u64(WCHAR16 *buffer, uint32_t capacity, uint64_t value)
{
    WCHAR16 digits[24];
    uint32_t count = 0;
    do { digits[count++] = (WCHAR16)('0' + value % 10u); value /= 10u; }
    while (value != 0 && count < 24);
    while (count != 0 && count < 24) {
        WCHAR16 one[2] = { digits[--count], 0 };
        append_text(buffer, capacity, one);
    }
}

static void run_selected_file(void)
{
    GetWindowTextW(g_path_edit, g_selected_path, 32768);
    if (g_selected_path[0] == 0) {
        set_status(L"Selecciona un archivo primero.");
        return;
    }
    if (path_has_extension(g_selected_path, L".iso") || path_has_extension(g_selected_path, L".zip")) {
        set_status(L"Este prototipo no abre ISO/ZIP. Acepta un ELF PPC64 big-endian ya descifrado; no extrae el juego.");
        return;
    }
    WCHAR16 budget_text[64];
    GetWindowTextW(g_budget_edit, budget_text, 64);
    uint64_t budget = 0;
    if (!parse_budget_text(budget_text, &budget)) {
        set_status(L"El límite debe ser un entero decimal positivo.");
        return;
    }

    set_status(L"Leyendo y validando ELF...");
    uint8_t *file_bytes = 0;
    size_t file_size = 0;
    if (!load_file(g_selected_path, &file_bytes, &file_size)) {
        set_status(L"No se pudo leer el archivo o supera el límite de 1 GiB.");
        return;
    }
    PpuElfView elf;
    const PpuElfResult parsed = ppu_elf_parse(file_bytes, file_size, &elf);
    if (parsed != PPU_ELF_OK) {
        VirtualFree(file_bytes, 0, MEM_RELEASE);
        set_status(L"Archivo rechazado: se requiere ELF64 big-endian PPC64 crudo. EBOOT SELF y juegos no son compatibles aún.");
        return;
    }

    uint64_t guest_base = UINT64_MAX, guest_end = 0;
    for (uint16_t i = 0; i < elf.segment_count; ++i) {
        if (elf.segments[i].virtual_address < guest_base)
            guest_base = elf.segments[i].virtual_address;
        const uint64_t end = elf.segments[i].virtual_address + elf.segments[i].memory_size;
        if (end > guest_end) guest_end = end;
    }
    if (guest_base == UINT64_MAX || guest_end <= guest_base || guest_end - guest_base > MAX_GUEST_SPAN) {
        VirtualFree(file_bytes, 0, MEM_RELEASE);
        set_status(L"El rango de memoria del ELF está vacío o supera 512 MiB.");
        return;
    }
    const size_t guest_size = (size_t)(guest_end - guest_base);
    uint8_t *guest = (uint8_t *)VirtualAlloc(0, guest_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!guest) {
        VirtualFree(file_bytes, 0, MEM_RELEASE);
        set_status(L"Windows no pudo reservar memoria para el ELF.");
        return;
    }
    const PpuElfResult loaded = ppu_elf_load_segments(file_bytes, file_size, &elf,
                                                       guest_base, guest, guest_size);
    VirtualFree(file_bytes, 0, MEM_RELEASE);
    if (loaded != PPU_ELF_OK) {
        VirtualFree(guest, 0, MEM_RELEASE);
        set_status(L"No se pudieron mapear los segmentos ELF en memoria.");
        return;
    }

    PpuState cpu;
    ppu_state_init(&cpu, guest, guest_size, elf.entry);
    ppu_set_code_base(&cpu, guest_base);
    ppu_set_memory_at(&cpu, guest, guest_size, guest_base);
    uint64_t executed = 0;
    const PpuStepResult result = ppu_run(&cpu, budget, &executed);
    WCHAR16 report[512] = L"PPU detenido con estado ";
    append_u64(report, 512, (uint64_t)result);
    append_text(report, 512, L" tras ");
    append_u64(report, 512, executed);
    append_text(report, 512, L" instrucciones. PC=0x");
    static const WCHAR16 hex[] = L"0123456789abcdef";
    WCHAR16 pc_digits[16];
    for (uint32_t i = 0; i < 16; ++i)
        pc_digits[15u - i] = hex[(cpu.pc >> (i * 4u)) & 15u];
    WCHAR16 pc_text[17];
    for (uint32_t i = 0; i < 16; ++i) pc_text[i] = pc_digits[i];
    pc_text[16] = 0;
    append_text(report, 512, pc_text);
    append_text(report, 512, L".\r\n");
    if (result == PPU_STEP_SYSCALL)
        append_text(report, 512, L"Punto de llamada al sistema: faltan los servicios PS3.\r\n");
    else if (result == PPU_STEP_LIMIT_REACHED)
        append_text(report, 512, L"Se alcanzó el presupuesto de instrucciones.\r\n");
    else
        append_text(report, 512, L"Ejecución detenida: instrucción no compatible o fallo de memoria.\r\n");
    append_text(report, 512, L"Este prototipo no ejecuta todavía God of War III ni renderiza Vulkan.");
    set_status(report);
    VirtualFree(guest, 0, MEM_RELEASE);
}

static void browse_for_file(HWND window)
{
    OpenFileName dialog;
    zero_memory(&dialog, sizeof(dialog));
    g_selected_path[0] = 0;
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Ejecutables PS3 (ELF/BIN/ISO)\0*.elf;*.bin;*.iso\0Todos los archivos\0*.*\0\0";
    dialog.lpstrFile = g_selected_path;
    dialog.nMaxFile = 32768;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrTitle = L"Seleccionar archivo de juego o ELF";
    if (GetOpenFileNameW(&dialog)) {
        SetWindowTextW(g_path_edit, g_selected_path);
        if (path_has_extension(g_selected_path, L".iso"))
            set_status(L"ISO seleccionada. Este prototipo no la abre; requiere un ELF PPC64 descifrado.");
        else
            set_status(L"Archivo seleccionado. Pulsa Cargar y ejecutar para validar el ELF.");
    }
}

static HWND add_control(HWND parent, const WCHAR16 *klass, const WCHAR16 *title,
                        DWORD style, int x, int y, int width, int height, int id)
{
    return CreateWindowExW(0, klass, title, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, (HMENU)(intptr_t)id, g_instance, 0);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        add_control(window, L"STATIC", L"God of War III PS3 — host experimental", 0, 24, 18, 620, 28, 0);
        add_control(window, L"STATIC", L"Archivo ELF PPC64 descifrado:", 0, 24, 60, 340, 22, 0);
        g_path_edit = add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 24, 84, 490, 30, ID_PATH);
        add_control(window, L"BUTTON", L"Buscar...", BS_PUSHBUTTON, 526, 84, 112, 30, ID_BROWSE);
        add_control(window, L"STATIC", L"Presupuesto de instrucciones:", 0, 24, 132, 250, 22, 0);
        g_budget_edit = add_control(window, L"EDIT", L"10000000", WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER, 270, 128, 150, 30, ID_BUDGET);
        add_control(window, L"BUTTON", L"Cargar y ejecutar", BS_DEFPUSHBUTTON, 24, 174, 180, 36, ID_RUN);
        add_control(window, L"STATIC", L"Estado:", 0, 24, 226, 100, 22, 0);
        g_status_edit = add_control(window, L"EDIT", L"Selecciona un ELF para probar el PPU. Una ISO no se puede ejecutar todavía.",
            WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            24, 250, 614, 190, ID_STATUS);
        add_control(window, L"STATIC", L"Aún no hay juego jugable ni renderer Vulkan integrado.", 0, 24, 454, 614, 24, 0);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_BROWSE) browse_for_file(window);
        else if (LOWORD(wparam) == ID_RUN) run_selected_file();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

void entry(void)
{
    g_instance = GetModuleHandleW(0);
    WindowClass window_class;
    zero_memory(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = g_instance;
    window_class.lpszClassName = L"Gow3PpuPrototypeWindow";
    window_class.hCursor = LoadCursorW(0, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassW(&window_class)) ExitProcess(1);
    HWND window = CreateWindowExW(0, window_class.lpszClassName,
        L"God of War III PS3 — runtime experimental", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 540, 0, 0, g_instance, 0);
    if (!window) ExitProcess(2);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    Message message;
    while (GetMessageW(&message, 0, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    ExitProcess((uint32_t)message.wParam);
}
