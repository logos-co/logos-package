#include <gtest/gtest.h>
#include "crypto/signing.h"
#include "crypto/manifest_sig.h"
#include "crypto/keyring.h"

#include <filesystem>
#include <fstream>

using namespace lgx;
using namespace lgx::crypto;
namespace fs = std::filesystem;

// =============================================================================
// Base64url Encode/Decode Tests
// =============================================================================

TEST(CryptoTest, Base64UrlEncode_RoundTrip) {
    ASSERT_TRUE(init());

    // Test with various byte patterns
    std::vector<uint8_t> data = {0, 1, 2, 3, 255, 254, 128, 64};
    std::string encoded = base64UrlEncode(data.data(), data.size());
    EXPECT_FALSE(encoded.empty());

    // Should not contain standard base64 characters that differ in URL variant
    EXPECT_EQ(encoded.find('+'), std::string::npos);
    EXPECT_EQ(encoded.find('/'), std::string::npos);
    EXPECT_EQ(encoded.find('='), std::string::npos);

    auto decoded = base64UrlDecode(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

TEST(CryptoTest, Base64UrlEncode_EmptyInput) {
    ASSERT_TRUE(init());

    std::string encoded = base64UrlEncode(nullptr, 0);
    EXPECT_TRUE(encoded.empty());
}

TEST(CryptoTest, Base64UrlEncode_StringOverload) {
    ASSERT_TRUE(init());

    std::string input = "Hello, World!";
    std::string encoded = base64UrlEncode(input);
    EXPECT_FALSE(encoded.empty());

    auto decoded = base64UrlDecode(encoded);
    ASSERT_TRUE(decoded.has_value());
    std::string result(decoded->begin(), decoded->end());
    EXPECT_EQ(result, input);
}

TEST(CryptoTest, Base64UrlDecode_InvalidInput) {
    ASSERT_TRUE(init());

    // Invalid base64url strings
    auto result = base64UrlDecode("!!!invalid!!!");
    EXPECT_FALSE(result.has_value());
}

TEST(CryptoTest, Base64UrlEncode_32ByteKey) {
    ASSERT_TRUE(init());

    // 32 bytes like an Ed25519 public key
    std::vector<uint8_t> key(32);
    for (size_t i = 0; i < 32; i++) key[i] = static_cast<uint8_t>(i);

    std::string encoded = base64UrlEncode(key.data(), key.size());
    auto decoded = base64UrlDecode(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->size(), 32u);
    EXPECT_EQ(*decoded, key);
}

// =============================================================================
// DID <-> PublicKey Conversion Tests
// =============================================================================

TEST(CryptoTest, PublicKeyToDid_Format) {
    ASSERT_TRUE(init());

    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    // Must start with did:jwk:
    EXPECT_EQ(did.substr(0, 8), "did:jwk:");

    // The rest should be base64url-encoded JWK
    std::string jwkEncoded = did.substr(8);
    EXPECT_FALSE(jwkEncoded.empty());

    // Decode and verify JWK structure
    auto jwkBytes = base64UrlDecode(jwkEncoded);
    ASSERT_TRUE(jwkBytes.has_value());
    std::string jwkStr(jwkBytes->begin(), jwkBytes->end());

    // Should contain the required JWK fields
    EXPECT_NE(jwkStr.find("\"crv\":\"Ed25519\""), std::string::npos);
    EXPECT_NE(jwkStr.find("\"kty\":\"OKP\""), std::string::npos);
    EXPECT_NE(jwkStr.find("\"x\":\""), std::string::npos);
}

TEST(CryptoTest, PublicKeyToDid_Deterministic) {
    ASSERT_TRUE(init());

    auto kp = generateKeypair();
    std::string did1 = publicKeyToDid(kp.publicKey);
    std::string did2 = publicKeyToDid(kp.publicKey);

    EXPECT_EQ(did1, did2);
}

TEST(CryptoTest, PublicKeyToDid_DifferentKeysProduceDifferentDids) {
    ASSERT_TRUE(init());

    auto kp1 = generateKeypair();
    auto kp2 = generateKeypair();

    std::string did1 = publicKeyToDid(kp1.publicKey);
    std::string did2 = publicKeyToDid(kp2.publicKey);

    EXPECT_NE(did1, did2);
}

TEST(CryptoTest, DidToPublicKey_RoundTrip) {
    ASSERT_TRUE(init());

    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    auto extracted = didToPublicKey(did);
    ASSERT_TRUE(extracted.has_value());
    EXPECT_EQ(*extracted, kp.publicKey);
}

TEST(CryptoTest, DidToPublicKey_InvalidPrefix) {
    auto result = didToPublicKey("did:key:z1234");
    EXPECT_FALSE(result.has_value());
}

TEST(CryptoTest, DidToPublicKey_EmptyString) {
    auto result = didToPublicKey("");
    EXPECT_FALSE(result.has_value());
}

TEST(CryptoTest, DidToPublicKey_InvalidBase64) {
    auto result = didToPublicKey("did:jwk:!!!invalid!!!");
    EXPECT_FALSE(result.has_value());
}

TEST(CryptoTest, DidToPublicKey_InvalidJwk) {
    ASSERT_TRUE(init());

    // Valid base64url but not a valid JWK (missing x field)
    std::string fakeJwk = "{\"kty\":\"OKP\",\"crv\":\"Ed25519\"}";
    std::string encoded = base64UrlEncode(fakeJwk);
    auto result = didToPublicKey("did:jwk:" + encoded);
    EXPECT_FALSE(result.has_value());
}

TEST(CryptoTest, DidToPublicKey_WrongKeySize) {
    ASSERT_TRUE(init());

    // JWK with wrong-sized x value (16 bytes instead of 32)
    std::vector<uint8_t> shortKey(16, 0x42);
    std::string xValue = base64UrlEncode(shortKey.data(), shortKey.size());
    std::string fakeJwk = "{\"crv\":\"Ed25519\",\"kty\":\"OKP\",\"x\":\"" + xValue + "\"}";
    std::string encoded = base64UrlEncode(fakeJwk);
    auto result = didToPublicKey("did:jwk:" + encoded);
    EXPECT_FALSE(result.has_value());
}

TEST(CryptoTest, PublicKeyToDid_JwkKeysAlphabeticallySorted) {
    ASSERT_TRUE(init());

    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    // Decode the JWK
    auto jwkBytes = base64UrlDecode(did.substr(8));
    ASSERT_TRUE(jwkBytes.has_value());
    std::string jwk(jwkBytes->begin(), jwkBytes->end());

    // crv should come before kty, kty before x
    auto crvPos = jwk.find("\"crv\"");
    auto ktyPos = jwk.find("\"kty\"");
    auto xPos = jwk.find("\"x\"");

    EXPECT_LT(crvPos, ktyPos);
    EXPECT_LT(ktyPos, xPos);
}

// =============================================================================
// ManifestSig Serialization Tests
// =============================================================================

TEST(ManifestSigTest, ToJson_Roundtrip) {
    ManifestSig sig;
    sig.version = 1;
    sig.algorithm = "ed25519";
    sig.did = "did:jwk:eyJjcnYiOiJFZDI1NTE5Iiwia3R5IjoiT0tQIiwieCI6InRlc3QifQ";
    sig.signature = "dGVzdHNpZw==";
    sig.signerName = "Test Publisher";
    sig.signerUrl = "https://example.com";

    std::string json = sig.toJson();
    auto parsed = ManifestSig::fromJson(json);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->version, 1);
    EXPECT_EQ(parsed->algorithm, "ed25519");
    EXPECT_EQ(parsed->did, sig.did);
    EXPECT_EQ(parsed->signature, sig.signature);
    EXPECT_EQ(parsed->signerName, "Test Publisher");
    EXPECT_EQ(parsed->signerUrl, "https://example.com");
    EXPECT_TRUE(parsed->linkedDids.empty());
}

TEST(ManifestSigTest, ToJson_WithLinkedDids) {
    ManifestSig sig;
    sig.version = 1;
    sig.algorithm = "ed25519";
    sig.did = "did:jwk:test";
    sig.signature = "dGVzdA==";
    sig.linkedDids = {"did:pkh:eip155:1:0xabc", "did:pkh:eip155:1:0xdef"};

    std::string json = sig.toJson();
    auto parsed = ManifestSig::fromJson(json);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->linkedDids.size(), 2u);
    EXPECT_EQ(parsed->linkedDids[0], "did:pkh:eip155:1:0xabc");
    EXPECT_EQ(parsed->linkedDids[1], "did:pkh:eip155:1:0xdef");
}

