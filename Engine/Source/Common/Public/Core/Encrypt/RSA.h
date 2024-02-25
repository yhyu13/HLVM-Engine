/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "EncryptDefinition.h"

#include <botan/auto_rng.h>
#include <botan/pk_keys.h>
#include <botan/pubkey.h>
#include <botan/pkcs8.h>

HLVM_INLINE_VAR const char* HLVM_PRIVATE_KEY_PKCS8 = "-----BEGIN PRIVATE KEY-----\n"
													 "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCcky+A7QIaNdnj\n"
													 "Vfz7p61+vdbgYegcqBFuxB9LhS9SIPiTMRd5QTbmW3noLoh7nf2zOorYpgGA9s4Q\n"
													 "oH1iRJt2hJFmwHjchzkWS+m9dpU573L4fXkFcEVKDX2Hcox53xMriLGwph/IIhTM\n"
													 "B0UOTD9a3soMP2dy6ca4SIzS2MQLKhEJ9W4J7DDb3X/BgwLyL853tIXTXMXEaK01\n"
													 "5rA/D0vOBOVYLuoyGvTejG+dJtjC645qyW2QAH8O9eeiHpnrUWlLEB0rbSuMtLLc\n"
													 "nqpNv3mMGJSXPVH+Z7bvp/DCtSHGFdvPqJHmHDowoELySCJWBo9rzjbMG7yWbZhu\n"
													 "hwJ1O5HVAgMBAAECggEALhc1uKIMKGMJzN4XJo1piRGhG1Q225it6QlanQSLtYV3\n"
													 "Jv0gh9UmsBDlhe+MxbbwY74r+TKE5s3AQyy7PB4uFWlY1AJ8kY5Mw799AwTRUMin\n"
													 "83YcFfnCq9XkyeZya727CummRPXiDSvbK2RIc7kGPV2v5IMVlMI0eTMCIV8l9pLp\n"
													 "cwdX6sCVsgowi/WikPpc4kqO1eN31h/yyPRgRiZfl8x5eNhBMXWA89rDeVudhOG1\n"
													 "hGuggsa4HRhL1BMhhZgiTCXGXg0yNuH4PdNsu/6bxi8HU7VuLuCJvXyMi3/F2E4O\n"
													 "LQYw8dJXAsKiFRDRZhMCNEX7VoZnRhxSHRzAkpLQ0QKBgQDUqzvEDx+xlCSMNV5V\n"
													 "JdQ7+MCHW9CPpHTxY6dieXCLKu8eujPySyKRb6Lu7rKLRpTUqM3C+BdYzEIO/3ch\n"
													 "SDp6TU1S/YDX6Ns1YVoECUeHW16JYsgFSCIIaPVKI/sDtLnxKO0L++1ES/Q/EzI5\n"
													 "/sOmbhMhE6RRKbTCyeU7aZCcmwKBgQC8ehp+euzDZhXOJrvC45gOvsLvHwjRaa6B\n"
													 "sBNWjK8vEMxnYDn/yctK1vNiXyqXQkfU5+tb2Gl0xFDlDQnOMNdz2CB+cB5S18Il\n"
													 "o3QRcRBJLSIclBi0bJ2ShtOjmAjsA2aNdPCF3I03v+thWPg0JwHU4jvA5MOrlyMz\n"
													 "rhhVaZKaTwKBgQC+/FP8+3QD/r1fqOHhZ8qUXQ1RwfOnvBJFYbBFcDfQ0yrRFnlm\n"
													 "I3GU2IOjPXvcNfOck0fNywz0cuL0IxnyKrJReTBy0jQuMfECnD3BmC/DcBuTF8H3\n"
													 "dFMT6GY4Qd2/80J1P8K523G1vtxG9M9LY+6CfSHyt3f+Z4zszXFujJwQ9wKBgGjh\n"
													 "dZ7OXrQ5OYcXLMK9jZ8e7jDMT0bhDTejuI8gCFgje+tGs9+v9k12Iceq93Nmcbx7\n"
													 "NhBM9BoDKJTdVYiEy0/ug954G5ez8pipRWxzQ0HFOMc4birwihrApkLR1p0nI0ky\n"
													 "Oqny2i6cwKnSLYQv9Kf3IJMteekhWHhot6fH8MmnAoGBALj74j42t5724DORgBUS\n"
													 "oqRIYup7MBxjM9XQrGghmJHV1L0dSSbT/D1zxQn5Epfy7Ua16+SdBcCMLDAAyRSK\n"
													 "bYdnR7mpTKNOezSFqeJ1ZluwR1zyBXyrFdS0N1kosKA3VobVXy0SDOXATk2x3Hub\n"
													 "fAHoeqUUsEqkp4lJIXvMrw0/\n"
													 "-----END PRIVATE KEY-----";

HLVM_INLINE_VAR Botan::AutoSeeded_RNG rng;
// load keypair
HLVM_INLINE_VAR Botan::secure_vector<uint8_t> in{ HLVM_PRIVATE_KEY_PKCS8, HLVM_PRIVATE_KEY_PKCS8 + std::strlen(HLVM_PRIVATE_KEY_PKCS8) };
// encrypt with pk
HLVM_INLINE_VAR auto kp = Botan::PKCS8::load_key(in);
HLVM_INLINE_VAR Botan::PK_Encryptor_EME enc(*kp, rng, "OAEP(SHA-256)");
// decrypt with pk
HLVM_INLINE_VAR Botan::PK_Decryptor_EME dec(*kp, rng, "OAEP(SHA-256)");

class FRSA
{
public:
	HLVM_INLINE_FUNC HLVM_STATIC_FUNC auto HPCKS8_Encrypt(const std::span<std::byte>& Buffer)
	{
		std::vector<uint8_t> out = enc.encrypt(R_C(const uint8_t*, Buffer.data()), Buffer.size(), rng);
		return out;
	}

	HLVM_INLINE_FUNC HLVM_STATIC_FUNC auto PCKS8_Decrypt(const std::span<std::byte>& Buffer)
	{
		Botan::secure_vector<uint8_t> out = dec.decrypt(R_C(const uint8_t*, Buffer.data()), Buffer.size());
		return out;
	}
};
