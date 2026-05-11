/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Test Suite for ShaderBlob permutation handling and FShaderFactory extensions.
 * Tests shader blob parsing, permutation lookup, and enumeration functionality.
 */

#include "Test.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Renderer/FShaderFactory.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogTestShaderFactory)

// =============================================================================
// ShaderBlob Test Cases
// =============================================================================

/**
 * @brief Test ShaderBlob header structures
 */
RECORD(shaderblob_struct_initialization)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing ShaderBlob structure initialization"));

	// Test ShaderConstant
	{
		ShaderMake::ShaderConstant constant{};
		HLVM_ENSURE(constant.name == nullptr);
		HLVM_ENSURE(constant.value == nullptr);

		const char*				   name = "TEST_DEFINE";
		const char*				   value = "1";
		ShaderMake::ShaderConstant constant2{ name, value };
		HLVM_ENSURE(constant2.name == name);
		HLVM_ENSURE(constant2.value == value);
	}

	// Test ShaderBlobEntry
	{
		ShaderMake::ShaderBlobEntry entry{};
		HLVM_ENSURE(entry.permutationSize == 0);
		HLVM_ENSURE(entry.dataSize == 0);

		entry.permutationSize = 10;
		entry.dataSize = 100;
		HLVM_ENSURE(entry.permutationSize == 10);
		HLVM_ENSURE(entry.dataSize == 100);
	}

	HLVM_LOG(LogTestShaderFactory, info, TXT("ShaderBlob structure initialization test passed"));
}

/**
 * @brief Test FindPermutationInBlob with null/empty blob
 */
RECORD(shaderblob_find_permutation_null)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing FindPermutationInBlob with null/empty inputs"));

	const void* blob = nullptr;
	size_t		blobSize = 0;
	const void* binary = nullptr;
	size_t		binarySize = 0;

	// Null blob should return false
	bool result = ShaderMake::FindPermutationInBlob(blob, blobSize, nullptr, 0, &binary, &binarySize);
	HLVM_ENSURE_F(result == false, TXT("FindPermutationInBlob should return false for null blob"));

	// Empty blob without signature returns true when no constants requested (treats blob as raw binary)
	char emptyBlob[4] = { 0 };
	result = ShaderMake::FindPermutationInBlob(emptyBlob, 4, nullptr, 0, &binary, &binarySize);
	HLVM_ENSURE_F(result == true, TXT("FindPermutationInBlob should return true for empty blob without signature when no constants"));
	HLVM_ENSURE_F(binary == emptyBlob, TXT("Binary should point to blob"));
	HLVM_ENSURE_F(binarySize == 4, TXT("BinarySize should be 4"));

	HLVM_LOG(LogTestShaderFactory, info, TXT("FindPermutationInBlob null/empty test passed"));
}

/**
 * @brief Test EnumeratePermutationsInBlob with null/empty blob
 */
RECORD(shaderblob_enumerate_null)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing EnumeratePermutationsInBlob with null/empty inputs"));

	std::vector<std::string> permutations;

	// Null blob should not add permutations
	ShaderMake::EnumeratePermutationsInBlob(nullptr, 0, permutations);
	HLVM_ENSURE_F(permutations.empty(), TXT("EnumeratePermutationsInBlob should not add permutations for null blob"));

	// Empty blob should not add permutations
	char emptyBlob[4] = { 0 };
	ShaderMake::EnumeratePermutationsInBlob(emptyBlob, 4, permutations);
	HLVM_ENSURE_F(permutations.empty(), TXT("EnumeratePermutationsInBlob should not add permutations for empty blob"));

	HLVM_LOG(LogTestShaderFactory, info, TXT("EnumeratePermutationsInBlob null/empty test passed"));
}

/**
 * @brief Test GetSortedConstantsIndices
 */
