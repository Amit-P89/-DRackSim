	/*
 * Copyright 2002-2019 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials, and your
 * use of them is governed by the express license under which they were provided to
 * you ("License"). Unless the License provides otherwise, you may not use, modify,
 * copy, publish, distribute, disclose or transmit this software or the related
 * documents without Intel's prior written permission.
 *
 * This software and the related documents are provided as is, with no express or
 * implied warranties, other than those that are expressly stated in the License.
 */

/*! @file
 *  This file contains a configurable TLB class
 */

typedef uint32_t TLB_STATS;

#ifndef TLB_H
#define TLB_H

#include <string>
#include <assert.h>
/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(uint32_t n)
{
    return ((n & (n - 1)) == 0);
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline uint32_t FloorLog2(uint32_t n)
{
    uint32_t p = 0;

    if (n == 0)
        return -1;

    if (n & 0xffff0000)
    {
        p += 16;
        n >>= 16;
    }
    if (n & 0x0000ff00)
    {
        p += 8;
        n >>= 8;
    }
    if (n & 0x000000f0)
    {
        p += 4;
        n >>= 4;
    }
    if (n & 0x0000000c)
    {
        p += 2;
        n >>= 2;
    }
    if (n & 0x00000002)
    {
        p += 1;
    }

    return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline uint32_t CeilLog2(uint32_t n)
{
    return FloorLog2(n - 1) + 1;
}

/*!
 *  @brief TLB tag - self clearing on creation
 */
class TLB_TAG
{
private:
    uint64_t _tag;

public:
    TLB_TAG(uint64_t tag = 0) { _tag = tag; }
    bool operator==(const TLB_TAG &right) const { return _tag == right._tag; }
    operator uint64_t() const { return _tag; }
};

/*!
 * Everything related to Tlb sets
 */
namespace TLB_SET
{

    /*!
     *  @brief Tlb set direct mapped
     */
    class DIRECT_MAPPED
    {
    private:
        TLB_TAG _tag;
        int32_t _ASID;
        uint64_t _PPN;
        int32_t _valid;


    public:
        DIRECT_MAPPED(uint32_t associativity = 1)
        {
            assert(associativity == 1);
            _tag = -1;
            _ASID = -1;
            _PPN = NULL;
            _valid = 0;
        }

        void SetAssociativity(uint32_t associativity) { assert(associativity == 1); }
        uint32_t GetAssociativity(uint32_t associativity) { return 1; }

        uint32_t Find_And_Access(TLB_TAG tag, int32_t ASID, uint64_t &paddr)
        {
            if (_valid == 1 && _tag == tag && _ASID == ASID)
            {
                paddr = _PPN;
                return true;
            }
            else
            {
                paddr = NULL;
                return false;
            }
        }
        void Replace(TLB_TAG tag, int32_t ASID, uint64_t paddr)
        {
            _tag = tag;
            _ASID = ASID;
            _PPN = paddr;
            _valid = 1;
        }
        void Invalidate(TLB_TAG tag, int32_t ASID)
        {
            if (_valid == 1 && _tag == tag && _ASID == ASID)
                _valid = 0;
        }
        void Flush()
        {
            _tag = 0;
            _ASID = 0;
            _valid = 0;
            _PPN = NULL;
        }
    };

    /*!
     *  @brief Tlb set with round robin replacement
     */
    template <uint32_t MAX_ASSOCIATIVITY = 4>
    class ROUND_ROBIN
    {
    private:
        TLB_TAG _tags[MAX_ASSOCIATIVITY];
        uint32_t _tagsLastIndex;
        uint32_t _nextReplaceIndex;
        int32_t _ASID[MAX_ASSOCIATIVITY];
        uint64_t _PPN[MAX_ASSOCIATIVITY];
        int32_t _valid[MAX_ASSOCIATIVITY];

    public:
        ROUND_ROBIN(uint32_t associativity = MAX_ASSOCIATIVITY)
            : _tagsLastIndex(associativity - 1)
        {
            assert(associativity <= MAX_ASSOCIATIVITY);
            _nextReplaceIndex = _tagsLastIndex;

            for (int32_t index = _tagsLastIndex; index >= 0; index--)
            {
                _tags[index] = TLB_TAG(0);
                _ASID[index] = 0;
                _PPN[index] = NULL;
                _valid[index] = 0;
            }
        }

        void SetAssociativity(uint32_t associativity)
        {
            assert(associativity <= MAX_ASSOCIATIVITY);
            _tagsLastIndex = associativity - 1;
            _nextReplaceIndex = _tagsLastIndex;
        }
        uint32_t GetAssociativity(uint32_t associativity) { return _tagsLastIndex + 1; }

        uint32_t Find_and_Access(TLB_TAG tag, int32_t ASID, uint64_t &paddr)
        {
            // std::cout<<tag<<" "<<ASID<<" "<<paddr;

            bool result = true;

            for (int32_t index = _tagsLastIndex; index >= 0; index--)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                if (_tags[index] == tag && _ASID[index] == ASID && _valid[index] == 1)
                {
                    paddr = _PPN[index];
                    goto end;
                }
            }
            result = false;
            paddr = NULL;

        end:
            return result;
        }

        void Replace(TLB_TAG tag, int32_t ASID, uint64_t paddr, uint64_t &victim_paddr, uint64_t victim_tag)
        {
            // g++ -O3 too dumb to do CSE on following lines?!
            const uint32_t index = _nextReplaceIndex;

            victim_paddr=_PPN[index];
            victim_tag=_tags[index];

            _tags[index] = tag;
            _ASID[index] = ASID;
            _PPN[index] = paddr;
            _valid[index] = 1;
            // condition typically faster than modulo
            _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
        }

        void Invalidate(TLB_TAG tag, int32_t ASID)
        {
            bool result = true;

            for (int32_t index = _tagsLastIndex; index >= 0; index--)
            {
                // this is an ugly micro-optimization, but it does cause a
                // tighter assembly loop for ARM that way ...
                if (_tags[index] == tag && _ASID[index] == ASID && _valid[index] == 1)
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
                _PPN[index] = NULL;
                _valid[index] = 0;
            }
            _nextReplaceIndex = _tagsLastIndex;
        }
    };

} // namespace TLB_SET

namespace TLB_ALLOC
{
    typedef enum
    {
        STORE_ALLOCATE,
        STORE_NO_ALLOCATE
    } STORE_ALLOCATION;
}

/*!
 *  @brief Generic tlb base class; no allocate specialization, no tlb set specialization
 */
class TLB_BASE
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
    TLB_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

private:
    // input params
    const std::string _name;
    const uint32_t _tlbSize;
    const uint32_t _lineSize;
    const uint32_t _associativity;
    uint32_t _numberOfFlushes;
    uint32_t _numberOfResets;

    // computed params
    const uint32_t _lineShift;
    const uint32_t _setIndexMask;

    TLB_STATS SumAccess(bool hit) const
    {
        TLB_STATS sum = 0;

        for (uint32_t accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

protected:
    uint32_t NumSets() const { return _setIndexMask + 1; }

public:

    uint32_t get_linesize()
    {
        return _lineShift;
    }
    // constructors/destructors
    TLB_BASE(std::string name, uint32_t tlbSize, uint32_t lineSize, uint32_t associativity);

    // accessors
    uint32_t TlbSize() const { return _tlbSize; }
    uint32_t LineSize() const { return _lineSize; }
    uint32_t Associativity() const { return _associativity; }
    //
    TLB_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true]; }
    TLB_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false]; }
    TLB_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType); }
    TLB_STATS Hits() const { return SumAccess(true); }
    TLB_STATS Misses() const { return SumAccess(false); }
    TLB_STATS Accesses() const { return Hits() + Misses(); }

    TLB_STATS Flushes() const { return _numberOfFlushes; }
    TLB_STATS Resets() const { return _numberOfResets; }

    void SplitAddress(const uint64_t addr, TLB_TAG &tag, uint32_t &setIndex) const
    {
        tag = addr >> _lineShift;
        setIndex = tag & _setIndexMask;
    }

    void SplitAddress(const uint64_t addr, TLB_TAG &tag, uint32_t &setIndex, uint32_t &lineIndex) const
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

    std::ostream &StatsLong(std::ostream &out) const;
};

