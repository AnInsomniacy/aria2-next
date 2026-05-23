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
set(CXX11_OVERRIDE "override")
set(STDC_HEADERS 1)
set(HAVE_CXX11 1)
set(HAVE_ALLOCA 1)
set(SELECT_TYPE_ARG1 int)
set(SELECT_TYPE_ARG234 "(fd_set *)")
set(SELECT_TYPE_ARG5 "(struct timeval *)")

if(NOT ARIA2_CA_BUNDLE AND NOT WIN32)
  foreach(aria2_ca_bundle_candidate
      /etc/ssl/certs/ca-certificates.crt
      /etc/pki/tls/certs/ca-bundle.crt
      /etc/ssl/cert.pem)
    if(EXISTS "${aria2_ca_bundle_candidate}")
      set(ARIA2_CA_BUNDLE "${aria2_ca_bundle_candidate}" CACHE FILEPATH
          "CA bundle fallback path for OpenSSL and GnuTLS builds" FORCE)
      break()
    endif()
  endforeach()
endif()

if(ARIA2_CA_BUNDLE)
  set(CA_BUNDLE "${ARIA2_CA_BUNDLE}")
endif()
if(ARIA2_DEFAULT_DISK_CACHE)
  set(DEFAULT_DISK_CACHE "${ARIA2_DEFAULT_DISK_CACHE}")
endif()

if(WIN32)
  set(SECURITY_WIN32 1)
endif()

aria2_check_include("alloca.h" HAVE_ALLOCA_H)
aria2_check_include("argz.h" HAVE_ARGZ_H)
aria2_check_include("arpa/inet.h" HAVE_ARPA_INET_H)
aria2_check_include("dlfcn.h" HAVE_DLFCN_H)
aria2_check_include("fcntl.h" HAVE_FCNTL_H)
aria2_check_include("float.h" HAVE_FLOAT_H)
aria2_check_include("ifaddrs.h" HAVE_IFADDRS_H)
aria2_check_include("inttypes.h" HAVE_INTTYPES_H)
aria2_check_include("io.h" HAVE_IO_H)
aria2_check_include("iphlpapi.h" HAVE_IPHLPAPI_H)
aria2_check_include("langinfo.h" HAVE_LANGINFO_H)
aria2_check_include("limits.h" HAVE_LIMITS_H)
aria2_check_include("locale.h" HAVE_LOCALE_H)
aria2_check_include("malloc.h" HAVE_MALLOC_H)
aria2_check_include("memory.h" HAVE_MEMORY_H)
aria2_check_include("mmsystem.h" HAVE_MMSYSTEM_H)
aria2_check_include("netdb.h" HAVE_NETDB_H)
aria2_check_include("netinet/in.h" HAVE_NETINET_IN_H)
aria2_check_include("netinet/tcp.h" HAVE_NETINET_TCP_H)
aria2_check_include("poll.h" HAVE_POLL_H)
aria2_check_include("port.h" HAVE_PORT_H)
aria2_check_include("pwd.h" HAVE_PWD_H)
aria2_check_include("share.h" HAVE_SHARE_H)
aria2_check_include("signal.h" HAVE_SIGNAL_H)
aria2_check_include("stdbool.h" HAVE_STDBOOL_H)
aria2_check_include("stddef.h" HAVE_STDDEF_H)
aria2_check_include("stdint.h" HAVE_STDINT_H)
aria2_check_include("stdio.h" HAVE_STDIO_H)
aria2_check_include("stdio_ext.h" HAVE_STDIO_EXT_H)
aria2_check_include("stdlib.h" HAVE_STDLIB_H)
aria2_check_include("string.h" HAVE_STRING_H)
aria2_check_include("strings.h" HAVE_STRINGS_H)
aria2_check_include("sys/ioctl.h" HAVE_SYS_IOCTL_H)
aria2_check_include("sys/param.h" HAVE_SYS_PARAM_H)
aria2_check_include("sys/resource.h" HAVE_SYS_RESOURCE_H)
aria2_check_include("sys/select.h" HAVE_SYS_SELECT_H)
aria2_check_include("sys/signal.h" HAVE_SYS_SIGNAL_H)
aria2_check_include("sys/socket.h" HAVE_SYS_SOCKET_H)
aria2_check_include("sys/stat.h" HAVE_SYS_STAT_H)
aria2_check_include("sys/time.h" HAVE_SYS_TIME_H)
aria2_check_include("sys/types.h" HAVE_SYS_TYPES_H)
aria2_check_include("sys/uio.h" HAVE_SYS_UIO_H)
aria2_check_include("sys/utsname.h" HAVE_SYS_UTSNAME_H)
aria2_check_include("termios.h" HAVE_TERMIOS_H)
aria2_check_include("unistd.h" HAVE_UNISTD_H)
aria2_check_include("utime.h" HAVE_UTIME_H)
aria2_check_include("vfork.h" HAVE_VFORK_H)
aria2_check_include("wchar.h" HAVE_WCHAR_H)
aria2_check_include("windows.h" HAVE_WINDOWS_H)
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

