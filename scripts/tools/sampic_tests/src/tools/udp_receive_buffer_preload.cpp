#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/socket.h>

namespace {
using SocketFunction = int (*)(int, int, int);
}

extern "C" int socket(int domain, int type, int protocol) {
  static auto real_socket =
      reinterpret_cast<SocketFunction>(dlsym(RTLD_NEXT, "socket"));
  if (!real_socket) {
    errno = ENOSYS;
    return -1;
  }

  const int fd = real_socket(domain, type, protocol);
  const int socket_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (fd < 0 || socket_type != SOCK_DGRAM) return fd;

  const char* value = std::getenv("SAMPIC_UDP_RCVBUF_BYTES");
  if (!value || !*value) return fd;
  char* end = nullptr;
  const long requested = std::strtol(value, &end, 10);
  if (end == value || *end != '\0' || requested < 0 ||
      requested > 2147483647L) {
    std::fprintf(stderr,
                 "[udp-rcvbuf] invalid SAMPIC_UDP_RCVBUF_BYTES=%s\n", value);
    return fd;
  }

  if (requested == 0) {
    int effective = 0;
    socklen_t length = sizeof(effective);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &effective, &length) == 0) {
      std::fprintf(stderr,
                   "[udp-rcvbuf] fd=%d inherited effective=%d\n",
                   fd, effective);
    }
    return fd;
  }

  const int requested_int = static_cast<int>(requested);
  const int result =
      setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &requested_int,
                 sizeof(requested_int));
  int effective = 0;
  socklen_t length = sizeof(effective);
  const int query_result =
      getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &effective, &length);
  if (result != 0) {
    std::fprintf(stderr,
                 "[udp-rcvbuf] fd=%d request=%d failed errno=%d\n",
                 fd, requested_int, errno);
  } else if (query_result == 0) {
    std::fprintf(stderr,
                 "[udp-rcvbuf] fd=%d requested=%d effective=%d\n",
                 fd, requested_int, effective);
  }
  return fd;
}
