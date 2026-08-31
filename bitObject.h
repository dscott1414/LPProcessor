/*
	bitObject.h - fixed-size bitset used for pattern / form / mandatory-pattern masks

	Overview:
		cBitObject is a storeSize-word bit vector (default 16 * 32 = 512 bits) with
		set/reset/isSet, in-place |= &= -=, and two cursors (first/next and
		firstNotIn/nextNotIn) that walk set bits.  hasAll(o) is "every bit of *this
		is also set in o" and is how mandatoryPatterns are tested against
		matchedPatterns.

	Pipeline position:
		Used on cWordMatch (forms, flags) and on cPattern (mandatoryPatterns etc.)
		during stages 3-4.  read()/write() are the binary-cache codec.

	Notes / gotchas:
		- Default parameters: sizeOfInteger=32, bitsPerInteger=5 (i.e. >>5 is /32),
			T=unsigned int, storeSize=16.  Changing one without the others breaks set().
		- `1 << (bit & 31)` is a signed shift; bit 31 is undefined for signed 1.
		- write() memcpy's BEFORE checking 'limit', so a short cache buffer is already
			overrun by the time LOG_FATAL_ERROR runs (and FATAL exits, so the
			return-false is dead).
		- first()/next() keep cursor state in byteIndex/bitIndex; they are not
			re-entrant and are not serialized (reset to -1 on read).
*/
#pragma once
template <int sizeOfInteger = 32, int bitsPerInteger = 5, class T = unsigned int, int storeSize = 16> class cBitObject
{
public:
	// Zero the bit words and reset the first/next cursor.  All bits start clear.
	cBitObject(void)
	{
		memset(bits, 0, sizeof(bits));
		byteIndex = bitIndex = -1;
	}
	// Clear every bit.  Does not reset the first/next cursor (call first() next).
	void clear(void)
	{
		memset(bits, 0, sizeof(bits));
	}
	// FATAL-exit if 'limit' is outside the bit storage.  Returns true on success
	// (the return is dead after FATAL).  'type' is only for the error message.
	bool check(const wchar_t *type, int limit)
	{
		if (limit >= (sizeof(bits) << 3))
			lplog(LOG_FATAL_ERROR, L"FATAL ERROR: bit storage for %s exceeded - size %d cannot fit into bit storage for %d.", type, limit, (sizeof(bits) << 3) - 1);
		return true;
	}
	// Set bit.  No range check - callers must check() first.  `1 << (bit&31)` is
	// a signed shift (UB at bit 31).
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
	// Word-wise inequality.  Cursor state is not compared.
	bool operator != (const cBitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			if (bits[I] != o.bits[I]) return true;
		return false;
	}
	bool operator == (const cBitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			if (bits[I] != o.bits[I]) return false;
		return true;
	}
	// In-place union / intersection / difference (bits &= ~o.bits).
	void operator |= (const cBitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			bits[I] |= o.bits[I];
	}
	void operator &= (const cBitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			bits[I] &= o.bits[I];
	}
	void operator -= (const cBitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			bits[I] &= ~o.bits[I];
	}
	// Reset the cursor and return the lowest set bit, or -1 if empty.
	int first(void)
	{
		byteIndex = 0;
		bitIndex = -1;
		return next();
	}
	// Advance the cursor and return the next set bit, or -1 when exhausted
	// (cursor is then reset).  Not re-entrant.
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
	// Like first(), but only bits set in *this and clear in o.
	int firstNotIn(const cBitObject &o)
	{
		byteIndex = 0;
		bitIndex = -1;
		return nextNotIn(o);
	}
	// Like next(), but only bits set in *this and clear in o.
	int nextNotIn(const cBitObject &o)
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
	// True iff every bit set in *this is also set in o (this ⊆ o).
	// Used as patterns[p]->mandatoryPatterns.hasAll(matchedPatterns).
	bool hasAll(const cBitObject &o)
	{
		for (unsigned int I = 0; I < sizeof(bits) / sizeof(*bits); I++)
			if ((bits[I] & o.bits[I]) != bits[I])
				return false;
		return true;
	}
	// True if no bit is set.
	bool isEmpty(void)
	{
		unsigned int I;
		for (I = 0; I < sizeof(bits) / sizeof(*bits) && !bits[I]; I++);
		return I == sizeof(bits) / sizeof(*bits);
	}
	// Deserialize sizeof(bits) bytes from buffer[where] and advance where.
	// FATAL-exits (does not return false) if the remaining buffer is too small.
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
	// Serialize sizeof(bits) bytes to buffer[where] and advance where.
	// memcpy happens BEFORE the limit check - a short buffer is already overrun
	// by the time FATAL fires.
	bool write(void *buffer, int &where, int limit)
	{
		if (where + sizeof(bits) > limit)
			lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached (2)!", limit);
		memcpy(((char *)buffer) + where, bits, sizeof(bits));
		where += sizeof(bits);
		return true;
	}
private:
	T bits[storeSize];
	unsigned short byteIndex, bitIndex;
};