aria2_check_c_symbol(__argz_count HAVE___ARGZ_COUNT "argz.h")
aria2_check_c_symbol(__argz_next HAVE___ARGZ_NEXT "argz.h")
aria2_check_c_symbol(__argz_stringify HAVE___ARGZ_STRINGIFY "argz.h")
aria2_check_c_symbol(alarm HAVE_ALARM "unistd.h")
aria2_check_c_symbol(atexit HAVE_ATEXIT "stdlib.h")
aria2_check_c_symbol(basename HAVE_BASENAME "libgen.h")
aria2_check_c_symbol(daemon HAVE_DAEMON "unistd.h")
aria2_check_c_symbol(epoll_create HAVE_EPOLL_CREATE "sys/epoll.h")
aria2_check_c_symbol(fallocate HAVE_FALLOCATE "fcntl.h")
aria2_check_c_symbol(fork HAVE_FORK "unistd.h")
aria2_check_c_symbol(ftruncate HAVE_FTRUNCATE "unistd.h")
aria2_check_c_symbol(gai_strerror HAVE_GAI_STRERROR "netdb.h")
aria2_check_c_symbol(getaddrinfo HAVE_GETADDRINFO "sys/types.h;sys/socket.h;netdb.h")
aria2_check_c_symbol(getcwd HAVE_GETCWD "unistd.h")
aria2_check_c_symbol(getentropy HAVE_GETENTROPY "unistd.h")
aria2_check_c_symbol(gethostbyaddr HAVE_GETHOSTBYADDR "netdb.h")
aria2_check_c_symbol(gethostbyname HAVE_GETHOSTBYNAME "netdb.h")
aria2_check_c_symbol(getifaddrs HAVE_GETIFADDRS "ifaddrs.h")
aria2_check_c_symbol(getnameinfo HAVE_GETNAMEINFO "sys/types.h;sys/socket.h;netdb.h")
aria2_check_c_symbol(getpagesize HAVE_GETPAGESIZE "unistd.h")
aria2_check_c_symbol(gettimeofday HAVE_GETTIMEOFDAY "sys/time.h")
aria2_check_c_symbol(kqueue HAVE_KQUEUE "sys/types.h;sys/event.h")
aria2_check_c_symbol(memchr HAVE_MEMCHR "string.h")
aria2_check_c_symbol(memcpy HAVE_MEMCPY "string.h")
aria2_check_c_symbol(memmove HAVE_MEMMOVE "string.h")
aria2_check_c_symbol(mempcpy HAVE_MEMPCPY "string.h")
aria2_check_c_symbol(memset HAVE_MEMSET "string.h")
aria2_check_c_symbol(mkdir HAVE_MKDIR "sys/stat.h")
aria2_check_c_symbol(mmap HAVE_MMAP "sys/mman.h")
aria2_check_c_symbol(munmap HAVE_MUNMAP "sys/mman.h")
aria2_check_c_symbol(nl_langinfo HAVE_NL_LANGINFO "langinfo.h")
aria2_check_c_symbol(poll HAVE_POLL "poll.h")
aria2_check_c_symbol(port_associate HAVE_PORT_ASSOCIATE "port.h")
aria2_check_c_symbol(posix_fadvise HAVE_POSIX_FADVISE "fcntl.h")
aria2_check_c_symbol(posix_fallocate HAVE_POSIX_FALLOCATE "fcntl.h")
aria2_check_c_symbol(posix_memalign HAVE_POSIX_MEMALIGN "stdlib.h")
aria2_check_c_symbol(pow HAVE_POW "math.h")
aria2_check_c_symbol(putenv HAVE_PUTENV "stdlib.h")
aria2_check_c_symbol(rmdir HAVE_RMDIR "unistd.h")
aria2_check_c_symbol(select HAVE_SELECT "sys/select.h")
aria2_check_c_symbol(setlocale HAVE_SETLOCALE "locale.h")
aria2_check_c_symbol(sigaction HAVE_SIGACTION "signal.h")
aria2_check_c_symbol(sleep HAVE_SLEEP "unistd.h")
aria2_check_c_symbol(socket HAVE_SOCKET "sys/types.h;sys/socket.h")
aria2_check_c_symbol(stpcpy HAVE_STPCPY "string.h")
aria2_check_c_symbol(strcasecmp HAVE_STRCASECMP "strings.h")
aria2_check_c_symbol(strchr HAVE_STRCHR "string.h")
aria2_check_c_symbol(strcspn HAVE_STRCSPN "string.h")
aria2_check_c_symbol(strdup HAVE_STRDUP "string.h")
aria2_check_c_symbol(strerror HAVE_STRERROR "string.h")
aria2_check_c_symbol(strftime HAVE_STRFTIME "time.h")
aria2_check_c_symbol(strncasecmp HAVE_STRNCASECMP "strings.h")
aria2_check_c_symbol(strstr HAVE_STRSTR "string.h")
aria2_check_c_symbol(strtol HAVE_STRTOL "stdlib.h")
aria2_check_c_symbol(strtoul HAVE_STRTOUL "stdlib.h")
aria2_check_c_symbol(strtoull HAVE_STRTOULL "stdlib.h")
aria2_check_c_symbol(timegm HAVE_TIMEGM "time.h")
aria2_check_c_symbol(tzset HAVE_TZSET "time.h")
aria2_check_c_symbol(unsetenv HAVE_UNSETENV "stdlib.h")
aria2_check_c_symbol(usleep HAVE_USLEEP "unistd.h")
aria2_check_c_symbol(utime HAVE_UTIME "utime.h")
aria2_check_c_symbol(utimes HAVE_UTIMES "sys/time.h")
aria2_check_c_symbol(vfork HAVE_VFORK "unistd.h")
aria2_check_c_symbol(vprintf HAVE_VPRINTF "stdio.h")

