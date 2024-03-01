/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedToken.h"
#include "Core/Assert.h"

#include <rapidjson/prettywriter.h>

bool SerializeTo(const FPackedTokenEntry& Data, FByteBuffer& Buffer)
{
	const bool bValid = Buffer.size() == FPackedTokenEntry_SerializedSize;
	HLVM_ASSERT(bValid, TXT("Buffer size {} is not enough for serialized data size {}"), Buffer.size(), FPackedTokenEntry_SerializedSize);
	return std::memcpy(Buffer.data(), &Data, FPackedTokenEntry_SerializedSize) == Buffer.data();
}

bool SerializeFrom(FPackedTokenEntry& Data, const FConstByteBuffer& Buffer)
{
	const bool bValid = Buffer.size() == FPackedTokenEntry_SerializedSize;
	HLVM_ASSERT(bValid, TXT("Buffer size {} is not enough for serialized data size {}"), Buffer.size(), FPackedTokenEntry_SerializedSize);
	return std::memcpy(&Data, Buffer.data(), FPackedTokenEntry_SerializedSize) == &Data;
}

std::string ToJson(const FPackedTokenEntry& Entry)
{
	using namespace rapidjson;
	StringBuffer			   sb;
	PrettyWriter<StringBuffer> writer(sb);

	writer.StartObject();
	{
		writer.String("FPathHash");
		writer.Uint64(Entry.PathHash);

		writer.String("FPackedTokenEntryData");
		writer.StartObject();
		{
			writer.String("StartPos");
			writer.Uint64(Entry.Data.StartPos);
			writer.String("Size");
			writer.Uint64(Entry.Data.Size);
			writer.String("DecompressSize");
			writer.Uint64(Entry.Data.DecompressSize);
			writer.String("EncryptType");
			writer.String(magic_enum::enum_name(Entry.Data.EncryptType).data());
			writer.String("CompressType");
			writer.String(magic_enum::enum_name(Entry.Data.CompressType).data());
		}
		writer.EndObject();
	}
	writer.EndObject();

	return sb.GetString();
}

std::string ToJson(const FPackedTokenEntryWithPath& Data)
{
	using namespace rapidjson;
	StringBuffer			   sb;
	PrettyWriter<StringBuffer> writer(sb);

	writer.StartObject();
	{
		writer.String("FPath");
		writer.String(Data.Path.c_str());

		writer.String("FPackedTokenEntryData");
		writer.StartObject();
		{
			writer.String("FPathHash");
			writer.Uint64(Data.Entry.PathHash);

			writer.String("FPackedTokenEntryData");
			writer.StartObject();
			{
				writer.String("StartPos");
				writer.Uint64(Data.Entry.Data.StartPos);
				writer.String("Size");
				writer.Uint64(Data.Entry.Data.Size);
				writer.String("DecompressSize");
				writer.Uint64(Data.Entry.Data.DecompressSize);
				writer.String("EncryptType");
				writer.String(magic_enum::enum_name(Data.Entry.Data.EncryptType).data());
				writer.String("CompressType");
				writer.String(magic_enum::enum_name(Data.Entry.Data.CompressType).data());
			}
			writer.EndObject();
		}
		writer.EndObject();
	}
	writer.EndObject();

	return sb.GetString();
}
