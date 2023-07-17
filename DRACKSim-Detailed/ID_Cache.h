typedef uint32_t CACHE_STATS;

#ifndef I_D_CACHE_H
#define I_D_CACHE_H

#include <string>
#include<assert.h>

/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
// static inline bool IsPower2(uint32_t n)
// {
//     return ((n & (n - 1)) == 0);
// }

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
// static inline int32_t FloorLog2(uint32_t n)
// {
//     int32_t p = 0;

//     if (n == 0) return -1;

//     if (n & 0xffff0000) { p += 16; n >>= 16; }
//     if (n & 0x0000ff00)	{ p +=  8; n >>=  8; }
//     if (n & 0x000000f0) { p +=  4; n >>=  4; }
//     if (n & 0x0000000c) { p +=  2; n >>=  2; }
//     if (n & 0x00000002) { p +=  1; }

//     return p;
// }

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
// static inline int32_t CeilLog2(uint32_t n)
// {
//     return FloorLog2(n - 1) + 1;
// }

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG
{
  private:
    uint64_t _tag;

  public:
    CACHE_TAG(uint64_t tag = 0) { _tag = tag; }
    bool operator==(const   CACHE_TAG &right) const { return _tag == right._tag; }
    operator uint64_t() const { return _tag; }
};


/*!
 * Everything related to cache sets
 */
namespace CACHE_SET
{

/*!
 *  @brief Cache set direct mapped
 */
class DIRECT_MAPPED
{
  private:
    CACHE_TAG _tag;
    int32_t _ASID;
    int32_t _valid;

  public:
    DIRECT_MAPPED(uint32_t associativity = 1) { assert(associativity == 1); _tag=-1; _ASID=-1; _valid=0;}

    void SetAssociativity(uint32_t associativity) { assert(associativity == 1); }
    uint32_t GetAssociativity(uint32_t associativity) { return 1; }

    uint32_t Find_And_Access(CACHE_TAG tag, int32_t ASID) 
    { 
    	if(_valid==1 && _tag == tag && _ASID==ASID)
		return true; 
    	else
    		return false;
    }
      
    void Replace(CACHE_TAG tag, int32_t ASID) { _tag = tag; _ASID=ASID; _valid=1;}
    void Invalidate(CACHE_TAG tag, int32_t ASID) 
    {
    	if(_valid==1 && _tag == tag && _ASID==ASID) 
    		_valid=0;
    }
    void Flush() { _tag = 0; _ASID=0; _valid=0;}
};

/*!
 *  @brief Cache set with round robin replacement
 */
template <uint32_t MAX_ASSOCIATIVITY = 4>
class ROUND_ROBIN
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    uint32_t _tagsLastIndex;
    uint32_t _nextReplaceIndex;
    int32_t _ASID[MAX_ASSOCIATIVITY];
    int32_t _valid[MAX_ASSOCIATIVITY];
    bool _modify[MAX_ASSOCIATIVITY];
    

  public:
    ROUND_ROBIN(uint32_t associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        assert(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (int32_t index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = CACHE_TAG(0);
            _ASID[index] = 0;
            _valid[index] = 0;
            _modify[index] = 0;
        }
    }

    void SetAssociativity(uint32_t associativity)
    {
        assert(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    uint32_t GetAssociativity(uint32_t associativity) { return _tagsLastIndex + 1; }

    uint32_t Find_And_Access(CACHE_TAG tag, int32_t ASID, bool is_write)
    {
        bool result = true;

        for (int32_t index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag && _ASID[index] == ASID && _valid[index]==1) 
            {
            	if(is_write==true)
            		_modify[index]=1;
            	goto end;
            }
        }
        result = false;

        end: return result;
    }

    bool Replace(CACHE_TAG tag, int32_t ASID, uint64_t &victim_tag)
    {
        bool victim_cache_modfiy_bit=0;
        // g++ -O3 too dumb to do CSE on following lines?!
        const uint32_t index = _nextReplaceIndex;

        victim_cache_modfiy_bit=_modify[index];
        victim_tag= _tags[index];


        _tags[index] = tag;
        _ASID[index] = ASID;
        _valid[index] = 1;
        _modify[index] = 0;
        // condition typically faster than modulo
        _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);

        return victim_cache_modfiy_bit;
    }
    
    void Invalidate(CACHE_TAG tag, int32_t ASID)
    {
        bool result = true;

        for (int32_t index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            uint64_t tag_bits= _tags[index] >> 6;
            if(_tags[index] == tag && _ASID[index] == ASID && _valid[index]==1)
            {
		        _valid[index] = 0;
            }
        }
    }
    
    void Flush()
    {
      for (int32_t index = _tagsLastIndex; index >= 0; index--)
      {
	    _tags[index] = 0;
    	_ASID[index] = 0;
    	_valid[index] = 0;
      }
      _nextReplaceIndex=_tagsLastIndex;
    }
};

} // namespace CACHE_SET

