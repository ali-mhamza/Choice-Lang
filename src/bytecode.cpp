#include "../include/bytecode.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <cmath>
#include <cstring>
#include <fstream>

ByteCode::ByteCode() :
	block(0), pool(0) {}

ByteCode::ByteCode(const vByte& block) :
	block(block) {}

ByteCode::ByteCode(const vByte& block, const vObj& pool) :
	block(block), pool(pool) {}

void ByteCode::addByte(ui8 byte)
{
	block.push_back(byte);
}

void ByteCode::addShort(ui16 bytes)
{
	addByte((bytes >> 8) & 0xff);
	addByte(bytes & 0xff);
}

void ByteCode::addLong(ui32 bytes)
{
	addByte((bytes >> 24) & 0xff);
	addByte((bytes >> 16) & 0xff);
	addByte((bytes >> 8) & 0xff);
	addByte(bytes & 0xff);
}

void ByteCode::addOp(Opcode op)
{
	addByte(static_cast<ui8>(op));
}

ui64 ByteCode::addJump(Opcode op, i16 reg)
{
	if (reg == -1)
		addByte(static_cast<ui8>(op));
	else
		addBytes(static_cast<ui8>(op), static_cast<ui8>(reg));
	ui64 offset = block.size();
	block.push_back(static_cast<ui8>(0));
	block.push_back(static_cast<ui8>(0));
	return offset;
}

void ByteCode::patchJump(ui64 offset)
{
	// We've skipped the 2 jump bytes.
	ui16 diff = static_cast<ui16>(block.size() - offset - 2);
	if (diff < (1 << 16))
	{
		block[offset] = static_cast<ui8>((diff >> 8) & 0xff);
		block[offset + 1] = static_cast<ui8>(diff & 0xff);
	}
	else
	{
		// Jump is too big.
	}
}

void ByteCode::addLoop(ui64 start)
{
	// We jump back the difference from our opcode to the
	// start, plus 2 more for the decoded offset bytes,
	// plus another 1 since we are on the instruction
	// *after* the two bytes by the time we've decoded the
	// offset.

	ui16 diff = static_cast<ui16>(block.size() - start + 3);	
	addByte(OP_LOOP);
	addByte(static_cast<ui8>((diff >> 8) & 0xff));
	addByte(static_cast<ui8>(diff & 0xff));
}

void ByteCode::loadReg(ui8 reg, ui8 op)
{
	addBytes(static_cast<ui8>(OP_LOAD_R), reg, op);
}

#define IS_SMALL(val) ((-3 < (val)) && ((val) < 3))

void ByteCode::loadRegConst(Object& constant, ui8 reg)
{
	addBytes(static_cast<ui8>(OP_LOAD_R), reg); // Destination first.

	if (IS_(INT, constant))
	{
		if (IS_SMALL(constant.as.intVal))
		{
			addByte(static_cast<ui8>(constant.as.intVal + 2));
			return;
		}
	}
	else if (IS_(DEC, constant))
	{
		if (IS_SMALL(constant.as.decVal)
			&& (fmod(constant.as.decVal, 1.0) == 0.0))
		{
			addByte(static_cast<ui8>(constant.as.decVal + 2));
			return;
		}
	}

	pool.push_back(std::move(constant));

	size_t size = pool.size();
	if (size - 1 < (1 << 8))
	{
		addByte(OP_BYTE_OPER);
		addByte(static_cast<ui8>(size - 1));
	}
	else if (size - 1 < (1 << 16))
	{
		addByte(OP_SHORT_OPER);
		addShort(static_cast<ui16>(size - 1));
	}
	else
	{
		addByte(OP_LONG_OPER);
		addLong(static_cast<ui32>(size - 1));
	}
}

ui64 ByteCode::countPool() const
{
	ui64 count = 0;
	
	for (const Object& obj : pool)
	{
		if (IS_(INT, obj) || IS_(DEC, obj))
			count += 9; // 8 bytes + 1 type byte.
		else if (IS_HEAP_OBJ(obj))
		{
			switch (obj.type)
			{
				case OBJ_FUNC:
				{
					const Function& func = *(AS_(func, obj));
					// Added type byte (1) and null byte (1)
					// and argCount byte (1) and lambda Boolean byte (1).
					count += 1 + strlen(func.name) + 1 + 1 + 1;
					// Added code size and pool size values,
					// as well as the actual sizes of the code and pool.
					count += 2 * sizeof(ui64) + func.code.codeSize()
						+ func.code.countPool();
					break;
				}
				case OBJ_STRING:
					// Added type byte (1) and null byte (1).
					count += 1 + AS_(string, obj)->str.size() + 1;
					break;
				case OBJ_RANGE:
					// Added type byte (1) and three i64 (3 * 8) values.
					count += 1 + 3 * 8;
					break;
				default: UNREACHABLE();
			}
		}
	}

	return count;
}

void ByteCode::cacheStream(std::ofstream& os) const
{
	// Magic.
	os.write("choice", 6);

	// Version number.
	os.put(static_cast<char>(VERSION_MAJOR));
	os.put(static_cast<char>(VERSION_MINOR));
	os.put(static_cast<char>(VERSION_PATCH));

	// File name length.
	os.put(static_cast<char>(file.size()));

	// Bytecode length.
	ui64 codeSize = block.size();
	os.write(reinterpret_cast<const char*>(&codeSize),
		sizeof(ui64));

	// Constant pool length.
	ui64 poolSize = countPool();
	os.write(reinterpret_cast<const char*>(&poolSize),
		sizeof(ui64));

	// File name.
	os.write(file.data(), file.size());

	// Bytecode.
	os.write(reinterpret_cast<const char*>(block.data()),
		block.size());

	// Constant pool.
	for (const Object& constant : pool)
		constant.emit(os);
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