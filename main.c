#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <dlfcn.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef const char* (*PrintMessageFunc)();

/* 
    Dynamic Module stuff 
    TODO: put them all in a single struct
*/
#ifdef _WIN32
    HMODULE lib_handle = NULL;
#else
    void* lib_handle = NULL;
#endif

PrintMessageFunc print_message_ptr = NULL;
uint64_t last_change;

/* --------------------------------------- */

/* Terminal RAW mode stuff for linux */
#ifndef _WIN32
    struct termios orig_termios;
    void restore_terminal(void) 
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }
    void init_terminal(void)
    {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(restore_terminal);
        struct termios new_termios = orig_termios;
        new_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    }
#endif

/* Get the last modification time of a file */
uint64_t get_last_change(const char *path)
{
    #ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA attr;
        GetFileAttributesExA(path, GetFileExInfoStandard, &attr);
        ULARGE_INTEGER ull;
        ull.LowPart = attr.ftLastWriteTime.dwLowDateTime;
        ull.HighPart = attr.ftLastWriteTime.dwHighDateTime;
        return ull.QuadPart;
    #else
        struct stat attr;
        stat(path, &attr);
        return (long long)attr.st_mtime;
    #endif
}

/* TODO: check for compilation errors */
void compile_library()
{
    #ifdef _WIN32
        system("gcc -shared -o message.dll ./libmessage.c");
    #else
        system("gcc -shared -fPIC -o libmessage.so libmessage.c");
    #endif
}

/* TODO: don't hardcode paths */
int load_library() 
{
    #ifdef _WIN32
        lib_handle = LoadLibraryA("message.dll");
        if (!lib_handle) {
            printf("LoadLibrary failed (%lu)\n", GetLastError());
            return 0;
        }
        print_message_ptr = (PrintMessageFunc)GetProcAddress(lib_handle, "print_message");
    #else
        lib_handle = dlopen("./libmessage.so", RTLD_LAZY);
        if (!lib_handle) {
            printf("dlopen failed: %s\n", dlerror());
            return 0;
        }
        print_message_ptr = (PrintMessageFunc)dlsym(lib_handle, "print_message");
    #endif

    if (!print_message_ptr) 
    {
        printf("Failed to find print_message\n");

        #ifdef _WIN32
            FreeLibrary(lib_handle);
        #else
            dlclose(lib_handle);
        #endif

        lib_handle = NULL;
        return 0;
    }
    return 1;
}

void unload_library() 
{
    if (lib_handle) 
    {
    #ifdef _WIN32
            FreeLibrary(lib_handle);
    #else
            dlclose(lib_handle);
    #endif
        lib_handle = NULL;
        print_message_ptr = NULL;
    }
}

int key_pressed() 
{
    #ifdef _WIN32
        return _kbhit();
    #else
        /* Non-blocking way to check for user input */
        struct timeval tv = {0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
    #endif
}

char get_key() 
{
    #ifdef _WIN32
        return _getch();
    #else
        char c = 0;
        read(STDIN_FILENO, &c, 1);
        return c;
    #endif
}

int main(void) 
{
    #ifndef _WIN32
        init_terminal();
    #endif

    if (!load_library()) 
    {
        fprintf(stderr, "Initial load failed\n");
        return 1;
    }

    while (1) 
    {
        if (print_message_ptr) {
            printf("%s\n", print_message_ptr());
        } else {
            printf("No message loaded\n");
        }
        
        /* Reload library when a certain key is pressed */
        if (key_pressed()) 
        {
            char c = get_key();
            if (c == 'r' || c == 'R') 
            {
                printf("Reloading...\n");
                
                unload_library();
                compile_library();
                if (load_library()) {
                    printf("Reload successful\n");
                } else {
                    printf("Reload failed\n");
                }
            }
        }

        #ifdef _WIN32
            Sleep(1000);
        #else
            sleep(1);
        #endif
    }

    return 0;
}