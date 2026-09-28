// SquachWatch on the FREE-WILi 2: no SquachMesh cipher, deliberately.
//
// SquachMesh sends encrypted messages between SquachWatches in Bluetooth
// adverts. The FW2's Bluetooth is the stock ESP32-C5 firmware's, which can
// neither send those adverts nor report them, so no message could travel
// either way. SquachWatch's real cipher (src/meshcrypto.cpp) is mbedtls,
// which isn't part of this build, and the simulator's stand-in
// (sim/meshcrypto_sim.cpp) says it must never be linked into firmware.
// So every operation here fails: the self-test reports FAIL, no key is
// ever loaded, and nothing is sealed or opened.
#include "meshcrypto.h"
#include <string.h>

namespace {
bool noDerive(const char*, size_t, const uint8_t*, size_t, uint32_t, uint8_t*) { return false; }
bool noSetKey(const uint8_t*) { return false; }
bool noSeal(const uint8_t*, const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, uint8_t*) { return false; }
bool noOpen(const uint8_t*, const uint8_t*, size_t, const uint8_t*, size_t, const uint8_t*, uint8_t*) { return false; }
const MeshMsg::Crypto NONE = { noDerive, noSetKey, noSeal, noOpen };
}  // namespace

const MeshMsg::Crypto& MeshCrypto::impl() { return NONE; }
bool MeshCrypto::selfTest() { return false; }
void MeshCrypto::sha256(const uint8_t*, size_t, uint8_t out[32]) { memset(out, 0, 32); }
bool MeshCrypto::dhKeypair(uint8_t priv[DH_LEN], uint8_t pub[DH_LEN]) {
    memset(priv, 0, DH_LEN);
    memset(pub, 0, DH_LEN);
    return false;
}
bool MeshCrypto::dhShared(const uint8_t*, const uint8_t*, uint8_t out[DH_LEN]) { memset(out, 0, DH_LEN); return false; }
void MeshCrypto::dhSessionKey(const uint8_t*, uint8_t key[MeshMsg::KEY_LEN]) { memset(key, 0, MeshMsg::KEY_LEN); }
uint16_t MeshCrypto::dhCode(const uint8_t*, const uint8_t*) { return 0; }