aria2_check_c_compiles(HAVE_ASCTIME_R "
#include <time.h>
int main(void) {
  struct tm value;
  char buffer[32];
  return asctime_r(&value, buffer) == 0;
}")

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
if(ARIA2_WITH_ZLIB AND ZLIB_FOUND)
  set(HAVE_ZLIB 1)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES PkgConfig::ZLIB)
  check_function_exists(gzbuffer HAVE_GZBUFFER)
  check_function_exists(gzsetparams HAVE_GZSETPARAMS)
  cmake_pop_check_state()
endif()

aria2_pkg_check(LIBXML2 "libxml-2.0>=${ARIA2_MIN_LIBXML2_VERSION}")
if(ARIA2_WITH_LIBXML2 AND LIBXML2_FOUND)
  set(HAVE_LIBXML2 1)
endif()

if(ARIA2_WITH_EXPAT AND NOT HAVE_LIBXML2)
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

aria2_pkg_check(LIBUV "libuv>=${ARIA2_MIN_LIBUV_VERSION}")
if(ARIA2_WITH_LIBUV AND LIBUV_FOUND)
  set(HAVE_LIBUV 1)
endif()

aria2_pkg_check(LIBGNUTLS "gnutls>=${ARIA2_MIN_GNUTLS_VERSION}")
aria2_pkg_check(OPENSSL "openssl>=${ARIA2_MIN_OPENSSL_VERSION}")
aria2_pkg_check(LIBNETTLE "nettle>=${ARIA2_MIN_LIBNETTLE_VERSION}")
aria2_pkg_check(LIBGCRYPT "libgcrypt>=${ARIA2_MIN_LIBGCRYPT_VERSION}")
aria2_pkg_check(LIBTORRENT_RASTERBAR "libtorrent-rasterbar")
find_package(Boost QUIET)

