/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>

namespace aria2::ed2k {

constexpr char SERVER_DH_PRIME_HEX[] =
    "F2BF52C55F587ADD5371A936E886EB3C6217A33EC34CB40DC73A41A643AFFCE7"
    "21FC286366535BDBCE259F2286DA4A91B207CBAA5255D4F61CCEAED45AD5E074"
    "7DF7781828105F340F762387F88B289142FB42688F05150F548B5F436AF70DF3";

std::string createTcpObfuscationKey(const std::string& userHash,
                                    uint8_t magicValue, uint32_t randomKeyPart);

std::string createServerTcpObfuscationKey(const std::string& sharedSecret,
                                          uint8_t magicValue);

std::string encryptServerUdpDatagram(const std::string& datagram,
                                     uint32_t baseKey, uint16_t randomKeyPart);
bool decryptServerUdpDatagram(std::string& datagram,
                              const std::string& encrypted, uint32_t baseKey);
std::string encryptPeerUdpDatagram(const std::string& datagram,
                                   const std::string& remoteUserHash,
                                   uint32_t localPublicIp,
                                   uint16_t randomKeyPart);
bool decryptPeerUdpDatagram(std::string& datagram, const std::string& encrypted,
                            const std::string& localUserHash,
                            uint32_t remotePublicIp);

} // namespace aria2::ed2k