namespace CACHE_ALLOC
{
    typedef enum
    {
        STORE_ALLOCATE,
        STORE_NO_ALLOCATE
    } STORE_ALLOCATION;
}

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class CACHE_BASE
{
  public:
    // types, constants
    typedef enum
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;

  protected:
    static const uint32_t HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

  private:
    // input params
    const std::string _name;
    const uint32_t _cacheSize;
    const uint32_t _lineSize;
    const uint32_t _associativity;
    uint32_t _numberOfFlushes;
    uint32_t _numberOfResets;

    // computed params
    const uint32_t _lineShift;
    const uint32_t _setIndexMask;

    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (uint32_t accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

  protected:
    uint32_t NumSets() const { return _setIndexMask + 1; }

  public:
    // constructors/destructors
    CACHE_BASE(std::string name, uint32_t cacheSize, uint32_t lineSize, uint32_t associativity);

    // accessors
    uint32_t CacheSize() const { return _cacheSize; }
    uint32_t LineSize() const { return _lineSize; }
    uint32_t Associativity() const { return _associativity; }
    //
    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true];}
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false];}
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType);}
    CACHE_STATS Hits() const { return SumAccess(true);}
    CACHE_STATS Misses() const { return SumAccess(false);}
    CACHE_STATS Accesses() const { return Hits() + Misses();}

    CACHE_STATS Flushes() const { return _numberOfFlushes;}
    CACHE_STATS Resets() const { return _numberOfResets;}

    void SplitAddress(const uint64_t addr, CACHE_TAG & tag, uint32_t & setIndex) const
    {
        tag = addr >> _lineShift;
        setIndex = tag & _setIndexMask;
   }

    void SplitAddress(const uint64_t addr, CACHE_TAG & tag, uint32_t & setIndex, uint32_t & lineIndex) const
    {
        const uint32_t lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tag, setIndex);
    }

    void IncFlushCounter()
    {
	_numberOfFlushes += 1;
    }

    void IncResetCounter()
    {
	_numberOfResets += 1;
    }
	std::string GetName()
	{
		return _name;
	}

    std::ostream & StatsLong(std::ostream & out) const;
};

