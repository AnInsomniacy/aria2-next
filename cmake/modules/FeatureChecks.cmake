set(PACKAGE "Aria2 Next")
set(PACKAGE_NAME "Aria2 Next")
set(PACKAGE_TARNAME "aria2-next")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "Aria2 Next ${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/AnInsomniacy/aria2-next/issues")
set(PACKAGE_URL "https://github.com/AnInsomniacy/aria2-next")
set(VERSION "${PROJECT_VERSION}")
set(BUILD "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}")
set(HOST "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}")
set(TARGET "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}")
set(STDC_HEADERS 1)

if(ARIA2_DEFAULT_DISK_CACHE)
  set(DEFAULT_DISK_CACHE "${ARIA2_DEFAULT_DISK_CACHE}")
endif()

if(WIN32)
  set(SECURITY_WIN32 1)
endif()

aria2_check_include("alloca.h" HAVE_ALLOCA_H)
aria2_check_include("arpa/inet.h" HAVE_ARPA_INET_H)
aria2_check_include("ifaddrs.h" HAVE_IFADDRS_H)
aria2_check_include("inttypes.h" HAVE_INTTYPES_H)
aria2_check_include("io.h" HAVE_IO_H)
aria2_check_include("iphlpapi.h" HAVE_IPHLPAPI_H)
aria2_check_include("malloc.h" HAVE_MALLOC_H)
aria2_check_include("memory.h" HAVE_MEMORY_H)
aria2_check_include("netdb.h" HAVE_NETDB_H)
aria2_check_include("netinet/in.h" HAVE_NETINET_IN_H)
aria2_check_include("netinet/tcp.h" HAVE_NETINET_TCP_H)
aria2_check_include("poll.h" HAVE_POLL_H)
aria2_check_include("pwd.h" HAVE_PWD_H)
aria2_check_include("share.h" HAVE_SHARE_H)
aria2_check_include("signal.h" HAVE_SIGNAL_H)
aria2_check_include("stdint.h" HAVE_STDINT_H)
aria2_check_include("stdlib.h" HAVE_STDLIB_H)
aria2_check_include("string.h" HAVE_STRING_H)
aria2_check_include("sys/ioctl.h" HAVE_SYS_IOCTL_H)
aria2_check_include("sys/param.h" HAVE_SYS_PARAM_H)
aria2_check_include("sys/resource.h" HAVE_SYS_RESOURCE_H)
aria2_check_include("sys/signal.h" HAVE_SYS_SIGNAL_H)
aria2_check_include("sys/socket.h" HAVE_SYS_SOCKET_H)
aria2_check_include("sys/types.h" HAVE_SYS_TYPES_H)
aria2_check_include("sys/uio.h" HAVE_SYS_UIO_H)
aria2_check_include("sys/utsname.h" HAVE_SYS_UTSNAME_H)
aria2_check_include("termios.h" HAVE_TERMIOS_H)
aria2_check_include("utime.h" HAVE_UTIME_H)
aria2_check_include("winsock2.h" HAVE_WINSOCK2_H)
aria2_check_include("ws2tcpip.h" HAVE_WS2TCPIP_H)

