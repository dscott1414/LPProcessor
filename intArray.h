#pragma warning (disable: 4996)
#pragma warning (disable: 4503)
class cIntArray
{
public:

	int binary_search_lower_bound(int x) 
	{
		int lowIndex = 0,highIndex = count; // Not n - 1
		while (lowIndex < highIndex) 
		{
			int midIndex = (lowIndex + highIndex) / 2;
			if (x <= content[midIndex]) 
				highIndex = midIndex;
			else 
				lowIndex = midIndex + 1;
		}
		return lowIndex-1;
	}

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
	cIntArray(IOHANDLE file)
	{
		read(file);
		zeroOutSpace=false;
	};
	cIntArray(bool inZeroOutSpace=false)
	{
		zeroOutSpace=inZeroOutSpace;
		count=0;
		allocated=0;
		content=NULL;
	};
	~cIntArray() 
	{
		if (allocated) tfree(allocated*sizeof(*content),content);
		count=0;
		allocated=0;
		content=NULL;
	}
	cIntArray(const cIntArray &rhs, bool inZeroOutSpace = false)
	{
		zeroOutSpace = inZeroOutSpace;
		count=rhs.count;
		allocated=rhs.allocated;
		content=NULL;
		if (allocated) 
		{
			content = (int *)tmalloc(allocated*sizeof(*content));
			if (!content) 
        lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (9)");
			if (zeroOutSpace)
				memset(content,0,allocated*sizeof(*content));
			memcpy(content,rhs.content,count*sizeof(*content));
		}
	} 	
	bool write(IOHANDLE file)
	{
		_write(file,&count,sizeof(count));
		_write(file,content,count*sizeof(*content));
		return true;
	}
	bool read(IOHANDLE file)
	{
		_read(file,&count,sizeof(count));
		allocated=count;
		content=(int *)tmalloc(count*sizeof(*content));
		if (!content) 
      lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (10)");
		_read(file,content,count*sizeof(*content));
		return true;
	}
	bool write(void *buffer,int &where,unsigned int limit)
	{
		if (!copy(buffer,count,where,limit)) return false;
		if (where+count*sizeof(*content)>limit) 
			lplog(LOG_FATAL_ERROR,L"Maximum copy limit of %d bytes reached (2)!",limit);
		memcpy(((char *)buffer)+where,content,count*sizeof(*content));
		where+=count*sizeof(*content);
		return true;
	}
	bool read(void *buffer,int &where,unsigned int limit)
	{
		if (!copy(count,buffer,where,limit)) return false;
		allocated=count;
		content=(int *)tmalloc(count*sizeof(*content));
		if (where+count*sizeof(*content)>limit) 
			lplog(LOG_FATAL_ERROR,L"Maximum read copy limit of %d bytes reached (2)!",limit);
		if (!content) 
      lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (11)");
		memcpy(content,((char *)buffer)+where,count*sizeof(*content));
		where+=count*sizeof(*content);
		return true;
	}
  bool operator==(const cIntArray other) const
  {
		if (count!=other.count) return false;
    return memcmp(content, other.content,count*sizeof(*content))==0;
  }
  cIntArray& operator=(const cIntArray &rhs) 
  {
		if (allocated) tfree(allocated*sizeof(*content),content);
		count=rhs.count;
		allocated=rhs.allocated;
		content=NULL;
		if (allocated) 
		{
			content = (int *)tmalloc(allocated*sizeof(*content));
			if (!content) 
        lplog(LOG_FATAL_ERROR,L"OUT OF MEMORY (12)");
			memcpy(content,rhs.content,count*sizeof(*content));
		}
		return *this;
  }
  bool operator!=(const cIntArray other) const
  {
		if (count!=other.count) return true;
    return memcmp(content, other.content,count*sizeof(*content))!=0;
  }
	typedef allocator<int>::reference reference;
	typedef allocator<int>::const_reference const_reference;
	inline reference operator[](unsigned int _P0)
	{
		#ifdef INDEX_CHECK
			if (_P0>=count) 
			{
				lplog(LOG_ERROR,L"Illegal reference (4) to element %d in an array with only %d elements!",_P0,count);
				if (count==0) add(0);
				return content[0];
			}
		#endif
		return (content[_P0]); 
	}
	inline const_reference operator[](unsigned int _P0) const
	{
		#ifdef INDEX_CHECK
			if (_P0>=count) 
			{
				lplog(LOG_ERROR,L"Illegal reference (5) to element %d in an array with only %d elements!",_P0,count);
				if (count==0) throw;
				return content[0];
			}
		#endif
		return (content[_P0]); 
	}
	void assign(unsigned int I,int value)
	{
		if (allocated<=I)
		{
			int oldAllocated=allocated;
			if (!allocated) allocated=5;
			else allocated*=2;
			content=(int *)trealloc(13,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
			if (zeroOutSpace)
				memset(content+oldAllocated,0,(allocated-oldAllocated)*sizeof(*content));
		}
		if (count<=I) count=I+1;
		content[I]=value;
	}
	int query(int qContent)
	{
		for (unsigned int c=0; c<count; c++) 
		{
			if (content[c]==qContent) 
				return c;
		}
		return -1;
	}
	bool contains(int qContent)
	{
		return query(qContent)>=0;
	}
	bool containsOneOf(int flag_mask)
	{
		for (unsigned int c=0; c<count; c++) 
			if ((content[c]&flag_mask)==content[c]) 
				return true;
		return false;
	}
	int add(int addContent)
	{
		count++;
		if (allocated<=count)
		{
			int oldAllocated=allocated;
			if (!allocated) allocated=5;
			else allocated*=2;
			content=(int *)trealloc(14,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
			if (zeroOutSpace)
				memset(content+oldAllocated,0,(allocated-oldAllocated)*sizeof(*content));
		}
		content[count-1]=addContent;
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
	unsigned int mark(unsigned int at,unsigned int markContent)
	{
		if (allocated<=at)
		{
			int oldAllocated=allocated;
			allocated=at+10;
			content=(int *)trealloc(15,content,oldAllocated*sizeof(*content),allocated*sizeof(*content));
			if (zeroOutSpace)
				memset(content+oldAllocated,0,(allocated-oldAllocated)*sizeof(*content));
		}
		content[at]=markContent;
		return markContent;
	}
	int erase(unsigned int at)
	{
		#ifdef INDEX_CHECK
			if (at>=count || at<0) 
			{
				lplog(LOG_ERROR,L"Illegal reference (6) to element %d in an array with only %d elements!",at,count);
				if (count==0) throw;
				return count;
			}
		#endif
		memmove(content+at,content+at+1,(count-at-1)*sizeof(content[at]));
		count--;
		return count;
	}
	int erase(void)
	{
		count=0;
		return 0;
	}
	wstring concatToString(wstring &trail)
	{
		trail.clear();
		wchar_t temp[10];
		for (unsigned int I=0; I<count; I++) trail=trail+wstring(_itow(content[I],temp,10))+L" ";
		return trail;
	}
	#define BITS_PER_RULE 10
	#define TOTAL_BITS 30
	// previously concat
	unsigned int encode()
	{
		unsigned int val = 0;
		{
			for (unsigned int I = 0; I < (TOTAL_BITS / BITS_PER_RULE) - 1; I++, val <<= BITS_PER_RULE)
				if (I < count)
					val += (unsigned)content[I];
			if (((TOTAL_BITS / BITS_PER_RULE) - 1) < count)
				val += ((unsigned)content[((TOTAL_BITS / BITS_PER_RULE) - 1)])&((1 << BITS_PER_RULE) - 1);
		}
		return val;
	}
	void decode(unsigned int val)
	{
		int lastNonZero = 0;
		for (int tc=0,bitFieldCount = TOTAL_BITS; bitFieldCount >= 0; bitFieldCount -= BITS_PER_RULE, tc++)
		{
			assign(tc, (val >> (bitFieldCount - BITS_PER_RULE))&((1 << BITS_PER_RULE) - 1));
			if (content[tc])
			{
				lastNonZero = tc;
				if (content[tc] & (1 << (BITS_PER_RULE - 1)))
					content[tc] -= (1 << BITS_PER_RULE); // 2s complement
			}
		}
		count = lastNonZero + 1;
	}
	void insert(int where,int value)
	{
		add(0);
		memmove(content+where+1,content+where,(count-where-1)*sizeof(content[where]));
		content[where]=value;
	}
  int *end() 
  {
    return content+count;
  }
  int *begin() 
  {
    return content;
  }
  private:
  	int *content;

};

bool copy(void *buf,cIntArray &a,int &where,int limit);
bool copy(cIntArray &a,void *buf,int &where,int limit);