TEST(ManifestSigTest, ToJson_EmptySignerMetadata) {
    ManifestSig sig;
    sig.version = 1;
    sig.algorithm = "ed25519";
    sig.did = "did:jwk:test";
    sig.signature = "dGVzdA==";
    // signerName and signerUrl left empty

    std::string json = sig.toJson();
    auto parsed = ManifestSig::fromJson(json);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->signerName.empty());
    EXPECT_TRUE(parsed->signerUrl.empty());
}

TEST(ManifestSigTest, FromJson_MissingDid) {
    const char* json = R"({
        "version": 1,
        "algorithm": "ed25519",
        "signature": "dGVzdA==",
        "signer": {},
        "linkedDids": []
    })";

    auto parsed = ManifestSig::fromJson(json);
    EXPECT_FALSE(parsed.has_value());
}

TEST(ManifestSigTest, FromJson_MissingSignature) {
    const char* json = R"({
        "version": 1,
        "algorithm": "ed25519",
        "did": "did:jwk:test",
        "signer": {},
        "linkedDids": []
    })";

    auto parsed = ManifestSig::fromJson(json);
    EXPECT_FALSE(parsed.has_value());
}

TEST(ManifestSigTest, FromJson_UnsupportedVersion) {
    const char* json = R"({
        "version": 2,
        "algorithm": "ed25519",
        "did": "did:jwk:test",
        "signature": "dGVzdA==",
        "signer": {},
        "linkedDids": []
    })";

    auto parsed = ManifestSig::fromJson(json);
    EXPECT_FALSE(parsed.has_value());
}