if(LIBNETTLE_FOUND)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES PkgConfig::LIBNETTLE)
  check_cxx_source_compiles("
#include <nettle/nettle-meta.h>
int main() {
  nettle_sha1.digest(0, 0);
  return 0;
}" HAVE_NETTLE_HASH_DIGEST_WITHOUT_LENGTH)
  cmake_pop_check_state()
endif()

set(have_native_tls OFF)
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
  set(have_native_tls ON)
elseif(ARIA2_ENABLE_SSL AND ARIA2_WITH_OPENSSL AND OPENSSL_FOUND)
  set(HAVE_OPENSSL 1)
  set(ENABLE_SSL 1)
elseif(ARIA2_ENABLE_SSL AND ARIA2_WITH_GNUTLS AND LIBGNUTLS_FOUND)
  set(HAVE_LIBGNUTLS 1)
  set(ENABLE_SSL 1)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES PkgConfig::LIBGNUTLS)
  check_function_exists(gnutls_certificate_set_x509_system_trust HAVE_GNUTLS_CERTIFICATE_SET_X509_SYSTEM_TRUST)
  cmake_pop_check_state()
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

if(ARIA2_WITH_LIBNETTLE AND LIBNETTLE_FOUND AND NOT HAVE_OPENSSL AND NOT have_native_tls)
  set(HAVE_LIBNETTLE 1)
endif()

if(ARIA2_WITH_GMP AND ARIA2_ENABLE_BITTORRENT AND NOT HAVE_OPENSSL AND (HAVE_LIBNETTLE OR have_native_tls))
  check_library_exists(gmp __gmpz_init "" HAVE_LIBGMP)
  if(HAVE_LIBGMP)
    check_library_exists(gmp __gmpz_powm_sec "" HAVE___GMPZ_POWM_SEC)
    if(HAVE___GMPZ_POWM_SEC)
      set(HAVE_GMP_SEC 1)
    endif()
  endif()
endif()

if(ARIA2_WITH_LIBGCRYPT AND LIBGCRYPT_FOUND AND NOT HAVE_OPENSSL AND NOT HAVE_LIBNETTLE AND NOT have_native_tls)
  set(HAVE_LIBGCRYPT 1)
endif()

if(HAVE_OPENSSL)
  set(USE_OPENSSL_MD 1)
elseif(HAVE_LIBNETTLE)
  set(USE_LIBNETTLE_MD 1)
elseif(HAVE_LIBGCRYPT)
  set(USE_LIBGCRYPT_MD 1)
else()
  set(USE_INTERNAL_MD 1)
endif()

if(NOT HAVE_LIBGMP AND NOT HAVE_LIBGCRYPT AND NOT HAVE_OPENSSL)
  set(USE_INTERNAL_BIGNUM 1)
endif()
if(NOT HAVE_LIBNETTLE AND NOT HAVE_LIBGCRYPT AND NOT HAVE_OPENSSL)
  set(USE_INTERNAL_ARC4 1)
endif()

if(ARIA2_ENABLE_BITTORRENT)
  if(NOT LIBTORRENT_RASTERBAR_FOUND)
    message(FATAL_ERROR
      "BitTorrent support now requires libtorrent-rasterbar. "
      "Install libtorrent-rasterbar development files or configure with "
      "-DARIA2_ENABLE_BITTORRENT=OFF.")
  endif()
  if(NOT Boost_FOUND)
    message(FATAL_ERROR
      "BitTorrent support now requires Boost headers for "
      "libtorrent-rasterbar. Install Boost development files or configure "
      "with -DARIA2_ENABLE_BITTORRENT=OFF.")
  endif()
  set(HAVE_LIBTORRENT_RASTERBAR 1)
  set(ENABLE_BITTORRENT 1)
endif()
if(HAVE_LIBXML2 OR HAVE_LIBEXPAT)
  set(HAVE_SOME_XMLLIB 1)
  set(ENABLE_XML_RPC 1)
endif()
if(ARIA2_ENABLE_METALINK AND HAVE_SOME_XMLLIB)
  set(ENABLE_METALINK 1)
endif()
if(ARIA2_ENABLE_WEBSOCKET)
  set(ENABLE_WEBSOCKET 1)
endif()
if(ARIA2_ENABLE_LIBARIA2)
  set(ENABLE_LIBARIA2 1)
endif()
if(ARIA2_WITH_GNUTLS_SYSTEM_CRYPTO_POLICY)
  set(USE_GNUTLS_SYSTEM_CRYPTO_POLICY 1)
endif()