CACHE_BASE::CACHE_BASE(std::string name, uint32_t cacheSize, uint32_t lineSize, uint32_t associativity)
  : _name(name),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{

    assert(IsPower2(_lineSize));
    assert(IsPower2(_setIndexMask + 1));

    for (uint32_t accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

/*!
 *  @brief Stats output method
 */
std::ostream & CACHE_BASE::StatsLong(std::ostream & out) const
{
    const uint32_t headerWidth = 19;
    const uint32_t numberWidth = 10;

    out << _name << ":" << std::endl;

    for (uint32_t i = 0; i < ACCESS_TYPE_NUM; i++)
    {
        const ACCESS_TYPE accessType = ACCESS_TYPE(i);

        std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

        out << StringString(type + " Hits:      ", headerWidth)
            << StringInt(Hits(accessType), numberWidth) << std::endl;
        out << StringString(type + " Misses:    ", headerWidth)
            << StringInt(Misses(accessType), numberWidth) << std::endl;
        out << StringString(type + " Accesses:  ", headerWidth)
            << StringInt(Accesses(accessType), numberWidth) << std::endl;
        out << StringString(type + " Miss Rate: ", headerWidth)
            << StringFlt(100.0 * Misses(accessType) / Accesses(accessType), 2, numberWidth-1) << "%" << std::endl;
        out << std::endl;
    }

    out << StringString("Total Hits:      ", headerWidth, ' ')
        << StringInt(Hits(), numberWidth) << std::endl;
    out << StringString("Total Misses:    ", headerWidth, ' ')
        << StringInt(Misses(), numberWidth) << std::endl;
    out << StringString("Total Accesses:  ", headerWidth, ' ')
        << StringInt(Accesses(), numberWidth) << std::endl;
    out << StringString("Total Miss Rate: ", headerWidth, ' ')
        << StringFlt(100.0 * Misses() / Accesses(), 2, numberWidth-1) << "%" << std::endl;

    out << StringString("Flushes:         ", headerWidth, ' ')
        << StringInt(Flushes(), numberWidth) << std::endl;
    out << StringString("Stat Resets:     ", headerWidth, ' ')
        << StringInt(Resets(), numberWidth) << std::endl;

    out << std::endl;

    return out;
}

/// ostream operator for CACHE_BASE
std::ostream & operator<< (std::ostream & out, const CACHE_BASE & cacheBase)
{
    return cacheBase.StatsLong(out);
}

/*!
 *  @brief Templated cache class with specific cache set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of cache sets.
 */
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
class CACHE : public CACHE_BASE
{
  private:
    SET _sets[MAX_SETS];

  public:
    // constructors/destructors
    CACHE(std::string name, uint32_t cacheSize, uint32_t lineSize, uint32_t associativity)
      : CACHE_BASE(name, cacheSize, lineSize, associativity)
    {
        assert(NumSets() <= MAX_SETS);

        for (uint32_t i = 0; i < NumSets(); i++)
        {
            _sets[i].SetAssociativity(associativity);
        }
    }

    // modifiers
    /// Cache access from addr to addr+size-1
    bool AccessMultiCacheLine(uint64_t addr, uint32_t size, ACCESS_TYPE accessType, int32_t ASID, int &line_read, bool *linehit);
    bool AccessCacheMulti(uint64_t addr, uint32_t size, ACCESS_TYPE accessType, int32_t ASID);
    /// Cache access at addr that does not span cache lines
    bool AccessSingleCacheLine(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID);
    bool ReplaceCACHE(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID, uint64_t &victim_addr);
    bool ReplaceCACHE(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID);
    void InvalidateCACHE(uint64_t addr, int32_t ASID);
    void Flush();
    void ResetStats();
};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessMultiCacheLine(uint64_t addr, uint32_t size, ACCESS_TYPE accessType, int32_t ASID, int &line_read, bool *linehit)
{
    const uint64_t highAddr = addr + size;
    bool allHit = true;

    const uint64_t lineSize = LineSize();
    const uint64_t notLineMask = ~(lineSize - 1);
    do
    {
    	line_read++;
    	CACHE_TAG tag;
        uint32_t setIndex;

        SplitAddress(addr, tag, setIndex);

        SET & set = _sets[setIndex];

	bool is_write=false;
	
	if(accessType == ACCESS_TYPE_STORE)
        	is_write=true;
        bool localHit = set.Find_And_Access(tag,ASID,is_write);
        allHit &= localHit;
        linehit[line_read-1]=localHit;
        

        // on miss, loads always allocate, stores optionally
        //if ( (! localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        //{
        //    set.Replace(tag, ASID);
        //}
        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);
    _access[accessType][allHit]++;

    return allHit;
}

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessCacheMulti(uint64_t addr, uint32_t size, ACCESS_TYPE accessType, int32_t ASID)
{
    const uint64_t highAddr = addr + size;
    bool allHit = true;

    const uint64_t lineSize = LineSize();
    const uint64_t notLineMask = ~(lineSize - 1);
    do
    {
    	CACHE_TAG tag;
        uint32_t setIndex;

        SplitAddress(addr, tag, setIndex);

        SET & set = _sets[setIndex];

        bool localHit = set.Find_And_Access(tag,ASID,0);
        allHit &= localHit;

        // on miss, loads always allocate, stores optionally
        //if ( (! localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        //{
        //    set.Replace(tag, ASID);
        //}

        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);
    _access[accessType][allHit]++;

    return allHit;
}

/*!
 *  @return true if accessed cache line hits
 */
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessSingleCacheLine(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID)
{
    CACHE_TAG tag;
    uint32_t setIndex;

    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];
	
    bool is_write=false;
    if(accessType == ACCESS_TYPE_STORE)
	is_write=true;
    bool hit = set.Find_And_Access(tag,ASID,is_write);

    // on miss, loads always allocate, stores optionally
    //if ( (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    //{
    //    set.Replace(tag,ASID);
    //}

    _access[accessType][hit]++;

    return hit;
}
/*!
 *  @return true if accessed cache line hits
 */
 
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::ReplaceCACHE(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID, uint64_t &victim_addr)
{
    bool victim_cache_modify_bit=0;
    uint64_t victim_tag;
    CACHE_TAG tag;
    uint32_t setIndex;
    
    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];

    //bool hit = set.Find(tag,ASID,paddr);

    // on miss, loads always allocate, stores optionally
    if ((accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        victim_cache_modify_bit=set.Replace(tag,ASID,victim_tag);
    }

    victim_addr=victim_tag<<6;

    return victim_cache_modify_bit;
}

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::ReplaceCACHE(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID)
{
    bool victim_cache_modify_bit=0;
    uint64_t victim_tag;
    CACHE_TAG tag;
    uint32_t setIndex;
    
    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];

    //bool hit = set.Find(tag,ASID,paddr);

    // on miss, loads always allocate, stores optionally
    if ((accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        victim_cache_modify_bit=set.Replace(tag,ASID,victim_tag);
    }

    return victim_cache_modify_bit;
}
 
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void CACHE<SET,MAX_SETS,STORE_ALLOCATION>::InvalidateCACHE(uint64_t addr, int32_t ASID)
{
    CACHE_TAG tag;
    uint32_t setIndex;

    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];

    //bool hit = set.Find(tag,ASID,paddr);

    // on miss, loads always allocate, stores optionally
    set.Invalidate(tag,ASID);
    
//    if ((accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
//    {
//        set.Replace(tag,ASID,paddr);
//    }

	//_access[accessType][hit]++;

    	//return hit;
} 

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void CACHE<SET,MAX_SETS,STORE_ALLOCATION>::Flush()
{
    for (int32_t index = NumSets(); index >= 0; index--) {
      SET & set = _sets[index];
      set.Flush();
    }
    IncFlushCounter();
}

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void CACHE<SET,MAX_SETS,STORE_ALLOCATION>::ResetStats()
{
    for (uint32_t accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
	_access[accessType][false] = 0;
	_access[accessType][true] = 0;
    }
    IncResetCounter();
}

