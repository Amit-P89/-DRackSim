#include <bitset>
#include <vector>

class page
{
	
	unsigned long page_vaddr = 0;
  	unsigned long page_paddr = 0;
	
public:
  	bool referenced_bit = 0;
	bool TLB_present_bit = 0;
	bool in_victim_list_bit = 0;
  	page(unsigned long p_vaddr, unsigned long p_paddr)
	{
	  	page_vaddr=p_vaddr;
	  	page_paddr=p_paddr;
  	}

	void set_page_map(unsigned long vaddr, unsigned long paddr)
  	{
    	page_vaddr = vaddr;
		page_paddr = paddr;
	}

	unsigned long get_page_vaddr()
	{
		return page_vaddr;
	}

	unsigned long get_page_base_addr(unsigned long vaddr)
	{
		if(vaddr==page_vaddr)
			return page_paddr;
		else
			return 0L;
	}

	unsigned long get_page_physical_addr()
	{
		return this->page_paddr;
	}

};

class pte
{
	
	std::vector<page> _pte;
	unsigned long pte_vaddr = 0L;

public:
	
	pte(unsigned long vaddr): pte_vaddr(vaddr)
	{
		_pte.reserve(512);
	}

	void add_in_pte(unsigned long vaddr, unsigned long paddr)
	{
		if(_pte.size() > 512)
		return;

		_pte.emplace_back(vaddr, paddr);
	}

	page* access_in_pte(unsigned long vaddr)
	{
		for (page& p: _pte)
		if (p.get_page_vaddr() == vaddr)
			return &p;
  
		return nullptr;
	}

	void set_pte_vaddr(unsigned long vaddr)
	{
		pte_vaddr = vaddr;
	}

	unsigned long get_pte_vaddr()
	{
		return pte_vaddr;
 	}
};

class pmd
{
	
	std::vector<pte> _pmd;
	unsigned long pmd_vaddr = 0L;

public:
	
	pmd(unsigned long vaddr): pmd_vaddr(vaddr)
	{
		_pmd.reserve(512);
	}

	void add_in_pmd(unsigned long vaddr)
	{
		if(_pmd.size() > 512)
		return;

		_pmd.emplace_back(vaddr);
	}

	pte* access_in_pmd(unsigned long vaddr)
	{
		for (pte& p: _pmd)
		if (p.get_pte_vaddr() == vaddr)
			return &p;
  
		return nullptr;
	}

	void set_pmd_vaddr(unsigned long vaddr)
	{
		pmd_vaddr = vaddr;
	}

	unsigned long get_pmd_vaddr()
	{
		return pmd_vaddr;
 	}
};

class pud
{
	
	std::vector<pmd> _pud;
	unsigned long pud_vaddr = 0L;

public:
	
	pud(unsigned long vaddr): pud_vaddr(vaddr)
	{
		_pud.reserve(512);
	}

	void add_in_pud(unsigned long vaddr)
	{
		if(_pud.size() > 512)
		return;

		_pud.emplace_back(vaddr);
	}

	pmd* access_in_pud(unsigned long vaddr)
	{
		for (pmd& p: _pud)
		{
			if (p.get_pmd_vaddr() == vaddr)
			return &p;
		}
		return nullptr;
	}

	void set_pud_vaddr(unsigned long vaddr)
	{
		pud_vaddr = vaddr;
	}

	unsigned long get_pud_vaddr()
	{
		return pud_vaddr;
 	}
};

class pgd
{
	
	std::vector<pud> _pgd;
	unsigned long pgd_vaddr = 0L;

public:

	pgd() = default;
	
	pgd(unsigned long vaddr): pgd_vaddr(vaddr)
	{
		_pgd.reserve(512);
	}

	void add_in_pgd(unsigned long vaddr)
	{
		if(_pgd.size() > 512)
		return;

		_pgd.emplace_back(vaddr);
	}

	pud* access_in_pgd(unsigned long vaddr)
	{
		for (pud& p: _pgd)
		if (p.get_pud_vaddr() == vaddr)
			return &p;
  
		return nullptr;
	}

	void set_pgd_vaddr(unsigned long vaddr)
	{
		pgd_vaddr = vaddr;
	}

	unsigned long get_pgd_vaddr()
	{
		return pgd_vaddr;
 	}
};


unsigned long get_page_addr(unsigned long paddr)
{
	unsigned long page_addr=paddr & (0xfffffffff000);
	page_addr=page_addr>>12;
	return page_addr;
}

void split_vaddr(unsigned long &pgd, unsigned long &pud, unsigned long &pmd, unsigned long &pte, unsigned long &page_offset, unsigned long vaddr)
{
	//cout<<"\nvaddr ="<<bitset<48>(vaddr)<<"\n";
	
	page_offset=vaddr & (0x000000000fff);
	
	pte=vaddr  & (0x0000001ff000);
	pte=pte>>12;	

	pmd=vaddr  & (0x00003fe00000);
	pmd=pmd>>21;
	
	pud=vaddr  & (0x0007fc0000000);
	pud=pud>>30;
	
	pgd=vaddr  & (0xff8000000000);
	pgd=pgd>>39;
	//cout<<"\n"<<(pte)<<"\n";

}


/*int main()
{

	pgd p;
	p.add_in_pgd(1);
	pud *a=p.access_in_pgd(1);
	a->add_in_pud(12);
	pmd *b=a->access_in_pud(12);
	b->add_in_pmd(123);
	pte *c=b->access_in_pmd(123);
	c->add_in_pte(10,20);

	a->add_in_pud(23);
	p.add_in_pgd(2);
	a=p.access_in_pgd(2);
	a->add_in_pud(1234);
	b=a->access_in_pud(1234);
	b->add_in_pmd(456);
	c=b->access_in_pmd(456);
	c->add_in_pte(20,30);
	p.add_in_pgd(3);
	a=p.access_in_pgd(3);
	a->add_in_pud(34);

	b->add_in_pmd(567);
	c->add_in_pte(30,40);

	pgd _pgd[5];	//to accomodate 5-processes, can be dynamically declared as per need
	long int vaddr=0x0000001f1ffe;

	long int a,b,c,d,e;
	int proc_id=1;
	
//	long int paddr;

	split_vaddr(a,b,c,d,e,vaddr);

	return 0;
}*/