TEST(ManifestSigTest, FromJson_UnsupportedAlgorithm) {
    const char* json = R"({
        "version": 1,
        "algorithm": "rsa",
        "did": "did:jwk:test",
        "signature": "dGVzdA==",
        "signer": {},
        "linkedDids": []
    })";

    auto parsed = ManifestSig::fromJson(json);
    EXPECT_FALSE(parsed.has_value());
}

TEST(ManifestSigTest, FromJson_InvalidJson) {
    auto parsed = ManifestSig::fromJson("{ not valid }");
    EXPECT_FALSE(parsed.has_value());
    EXPECT_FALSE(ManifestSig::getLastError().empty());
}

TEST(ManifestSigTest, ToJson_Deterministic) {
    ManifestSig sig;
    sig.version = 1;
    sig.algorithm = "ed25519";
    sig.did = "did:jwk:test";
    sig.signature = "dGVzdA==";
    sig.signerName = "Publisher";
    sig.signerUrl = "https://example.com";

    EXPECT_EQ(sig.toJson(), sig.toJson());
}

TEST(ManifestSigTest, ToJson_ContainsSignerObject) {
    ManifestSig sig;
    sig.version = 1;
    sig.algorithm = "ed25519";
    sig.did = "did:jwk:test";
    sig.signature = "dGVzdA==";
    sig.signerName = "Publisher";
    sig.signerUrl = "https://example.com";

    std::string json = sig.toJson();
    // Should have a signer object with name and url
    EXPECT_NE(json.find("\"signer\""), std::string::npos);
    EXPECT_NE(json.find("\"name\""), std::string::npos);
    EXPECT_NE(json.find("\"url\""), std::string::npos);
}

// =============================================================================
// Keyring Tests
// =============================================================================

class KeyringTest : public ::testing::Test {
protected:
    fs::path tempDir;
    fs::path trustedKeysDir;
    fs::path keysDir;

    void SetUp() override {
        ASSERT_TRUE(init());
        tempDir = fs::temp_directory_path() / ("lgx_keyring_test_" + std::to_string(rand()));
        trustedKeysDir = tempDir / "trusted-keys";
        keysDir = tempDir / "keys";
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }
};

TEST_F(KeyringTest, AddKey_ValidDid) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    bool result = keyring.addKey("test-publisher", did, "Test Publisher", "https://example.com");
    EXPECT_TRUE(result);

    // File should exist
    EXPECT_TRUE(fs::exists(trustedKeysDir / "test-publisher.json"));
}

