#include "../include/bytecode.h"
#include "../include/bytes.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <utility>

ByteCode::ByteCode(const vByte& block) :
	block(block) {}

ByteCode::ByteCode(const vByte& block, const vObj& pool) :
	block(block), pool(pool) {}

void ByteCode::addOp(Opcode op)
{
	addByte(static_cast<u8>(op));
}

void ByteCode::addByte(u8 byte)
{
	block.push_back(byte);
}

void ByteCode::addShort(u16 bytes)
{
	addByte((bytes >> 8) & 0xff);
	addByte(bytes & 0xff);
}

void ByteCode::addLong(u32 bytes)
{
	addByte((bytes >> 24) & 0xff);
	addByte((bytes >> 16) & 0xff);
	addByte((bytes >> 8) & 0xff);
	addByte(bytes & 0xff);
}

const DebugRange& ByteCode::getErrorRange(const u8* ip) const
{
	CH_ASSERT(ip >= block.data(), "Wrong IP passed for error reporting.");

    u64 offset{static_cast<u64>(ip - block.data())};
    for (const auto& range : metadata)
    {
        if ((offset >= range.byteStart) && (offset <= range.byteEnd))
            return range;
    }

    CH_UNREACHABLE();
}

void ByteCode::setDebugData(FileID id, const DebugMetadata& metadata)
{
	this->id = id;
	this->metadata = metadata;
	sortMetadata();
}

u64 ByteCode::addJump(Opcode op, i16 reg)
{
	if (reg == -1)
		addOp(op);
	else
		addOp(op, static_cast<u8>(reg));
	u64 offset{block.size()};
	block.emplace_back();
	block.emplace_back();
	return offset;
}

void ByteCode::patchJump(u64 offset)
{
	// We've skipped the 2 jump bytes.
	u64 diff{block.size() - offset - 2};
	if (diff <= BYTE_JUMP_MAX)
	{
		block[offset] = static_cast<u8>((diff >> 8) & 0xff);
		block[offset + 1] = static_cast<u8>(diff & 0xff);
	}
	else
	{
		// Jump is too big.
		// TODO: Report error (somehow).
	}
}

void ByteCode::addLoop(u64 start)
{
	// We jump back the difference from our opcode to the
	// start, plus 2 more for the decoded offset bytes,
	// plus another 1 since we are on the instruction
	// *after* the two bytes by the time we've decoded the
	// offset.

	// TODO: add error-handling if jump is too large.

	u16 diff{static_cast<u16>(block.size() - start + 3)};
	addByte(OP_LOOP);
	addByte(static_cast<u8>((diff >> 8) & 0xff));
	addByte(static_cast<u8>(diff & 0xff));
}

void ByteCode::loadReg(u8 reg, u8 op)
{
	addOp(OP_LOAD_R, reg, op);
}

#define IS_SMALL(val) ((-3 < (val)) && ((val) < 3))

void ByteCode::loadRegConst(Object& constant, u8 reg)
{
	addOp(OP_LOAD_R, reg); // Destination first.

	if (IS_INT(constant))
	{
		if (IS_SMALL(constant.as.intVal))
		{
			addByte(static_cast<u8>(constant.as.intVal + 2));
			return;
		}
	}
	else if (IS_DEC(constant))
	{
		if (IS_SMALL(constant.as.decVal)
			&& (fmod(constant.as.decVal, 1.0) == 0.0))
		{
			addByte(static_cast<u8>(constant.as.decVal + 2));
			return;
		}
	}

	pool.push_back(std::move(constant));

	size_t size{pool.size()};
	if (size - 1 < (1 << 8))
	{
		addByte(OP_BYTE_OPER);
		addByte(static_cast<u8>(size - 1));
	}
	else if (size - 1 < (1 << 16))
	{
		addByte(OP_SHORT_OPER);
		addShort(static_cast<u16>(size - 1));
	}
	else
	{
		addByte(OP_LONG_OPER);
		addLong(static_cast<u32>(size - 1));
	}
}

#undef IS_SMALL

u64 ByteCode::countPool() const
{
	u64 count{0};

	for (const Object& obj : pool)
	{
		switch (obj.type())
		{
			case OBJ_INT:	case OBJ_DEC:
			{
				count += 9;
				break;
			}
			case OBJ_FUNC:	case OBJ_LAMBDA:
			{
				count += AS_FUNC(obj)->byteSize();
				break;
			}
			case OBJ_STRING:
			{
				count += AS_STRING(obj)->byteSize();
				break;
			}
			default:
				CH_UNREACHABLE();
		}
	}

	return count;
}

void ByteCode::clearCode()
{
	block.clear();
}

void ByteCode::clearPool()
{
	pool.clear();
}

void ByteCode::clear()
{
	clearCode();
	clearPool();
}

void ByteCode::sortMetadata()
{
	std::sort(metadata.begin(), metadata.end(),
		[](const DebugRange& a, const DebugRange& b)
		{
			bool prior{(a.byteEnd <= b.byteStart)};
			return (a.bytesInside(b) || a.locationInside(b) || prior);
		}
	);
	metadata.erase(
		std::unique(metadata.begin(), metadata.end()),
		metadata.end()
	);
}

void ByteCode::encodeHeaders(std::ofstream& os) const
{
	// Magic.
	os.write("choice", sizeof("choice") - 1);

	// Version number.
	os.put(static_cast<char>(CH_VERSION_MAJOR));
	os.put(static_cast<char>(CH_VERSION_MINOR));
	os.put(static_cast<char>(CH_VERSION_PATCH));

	os.put(static_cast<char>(debugInfoState));

	std::string fileName{sourceManager.getFile(id)};
	os.put(static_cast<char>(fileName.size())); // File name length.
	os.write(fileName.data(), static_cast<std::streamsize>(fileName.size()));
}

void ByteCode::encodeData(std::ofstream& os) const
{
	u64 codeSize{block.size()}; // Bytecode length.
	Bytes::encodeValue(os, codeSize);

	u64 poolSize{countPool()}; 	// Constant pool length.
	Bytes::encodeValue(os, poolSize);

	// Check to avoid narrowing conversions below.

	constexpr auto maxSize{static_cast<u64>(
		std::numeric_limits<std::streamsize>::max()
	)};

	if (codeSize > maxSize) return; // Report an error.

	// Bytecode.
	os.write(reinterpret_cast<const char*>(block.data()),
		static_cast<std::streamsize>(block.size()));

	// Constant pool.
	for (const Object& constant : pool)
		constant.emit(os);
}

void ByteCode::encodeMetadata(std::ofstream& os) const
{
	// When debug metadata is combined with the bytecode,
	// each function's metadata directly follows its bytecode
	// data, so they aren't composed like this.
	if (debugInfoState == DEBUG_SEPARATE)
	{
		// Function objects have their metadata encoded first
		// since their disassembling is completed first when
		// reading cached bytecode data.
		for (const Object& obj : pool)
		{
			if (IS_FUNC(obj))
				AS_FUNC(obj)->code.encodeMetadata(os);
		}
	}

	u64 size{metadata.size()};
	Bytes::encodeValue(os, size);

	for (const auto& range : metadata)
	{
		Bytes::encodeValue(os, range.byteStart);
		Bytes::encodeValue(os, range.byteEnd);
		Bytes::encodeValue(os, range.sourceStart);
		Bytes::encodeValue(os, range.sourceEnd);
	}
}