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
#ifndef D_ED2K_SERVER_H
#define D_ED2K_SERVER_H

#include "common.h"
#include "ed2k_link.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

struct ServerIdChange {
  uint32_t clientId = 0;
  uint32_t tcpFlags = 0;
  uint32_t auxPort = 0;
  bool highId = false;
  std::string ipAddress;
};

struct ServerStatus {
  uint32_t challenge = 0;
  uint32_t users = 0;
  uint32_t files = 0;
  uint32_t maxUsers = 0;
  uint32_t softFiles = 0;
  uint32_t hardFiles = 0;
  uint32_t udpFlags = 0;
  uint32_t lowIdUsers = 0;
  uint16_t udpObfuscationPort = 0;
  uint16_t tcpObfuscationPort = 0;
  uint32_t udpKey = 0;
};

struct ServerState {
  Endpoint endpoint;
  bool connecting = false;
  bool connected = false;
  bool handshakeCompleted = false;
  uint32_t clientId = 0;
  bool highId = false;
  std::string ipAddress;
  uint32_t tcpFlags = 0;
  uint32_t users = 0;
  uint32_t files = 0;
  uint32_t maxUsers = 0;
  uint32_t softFiles = 0;
  uint32_t hardFiles = 0;
  uint32_t udpFlags = 0;
  uint32_t lowIdUsers = 0;
  uint16_t udpObfuscationPort = 0;
  uint16_t tcpObfuscationPort = 0;
  uint32_t udpKey = 0;
  uint32_t udpStatusChallenge = 0;
  int64_t lastUdpStatusTime = 0;
  uint32_t failCount = 0;
  int64_t lastFailureTime = 0;
  int64_t nextRetryTime = 0;
  std::string lastMessage;
};

std::vector<Endpoint> parseServerMet(const std::string& data);
std::string createLoginRequestPayload(const std::string& clientHash,
                                      uint16_t listenPort,
                                      const std::string& clientName);
std::string createGetSourcesPayload(const std::string& fileHash,
                                    int64_t fileSize);
std::string createFoundSourcesPayload(const std::string& fileHash,
                                      const std::vector<Endpoint>& sources);
std::vector<Endpoint> parseFoundSourcesPayload(const std::string& payload);
bool parseFoundSourcesPayload(std::vector<Endpoint>& sources,
                              const std::string& payload,
                              const std::string& expectedFileHash);
bool parseFoundSourcesPayload(std::vector<FoundSource>& sources,
                              const std::string& payload,
                              const std::string& expectedFileHash);
std::string createCallbackRequestPayload(uint32_t clientId);
bool parseCallbackRequestIncomingPayload(Endpoint& endpoint,
                                         const std::string& payload);
bool parseServerIdChangePayload(ServerIdChange& idChange,
                                const std::string& payload);
bool parseServerStatusPayload(ServerStatus& status,
                              const std::string& payload);
bool parseServerMessagePayload(std::string& message,
                               const std::string& payload);
std::string createServerListPayload(const std::vector<Endpoint>& servers);
bool parseServerListPayload(std::vector<Endpoint>& servers,
                            const std::string& payload);
std::string createServerStatePayload(const ServerState& state);
bool parseServerStatePayload(ServerState& state, const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_SERVER_H
