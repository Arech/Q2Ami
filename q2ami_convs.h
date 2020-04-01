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

T18_COMP_SILENCE_OLD_STYLE_CAST;
T18_COMP_SILENCE_DROP_CONST_QUAL;
T18_COMP_SILENCE_DEPRECATED;
T18_COMP_SILENCE_ZERO_AS_NULLPTR;
#include "../_extern/inih/INIReader.h"
T18_COMP_POP; T18_COMP_POP; T18_COMP_POP; T18_COMP_POP;

T18_COMP_SILENCE_COVERED_SWITCH;
T18_COMP_SILENCE_OLD_STYLE_CAST;
T18_COMP_SILENCE_SIGN_CONVERSION;
#include "../_extern/readerwriterqueue/readerwriterqueue.h"
T18_COMP_POP; T18_COMP_POP; T18_COMP_POP;

#include "../t18/t18/utils/myFile.h"

#include "q2ami_supl.h"

namespace t18 {
	namespace _Q2Ami {

		class TickerCfgData; //fwd declaration

		//everything that process default ticks (as well as returns non processed ticks) MUST be derived from this class
		//Objects of any convBase derived class are used from within a single ami-spawned thread.
		struct convBase {
		public:
			static constexpr size_t maxStringCodeLen = 15;

			static constexpr size_t maxPfxIdStringLen = 3;
			static_assert(sizeof(modePfxId_t) == 1, "update maxPfxIdStringLen");

		protected:
			//we need the following two timestamps to make sure no two ticks are ever emitted with the same timestamps
			mxTimestamp m_prevTs;//last (possibly incremented) written timestamp
			//mxTimestamp m_lastRealTs;//timestamp of the real last seen deal

		public:
			size_t nextDealToProcess{ 0 };//for external use only

			const ::std::string amiName;
			const char*const amiPfx;
			const modePfxId_t pfxId; //index of the object in ticker's modes vector 

			//////////////////////////////////////////////////////////////////////////
		protected:
			convBase(::spdlog::logger& lgr, ::std::string&& an, const char* pfx, modePfxId_t i)
				: amiName(::std::move(an)), amiPfx(pfx), pfxId(i)
			{
				if (UNLIKELY(::std::strlen(pfx) > maxStringCodeLen || amiName.length() > (3 * maxStringCodeLen + 2))) {//strlen not an issue here
					throw ::std::logic_error("Invalid string code length");
				}
				lgr.info("Converter {} for amiTicker {} created", amiPfx, amiName);
			}

		public:
			virtual ~convBase() {};

			//must return new nLastValid
			virtual int processDeal(const proxy::prxyTsDeal& /*tsd*/, const proxy::prxyTickerInfo& /*pti*/
				, Quotation*const /*pQuotes*/, int /*nLastValid*/, const int /*nSize*/)
			{
				return -1;
			}

		protected:
			friend TickerCfgData;

			//expecting it be called when network thread is shutdown
			virtual void _resetConnection() {
				nextDealToProcess = 0;
			};

		public:
			// pQ might be null when no history available (t in that case ==0, but not empty)
			// Note that timestamp t may be set to a later time, that is pointed by pQ->DateTime
			virtual void setPrevQuot(mxTimestamp t, const Quotation* /*pQ*/)noexcept {
				T18_ASSERT(!t.empty());
				m_prevTs = t;
				//m_lastRealTs = t;
			}

			//default implementation does nothing
			/*virtual int dropPossiblyUnfinished(const Quotation*const pQuotes, const int nSize, int nLastValid) {
				T18_UNREF(pQuotes); T18_UNREF(nSize);
				return nLastValid;
			}*/
			//the dropPossiblyUnfinished() makes no sence for using within Ami, because to we must run this function
			//for each ticker/quotes array known to ami, that is based on a single real ticker data, to find out real
			// common last quote. But we can't query these arrays from within GetQuotesEx() plugin function. Therefore
			// we have no choice but to query all known history for the real ticker (it'll suit for each converter/Ami ticker)
			// and this function is redundant.

