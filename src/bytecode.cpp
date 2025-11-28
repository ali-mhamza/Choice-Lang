#include "../include/bytecode.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include <cstring>
#include <fstream>
#include <type_traits>
using namespace Object;

ByteCode::ByteCode() :
	block(0), pool(0) {}

ByteCode::ByteCode(const vByte& block) :
	block(block) {}

ByteCode::ByteCode(const vByte& block, vObj pool) :
	block(block), pool(std::move(pool)) {}

void ByteCode::addByte(uint8_t byte)
{
	block.push_back(byte);
}

void ByteCode::addShort(uint16_t bytes)
{
	addByte((bytes >> 8) & 0xff);
	addByte(bytes & 0xff);
}

void ByteCode::addLong(uint32_t bytes)
{
	addByte((bytes >> 24) & 0xff);
	addByte((bytes >> 16) & 0xff);
	addByte((bytes >> 8) & 0xff);
	addByte(bytes & 0xff);
}

#include <iostream> // FOR DEBUGGING. REMOVE.
void ByteCode::addConst(BaseUP constant)
{
	if (constant->type == OBJ_INT)
	{
		Int<int8_t>* temp = dynamic_cast<Int<int8_t>*>(constant.get());
		switch (temp->value)
		{
			case 0:     addByte(OP_ZERO);       return;
			case 1:     addByte(OP_ONE);        return;
			case 2:     addByte(OP_TWO);        return;
			case -1:    addByte(OP_NEG_ONE);    return;
			case -2:    addByte(OP_NEG_TWO);    return;
		}
	}

	else if (constant->type == OBJ_UINT)
	{
		UInt<uint8_t>* temp = dynamic_cast<UInt<uint8_t>*>(constant.get());
		switch (temp->value)
		{
			case 0:		addByte(OP_ZERO);		return;
			case 1:		addByte(OP_ONE);		return;
			case 2:		addByte(OP_TWO);		return;
			case -1:	addByte(OP_NEG_ONE);	return;
			case -2:	addByte(OP_NEG_TWO);	return;
		}
	}
	
	pool.push_back(std::move(constant));
	addByte(OP_CONST);

	size_t size = pool.size();
	if (size - 1 < 256)
	{
		addByte(OP_BYTE_OPER);
		addByte(static_cast<uint8_t>(size - 1));
	}

	else if (size - 1 < 65536)
	{
		addByte(OP_SHORT_OPER);
		addShort(static_cast<uint16_t>(size - 1));
	}

	else
	{
		addByte(OP_LONG_OPER);
		addLong(static_cast<uint32_t>(size - 1));
	}
}

void ByteCode::cacheStream(std::ofstream& os) const
{
	os.write(reinterpret_cast<const char*>(block.data()), 
				block.size());

	// Hypothetical for constant pool.
	constexpr uint8_t POOL_START = 100; // Beyond any opcodes we have.
	os.put(static_cast<char>(POOL_START));
	if (file != "")
		os.write(file.data(), file.length());
	os.put('\0');
	for (const BaseUP& constant : pool)
		constant->emit(os);
}

// For testing.

void ByteCode::loadReg(uint8_t reg, uint8_t op)
{
	addBytes(static_cast<uint8_t>(OP_LOAD_R), reg, op);
}

void ByteCode::loadRegConst(BaseUP constant, uint8_t reg)
{
	addByte(OP_LOAD_R);
	// Destination first.
	addByte(reg);

	if (constant->type == OBJ_INT)
	{
		Int<int8_t>* temp = dynamic_cast<Int<int8_t>*>(constant.get());
		switch (temp->value)
		{
			case 0:     addByte(OP_ZERO);       return;
			case 1:     addByte(OP_ONE);        return;
			case 2:     addByte(OP_TWO);        return;
			case -1:    addByte(OP_NEG_ONE);    return;
			case -2:    addByte(OP_NEG_TWO);    return;
		}
	}

	else if (constant->type == OBJ_UINT)
	{
		UInt<uint8_t>* temp = dynamic_cast<UInt<uint8_t>*>(constant.get());
		switch (temp->value)
		{
			case 0:		addByte(OP_ZERO);		return;
			case 1:		addByte(OP_ONE);		return;
			case 2:		addByte(OP_TWO);		return;
			case -1:	addByte(OP_NEG_ONE);	return;
			case -2:	addByte(OP_NEG_TWO);	return;
		}
	}

	pool.push_back(std::move(constant));

	size_t size = pool.size();
	if (size - 1 < 256)
	{
		addByte(OP_BYTE_OPER);
		addByte(static_cast<uint8_t>(size - 1));
	}

	else if (size - 1 < 65536)
	{
		addByte(OP_SHORT_OPER);
		addShort(static_cast<uint16_t>(size - 1));
	}

	else
	{
		addByte(OP_LONG_OPER);
		addLong(static_cast<uint32_t>(size - 1));
	}
}