if(WIN32)
  check_cxx_source_compiles("
#include <winsock2.h>
#include <windows.h>
#include <winioctl.h>
int main() {
  DWORD bytesReturned = 0;
  (void)bytesReturned;
  return FSCTL_SET_SPARSE == 0;
}" HAVE_WINIOCTL_H)
else()
  aria2_check_include("winioctl.h" HAVE_WINIOCTL_H)
endif()

check_type_size("ptrdiff_t" PTRDIFF_T LANGUAGE CXX)
if(HAVE_PTRDIFF_T)
  set(HAVE_PTRDIFF_T 1)
endif()
check_cxx_source_compiles("
#include <time.h>
int main() {
  struct timespec value;
  (void)value;
  return 0;
}" ARIA2_HAVE_STRUCT_TIMESPEC)
if(ARIA2_HAVE_STRUCT_TIMESPEC)
  set(HAVE_A2_STRUCT_TIMESPEC 1)
endif()
test_big_endian(WORDS_BIGENDIAN)
if(WORDS_BIGENDIAN)
  set(WORDS_BIGENDIAN 1)
else()
  set(WORDS_BIGENDIAN "")
endif()

aria2_check_c_symbol(daemon HAVE_DAEMON "unistd.h")
aria2_check_c_symbol(fallocate HAVE_FALLOCATE "fcntl.h")
aria2_check_c_symbol(gai_strerror HAVE_GAI_STRERROR "netdb.h")
aria2_check_c_symbol(getaddrinfo HAVE_GETADDRINFO "sys/types.h;sys/socket.h;netdb.h")
aria2_check_c_symbol(getifaddrs HAVE_GETIFADDRS "ifaddrs.h")
aria2_check_c_symbol(getnameinfo HAVE_GETNAMEINFO "sys/types.h;sys/socket.h;netdb.h")
aria2_check_c_symbol(gettimeofday HAVE_GETTIMEOFDAY "sys/time.h")
aria2_check_c_symbol(kqueue HAVE_KQUEUE "sys/types.h;sys/event.h")
aria2_check_c_symbol(memcpy HAVE_MEMCPY "string.h")
aria2_check_c_symbol(mmap HAVE_MMAP "sys/mman.h")
aria2_check_c_symbol(poll HAVE_POLL "poll.h")
aria2_check_c_symbol(posix_fadvise HAVE_POSIX_FADVISE "fcntl.h")
aria2_check_c_symbol(posix_fallocate HAVE_POSIX_FALLOCATE "fcntl.h")
aria2_check_c_symbol(posix_memalign HAVE_POSIX_MEMALIGN "stdlib.h")
aria2_check_c_symbol(sigaction HAVE_SIGACTION "signal.h")
aria2_check_c_symbol(sleep HAVE_SLEEP "unistd.h")
aria2_check_c_symbol(timegm HAVE_TIMEGM "time.h")
aria2_check_c_symbol(usleep HAVE_USLEEP "unistd.h")
aria2_check_c_symbol(utimes HAVE_UTIMES "sys/time.h")

aria2_check_c_compiles(HAVE_LOCALTIME_R "
#include <time.h>
int main(void) {
  time_t value = 0;
  struct tm result;
  return localtime_r(&value, &result) == 0;
}")

aria2_check_c_compiles(HAVE_STRPTIME "
#include <time.h>
int main(void) {
  struct tm result;
  return strptime(\"1970\", \"%Y\", &result) == 0;
}")

aria2_check_c_compiles(HAVE_WORKING_FORK "
#include <sys/types.h>
#include <unistd.h>
int main(void) {
  pid_t pid = fork();
  return pid == (pid_t)-1;
}")

aria2_check_c_compiles(HAVE_WORKING_VFORK "
#include <sys/types.h>
#include <unistd.h>
int main(void) {
  pid_t pid = vfork();
  return pid == (pid_t)-1;
}")

if(WIN32)
  set(HAVE_GETADDRINFO 1)
  set(HAVE_GAI_STRERROR "")
endif()

if(ARIA2_ENABLE_EPOLL AND HAVE_EPOLL_CREATE)
  set(HAVE_EPOLL 1)
endif()

if(HAVE_POSIX_FALLOCATE OR HAVE_FALLOCATE OR APPLE OR WIN32)
  set(HAVE_SOME_FALLOCATE 1)
endif()

check_struct_has_member("struct sockaddr_in" sin_len "sys/types.h;sys/socket.h;netinet/in.h" HAVE_SOCKADDR_IN_SIN_LEN LANGUAGE CXX)
check_struct_has_member("struct sockaddr_in6" sin6_len "sys/types.h;sys/socket.h;netinet/in.h" HAVE_SOCKADDR_IN6_SIN6_LEN LANGUAGE CXX)

check_cxx_source_compiles("
#include <unistd.h>
#include <getopt.h>
int main() {
  const char* name = \"name\";
  struct option option_value = { name, 0, 0, 0 };
  (void)option_value;
  return 0;
}" HAVE_OPTION_CONST_NAME)

if(HAVE_KQUEUE)
  check_cxx_source_compiles("
  #include <cstdint>
  #include <sys/types.h>
  #include <sys/event.h>
  #include <sys/time.h>
  int main() {
    struct kevent event;
    event.udata = reinterpret_cast<intptr_t>(&event);
    return 0;
  }" KEVENT_UDATA_INTPTR_T)
endif()

if(APPLE)
  set(HAVE_CFLOCALECOPYCURRENT 1)
  set(HAVE_CFPREFERENCESCOPYAPPVALUE 1)
endif()

aria2_pkg_check(ZLIB "zlib>=${ARIA2_MIN_ZLIB_VERSION}")
if(NOT ZLIB_FOUND)
  message(FATAL_ERROR "zlib >= ${ARIA2_MIN_ZLIB_VERSION} is required")
endif()
set(HAVE_ZLIB 1)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES PkgConfig::ZLIB)
check_function_exists(gzbuffer HAVE_GZBUFFER)
check_function_exists(gzsetparams HAVE_GZSETPARAMS)
cmake_pop_check_state()

if(ARIA2_WITH_EXPAT)
  aria2_pkg_check(EXPAT "expat")
  if(EXPAT_FOUND)
    set(HAVE_LIBEXPAT 1)
  endif()
endif()

aria2_pkg_check(SQLITE3 "sqlite3>=${ARIA2_MIN_SQLITE3_VERSION}")
if(ARIA2_WITH_SQLITE3 AND SQLITE3_FOUND)
  set(HAVE_SQLITE3 1)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES PkgConfig::SQLITE3)
  check_function_exists(sqlite3_open_v2 HAVE_SQLITE3_OPEN_V2)
  cmake_pop_check_state()
endif()

aria2_pkg_check(LIBCARES "libcares>=${ARIA2_MIN_LIBCARES_VERSION}")
if(ARIA2_WITH_CARES AND LIBCARES_FOUND)
  set(HAVE_LIBCARES 1)
  set(ENABLE_ASYNC_DNS 1)
endif()

aria2_pkg_check(LIBSSH2 "libssh2>=${ARIA2_MIN_LIBSSH2_VERSION}")
if(ARIA2_WITH_LIBSSH2 AND LIBSSH2_FOUND)
  set(HAVE_LIBSSH2 1)
endif()

aria2_pkg_check_dynamic(OPENSSL "openssl>=${ARIA2_MIN_OPENSSL_VERSION}")

if(ARIA2_ENABLE_SSL AND WIN32 AND ARIA2_WITH_WINTLS)
  cmake_push_check_state(RESET)
  check_cxx_source_compiles("
#include <winsock2.h>
#include <windows.h>
#include <security.h>
#include <schnlsp.h>
int main() {
  SCH_CREDENTIALS credentials;
  TLS_PARAMETERS tls_parameters;
  credentials.dwVersion = SCH_CREDENTIALS_VERSION;
  credentials.cTlsParameters = 1;
  credentials.pTlsParameters = &tls_parameters;
  return 0;
}" HAVE_SCH_CREDENTIALS)
  cmake_pop_check_state()
  set(HAVE_WINTLS 1)
  set(ENABLE_SSL 1)
elseif(ARIA2_ENABLE_SSL AND ARIA2_WITH_OPENSSL AND OPENSSL_FOUND)
  set(HAVE_OPENSSL 1)
  set(ENABLE_SSL 1)
endif()

if(HAVE_OPENSSL)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES PkgConfig::OPENSSL)
  check_function_exists(EVP_sha224 HAVE_EVP_SHA224)
  check_function_exists(EVP_sha256 HAVE_EVP_SHA256)
  check_function_exists(EVP_sha384 HAVE_EVP_SHA384)
  check_function_exists(EVP_sha512 HAVE_EVP_SHA512)
  cmake_pop_check_state()
endif()

# OpenSSL provides the message digests and MSE DH implementation. Other TLS
# backends use the internal implementations. RC4 is always internal and is
# compiled unconditionally (see ARIA2_SOURCES_ARC4).
if(HAVE_OPENSSL)
  set(USE_OPENSSL_MD 1)
else()
  set(USE_INTERNAL_MD 1)
  set(USE_INTERNAL_MSE_DH 1)
endif()

if(ARIA2_ENABLE_BITTORRENT)
  set(ENABLE_BITTORRENT 1)
endif()
set(ENABLE_ED2K 1)
if(HAVE_LIBEXPAT)
  set(ENABLE_XML_RPC 1)
endif()
if(ARIA2_ENABLE_METALINK AND HAVE_LIBEXPAT)
  set(ENABLE_METALINK 1)
endif()
if(ARIA2_ENABLE_WEBSOCKET)
  set(ENABLE_WEBSOCKET 1)
endif()
if(ARIA2_ENABLE_LIBARIA2)
  set(ENABLE_LIBARIA2 1)
endif()
