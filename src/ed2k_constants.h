/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_ED2K_CONSTANTS_H
#define D_ED2K_CONSTANTS_H

#include "common.h"

#include <cstdint>

namespace aria2 {

namespace ed2k {

constexpr uint8_t PROTO_EDONKEY = 0xe3;
constexpr uint8_t PROTO_EMULE = 0xc5;
constexpr uint8_t OP_LOGINREQUEST = 0x01;
constexpr uint8_t OP_GETSERVERLIST = 0x14;
constexpr uint8_t OP_SEARCHREQUEST = 0x16;
constexpr uint8_t OP_GETSOURCES = 0x19;
constexpr uint8_t OP_CALLBACKREQUEST = 0x1c;
constexpr uint8_t OP_QUERY_MORE_RESULT = 0x21;
constexpr uint8_t OP_SERVERLIST = 0x32;
constexpr uint8_t OP_SEARCHRESULT = 0x33;
constexpr uint8_t OP_SERVERSTATUS = 0x34;
constexpr uint8_t OP_CALLBACKREQUESTED = 0x35;
constexpr uint8_t OP_CALLBACK_FAIL = 0x36;
constexpr uint8_t OP_SERVERMESSAGE = 0x38;
constexpr uint8_t OP_IDCHANGE = 0x40;
constexpr uint8_t OP_FOUNDSOURCES = 0x42;
constexpr uint8_t OP_GLOBSERVSTATREQ = 0x96;
constexpr uint8_t OP_GLOBSERVSTATRES = 0x97;
constexpr uint8_t OP_HELLO = 0x01;
constexpr uint8_t OP_HELLOANSWER = 0x4c;
constexpr uint8_t OP_SETREQFILEID = 0x4f;
constexpr uint8_t OP_FILESTATUS = 0x50;
constexpr uint8_t OP_HASHSETREQUEST = 0x51;
constexpr uint8_t OP_HASHSETANSWER = 0x52;
constexpr uint8_t OP_STARTUPLOADREQ = 0x54;
constexpr uint8_t OP_ACCEPTUPLOADREQ = 0x55;
constexpr uint8_t OP_FILEREQANSNOFIL = 0x48;
constexpr uint8_t OP_CANCELTRANSFER = 0x56;
constexpr uint8_t OP_OUTOFPARTREQS = 0x57;
constexpr uint8_t OP_QUEUERANK = 0x5c;
constexpr uint8_t OP_QUEUERANKING = 0x60;
constexpr uint8_t OP_REQUESTFILENAME = 0x58;
constexpr uint8_t OP_REQFILENAMEANSWER = 0x59;
constexpr uint8_t OP_REQUESTPARTS = 0x47;
constexpr uint8_t OP_SENDINGPART = 0x46;
constexpr uint8_t OP_COMPRESSEDPART = 0x40;
constexpr uint8_t OP_EMULEINFO = 0x01;
constexpr uint8_t OP_EMULEINFOANSWER = 0x02;
constexpr uint8_t OP_REQUESTSOURCES = 0x81;
constexpr uint8_t OP_ANSWERSOURCES = 0x82;
constexpr uint8_t OP_REQUESTSOURCES2 = 0x83;
constexpr uint8_t OP_ANSWERSOURCES2 = 0x84;
constexpr uint8_t OP_AICHREQUEST = 0x9b;
constexpr uint8_t OP_AICHANSWER = 0x9c;
constexpr uint8_t OP_AICHFILEHASHANS = 0x9d;
constexpr uint8_t OP_AICHFILEHASHREQ = 0x9e;
constexpr uint8_t OP_COMPRESSEDPART_I64 = 0xa1;
constexpr uint8_t OP_REQUESTPARTS_I64 = 0xa3;
constexpr uint8_t OP_SENDINGPART_I64 = 0xa2;
constexpr uint8_t OP_REASKFILEPING = 0x90;
constexpr uint8_t OP_REASKACK = 0x91;
constexpr uint8_t OP_FILENOTFOUND = 0x92;
constexpr uint8_t OP_QUEUEFULL = 0x93;
constexpr uint8_t OP_REASKCALLBACKUDP = 0x94;
constexpr uint8_t OP_REASKCALLBACKTCP = 0x9a;
constexpr uint8_t KAD_PROTOCOL = 0xe4;
constexpr uint8_t KAD_BOOTSTRAP_REQ = 0x01;
constexpr uint8_t KAD_BOOTSTRAP_RES = 0x09;
constexpr uint8_t KAD_HELLO_REQ = 0x11;
constexpr uint8_t KAD_HELLO_RES = 0x19;
constexpr uint8_t KAD_REQ = 0x21;
constexpr uint8_t KAD_RES = 0x29;
constexpr uint8_t KAD_SEARCH_KEYS_REQ = 0x33;
constexpr uint8_t KAD_SEARCH_SOURCES_REQ = 0x34;
constexpr uint8_t KAD_SEARCH_RES = 0x3b;
constexpr uint8_t KAD_PUBLISH_SOURCE_REQ = 0x44;
constexpr uint8_t KAD_PUBLISH_RES = 0x4b;
constexpr uint8_t KAD_FIREWALLED_REQ = 0x53;
constexpr uint8_t KAD_FIREWALLED_RES = 0x58;
constexpr uint8_t KAD_PING = 0x60;
constexpr uint8_t KAD_PONG = 0x61;
constexpr uint8_t KAD_FIREWALLED_UDP = 0x62;
constexpr uint8_t KAD_FIND_VALUE = 0x02;
constexpr uint8_t KAD_STORE = 0x04;
constexpr uint8_t KAD_FIND_NODE = 0x0b;

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_CONSTANTS_H