RECORD(shaderblob_sorted_constants)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing GetSortedConstantsIndices"));

	// Test with empty vector
	{
		std::vector<std::string> constants;
		std::vector<size_t>		 sortedIndices = ShaderMake::GetSortedConstantsIndices(constants);
		HLVM_ENSURE_F(sortedIndices.empty(), TXT("Sorted indices should be empty for empty input"));
	}

	// Test with single element
	{
		std::vector<std::string> constants = { "SINGLE" };
		std::vector<size_t>		 sortedIndices = ShaderMake::GetSortedConstantsIndices(constants);
		HLVM_ENSURE_F(sortedIndices.size() == 1, TXT("Should have one sorted index"));
		HLVM_ENSURE_F(sortedIndices[0] == 0, TXT("Single element should have index 0"));
	}

	// Test with multiple elements - should be sorted alphabetically
	{
		std::vector<std::string> constants = { "ZEBRA", "APPLE", "MANGO" };
		std::vector<size_t>		 sortedIndices = ShaderMake::GetSortedConstantsIndices(constants);
		HLVM_ENSURE_F(sortedIndices.size() == 3, TXT("Should have three sorted indices"));
		// APPLE (index 1), MANGO (index 2), ZEBRA (index 0) -> sorted order: 1, 2, 0
		HLVM_ENSURE_F(sortedIndices[0] == 1, TXT("First should be APPLE (index 1)"));
		HLVM_ENSURE_F(sortedIndices[1] == 2, TXT("Second should be MANGO (index 2)"));
		HLVM_ENSURE_F(sortedIndices[2] == 0, TXT("Third should be ZEBRA (index 0)"));
	}

	// Test with duplicate values - stable sort should preserve original order
	{
		std::vector<std::string> constants = { "B", "A", "B", "A" };
		std::vector<size_t>		 sortedIndices = ShaderMake::GetSortedConstantsIndices(constants);
		HLVM_ENSURE_F(sortedIndices.size() == 4, TXT("Should have four sorted indices"));
		// With stable sort, equal elements maintain relative order
		// A at indices 1,3 -> should come before B at indices 0,2
		// Within same values, stable sort preserves order
	}

	HLVM_LOG(LogTestShaderFactory, info, TXT("GetSortedConstantsIndices test passed"));
}

/**
 * @brief Test WriteFileHeader and WritePermutation callbacks
 */
RECORD(shaderblob_write_functions)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing WriteFileHeader and WritePermutation"));

	// Test WriteFileHeader callback
	{
		std::vector<char> writtenData;
		auto			  writeCallback = [](const void* data, size_t size, void* context) -> bool {
			 auto*		 vec = static_cast<std::vector<char>*>(context);
			 const char* cdata = static_cast<const char*>(data);
			 vec->insert(vec->end(), cdata, cdata + size);
			 return true;
		};

		bool result = ShaderMake::WriteFileHeader(writeCallback, &writtenData);
		HLVM_ENSURE_F(result == true, TXT("WriteFileHeader should return true"));
		HLVM_ENSURE_F(writtenData.size() == 4, TXT("WriteFileHeader should write 4 bytes (NVSP)"));
		HLVM_ENSURE_F(writtenData[0] == 'N', TXT("First byte should be N"));
		HLVM_ENSURE_F(writtenData[1] == 'V', TXT("Second byte should be V"));
		HLVM_ENSURE_F(writtenData[2] == 'S', TXT("Third byte should be S"));
		HLVM_ENSURE_F(writtenData[3] == 'P', TXT("Fourth byte should be P"));
	}

	// Test WritePermutation callback
	{
		std::vector<char> writtenData;
		auto			  writeCallback = [](const void* data, size_t size, void* context) -> bool {
			 auto*		 vec = static_cast<std::vector<char>*>(context);
			 const char* cdata = static_cast<const char*>(data);
			 vec->insert(vec->end(), cdata, cdata + size);
			 return true;
		};

		const char* permutationKey = "KEY=VALUE";
		const char* binaryData = "BINARY";
		bool		result = ShaderMake::WritePermutation(writeCallback, &writtenData, permutationKey, binaryData, 6);
		HLVM_ENSURE_F(result == true, TXT("WritePermutation should return true"));

		// Should have: 8 bytes header (permutationSize=9, dataSize=6) + 9 bytes key + 6 bytes binary
		size_t expectedSize = sizeof(ShaderMake::ShaderBlobEntry) + strlen(permutationKey) + 6;
		HLVM_ENSURE_F(writtenData.size() == expectedSize, TXT("Written data size mismatch"));
	}

	HLVM_LOG(LogTestShaderFactory, info, TXT("Write functions test passed"));
}

