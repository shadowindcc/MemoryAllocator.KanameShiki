


namespace KanameShiki {



alignas(csCacheLine) std::atomic_uint64_t gnLocalPool(0);



uint64_t NumLocalPool() noexcept
{
	return gnLocalPool.load(std::memory_order_acquire);
}



LocalPool::~LocalPool() noexcept
{
	assert(mnCache.load(std::memory_order_acquire) == 0);
	
	#if KANAMESHIKI_DEBUG_LEVEL//[
	gnLocalPool.fetch_sub(1, std::memory_order_acq_rel);
	#endif//]
}



LocalPool::LocalPool(LocalCntx* pOwner, uint16_t o)
:mId(std::this_thread::get_id())
,mpOwner(pOwner)
,mo(o)
,mCache{}
,mbCache(true)
,mnCache(cnPoolParcel)
{
	assert(!(to_t(this) & cmCacheLine));
	
	Auto s = (o+1) * csSizeT;
	Auto sParcelT = Parcel::SizeofT();
	Auto sParcelS = Parcel::SizeofS(s);
	
	Auto vParcel = align_t(to_t(this+1) + sParcelT, csAlign) - sParcelT;
	for (auto n = mnCache.load(std::memory_order_acquire); n; --n){
		Auto pParcel = reinterpret_cast<Parcel*>(vParcel);
		mCache.ListST(pParcel);
		vParcel += sParcelS;
	}
	
	#if KANAMESHIKI_DEBUG_LEVEL//[
	gnLocalPool.fetch_add(1, std::memory_order_acq_rel);
	#endif//]
}



std::size_t LocalPool::Size(Parcel* pParcel) const noexcept
{
	return (mo+1) * csSizeT;
}



void LocalPool::Free(Parcel* pParcel) noexcept
{
	if (mbCache.load(std::memory_order_acquire)){
		if (mId == std::this_thread::get_id()){
			mCache.ListST(pParcel); return;
		} else {
			if (mCache.ListMT(pParcel)) return;
		}
	}
	if (DecCache(1) == 0) Delete();
}



void* LocalPool::Alloc() noexcept
{
	auto& rListST = mCache.mListST;
	Auto pParcel = rListST.p;
	if (pParcel){
		rListST.p = pParcel->Alloc(this);
		return pParcel->CastData();
	} else {
		Auto oRevolver = mCache.mRevolverAlloc.o & mCache.RevolverMask();
		pParcel = mCache.maListMT[oRevolver].p.exchange(nullptr, std::memory_order_acq_rel);
		if (pParcel){
			mCache.mRevolverAlloc.o = ++oRevolver;
			pParcel->Alloc(this);
			return pParcel->CastData();
		} else {
			Clearance();
			return mpOwner->NewPool(mo);
		}
	}
}



uint16_t LocalPool::Clearance() noexcept
{
	mbCache.store(false, std::memory_order_release);
	
	Auto nCache = mCache.Clearance();
	return DecCache(nCache);
}



void LocalPool::Delete() noexcept
{
	Auto bSelf = (mId == std::this_thread::get_id());
	
	this->~LocalPool();
	
	if (bSelf){
		LocalCntxPtr()->ReserverFree(this);
	} else {
		GlobalCntxPtr()->ReserverFree(this);
	}
}



void* LocalPool::operator new(std::size_t sThis, uint16_t o, const std::nothrow_t&) noexcept
{
	Auto s = (o+1) * csSizeT;
	Auto sParcelS = Parcel::SizeofS(s);
	
	Auto sBudget = sThis + csAlign + (sParcelS * cnPoolParcel);
	return LocalCntxPtr()->ReserverAlloc(sBudget);
}



uint16_t LocalPool::DecCache(uint16_t nCache) noexcept
{
	return mnCache.fetch_sub(nCache, std::memory_order_acq_rel) - nCache;
}



}
