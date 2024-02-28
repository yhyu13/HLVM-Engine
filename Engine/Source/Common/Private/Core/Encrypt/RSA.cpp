/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Encrypt/RSA.h"
#include "Utility/ScopedTimer.h"

DELCARE_LOG_CATEGORY(LogRSA)

std::vector<uint8_t> FRSA::EncryptPCKS8(const std::span<std::byte>& Buffer)
{
	HLVM_SCOPED_TIMER(FString::Format(TXT("PCKS8 Encrypt size {}"), Buffer.size()));

	std::vector<uint8_t> out = enc.encrypt(R_C(const uint8_t*, Buffer.data()), Buffer.size(), rng);

	HLVM_LOG(LogRSA, trace, TXT("PCKS8 Encrypt Buffer size {}, out size {}"), Buffer.size(), out.size());

	return out;
}

Botan::secure_vector<uint8_t> FRSA::DecryptPCKS8(const std::span<std::byte>& Buffer)
{
	HLVM_SCOPED_TIMER(FString::Format(TXT("PCKS8 Decrypt size {}"), Buffer.size()));

	Botan::secure_vector<uint8_t> out = dec.decrypt(R_C(const uint8_t*, Buffer.data()), Buffer.size());

	HLVM_LOG(LogRSA, trace, TXT("PCKS8 Decrypt Buffer size {}, out size {}"), Buffer.size(), out.size());

	return out;
}
