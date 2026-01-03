#include "../include/bytecode.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <cstring>
#include <fstream>
#include <type_traits>

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

void ByteCode::loadReg(ui8 reg, ui8 op)
{
	addBytes(static_cast<ui8>(OP_LOAD_R), reg, op);
}

#define IS_SMALL(val) ((-3 < (val)) && ((val) < 3))

void ByteCode::loadRegConst(Object& constant, ui8 reg)
{
	// Must do them separately since addBytes
	// cannot handle an opcode and a regular byte together.
	addByte(OP_LOAD_R);
	// Destination first.
	addByte(reg);

	if (constant.type == OBJ_INT)
	{
		if (IS_SMALL(constant.as.intVal))
		{
			addByte(static_cast<ui8>(constant.as.intVal + 2));
			return;
		}
	}
	else if (constant.type == OBJ_DEC)
	{
		if (IS_SMALL(constant.as.doubleVal))
		{
			addByte(static_cast<ui8>(constant.as.doubleVal + 2));
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
		if ((obj.type == OBJ_INT) || (obj.type == OBJ_DEC))
			count += 9; // 8 bytes + 1 type byte.
		else if (obj.type == OBJ_HEAP)
		{
			HeapObj* temp = AS_HEAP_PTR(obj);
			switch (temp->type)
			{
				case HEAP_STRING:
					// Added type bytes (2) and null byte (1).
					count += 2 + AS_STRING(temp).str.size() + 1;
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