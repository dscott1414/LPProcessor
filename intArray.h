/*
	intArray.h - growable int vector (cIntArray) used for PEMA chains, stem trails, and packed rule ids

	Overview:
		A malloc-backed int array with explicit count/allocated, optional zero-fill of
		slack, file and binary-cache read/write, and encode()/decode() that pack up to
		three 10-bit signed fields into a 30-bit unsigned (the old "concat" of stemmer
		rule numbers).  operator[] is unchecked unless INDEX_CHECK is defined.

	Pipeline position:
		Embedded in cStemmer::cSuffixRule::trail and in several pattern-match arrays.
		copy() overloads at the bottom are the cache codec (defined in utilities.cpp).

	Notes / gotchas:
		- add() increments count BEFORE the capacity check, then writes content[count-1].
			The doubling starts at 5.  trealloc result is assigned over 'content'; a
			failure is fatal (trealloc exits) so the leak-on-failure pattern is latent.
		- operator== / != take the rhs by value (full copy).
		- decode() shifts by (bitFieldCount - 10) with bitFieldCount running 30,20,10,0;
			the last iteration is `val >> -10`, which is undefined.
		- erase(unsigned) still tests `at < 0`, which is impossible.
		- write(buffer,...) FATAL-exits if the payload will not fit, then memcpy's
			anyway (dead after FATAL).
*/
#pragma warning (disable: 4996)
#pragma warning (disable: 4503)
class cIntArray
{
public:

	// Greatest index i with content[i] < x, or -1 if x is <= every element.
	// Requires content[0..count) sorted ascending.  highIndex is count, not count-1.
	int binary_search_lower_bound(int x)
	{
		int lowIndex = 0, highIndex = count; // Not n - 1
		while (lowIndex < highIndex)
		{
			int midIndex = (lowIndex + highIndex) / 2;
			if (x <= content[midIndex])
				highIndex = midIndex;
			else
				lowIndex = midIndex + 1;
		}
		return lowIndex - 1;
	}

	// First index i with content[i] > x, or count if x is >= every element.
	// Requires content[0..count) sorted ascending.
	int binary_search_upper_bound(int x)
	{
		int lowIndex = 0;
		int highIndex = count; // Not n - 1
		while (lowIndex < highIndex) {
			int midIndex = (lowIndex + highIndex) / 2;
			if (x >= content[midIndex]) {
				lowIndex = midIndex + 1;
			}
			else {
				highIndex = midIndex;
			}
		}
		return lowIndex;
	}