TLB_BASE::TLB_BASE(std::string name, uint32_t tlbSize, uint32_t lineSize, uint32_t associativity)
    : _name(name),
      _tlbSize(tlbSize),
      _lineSize(lineSize),
      _associativity(associativity),
      _lineShift(FloorLog2(lineSize)),
      _setIndexMask((tlbSize / (associativity * lineSize)) - 1)
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
std::ostream &TLB_BASE::StatsLong(std::ostream &out) const
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
            << StringFlt(100.0 * Misses(accessType) / Accesses(accessType), 2, numberWidth - 1) << "%" << std::endl;
        out << std::endl;
    }

    out << StringString("Total Hits:      ", headerWidth, ' ')
        << StringInt(Hits(), numberWidth) << std::endl;
    out << StringString("Total Misses:    ", headerWidth, ' ')
        << StringInt(Misses(), numberWidth) << std::endl;
    out << StringString("Total Accesses:  ", headerWidth, ' ')
        << StringInt(Accesses(), numberWidth) << std::endl;
    out << StringString("Total Miss Rate: ", headerWidth, ' ')
        << StringFlt(100.0 * Misses() / Accesses(), 2, numberWidth - 1) << "%" << std::endl;

    out << StringString("Flushes:         ", headerWidth, ' ')
        << StringInt(Flushes(), numberWidth) << std::endl;
    out << StringString("Stat Resets:     ", headerWidth, ' ')
        << StringInt(Resets(), numberWidth) << std::endl;

    out << std::endl;

    return out;
}

