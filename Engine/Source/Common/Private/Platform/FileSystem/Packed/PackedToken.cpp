/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedToken.h"
#include "Core/Assert.h"

#include <rapidjson/prettywriter.h>

bool FPackedTokenEntry::Serialize(FByteBuffer& Buffer) const
{
	const bool bValid = Buffer.size() == FPackedTokenEntry_SerializedSize;
	HLVM_ASSERT_F(bValid, TXT("Buffer size {} is not enough for serialized data size {}"), Buffer.size(), FPackedTokenEntry_SerializedSize);
	return std::memcpy(Buffer.data(), this, FPackedTokenEntry_SerializedSize) == Buffer.data();
}

bool FPackedTokenEntry::Deserialize(const FConstByteBuffer& Buffer)
{
	const bool bValid = Buffer.size() == FPackedTokenEntry_SerializedSize;
	HLVM_ASSERT_F(bValid, TXT("Buffer size {} is not enough for serialized data size {}"), Buffer.size(), FPackedTokenEntry_SerializedSize);
	return std::memcpy(this, Buffer.data(), FPackedTokenEntry_SerializedSize) == this;
}

std::string FPackedTokenEntry::ToJsonString() const
{
	using namespace rapidjson;
	StringBuffer			   sb;
	PrettyWriter<StringBuffer> writer(sb);
	const FPackedTokenEntry&   Entry = *this;

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

std::string FPackedTokenEntryWithPath::ToJsonString() const
{
	using namespace rapidjson;
	StringBuffer					 sb;
	PrettyWriter<StringBuffer>		 writer(sb);
	const FPackedTokenEntryWithPath& Data = *this;

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