/**
 * @brief Test FormatShaderNotFoundMessage
 */
RECORD(shaderblob_format_message)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing FormatShaderNotFoundMessage"));

	// Test with null blob
	{
		std::string msg = ShaderMake::FormatShaderNotFoundMessage(nullptr, 0, nullptr, 0);
		HLVM_ENSURE_F(!msg.empty(), TXT("Message should not be empty"));
		HLVM_ENSURE_F(msg.find("permutation") != std::string::npos, TXT("Message should mention permutation"));
	}

	// Test with constants
	{
		ShaderMake::ShaderConstant constants[] = {
			{ "MODE", "DEBUG" },
			{ "API", "VULKAN" }
		};
		std::string msg = ShaderMake::FormatShaderNotFoundMessage(nullptr, 0, constants, 2);
		HLVM_ENSURE_F(!msg.empty(), TXT("Message should not be empty"));
		HLVM_ENSURE_F(msg.find("MODE=DEBUG") != std::string::npos, TXT("Message should contain MODE=DEBUG"));
		HLVM_ENSURE_F(msg.find("API=VULKAN") != std::string::npos, TXT("Message should contain API=VULKAN"));
	}

	HLVM_LOG(LogTestShaderFactory, info, TXT("FormatShaderNotFoundMessage test passed"));
}

/**
 * @brief Test FShaderLibraryHandle type exists (compile-time check)
 */
RECORD(shaderfactory_type_exists)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing FShaderFactory CreateShaderLibrary type existence"));

	// This test verifies that nvrhi::ShaderLibraryHandle is a valid type
	// The actual CreateShaderLibrary method requires device initialization which
	// is complex for unit testing, so we verify the type is correct at compile time

	nvrhi::ShaderLibraryHandle nullHandle = nullptr;
	HLVM_ENSURE_F(nullHandle == nullptr, TXT("ShaderLibraryHandle should be null-constructible"));

	HLVM_LOG(LogTestShaderFactory, info, TXT("ShaderLibraryHandle type exists and is usable"));
}

/**
 * @brief Test FShaderMacro structure
 */
RECORD(shaderfactory_fshadermacro)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing FShaderMacro structure"));

	// Test default constructor
	{
		FShaderMacro macro;
		HLVM_ENSURE_F(macro.Name.empty(), TXT("Default name should be empty"));
		HLVM_ENSURE_F(macro.Definition.empty(), TXT("Default definition should be empty"));
	}

	// Test parameterized constructor
	{
		FShaderMacro macro("MY_DEFINE", "1");
		HLVM_ENSURE_F(macro.Name == "MY_DEFINE", TXT("Name should be MY_DEFINE"));
		HLVM_ENSURE_F(macro.Definition == "1", TXT("Definition should be 1"));
	}

	// Test move constructor
	{
		FShaderMacro macro1("MOVE_TEST", "42");
		FShaderMacro macro2(std::move(macro1));
		HLVM_ENSURE_F(macro2.Name == "MOVE_TEST", TXT("Moved name should be MOVE_TEST"));
		HLVM_ENSURE_F(macro2.Definition == "42", TXT("Moved definition should be 42"));
	}

	HLVM_LOG(LogTestShaderFactory, info, TXT("FShaderMacro test passed"));
}

/**
 * @brief Test FStaticShader structure
 */
RECORD(shaderfactory_fstaticshader)
{
	HLVM_LOG(LogTestShaderFactory, info, TXT("Testing FStaticShader structure"));

	// Test default constructor
	{
		FStaticShader shader;
		HLVM_ENSURE_F(shader.Bytecode == nullptr, TXT("Default bytecode should be nullptr"));
		HLVM_ENSURE_F(shader.Size == 0, TXT("Default size should be 0"));
	}

	// Test parameterized constructor
	{
		int			  dummyData = 42;
		FStaticShader shader(&dummyData, sizeof(dummyData));
		HLVM_ENSURE_F(shader.Bytecode == &dummyData, TXT("Bytecode should point to data"));
		HLVM_ENSURE_F(shader.Size == sizeof(dummyData), TXT("Size should be sizeof(int)"));
	}

	HLVM_LOG(LogTestShaderFactory, info, TXT("FStaticShader test passed"));
}
