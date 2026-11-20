// Copyright Do it Asphalt. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Byte writer and reader of the lot save format. Every value is little-endian, written byte by byte, so a save is
 * the same on every platform. Doubles are their 8 IEEE bytes, bools one byte that must be 0 or 1.
 */
namespace LotSaveIO
{
	/** Appends plain values to a byte array. */
	class FWriter
	{
	public:
		explicit FWriter(TArray<uint8>& InOut)
			: Out(InOut)
		{
		}

		void WriteU8(uint8 Value)
		{
			Out.Add(Value);
		}

		void WriteU32(uint32 Value)
		{
			for (int32 Shift = 0; Shift < 32; Shift += 8)
			{
				Out.Add(static_cast<uint8>(Value >> Shift));
			}
		}

		void WriteI32(int32 Value)
		{
			WriteU32(static_cast<uint32>(Value));
		}

		void WriteDouble(double Value)
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			for (int32 Shift = 0; Shift < 64; Shift += 8)
			{
				Out.Add(static_cast<uint8>(Bits >> Shift));
			}
		}

		void WriteBool(bool bValue)
		{
			WriteU8(bValue ? 1 : 0);
		}

		void WriteVector(const FVector2D& V)
		{
			WriteDouble(V.X);
			WriteDouble(V.Y);
		}

	private:
		TArray<uint8>& Out;
	};

	/** Reads plain values from a byte view; every read reports whether enough bytes were left. */
	class FReader
	{
	public:
		explicit FReader(TConstArrayView<uint8> InData)
			: Data(InData)
		{
		}

		bool ReadU8(uint8& Value)
		{
			if (Remaining() < 1)
			{
				return false;
			}
			Value = Data[Position++];
			return true;
		}

		bool ReadU32(uint32& Value)
		{
			if (Remaining() < 4)
			{
				return false;
			}
			Value = 0;
			for (int32 Shift = 0; Shift < 32; Shift += 8)
			{
				Value |= static_cast<uint32>(Data[Position++]) << Shift;
			}
			return true;
		}

		bool ReadI32(int32& Value)
		{
			uint32 Raw = 0;
			if (!ReadU32(Raw))
			{
				return false;
			}
			Value = static_cast<int32>(Raw);
			return true;
		}

		/** Any double, including NaN and infinity; the caller validates the range. */
		bool ReadDouble(double& Value)
		{
			if (Remaining() < 8)
			{
				return false;
			}
			uint64 Bits = 0;
			for (int32 Shift = 0; Shift < 64; Shift += 8)
			{
				Bits |= static_cast<uint64>(Data[Position++]) << Shift;
			}
			FMemory::Memcpy(&Value, &Bits, sizeof(Value));
			return true;
		}

		/** A double that must be finite. */
		bool ReadFinite(double& Value)
		{
			return ReadDouble(Value) && FMath::IsFinite(Value);
		}

		/** A bool is stored as exactly 0 or 1; any other byte is corrupt. */
		bool ReadBool(bool& bValue)
		{
			uint8 Byte = 0;
			if (!ReadU8(Byte) || Byte > 1)
			{
				return false;
			}
			bValue = Byte == 1;
			return true;
		}

		bool ReadVector(FVector2D& V)
		{
			return ReadFinite(V.X) && ReadFinite(V.Y);
		}

		/** Takes the next Count bytes as a view; false if fewer are left. */
		bool ReadBlock(int32 Count, TConstArrayView<uint8>& Block)
		{
			if (Count < 0 || Remaining() < Count)
			{
				return false;
			}
			Block = TConstArrayView<uint8>(Data.GetData() + Position, Count);
			Position += Count;
			return true;
		}

		int32 Remaining() const
		{
			return Data.Num() - Position;
		}

		int32 GetPosition() const
		{
			return Position;
		}

		bool AtEnd() const
		{
			return Position == Data.Num();
		}

	private:
		TConstArrayView<uint8> Data;
		int32 Position = 0;
	};
}