		protected:
			mxTimestamp _makeUniqueTs(mxTimestamp curTs)noexcept {
				/*T18_ASSERT(curTs >= m_lastRealTs);
				const auto origCurTS = curTs;
				if (curTs == m_lastRealTs || curTs <= m_prevTs) {
					curTs = m_prevTs.next();
				};
				m_lastRealTs = origCurTS;
				m_prevTs = curTs;*/

				if (curTs <= m_prevTs) {
					curTs = m_prevTs.next();
				};
				m_prevTs = curTs;
				return curTs;
			}

			template<typename T>
			static ::std::unique_ptr<convBase> _defFromCfg(::spdlog::logger& lgr, const size_t idx, const char*const sPfx
				, const ::std::string& tickerCode, const ::std::string& classCode)
			{
				if (idx > ::std::numeric_limits<modePfxId_t>::max()) {
					throw ::std::runtime_error("Too large mode index passed");
				}
				const auto i = static_cast<modePfxId_t>(idx);
				return ::std::make_unique<T>(lgr, makeAmiName(sPfx, i, tickerCode, classCode), sPfx, i);
			}

		public:
			static ::std::string makeAmiName(const char*const pfx, modePfxId_t i, const ::std::string& tickerCode, const ::std::string& classCode) {
				if (UNLIKELY(::std::strlen(pfx) > maxStringCodeLen || classCode.length() > maxStringCodeLen//strlen not an issue here
					|| tickerCode.length() > maxStringCodeLen))
				{
					throw ::std::logic_error("Invalid string code length");
				}
				::std::string r;
				r.reserve(3 * maxStringCodeLen + 3);
				r += pfx; r += "|"; r += ::std::to_string(static_cast<size_t>(i));
				r += "@"; r += tickerCode; r += "@"; r += classCode;
				return r;
			}

			template<size_t _pbs, size_t _tbs, size_t _cbs>
			static bool parseAmiName(::spdlog::logger& lgr, const char*const pszTicker
				, OUT char(&pPfx)[_pbs], OUT modePfxId_t& pfxId, OUT char(&pTicker)[_tbs], OUT char(&pClass)[_cbs])
			{
				const auto pPrePfxIdSep = ::std::strchr(pszTicker, '|');
				const auto _pfxL = static_cast<size_t>(pPrePfxIdSep - pszTicker);
				if (UNLIKELY(!pPrePfxIdSep || !_pfxL || _pfxL >= _pbs || _pfxL > maxStringCodeLen)) {
					lgr.critical("WTF? Invalid tickerclass code passed={}, wrong prefix code len={}", pszTicker, _pfxL);
				} else {
					const auto pPreTickerCodeSep = ::std::strchr(pPrePfxIdSep, '@');
					const auto _pfxIdLen = static_cast<size_t>(pPreTickerCodeSep - pPrePfxIdSep - 1);
					if (UNLIKELY(!pPreTickerCodeSep || !_pfxIdLen || _pfxIdLen > maxPfxIdStringLen)) {
						lgr.critical("WTF? Invalid tickerclass code passed={}, wrong prefix id len={}", pszTicker, _pfxIdLen);
					} else {
						const auto pTickerCode = pPreTickerCodeSep + 1;
						const auto pPreClassCodeSep = ::std::strchr(pTickerCode, '@');
						const auto _tl = static_cast<size_t>(pPreClassCodeSep - pTickerCode);
						if (UNLIKELY(!pPreClassCodeSep || !_tl || _tl >= _tbs || _tl > maxStringCodeLen)) {
							lgr.critical("WTF? Invalid tickerclass code passed={}, wrong ticker code len={}", pszTicker, _tl);
						} else {
							const auto _cl = ::std::strlen(pPreClassCodeSep) - 1u;//strlen not an issue here
							if (UNLIKELY(!_cl || _cl >= _cbs || _cl > maxStringCodeLen)) {
								lgr.critical("WTF? Invalid tickerclass code passed={}, wrong class code len={}", pszTicker, _cl);
							} else {
								const auto pPfxId = pPrePfxIdSep + 1;
								int iv = ::std::atoi(pPfxId);
								if (UNLIKELY(::strncpy_s(pPfx, pszTicker, _pfxL)
									|| iv < 0 || (0 == iv && '0' != *pPfxId) || iv > ::std::numeric_limits<modePfxId_t>::max()
									|| ::strncpy_s(pTicker, pTickerCode, _tl) || ::strncpy_s(pClass, pPreClassCodeSep + 1, _cl)))
								{
									lgr.critical("WTF? failed to copy tickerclass code={} to dest vars", pszTicker);
								} else {
									pfxId = static_cast<modePfxId_t>(iv);
									return true;
								}
							}
						}
					}
				}
				return false;
			}
		};