TEST_F(KeyringTest, AddKey_InvalidDid) {
    Keyring keyring(trustedKeysDir);
    bool result = keyring.addKey("test", "not-a-valid-did");
    EXPECT_FALSE(result);
}

TEST_F(KeyringTest, AddKey_EmptyName) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    bool result = keyring.addKey("", did);
    EXPECT_FALSE(result);
}

TEST_F(KeyringTest, FindByDid) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    keyring.addKey("test-pub", did, "Test Publisher", "https://example.com");

    auto found = keyring.findByDid(did);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "test-pub");
    EXPECT_EQ(found->did, did);
    EXPECT_EQ(found->displayName, "Test Publisher");
    EXPECT_EQ(found->url, "https://example.com");
    EXPECT_EQ(found->publicKey, kp.publicKey);
    EXPECT_FALSE(found->addedAt.empty());
}

TEST_F(KeyringTest, FindByDid_NotFound) {
    Keyring keyring(trustedKeysDir);
    auto found = keyring.findByDid("did:jwk:nonexistent");
    EXPECT_FALSE(found.has_value());
}

TEST_F(KeyringTest, FindByPublicKey) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    keyring.addKey("test-pub", did);

    auto found = keyring.findByPublicKey(kp.publicKey);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "test-pub");
    EXPECT_EQ(found->did, did);
}

TEST_F(KeyringTest, FindByName) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    keyring.addKey("my-publisher", did, "My Publisher");

    auto found = keyring.findByName("my-publisher");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->did, did);
    EXPECT_EQ(found->displayName, "My Publisher");
}

TEST_F(KeyringTest, FindByName_NotFound) {
    Keyring keyring(trustedKeysDir);
    auto found = keyring.findByName("nonexistent");
    EXPECT_FALSE(found.has_value());
}

TEST_F(KeyringTest, RemoveKey) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    keyring.addKey("to-remove", did);

    EXPECT_TRUE(keyring.findByDid(did).has_value());

    bool removed = keyring.removeKey("to-remove");
    EXPECT_TRUE(removed);

    EXPECT_FALSE(keyring.findByDid(did).has_value());
    EXPECT_FALSE(fs::exists(trustedKeysDir / "to-remove.json"));
}

TEST_F(KeyringTest, RemoveKey_NonExistent) {
    Keyring keyring(trustedKeysDir);
    bool removed = keyring.removeKey("nonexistent");
    EXPECT_FALSE(removed);
}

TEST_F(KeyringTest, ListKeys) {
    auto kp1 = generateKeypair();
    auto kp2 = generateKeypair();

    Keyring keyring(trustedKeysDir);
    keyring.addKey("alpha", publicKeyToDid(kp1.publicKey), "Alpha Publisher");
    keyring.addKey("beta", publicKeyToDid(kp2.publicKey), "Beta Publisher");

    auto keys = keyring.listKeys();
    ASSERT_EQ(keys.size(), 2u);

    // Sorted by name
    EXPECT_EQ(keys[0].name, "alpha");
    EXPECT_EQ(keys[1].name, "beta");
    EXPECT_EQ(keys[0].displayName, "Alpha Publisher");
    EXPECT_EQ(keys[1].displayName, "Beta Publisher");
}

TEST_F(KeyringTest, ListKeys_EmptyKeyring) {
    Keyring keyring(trustedKeysDir);
    auto keys = keyring.listKeys();
    EXPECT_TRUE(keys.empty());
}

TEST_F(KeyringTest, AddKey_ReplaceExisting) {
    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    Keyring keyring(trustedKeysDir);
    keyring.addKey("test", did, "Original Name");

    // Replace with updated metadata
    keyring.addKey("test", did, "Updated Name", "https://new-url.com");

    auto found = keyring.findByName("test");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->displayName, "Updated Name");
    EXPECT_EQ(found->url, "https://new-url.com");
}

// =============================================================================
// Keypair Save/Load Tests
// =============================================================================

TEST_F(KeyringTest, SaveKeypair_CreatesFiles) {
    auto kp = generateKeypair();

    bool saved = Keyring::saveKeypair(keysDir, "test-key", kp);
    EXPECT_TRUE(saved);

    EXPECT_TRUE(fs::exists(keysDir / "test-key.jwk"));
    EXPECT_TRUE(fs::exists(keysDir / "test-key.pub"));
    EXPECT_TRUE(fs::exists(keysDir / "test-key.did"));
}