	bool zeroOutSpace;
	unsigned int count;
	unsigned int allocated;
	// Deserialize from an already-open low-io fd.  zeroOutSpace is forced false
	// (the file image is the only initialized prefix).
	cIntArray(IOHANDLE file)
	{
		read(file);
		zeroOutSpace = false;
	};
	// Empty array.  If inZeroOutSpace, every growth memset's the new slack to 0.
	cIntArray(bool inZeroOutSpace = false)
	{
		zeroOutSpace = inZeroOutSpace;
		count = 0;
		allocated = 0;
		content = NULL;
	};
	// tfree the buffer.  Safe on a default-constructed (NULL) content.
	~cIntArray()
	{
		if (allocated) tfree(allocated * sizeof(*content), content);
		count = 0;
		allocated = 0;
		content = NULL;
	}
	// Deep copy.  FATAL-exits on OOM (the `return` after lplog is dead).
	cIntArray(const cIntArray& rhs, bool inZeroOutSpace = false)
	{
		zeroOutSpace = inZeroOutSpace;
		count = rhs.count;
		allocated = rhs.allocated;
		content = NULL;
		if (allocated)
		{
			content = (int*)tmalloc(allocated * sizeof(*content));
			if (!content)
			{
				lplog(LOG_FATAL_ERROR, L"OUT OF MEMORY (9)");
				return;
			}
			if (zeroOutSpace)
				memset(content, 0, allocated * sizeof(*content));
			memcpy(content, rhs.content, count * sizeof(*content));
		}
	}
	// Write count, then count ints.  Return is always true; write errors are ignored.
	bool write(IOHANDLE file)
	{
		_write(file, &count, sizeof(count));
		_write(file, content, count * sizeof(*content));
		return true;
	}
	// Read count, allocate exactly that many ints, read them.  false on short read
	// or OOM (OOM is also FATAL).  allocated is set equal to count (no slack).
	bool read(IOHANDLE file)
	{
		if (_read(file, &count, sizeof(count)) < sizeof(count))
			return false;
		allocated = count;
		content = (int*)tmalloc(count * sizeof(*content));
		if (!content)
		{
			lplog(LOG_FATAL_ERROR, L"OUT OF MEMORY (10)");
			return false;
		}
		if (_read(file, content, count * sizeof(*content)) < count * sizeof(*content))
			return false;
		return true;
	}
	// Binary-cache write: count, then the payload.  FATAL if the payload will not
	// fit; memcpy still runs after the log (dead - FATAL exits).
	bool write(void* buffer, int& where, unsigned int limit)
	{
		if (!copy(buffer, count, where, limit)) return false;
		if (where + count * sizeof(*content) > limit)
			lplog(LOG_FATAL_ERROR, L"Maximum copy limit of %d bytes reached (2)!", limit);
		memcpy(((char*)buffer) + where, content, count * sizeof(*content));
		where += count * sizeof(*content);
		return true;
	}
	// Binary-cache read.  Allocates before the limit check, so a corrupt count
	// can request an enormous tmalloc and then FATAL.
	bool read(void* buffer, int& where, unsigned int limit)
	{
		if (!copy(count, buffer, where, limit)) return false;
		allocated = count;
		content = (int*)tmalloc(count * sizeof(*content));
		if (where + count * sizeof(*content) > limit)
			lplog(LOG_FATAL_ERROR, L"Maximum read copy limit of %d bytes reached (2)!", limit);
		if (!content)
		{
			lplog(LOG_FATAL_ERROR, L"OUT OF MEMORY (11)");
			return false;
		}
		memcpy(content, ((char*)buffer) + where, count * sizeof(*content));
		where += count * sizeof(*content);
		return true;
	}
	bool operator==(const cIntArray other) const
	{
		if (count != other.count) return false;
		return memcmp(content, other.content, count * sizeof(*content)) == 0;
	}
	// Replace contents with a deep copy of rhs.  Not self-assignment safe
	// (frees content first).  FATAL on OOM.
	cIntArray& operator=(const cIntArray& rhs)
	{
		if (allocated) tfree(allocated * sizeof(*content), content);
		count = rhs.count;
		allocated = rhs.allocated;
		content = NULL;
		if (allocated)
		{
			content = (int*)tmalloc(allocated * sizeof(*content));
			if (!content)
			{
				lplog(LOG_FATAL_ERROR, L"OUT OF MEMORY (12)");
				return *this;
			}
			memcpy(content, rhs.content, count * sizeof(*content));
		}
		return *this;
	}
	bool operator!=(const cIntArray other) const
	{
		if (count != other.count) return true;
		return memcmp(content, other.content, count * sizeof(*content)) != 0;
	}
	inline int &operator[](unsigned int _P0)
	{
#ifdef INDEX_CHECK
		if (_P0 >= count)
		{
			lplog(LOG_ERROR, L"Illegal reference (4) to element %d in an array with only %d elements!", _P0, count);
			if (count == 0) add(0);
			return content[0];
		}
#endif
		return (content[_P0]);
	}
	inline const int &operator[](unsigned int _P0) const
	{
#ifdef INDEX_CHECK
		if (_P0 >= count)
		{
			lplog(LOG_ERROR, L"Illegal reference (5) to element %d in an array with only %d elements!", _P0, count);
			if (count == 0) throw;
			return content[0];
		}
#endif
		return (content[_P0]);
	}
	// content[I] = value, growing (double, min 5) and raising count to I+1 if needed.
	void assign(unsigned int I, int value)
	{
		if (allocated <= I)
		{
			int oldAllocated = allocated;
			if (!allocated) allocated = 5;
			else allocated *= 2;
			content = (int*)trealloc(13, content, oldAllocated * sizeof(*content), allocated * sizeof(*content));
			if (zeroOutSpace)
				memset(content + oldAllocated, 0, (allocated - oldAllocated) * sizeof(*content));
		}
		if (count <= I) count = I + 1;
		content[I] = value;
	}
	// Linear search: index of qContent, or -1 if absent.
	int query(int qContent)
	{
		for (unsigned int c = 0; c < count; c++)
		{
			if (content[c] == qContent)
				return c;
		}
		return -1;
	}
	bool contains(int qContent)
	{
		return query(qContent) >= 0;
	}
	// True if some element is a subset of flag_mask (`(c & mask) == c`), i.e. every
	// set bit of c is also set in the mask.  Not "intersects".
	bool containsOneOf(int flag_mask)
	{
		for (unsigned int c = 0; c < count; c++)
			if ((content[c] & flag_mask) == content[c])
				return true;
		return false;
	}
	// Append.  count is incremented first, then the array is grown if
	// allocated <= count.  Always returns 0.
	int add(int addContent)
	{
		count++;
		if (allocated <= count)
		{
			int oldAllocated = allocated;
			if (!allocated) allocated = 5;
			else allocated *= 2;
			content = (int*)trealloc(14, content, oldAllocated * sizeof(*content), allocated * sizeof(*content));
			if (zeroOutSpace)
				memset(content + oldAllocated, 0, (allocated - oldAllocated) * sizeof(*content));
		}
		content[count - 1] = addContent;
		return 0;
	}
	int push_back(int addContent)
	{
		return add(addContent);
	}
	unsigned int size(void)
	{
		return count;
	}
	// content[at] = markContent, growing to at+10 if needed.  Does NOT raise
	// count, so a mark past count is invisible to size()/write() until add/assign.
	unsigned int mark(unsigned int at, unsigned int markContent)
	{
		if (allocated <= at)
		{
			int oldAllocated = allocated;
			allocated = at + 10;
			content = (int*)trealloc(15, content, oldAllocated * sizeof(*content), allocated * sizeof(*content));
			if (zeroOutSpace)
				memset(content + oldAllocated, 0, (allocated - oldAllocated) * sizeof(*content));
		}
		content[at] = markContent;
		return markContent;
	}
	// Remove the element at 'at' and compact.  Returns the new count.  The
	// `at < 0` INDEX_CHECK arm is dead (at is unsigned).
	int erase(unsigned int at)
	{
#ifdef INDEX_CHECK
		if (at >= count || at < 0)
		{
			lplog(LOG_ERROR, L"Illegal reference (6) to element %d in an array with only %d elements!", at, count);
			if (count == 0) throw;
			return count;
		}
#endif
		memmove(content + at, content + at + 1, (count - at - 1) * sizeof(content[at]));
		count--;
		return count;
	}
	// Logical clear (count=0).  Buffer is kept.
	int erase(void)
	{
		count = 0;
		return 0;
	}
	// Space-separated decimal dump into 'trail' (cleared first).  Returns trail.
	wstring concatToString(wstring& trail)
	{
		trail.clear();
		wchar_t temp[10];
		for (unsigned int I = 0; I < count; I++) trail = trail + wstring(_itow(content[I], temp, 10)) + L" ";
		return trail;
	}
#define BITS_PER_RULE 10
#define TOTAL_BITS 30
	// Pack up to three 10-bit fields (content[0..2]) into a 30-bit unsigned.
	// Extra elements are dropped.  Previously named concat.
	unsigned int encode()
	{
		unsigned int val = 0;
		{
			for (unsigned int I = 0; I < (TOTAL_BITS / BITS_PER_RULE) - 1; I++, val <<= BITS_PER_RULE)
				if (I < count)
					val += (unsigned)content[I];
			if (((TOTAL_BITS / BITS_PER_RULE) - 1) < count)
				val += ((unsigned)content[((TOTAL_BITS / BITS_PER_RULE) - 1)]) & ((1 << BITS_PER_RULE) - 1);
		}
		return val;
	}
	// Inverse of encode(): peel four 10-bit fields (the last shift is `>> -10`,
	// which is undefined) and sign-extend if the high bit of the 10-bit field is set.
	// count becomes (lastNonZero+1).
	void decode(unsigned int val)
	{
		int lastNonZero = 0;
		for (int tc = 0, bitFieldCount = TOTAL_BITS; bitFieldCount >= 0; bitFieldCount -= BITS_PER_RULE, tc++)
		{
			assign(tc, (val >> (bitFieldCount - BITS_PER_RULE)) & ((1 << BITS_PER_RULE) - 1));
			if (content[tc])
			{
				lastNonZero = tc;
				if (content[tc] & (1 << (BITS_PER_RULE - 1)))
					content[tc] -= (1 << BITS_PER_RULE); // 2s complement
			}
		}
		count = lastNonZero + 1;
	}
	// Insert value at 'where', shifting the tail right.  No bounds check on where.
	void insert(int where, int value)
	{
		add(0);
		memmove(content + where + 1, content + where, (count - where - 1) * sizeof(content[where]));
		content[where] = value;
	}
	int* end()
	{
		return content + count;
	}
	int* begin()
	{
		return content;
	}
private:
	int* content;

};

bool copy(void* buf, cIntArray& a, int& where, int limit);
bool copy(cIntArray& a, void* buf, int& where, int limit);