		namespace modes {
			//derive your own converter in a similar way

			struct ticks : public convBase {
				typedef convBase base_class_t;
				inline static constexpr char sPfx[] = "ticks";

				//////////////////////////////////////////////////////////////////////////

				ticks(::spdlog::logger& lgr, ::std::string&& an, const char* pfx, modePfxId_t i)
					: base_class_t(lgr, ::std::move(an), pfx, i)
				{}

				//return empty ::std::unique_ptr in case of non severe failure
				static ::std::unique_ptr<convBase> fromCfg(::spdlog::logger& lgr, const size_t idx
					, const ::std::string& tickerCode, const ::std::string& classCode, const INIReader&)
				{
					return base_class_t::_defFromCfg<ticks>(lgr, idx, sPfx, tickerCode, classCode);
				}


				//always called from network thread
				//must return nLastValid
				virtual int processDeal(const proxy::prxyTsDeal& tsd, const proxy::prxyTickerInfo& pti
					, Quotation*const pQuotes, int nLastValid, const int nSize) override
				{
					T18_ASSERT(nLastValid >= -1 && nLastValid < nSize);
					T18_UNREF(nSize);

					prxyTsDeal2Quotation(pQuotes[++nLastValid], pti.lotSize, base_class_t::_makeUniqueTs(tsd.ts), tsd);
					return nLastValid;
				}

				/*virtual void _resetConnection() override {
					//#todo 
				}*/
			};
		}

#if T18_HAS_INCLUDE("../t18+/Q2Ami/exp_convs.h")
	}
}
#include "../t18+/Q2Ami/exp_convs.h"
namespace t18 {
	namespace _Q2Ami {
#else
		struct ModesCreator {
			typedef ::std::function<::std::unique_ptr<convBase>(::spdlog::logger& lgr, const size_t
				, const ::std::string&, const ::std::string&, const INIReader&)> modeCreatorFunc_t;

			struct modeDescr {
				const modeCreatorFunc_t fCreate;
				const ::std::string pfx;

				modeDescr(modeCreatorFunc_t&& mcf, const char* p) :fCreate(::std::move(mcf)), pfx(p) {}
			};

			::std::vector<modeDescr> m_fList;

			ModesCreator() {
				using namespace modes;

				m_fList.reserve(1);
				m_fList.emplace_back(ticks::fromCfg, ticks::sPfx);
			}

			const modeCreatorFunc_t* find(const ::std::string& mode)noexcept {
				for (const auto& e : m_fList) {
					if (mode == e.pfx) return &e.fCreate;
				}
				return nullptr;
			}
		};
#endif

		struct ModesVector : public ::std::vector<::std::unique_ptr<convBase>> {
		private:
			typedef ::std::vector<::std::unique_ptr<convBase>> base_class_t;

		public:
			template<typename...Args>
			ModesVector(Args...a) : base_class_t(::std::forward<Args>(a)...) {}

			convBase* findMode(const char*const pPfx)const noexcept {
				for (auto& uPtr : *this) {
					auto ptr = uPtr.get();
					if (ptr->amiPfx == pPfx) return ptr;
				}
				return nullptr;
			}

			convBase* findMode(modePfxId_t _i, const char*const pPfx)const noexcept {
				const auto idx = static_cast<size_t>(_i);
				if (this->size() > idx) {
					T18_ASSERT(this->operator[](idx));
					convBase*const ret = this->operator[](idx).get();
					T18_ASSERT(ret);
					T18_UNREF(pPfx);
					T18_ASSERT(0 == ::std::strcmp(ret->amiPfx, pPfx));
					return ret;
				}
				return nullptr;
			}
		};

	}
}
