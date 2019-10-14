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

	typedef ::std::uint8_t modePfxId_t;

	inline mxTimestamp AmiDate2Timestamp(AmiDate ad)noexcept {
		const auto pd = ad.PackDate;
		return mxTimestamp(pd.Year, pd.Month, pd.Day, pd.Hour, pd.Minute, pd.Second, pd.MilliSec * 1000 + pd.MicroSec);
	}

	inline void timestamp2AmiDate(AmiDate& ad, const mxTimestamp s)noexcept {
		PackedDate pd;

		pd.Year = static_cast<unsigned>(s.Year());
		pd.Month = static_cast<unsigned>(s.Month());
		pd.Day = static_cast<unsigned>(s.Day());
		pd.Hour = static_cast<unsigned>(s.Hour());
		pd.Minute = static_cast<unsigned>(s.Minute());
		pd.Second = static_cast<unsigned>(s.Second());
		const int mks = s.Microsecond();
		const int ms = mks / 1000;
		pd.MilliSec = static_cast<unsigned>(ms);
		pd.MicroSec = static_cast<unsigned>(mks - ms * 1000);

		ad.PackDate = pd;
	}

	inline AmiDate timestamp2AmiDate(const mxTimestamp s)noexcept {
		AmiDate ad;
		timestamp2AmiDate(ad, s);
		return ad;
	}

	inline void prxyTsDeal2Quotation(Quotation& q, mxTimestamp correctTs, const proxy::prxyTsDeal& tsd) noexcept {
		if (correctTs.empty()) correctTs = tsd.ts;
		timestamp2AmiDate(q.DateTime, correctTs);

		const auto pr = static_cast<decltype(q.Price)>(tsd.pr);
		q.Price = pr;
		q.Open = pr;
		q.High = pr;
		q.Low = pr;

		const auto vl = static_cast<decltype(q.Volume)>(tsd.volLots);
		q.Volume = vl;
		q.OpenInterest = 0;

		const bool bLong = static_cast<bool>(tsd.bLong);
		q.AuxData1 = bLong ? vl : 0;
		q.AuxData2 = bLong ? 0 : vl;
	}

}