void GetNextBlockAddress(uint64_t &paddr, int32_t ASID)
{
    const uint64_t lineSize = 64;
    const uint64_t notLineMask = ~(lineSize - 1);
    paddr = (paddr & notLineMask) + lineSize;
}

// define shortcuts
#define CACHE_DIRECT_MAPPED(MAX_SETS, ALLOCATION) CACHE<CACHE_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

namespace IL1
{
    // 1st level instruction cache: 32 kB, 64 B lines, 8-way associative
    // const uint32_t cacheSize = 32*KILO;
    const uint32_t cacheSize = 32*KILO;
    const uint32_t lineSize = 64;
    const uint32_t associativity = 8;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const uint32_t max_sets = cacheSize / (lineSize * associativity);
    const uint32_t max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern IL1::CACHE il1s[core_count*num_nodes];

#define BOOST_PP_LOCAL_LIMITS   (0, ((core_count*num_nodes)-1))
#define BOOST_PP_LOCAL_MACRO(n) IL1::CACHE("L1_I"#n, IL1::cacheSize, IL1::lineSize, IL1::associativity), 
IL1::CACHE il1s[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};


namespace DL1
{
    // 1st level data cache: 32 kB, 64 B lines, 8-way associative
    // const uint32_t cacheSize = 32*KILO;
    const uint32_t cacheSize = 32*KILO;
    const uint32_t lineSize = 64;
    const uint32_t associativity = 8;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const uint32_t max_sets = cacheSize / (lineSize * associativity);
    const uint32_t max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern DL1::CACHE dl1s[core_count*num_nodes];

#define BOOST_PP_LOCAL_LIMITS   (0, ((core_count*num_nodes)-1))
#define BOOST_PP_LOCAL_MACRO(n) DL1::CACHE("L1_D"#n, DL1::cacheSize, DL1::lineSize, DL1::associativity), 
DL1::CACHE dl1s[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

namespace L2
{
    // 2nd level cache: 256 KB, 64 B lines, 4-way associative
    // const uint32_t cacheSize = 256*KILO;
    const uint32_t cacheSize = 256*KILO;
    const uint32_t lineSize = 64;
    const uint32_t associativity = 4;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const uint32_t max_sets = cacheSize / (lineSize * associativity);
    const uint32_t max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern L2::CACHE l2s[core_count*num_nodes];

#define BOOST_PP_LOCAL_LIMITS   (0, ((core_count*num_nodes)-1))
#define BOOST_PP_LOCAL_MACRO(n) L2::CACHE("L2_"#n, L2::cacheSize, L2::lineSize, L2::associativity), 
L2::CACHE l2s[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

namespace UL3
{
    // 3rd level unified cache: 16 MB, 64 B lines, 16-way associative
    const uint32_t cacheSize = (core_count*2*MEGA);
    //const uint32_t cacheSize = (8*KILO);
    const uint32_t lineSize = 64;
    const uint32_t associativity = 16;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const uint32_t max_sets = cacheSize / (lineSize * associativity);
    const uint32_t max_associativity = associativity;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}extern UL3::CACHE ul3[num_nodes];
#define BOOST_PP_LOCAL_LIMITS   (0, (num_nodes-1))
#define BOOST_PP_LOCAL_MACRO(n) UL3::CACHE("L3_"#n, UL3::cacheSize, UL3::lineSize, UL3::associativity), 
UL3::CACHE ul3[]=
{
#include "boost/preprocessor/iteration/detail/local.hpp" 
};

#endif // PIN_CACHE_H