/// ostream operator for TLB_BASE
std::ostream &operator<<(std::ostream &out, const TLB_BASE &tlbBase)
{
    return tlbBase.StatsLong(out);
}

/*!
 *  @brief Templated tlb class with specific tlb set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of tlb sets.
 */
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
class TLB : public TLB_BASE
{
private:
    SET _sets[MAX_SETS];

public:
    // constructors/destructors
    TLB(std::string name, uint32_t tlbSize, uint32_t lineSize, uint32_t associativity)
        : TLB_BASE(name, tlbSize, lineSize, associativity)
    {
        assert(NumSets() <= MAX_SETS);

        for (uint32_t i = 0; i < NumSets(); i++)
        {
            _sets[i].SetAssociativity(associativity);
        }
    }

    // modifiers
    /// Tlb access at addr
    bool AccessTLB(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID, uint64_t &paddr);
    void ReplaceTLB(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID, uint64_t &paddr, uint64_t &victim_paddr, uint64_t &victim_vaddr);
    void InvalidateTLB(uint64_t addr, int32_t ASID);
    void Flush();
    void ResetStats();
};

/*!
 *  @return true if accessed tlb line hits
 */
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
bool TLB<SET, MAX_SETS, STORE_ALLOCATION>::AccessTLB(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID, uint64_t &paddr)
{
    TLB_TAG tag;
    uint32_t setIndex;

    SplitAddress(addr, tag, setIndex);

    SET &set = _sets[setIndex];

    bool hit = set.Find_and_Access(tag, ASID, paddr);

    // on miss, loads always allocate, stores optionally

    _access[accessType][hit]++;

    return hit;
}
/*!
 *  @return true if accessed tlb line hits
 */
