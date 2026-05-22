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
#ifndef D_ED2K_KAD_H
#define D_ED2K_KAD_H

#include "common.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

struct KadRoutingSnapshot;

struct KadContact {
  std::string id;
  std::string host;
  uint16_t udpPort = 0;
  uint16_t tcpPort = 0;
  uint8_t version = 0;
  uint32_t udpKey = 0;
};

struct KadHello {
  std::string id;
  uint16_t tcpPort = 0;
  uint8_t version = 0;
  std::vector<Tag> tags;
};

struct KadBootstrapResponse {
  std::string id;
  uint16_t tcpPort = 0;
  uint8_t version = 0;
  std::vector<KadContact> contacts;
};

struct KadRequest {
  uint8_t searchType = 0;
  std::string targetId;
  std::string receiverId;
};

struct KadResponse {
  std::string targetId;
  std::vector<KadContact> contacts;
};

struct KadPublishResult {
  std::string fileId;
  uint8_t count = 0;
};

struct KadFirewalledRequest {
  uint16_t tcpPort = 0;
  std::string id;
  uint8_t options = 0;
};

struct KadFirewalledResponse {
  std::string ipAddress;
};

struct KadFirewalledUdp {
  uint8_t errorCode = 0;
  uint16_t tcpPort = 0;
};

struct KadCallbackRequest {
  std::string buddyId;
  std::string fileId;
  uint16_t tcpPort = 0;
};

struct KadObfuscatedDatagram {
  std::string datagram;
  uint32_t receiverVerifyKey = 0;
  uint32_t senderVerifyKey = 0;
};

struct NodesDat {
  uint32_t version = 0;
  uint32_t bootstrapEdition = 0;
  std::vector<KadContact> contacts;
  std::vector<bool> verified;
};

std::string ed2kHashToKadId(const std::string& hash);
std::string kadIdToEd2kHash(const std::string& id);
std::string kadIdToObfuscationKeyBytes(const std::string& id);
KadContact readKadContact(const std::string& data, size_t& offset);
std::string packKadContact(const KadContact& contact);
std::string createKadHelloPayload(const std::string& id, uint16_t tcpPort,
                                  uint8_t version);
bool parseKadHelloPayload(KadHello& hello, const std::string& payload);
std::string createKadBootstrapResponsePayload(
    const std::string& id, uint16_t tcpPort, uint8_t version,
    const std::vector<KadContact>& contacts);
bool parseKadBootstrapResponsePayload(KadBootstrapResponse& response,
                                      const std::string& payload);
std::string createKadRequestPayload(uint8_t searchType,
                                    const std::string& targetId,
                                    const std::string& receiverId);
bool parseKadRequestPayload(KadRequest& request, const std::string& payload);
std::string createKadResponsePayload(const std::string& targetId,
                                     const std::vector<KadContact>& contacts);
bool parseKadResponsePayload(KadResponse& response,
                             const std::string& payload);
std::string createKadPublishResultPayload(const std::string& fileId,
                                          uint8_t count);
bool parseKadPublishResultPayload(KadPublishResult& result,
                                  const std::string& payload);
std::string createKadFirewalledRequestPayload(uint16_t tcpPort,
                                              const std::string& id,
                                              uint8_t options);
bool parseKadFirewalledRequestPayload(KadFirewalledRequest& request,
                                      const std::string& payload);
std::string createKadFirewalledResponsePayload(const std::string& ipAddress);
bool parseKadFirewalledResponsePayload(KadFirewalledResponse& response,
                                       const std::string& payload);
std::string createKadFirewalledUdpPayload(uint8_t errorCode,
                                          uint16_t tcpPort);
bool parseKadFirewalledUdpPayload(KadFirewalledUdp& response,
                                  const std::string& payload);
std::string createKadCallbackRequestPayload(const std::string& buddyId,
                                            const std::string& fileId,
                                            uint16_t tcpPort);
bool parseKadCallbackRequestPayload(KadCallbackRequest& request,
                                    const std::string& payload);
std::string createKadObfuscatedDatagram(const std::string& datagram,
                                        const std::string& targetId,
                                        uint16_t randomKeyPart,
                                        uint32_t receiverVerifyKey = 0,
                                        uint32_t senderVerifyKey = 0);
std::string createKadObfuscatedDatagram(const std::string& datagram,
                                        uint32_t receiverVerifyKey,
                                        uint32_t senderVerifyKey,
                                        uint16_t randomKeyPart);
bool parseKadObfuscatedDatagram(KadObfuscatedDatagram& parsed,
                                const std::string& datagram,
                                const std::string& targetId);
bool parseKadObfuscatedDatagram(KadObfuscatedDatagram& parsed,
                                const std::string& datagram,
                                uint32_t receiverVerifyKey);
uint32_t createKadUdpVerifyKey(uint32_t baseKey, const std::string& host);
std::string createKadRoutingStatePayload(const KadRoutingSnapshot& snapshot);
bool parseKadRoutingStatePayload(KadRoutingSnapshot& snapshot,
                                 const std::string& payload);
bool parseNodesDat(NodesDat& nodes, const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_KAD_H