TEST_F(KeyringTest, SaveKeypair_DidFileContainsCorrectDid) {
    auto kp = generateKeypair();
    std::string expectedDid = publicKeyToDid(kp.publicKey);

    Keyring::saveKeypair(keysDir, "test-key", kp);

    std::ifstream didFile(keysDir / "test-key.did");
    std::string didContent;
    std::getline(didFile, didContent);

    EXPECT_EQ(didContent, expectedDid);
}

TEST_F(KeyringTest, SaveLoadSecretKey_RoundTrip) {
    auto kp = generateKeypair();

    bool saved = Keyring::saveKeypair(keysDir, "test-key", kp);
    ASSERT_TRUE(saved);

    auto loaded = Keyring::loadSecretKey(keysDir, "test-key");
    ASSERT_TRUE(loaded.has_value());

    // The loaded secret key should produce the same public key
    PublicKey originalPk = extractPublicKey(kp.secretKey);
    PublicKey loadedPk = extractPublicKey(*loaded);
    EXPECT_EQ(originalPk, loadedPk);
}

TEST_F(KeyringTest, SaveLoadSecretKey_SignVerifyRoundTrip) {
    auto kp = generateKeypair();
    Keyring::saveKeypair(keysDir, "test-key", kp);

    auto loadedSk = Keyring::loadSecretKey(keysDir, "test-key");
    ASSERT_TRUE(loadedSk.has_value());

    // Sign with loaded key and verify with original public key
    std::vector<uint8_t> message = {'h', 'e', 'l', 'l', 'o'};
    auto sig = sign(message, *loadedSk);

    EXPECT_TRUE(verify(message, kp.publicKey, sig));
}

TEST_F(KeyringTest, LoadSecretKey_NonExistent) {
    auto loaded = Keyring::loadSecretKey(keysDir, "nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(KeyringTest, SaveKeypair_JwkFileFormat) {
    auto kp = generateKeypair();
    Keyring::saveKeypair(keysDir, "test-key", kp);

    // Read and parse the JWK file
    std::ifstream file(keysDir / "test-key.jwk");
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Should be valid JSON containing the right fields
    EXPECT_NE(content.find("\"crv\""), std::string::npos);
    EXPECT_NE(content.find("\"Ed25519\""), std::string::npos);
    EXPECT_NE(content.find("\"kty\""), std::string::npos);
    EXPECT_NE(content.find("\"OKP\""), std::string::npos);
    EXPECT_NE(content.find("\"x\""), std::string::npos);
    EXPECT_NE(content.find("\"d\""), std::string::npos);
}

// =============================================================================
// Ed25519 Sign/Verify with DID Round-trip
// =============================================================================

TEST(CryptoTest, SignVerify_WithDid) {
    ASSERT_TRUE(init());

    auto kp = generateKeypair();
    std::string did = publicKeyToDid(kp.publicKey);

    // Sign a message
    std::vector<uint8_t> message = {'t', 'e', 's', 't', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e'};
    auto sig = sign(message, kp.secretKey);

    // Extract public key from DID and verify
    auto extractedPk = didToPublicKey(did);
    ASSERT_TRUE(extractedPk.has_value());

    EXPECT_TRUE(verify(message, *extractedPk, sig));
}

TEST(CryptoTest, SignVerify_WrongDid_Fails) {
    ASSERT_TRUE(init());

    auto kp1 = generateKeypair();
    auto kp2 = generateKeypair();

    std::vector<uint8_t> message = {'t', 'e', 's', 't'};
    auto sig = sign(message, kp1.secretKey);

    // Verify with wrong key's DID should fail
    auto wrongPk = didToPublicKey(publicKeyToDid(kp2.publicKey));
    ASSERT_TRUE(wrongPk.has_value());
    EXPECT_FALSE(verify(message, *wrongPk, sig));
}

// =============================================================================
// ExtractPublicKey Tests
// =============================================================================

TEST(CryptoTest, ExtractPublicKey_MatchesKeypair) {
    ASSERT_TRUE(init());

    auto kp = generateKeypair();
    PublicKey extracted = extractPublicKey(kp.secretKey);
    EXPECT_EQ(extracted, kp.publicKey);
}
