/*
    This file is a part of Q2Ami project (AmiBroker data-source plugin to fetch
    data from QUIK terminal over the net; requires https://github.com/Arech/t18qsrv)
    Copyright (C) 2019, Arech (aradvert@gmail.com; https://github.com/Arech)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

#include "../t18/t18/base.h"

//#include "Test.h"
#include "Plugin.h"
//#include "resource.h"
//#include "ConfigDlg.h"

namespace t18 {

	typedef ::std::uint16_t modePfxId_t;

	//////////////////////////////////////////////////////////////////////////
	//it must be unsigned or we may get UD with integer overflow
	//typedef ::std::uint64_t dealNumOffs_t;

	namespace _Q2Ami {

		struct extTickerInfo : public proxy::prxyTickerInfo {
		private:
			typedef proxy::prxyTickerInfo base_class_t;

		public:
			// to feed deal number into AmiBroker successfully we had to fit uint64 into 32-bit float somehow.
			// 32-bit float is able to represent whole numbers in range [-16777216 16777216] exactly, so we're 
			// going to subtract (the first deal number + 16777216) from every subsequent ticker deal number to
			// obtain a value that will fit into the range.
			//static constexpr dealnum_t _dealnum2floatOffset = 16777216;
			// 
			//static constexpr dealNumOffs_t _dealnum2floatOffset = 16777216;
			static constexpr dealnum_t _dealnum2floatOffset = 16700000;
			//for some unknown reason, evening MOEX SPBFUT session starts with wrong/unexpected orderId's that eventually
			// may underflow lower -16777216 limit, so making more tight offset.

			static constexpr dealnum_t _dealnum2floatOffsetLims = 16777216;

		protected:
			//#TODO or #NOTE or #BUGBUG - we'd better reset this value every day
			dealnum_t dealNumOffset{ 0 };
			bool bDealNumOffsetSpecified{ false };
			//these 2 vars can only be updated once (per day once update will be implemented) from the network thread.

		public:
			extTickerInfo()noexcept : base_class_t() {
				reset();
			}

			void setDealNumOffset(dealnum_t dn)noexcept {
				bDealNumOffsetSpecified = true;
				//dealNumOffset = static_cast<dealNumOffs_t>(dn) +_dealnum2floatOffset;
				dealNumOffset = dn;
			}
			dealnum_t getDealNumOffset()const noexcept {
				T18_ASSERT(bDealNumOffsetSpecified);
				return dealNumOffset;
			}
			bool isDealNumOffsetSpecified()const noexcept { return bDealNumOffsetSpecified; }

			bool isValid()const noexcept {
				return base_class_t::isValid() && isDealNumOffsetSpecified();
			}
			bool isPtiValid()const noexcept { return base_class_t::isValid(); }

			void setPti(const base_class_t*const p)noexcept {
				T18_ASSERT(p->isValid());
				T18_ASSERT(!isDealNumOffsetSpecified() || !"setPti() should be called only once after creation/reset()");
				*static_cast<base_class_t*>(this) = *p;
			}

			void reset()noexcept {
				base_class_t::reset();
				bDealNumOffsetSpecified = false;
			}
		};

	}

	//////////////////////////////////////////////////////////////////////////
	//for convenience these Ami-related functions are defined in the base namespace. Though probably it's better to move the into _Q2Ami namespace

	inline mxTimestamp AmiDate2Timestamp(const AmiDate ad)noexcept {
		const auto& pd = ad.PackDate;
		return mxTimestamp(pd.Year, pd.Month, pd.Day, pd.Hour, pd.Minute, pd.Second, pd.MilliSec * 1000 + pd.MicroSec);
	}

	inline void timestamp2AmiDate(AmiDate& ad, const mxTimestamp s)noexcept {
		PackedDate* pd = &ad.PackDate;

		pd->Year = static_cast<unsigned>(s.Year());
		pd->Month = static_cast<unsigned>(s.Month());
		pd->Day = static_cast<unsigned>(s.Day());
		pd->Hour = static_cast<unsigned>(s.Hour());
		pd->Minute = static_cast<unsigned>(s.Minute());
		pd->Second = static_cast<unsigned>(s.Second());
		const int mks = s.Microsecond();
		const int ms = mks / 1000;
		pd->MilliSec = static_cast<unsigned>(ms);
		pd->MicroSec = static_cast<unsigned>(mks - ms * 1000);

		//ad.PackDate = pd;
	}

	inline AmiDate timestamp2AmiDate(const mxTimestamp s)noexcept {
		AmiDate ad;
		timestamp2AmiDate(ad, s);
		return ad;
	}

	inline void prxyTsDeal2Quotation(Quotation& q, const proxy::volume_lots_t volLotSize
		, mxTimestamp correctTs, const proxy::prxyTsDeal& tsd, const dealnum_t /*dealNumOffset*/ = 0) noexcept
	{
		if (correctTs.empty()) correctTs = tsd.ts;
		timestamp2AmiDate(q.DateTime, correctTs);

		const auto pr = static_cast<decltype(q.Price)>(tsd.pr);
		q.Price = pr;
		q.Open = pr;
		q.High = pr;
		q.Low = pr;

		const bool bLong = static_cast<bool>(tsd.bLong);
		const auto vl = static_cast<decltype(q.Volume)>(tsd.volLots*volLotSize);
		static_assert(::std::is_same<decltype(q.Volume), decltype(q.AuxData1)>::value, "");
				
		//Total volume to V, buy volume in Aux1, sell volume to Aux2.
		// Note, that we can not just use V & Aux1 in Ami to calculate sell volume with (V-Aux1), because in some modes
		// Ami may overwrite V with it's own values. That's totally idiotic, but that's how it works now and there's nothing we can do with it.
		// So use V, A1 and A2 for volume
		q.Volume = vl;
		q.AuxData1 = bLong ? vl : 0;
		q.AuxData2 = bLong ? 0 : vl;

		
		/*
		 * The following code is fine, but it will use Aux2 that's reserved for Sell volume
		T18_ASSERT(tsd.dealNum >= dealNumOffset);
		typedef ::std::make_signed_t<dealnum_t> dealNumOffs_t;
		const dealNumOffs_t newDn = static_cast<dealNumOffs_t>(tsd.dealNum - dealNumOffset) - static_cast<dealNumOffs_t>(_Q2Ami::extTickerInfo::_dealnum2floatOffset);

		T18_ASSERT(dealNumOffset == 0 || (
			newDn >= -(static_cast<dealNumOffs_t>(_Q2Ami::extTickerInfo::_dealnum2floatOffsetLims))
			&& newDn <= static_cast<dealNumOffs_t>(_Q2Ami::extTickerInfo::_dealnum2floatOffsetLims)));
		q.AuxData2 = static_cast<decltype(q.AuxData2)>(newDn);*/

		q.OpenInterest = 0;
	}

}