template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void TLB<SET, MAX_SETS, STORE_ALLOCATION>::ReplaceTLB(uint64_t addr, ACCESS_TYPE accessType, int32_t ASID, uint64_t &paddr, uint64_t &victim_paddr, uint64_t &victim_vaddr)
{
    TLB_TAG tag;
    uint32_t setIndex;
    uint64_t victim_tag;

    SplitAddress(addr, tag, setIndex);

    SET &set = _sets[setIndex];

    // bool hit = set.Find(tag,ASID,paddr);

    // on miss, loads always allocate, stores optionally
    if ((accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == TLB_ALLOC::STORE_ALLOCATE))
    {
        set.Replace(tag, ASID, paddr, victim_paddr, victim_tag);
        victim_vaddr=victim_tag<<(get_linesize());
    }

    //_access[accessType][hit]++;

    // return hit;
}

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void TLB<SET, MAX_SETS, STORE_ALLOCATION>::InvalidateTLB(uint64_t addr, int32_t ASID)
{
    TLB_TAG tag;
    uint32_t setIndex;

    SplitAddress(addr, tag, setIndex);

    SET &set = _sets[setIndex];

    // bool hit = set.Find(tag,ASID,paddr);

    // on miss, loads always allocate, stores optionally
    set.Invalidate(tag, ASID);

    //    if ((accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == TLB_ALLOC::STORE_ALLOCATE))
    //    {
    //        set.Replace(tag,ASID,paddr);
    //    }

    //_access[accessType][hit]++;

    // return hit;
}

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void TLB<SET, MAX_SETS, STORE_ALLOCATION>::Flush()
{
    for (int32_t index = NumSets(); index >= 0; index--)
    {
        SET &set = _sets[index];
        set.Flush();
    }
    IncFlushCounter();
}

template <class SET, uint32_t MAX_SETS, uint32_t STORE_ALLOCATION>
void TLB<SET, MAX_SETS, STORE_ALLOCATION>::ResetStats()
{
    for (uint32_t accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
    IncResetCounter();
}

// define shortcuts
#define TLB_DIRECT_MAPPED(MAX_SETS, ALLOCATION) TLB<TLB_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define TLB_ROUND_ROBIN(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) \
    TLB<TLB_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

// TLB Data Structures For 'm' nodes each with 'n' cores
namespace ITLB
{
    // instruction TLB: 4 kB pages, 128 entries, 8-way associative
    const uint32_t lineSize = 4 * KILO;
    const uint32_t tlbSize = 512 * lineSize;
    const uint32_t associativity = 8;
    const TLB_ALLOC::STORE_ALLOCATION allocation = TLB_ALLOC::STORE_ALLOCATE;

    const uint32_t max_sets = tlbSize / (lineSize * associativity);
    const uint32_t max_associativity = associativity;

    typedef TLB_ROUND_ROBIN(max_sets, max_associativity, allocation) TLB;
}
extern ITLB::TLB itlbs[core_count * num_nodes];
#define BOOST_PP_LOCAL_LIMITS (0, ((core_count * num_nodes) - 1))
#define BOOST_PP_LOCAL_MACRO(n) ITLB::TLB("ITLB" #n, ITLB::tlbSize, ITLB::lineSize, ITLB::associativity),
ITLB::TLB itlbs[] =
    {
#include "boost/preprocessor/iteration/detail/local.hpp"
};

namespace DTLB
{
    // data TLB: 4 kB pages, 64 entries, 4-way associative
    const uint32_t lineSize = 4 * KILO;
    const uint32_t tlbSize = 1024 * lineSize;
    const uint32_t associativity = 4;
    const TLB_ALLOC::STORE_ALLOCATION allocation = TLB_ALLOC::STORE_ALLOCATE;

    const uint32_t max_sets = tlbSize / (lineSize * associativity);
    const uint32_t max_associativity = associativity;

    typedef TLB_ROUND_ROBIN(max_sets, max_associativity, allocation) TLB;
}
extern DTLB::TLB dtlbs[core_count * num_nodes];
#define BOOST_PP_LOCAL_LIMITS (0, ((core_count * num_nodes) - 1))
#define BOOST_PP_LOCAL_MACRO(n) DTLB::TLB("DTLB" #n, DTLB::tlbSize, DTLB::lineSize, DTLB::associativity),
DTLB::TLB dtlbs[] =
    {
#include "boost/preprocessor/iteration/detail/local.hpp"
};

#endif // PIN_TLB_H
