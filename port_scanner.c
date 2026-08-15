#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#endif

#define MAX_THREADS 100
#define TIMEOUT_SEC 2

typedef struct {
    char ip[46];
    int port;
    int is_open;
} ScanTask;

#ifdef _WIN32
    typedef HANDLE thread_t;
#else
    typedef pthread_t thread_t;
#endif

// Cross-platform socket close
void close_socket(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

// Cross-platform thread creation
int create_thread(thread_t* thread, void* (*func)(void*), void* arg) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

// Cross-platform thread join
int join_thread(thread_t thread) {
#ifdef _WIN32
    return (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0) ? 0 : -1;
#else
    return pthread_join(thread, NULL);
#endif
}

// Cross-platform socket initialization
int init_sockets() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("[-] Winsock initialization failed\n");
        return 0;
    }
#endif
    return 1;
}

// Cross-platform socket cleanup
void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Function to scan a single port
int scan_port(const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    
    // Resolve domain name if needed
    struct hostent* host = gethostbyname(ip);
    if (host == NULL) {
        close_socket(sock);
        return 0;
    }
    
    memcpy(&server_address.sin_addr, host->h_addr, host->h_length);
    
    int result = connect(sock, (struct sockaddr*)&server_address, sizeof(server_address));
    close_socket(sock);
    
    return (result == 0);
}

// Thread function for scanning - uses void* return for cross-platform compatibility
void* scan_thread(void* arg) {
    ScanTask* task = (ScanTask*)arg;
    task->is_open = scan_port(task->ip, task->port);
    return NULL;
}

// Windows-specific wrapper for thread function
#ifdef _WIN32
DWORD WINAPI scan_thread_wrapper(LPVOID arg) {
    scan_thread(arg);
    return 0;
}
#endif

// Cross-platform thread creation with proper casting
int create_thread_wrapper(thread_t* thread, void* arg) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, scan_thread_wrapper, arg, 0, NULL);
    return (*thread == NULL) ? -1 : 0;
#else
    return pthread_create(thread, NULL, scan_thread, arg);
#endif
}

const char* get_service_name(int port) {
    switch(port) {
        case 20: case 21: return "FTP";
        case 22: return "SSH";
        case 23: return "Telnet";
        case 25: return "SMTP";
        case 53: return "DNS";
        case 80: return "HTTP";
        case 110: return "POP3";
        case 111: return "RPCbind";
        case 135: return "MSRPC";
        case 139: return "NetBIOS";
        case 143: return "IMAP";
        case 443: return "HTTPS";
        case 445: return "SMB";
        case 993: return "IMAPS";
        case 995: return "POP3S";
        case 3306: return "MySQL";
        case 3389: return "RDP";
        case 5432: return "PostgreSQL";
        case 5900: return "VNC";
        case 8080: return "HTTP-ALT";
        case 8443: return "HTTPS-ALT";
        default: return "Unknown";
    }
}

// Function to scan a range of ports
void scan_range(const char* ip, int start_port, int end_port) {
    int total_ports = end_port - start_port + 1;
    printf("\n[*] Scanning %s from port %d to %d (%d ports)\n", 
           ip, start_port, end_port, total_ports);
    printf("[*] Using %d threads\n\n", MAX_THREADS);
    
    // Create threads
    thread_t threads[MAX_THREADS];
    ScanTask tasks[MAX_THREADS];
    int task_count = 0;
    int open_count = 0;
    
    for (int port = start_port; port <= end_port; port++) {
        // Create task
        strcpy(tasks[task_count].ip, ip);
        tasks[task_count].port = port;
        tasks[task_count].is_open = 0;
        
        // Create thread using wrapper
        create_thread_wrapper(&threads[task_count], &tasks[task_count]);
        task_count++;
        
        // If we've reached max threads, wait for them to finish
        if (task_count >= MAX_THREADS || port == end_port) {
            // Wait for all threads to complete
            for (int i = 0; i < task_count; i++) {
                join_thread(threads[i]);
            }
            
            // Process results
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].is_open) {
                    printf("[+] Port %d is OPEN   (%s)\n", 
                           tasks[i].port, get_service_name(tasks[i].port));
                    open_count++;
                }
            }
            
            // Show progress
            printf("[*] Progress: %d/%d ports scanned\r", 
                   port - start_port + 1, total_ports);
            fflush(stdout);
            
            task_count = 0;
        }
    }
    
    printf("\n\n========================================\n");
    printf("[*] Scan complete! Found %d open ports\n", open_count);
    printf("========================================\n");
}

int main(int argc, char* argv[]) {
    char target_ip[46] = "127.0.0.1";
    int start_port = 1;
    int end_port = 1024;
    
    // Initialize sockets (Windows only)
    if (!init_sockets()) {
        return 1;
    }
    
    // Parse command line arguments
    if (argc < 2) {
        printf("Usage: %s <IP or domain> [start_port] [end_port]\n", argv[0]);
        printf("Examples:\n");
        printf("  %s 192.168.1.1\n", argv[0]);
        printf("  %s 192.168.1.1 1 1000\n", argv[0]);
        printf("  %s google.com 80 443\n", argv[0]);
        printf("\nUsing default: 127.0.0.1 1-1024\n");
    } else {
        strcpy(target_ip, argv[1]);
        if (argc >= 3) start_port = atoi(argv[2]);
        if (argc >= 4) end_port = atoi(argv[3]);
    }
    
    printf("========================================\n");
    printf("     Port Scanner v1.5\n");
    printf("  (Cross-Platform Edition)\n");
    printf("========================================\n");
    printf("[*] Target: %s\n", target_ip);
    
    scan_range(target_ip, start_port, end_port);
    
    // Cleanup sockets (Windows only)
    cleanup_sockets();
    
    return 0;
}
