#pragma once
template <int sizeOfInteger = 32, int bitsPerInteger = 5, class T = unsigned int, int storeSize = 16> class bitObject
{
public:
	bitObject(void)
	{
		memset(bits, 0, sizeof(bits));
		byteIndex = bitIndex = -1;
	}
	void clear(void)
	{
		memset(bits, 0, sizeof(bits));
	}
	bool check(wchar_t *type, int limit)
	{
		if (limit >= (sizeof(bits) << 3))
			lplog(LOG_FATAL_ERROR, L"FATAL ERROR: bit storage for %s exceeded - size %d cannot fit into bit storage for %d.", type, limit, (sizeof(bits) << 3) - 1);
		return true;
	}
	inline void set(int bit)
	{
		bits[bit >> bitsPerInteger] |= 1 << (bit&(sizeOfInteger - 1));
	}
	inline void reset(int bit)
	{
		bits[bit >> bitsPerInteger] &= ~(1 << (bit&(sizeOfInteger - 1)));
	}
	inline bool isSet(int bit)
	{
		return (bits[bit >> bitsPerInteger] & (1 << (bit&(sizeOfInteger - 1)))) != 0;
	}
	bool operator != (const bitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			if (bits[I] != o.bits[I]) return true;
		return false;
	}
	bool operator == (const bitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			if (bits[I] != o.bits[I]) return false;
		return true;
	}
	void operator |= (const bitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			bits[I] |= o.bits[I];
	}
	void operator &= (const bitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			bits[I] &= o.bits[I];
	}
	void operator -= (const bitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			bits[I] &= ~o.bits[I];
	}
	int first(void)
	{
		byteIndex = 0;
		bitIndex = -1;
		return next();
	}
	int next(void)
	{
		if (++bitIndex >= sizeOfInteger)
		{
			byteIndex++;
			bitIndex = 0;
		}
		for (; byteIndex < sizeof(bits) / sizeof(*bits); byteIndex++)
		{
			for (int b = bits[byteIndex]; bitIndex < sizeOfInteger; bitIndex++)
				if (b&(1 << bitIndex))
					return bitIndex + byteIndex * sizeOfInteger;
			bitIndex = 0;
		}
		byteIndex = 0;
		bitIndex = -1;
		return -1;
	}
	int firstNotIn(const bitObject &o)
	{
		byteIndex = 0;
		bitIndex = -1;
		return nextNotIn(o);
	}
	int nextNotIn(const bitObject &o)
	{
		if (++bitIndex >= sizeOfInteger)
		{
			byteIndex++;
			bitIndex = 0;
		}
		for (; byteIndex < sizeof(bits) / sizeof(*bits); byteIndex++)
		{
			for (int b = bits[byteIndex] & ~o.bits[byteIndex]; bitIndex < sizeOfInteger; bitIndex++)
				if (b&(1 << bitIndex))
					return bitIndex + byteIndex * sizeOfInteger;
			bitIndex = 0;
		}
		byteIndex = 0;
		bitIndex = -1;
		return -1;
	}
	// bitObject must find all of itself in o
	// patterns[p]->mandatoryPatterns.hasAll(matchedPatterns)
	bool hasAll(const bitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			if ((bits[I] & o.bits[I]) != bits[I])
				return false;
		return true;
	}
	bool isEmpty(void)
	{
		unsigned int I;
		for (I = 0; I < sizeof(bits) / sizeof(*bits) && !bits[I]; I++);
		return I == sizeof(bits) / sizeof(*bits);
	}
	bool read(char *buffer, int &where, unsigned int limit)
	{
		if (where + sizeof(bits) > limit)
		{
			lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached (1)!", limit);
			return false;
		}
		memcpy(bits, buffer + where, sizeof(bits));
		where += sizeof(bits);
		byteIndex = bitIndex = -1;
		return true;
	}
	bool write(void *buffer, int &where, int limit)
	{
		memcpy(((char *)buffer) + where, bits, sizeof(bits));
		where += sizeof(bits);
		if (where > limit)
			lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached (1)!", limit);
		return true;
	}
private:
	T bits[storeSize];
	unsigned short byteIndex, bitIndex;
};

