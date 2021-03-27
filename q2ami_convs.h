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

		//////////////////////////////////////////////////////////////////////////
		//everything that process default ticks (as well as returns non processed ticks) MUST be derived from this class
		//Objects of any convBase derived class are used from within a single ami-spawned thread.
		// See Q2Ami::_doGetQuotes() implementation for use, some details and restictions to converters
		struct convBase {
		protected:
			//we need the following two timestamps to make sure no two ticks are ever emitted with the same timestamps
			mxTimestamp m_prevTs;//last (possibly incremented) written timestamp
			//mxTimestamp m_lastRealTs;//timestamp of the real last seen deal

		public:
			size_t nextDealToProcess{ 0 };//for external use only

			const ::std::string amiName;//full ticker name in Ami. Don't change it
			const char*const modeName; //note that this field is usually just a "mirror" of inline constexpr sModeName field defined in
			// implementation class. To have access to converter name (i.e. modeName) in base class we need either this field, or
			// make virtual get() function in derived, which is way slower

			//////////////////////////////////////////////////////////////////////////
		protected:
			convBase(::spdlog::logger& lgr, ::std::string&& an, const char* _modeName) : amiName(::std::move(an)), modeName(_modeName)
			{
				lgr.info("Converter {} for amiTicker {} created", modeName, amiName);
			}

		public:
			virtual ~convBase() {};

			//return 0 for completely processed tsd or number of bars required in array to finish processing.
			// If the latter, it'll be called again
			virtual int processDeal(const proxy::prxyTsDeal& /*tsd*/, const extTickerInfo & /*eTI*/
				, Quotation*const /*pQuotes*/, IN OUT int& nLastValid, const int /*nSize*/)
			{
				nLastValid = -1;
				return 0;
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
			//the dropPossiblyUnfinished() makes no sense for using within Ami, because to we must run this function
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
			static ::std::unique_ptr<convBase> _defFromCfg(::spdlog::logger& lgr, ::std::string&& amiTickerPfx) {
				return ::std::make_unique<T>(lgr, ::std::move(amiTickerPfx));
			}
		};

		namespace modes {
			//derive your own converter in a similar way
			//Place it to ../t18+/Q2Ami/exp_convs.h and redefine type Conv_Modes_t (see below) to include your list of converters

			struct ticks : public convBase {
				typedef convBase base_class_t;
				//redefine to name of your converter, used as a name of the mode in config.ini
				inline static constexpr char sModeName[] = "ticks";

				//////////////////////////////////////////////////////////////////////////

				ticks(::spdlog::logger& lgr, ::std::string&& an)
					: base_class_t(lgr, ::std::move(an), sModeName)
				{}

				//return empty ::std::unique_ptr in case of non severe failure
				static ::std::unique_ptr<convBase> fromCfg(::spdlog::logger& lgr, ::std::string&& amiTickerPfx
					, const ::std::string& tickerCode, const ::std::string& classCode, const INIReader& iniReader)
				{
					//you don't have to use default implementation.
					//Don't change amiTickerPfx string here, pass it to base class. It's used to match symbol in Ami to this very mode
					// converter object
					//note that there's a reference to config ini reader passed, you may use it to read additional parameters for the ticker from ini
					T18_UNREF(tickerCode); T18_UNREF(classCode); T18_UNREF(iniReader);
					return base_class_t::_defFromCfg<ticks>(lgr, ::std::move(amiTickerPfx));
				}


				//always called from network thread
				//return 0 for completely processed tsd or number of bars required in array to finish processing.
				// If the latter, it'll be called again
				virtual int processDeal(const proxy::prxyTsDeal& tsd, const extTickerInfo& eTI
					, Quotation*const pQuotes, IN OUT int& nLastValid, const int nSize) override
				{
					T18_ASSERT(nLastValid >= -1 && nLastValid < nSize);
					T18_UNREF(nSize);

					prxyTsDeal2Quotation(pQuotes[++nLastValid], eTI.lotSize, base_class_t::_makeUniqueTs(tsd.ts), tsd, eTI.getDealNumOffset());
					return 0;
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
		namespace modes {
			//define in a similar way a tuple type with your own converters in ../t18+/Q2Ami/exp_convs.h
			typedef decltype(hana::tuple_t<ticks>) Conv_Modes_t;
		}
#endif

		//ModesCreator aggregates all knowledge how to spawn any modes objects, listed in Conv_Modes_t tuple
		//basically it just assembles a proxy functions that calls modes's fromCfg() function to spawn object
		class ModesCreator {
		public:
			typedef ::std::function<::std::unique_ptr<convBase>(::spdlog::logger& lgr, ::std::string&&
				, const ::std::string&, const ::std::string&, const INIReader&)> modeCreatorFunc_t;

		protected:
			struct modeDescr {
				const modeCreatorFunc_t fCreate;
				const char*const modeName;

				modeDescr(modeCreatorFunc_t&& mcf, const char* _modeName) :fCreate(::std::move(mcf)), modeName(_modeName) {}
			};

			::std::vector<modeDescr> m_fList;

		public:
			ModesCreator() {
				using namespace modes;
				
				m_fList.reserve( hana::value(hana::length(modes::Conv_Modes_t())) );
				hana::for_each(modes::Conv_Modes_t(), [&fList = m_fList](auto&& modeT)noexcept {
					typedef typename ::std::decay_t<decltype(modeT)>::type mode_t;
					fList.emplace_back(mode_t::fromCfg, mode_t::sModeName);
				});
			}

			const modeCreatorFunc_t* find(const ::std::string& mode)noexcept {
				for (const auto& e : m_fList) {
					if (mode == e.modeName) return &e.fCreate;
				}
				return nullptr;
			}
		};

		//ModesVector keeps all conversion modes for a ticker in one place
		struct ModesVector : public ::std::vector<::std::unique_ptr<convBase>> {
		private:
			typedef ::std::vector<::std::unique_ptr<convBase>> base_class_t;

		public:
			template<typename...Args>
			ModesVector(Args...a) : base_class_t(::std::forward<Args>(a)...) {}

			convBase* findMode(const char*const pModeName)const noexcept {
				for (auto& uPtr : *this) {
					auto ptr = uPtr.get();
					if (ptr->modeName == pModeName) return ptr;
				}
				return nullptr;
			}

			//pModeName is optional here and used just to validate all is ok in debug build if passed
			template<typename modePfxIdT>
			convBase* findMode(modePfxIdT _i, const char*const pModeName=nullptr)const noexcept {
				T18_ASSERT(_i >= 0);
				if constexpr(::std::is_signed<::std::decay_t<modePfxIdT>>::value) {
					if (UNLIKELY(_i < 0)) return nullptr;
				}

				const auto idx = static_cast<size_t>(_i);
				if (this->size() > idx) {
					T18_ASSERT(this->operator[](idx));
					convBase*const ret = this->operator[](idx).get();
					T18_ASSERT(ret);					
					T18_UNREF(pModeName);
					T18_ASSERT(!pModeName || 0 == ::std::strcmp(ret->modeName, pModeName));
					return ret;
				}
				return nullptr;
			}
		};

	}
}
