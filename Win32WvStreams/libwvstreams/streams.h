#pragma once
#include <winsock2.h>

int close(int fd);
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);

// this little trick allows us to define our own close/read/write  
// (in streams.cc) that optionally call _close/_read/_write (defined in <io.h>)
#define __STDC__ 1 // prevents io.h from dllimporting close/read/write
#include <io.h>

struct socket_fd_pair
{
    SOCKET socket;
    int fd;
};

class SocketFromFDMaker
{
protected:
    HANDLE m_hThread;
    socket_fd_pair m_pair;
    SOCKET m_socket;
    bool m_wait;
public:
    SocketFromFDMaker(int fd, LPTHREAD_START_ROUTINE lpt, bool wait_for_termination = false);
    ~SocketFromFDMaker();
    SOCKET GetSocket() { return m_socket; }
};

DWORD WINAPI fd2socket_fwd(LPVOID lpThreadParameter);
DWORD WINAPI socket2fd_fwd(LPVOID lpThreadParameter);