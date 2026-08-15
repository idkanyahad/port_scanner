# port_scanner
========================================
    Port Scanner v1.5
  (Cross-Platform Edition)
========================================
[*] Target: 192.168.1.1

[*] Scanning 192.168.1.1 from port 1 to 1024 (1024 ports)
[*] Using 100 threads

[+] Port 22 is OPEN   (SSH)
[+] Port 80 is OPEN   (HTTP)
[+] Port 443 is OPEN  (HTTPS)

========================================
[*] Scan complete! Found 3 open ports
========================================



Linux/Mac: gcc -pthread port_scanner.c -o port_scanner

Windows (MinGW): gcc port_scanner.c -lws2_32 -o port_scanner.exe

Windows (MSVC): cl port_scanner.c ws2_32.lib
