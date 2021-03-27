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

//we need mutex&condition_variable to synchronize network thread with the main thread
#include <mutex>
#include <condition_variable>

//////////////////////////////////////////////////////////////////////////
//#define SPDLOG_NO_THREAD_ID
#define SPDLOG_NO_NAME
//#define SPDLOG_WCHAR_FILENAMES
//#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#define SPDLOG_DISABLE_DEFAULT_LOGGER

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
//////////////////////////////////////////////////////////////////////////

#include "../t18/t18/proxy/client.h"
#include "../t18/t18/utils/atomic_flags_set.h"

#include "q2ami_cfg.h"

namespace t18 {

	class Q2Ami {
		typedef Q2Ami self_t;

	protected:
		enum class State {
			NotInitialized
			, Err_configLoad
			, Connecting
			, Err_ConnectionFailed
			, Connected
			, QuikServerDisconnected
			//, Timeout
			, SomeRequestFailed
		};
		static const char* stateName(State s)noexcept {
			const char* r;
			switch (s) {
			case State::NotInitialized:
				r = "NotInitialized";
				break;
			case State::Err_configLoad:
				r = "Err_configLoad";
				break;
			case State::Connecting:
				r = "Connecting";
				break;
			case State::Err_ConnectionFailed:
				r = "Err_ConnectionFailed";
				break;
			case State::Connected:
				r = "Connected";
				break;
			case State::QuikServerDisconnected:
				r = "QuikServerDisconnected";
				break;
			/*case State::Timeout:
				r = "Timeout";
				break;*/
			case State::SomeRequestFailed:
				r = "SomeRequestFailed";
				break;
			}
			return r;
		}

		struct TickerInfo {
			::t18::proxy::prxyTickerInfo pti;
			const ::std::string tickerCode;
			const ::std::string classCode;

			TickerInfo() {}

			TickerInfo(const ::t18::proxy::prxyTickerInfo* ppti, const char* tickr, const char* clc)
				: pti(ppti ? *ppti : ::t18::proxy::prxyTickerInfo::createInvalid()), tickerCode(tickr), classCode(clc)
			{}
			
			bool empty()const noexcept {
				return tickerCode.empty();
			}
		};

		//static constexpr size_t allTradesQueueInitialCapacity = 512;

		typedef _Q2Ami::Cfg Cfg_t;
		typedef typename Cfg_t::TickerCfgData_t TickerCfgData_t;
		typedef typename Cfg_t::convBase_t convBase_t;

		typedef typename Cfg_t::dealsLock_t dealsLock_t;
		typedef typename Cfg_t::dealsLock_guard_ex_t dealsLock_guard_ex_t;
		typedef typename Cfg_t::dealsLock_guard_t dealsLock_guard_t;

		//typedef typename Cfg_t::modesVector_t modesVector_t;

		/*struct TickerRTInfo : public TickerInfo {
			::moodycamel::ReaderWriterQueue<proxy::prxyTsDeal> allTradesQueue;//unprocessed queue
			mxTimestamp lastKnownDeal;

			const TickerCfgData_t*const pTickerCfg;

			const ::std::string amiTicksTickerCode;//empty if no ticks requred
			const ::std::string amiMOTickerCode;//empty if no MO requred
			
		protected:
			//RecentInfo ri;

			// TickerRTInfo possible state:
			// The structure is created during handling of GetQuotesEx for the first time when no other structure
			// describing ticker&class code exists. Once it is created, the lastKnownDeal is set to non-empty value.
			// prxyTickerInfo thought is !valid() at this point.
			// Once the server returns information about the ticker, the prxyTickerInfo is either populated
			// (when ticker exists) or the lastKnownDeal is set to empty value to mark that no such ticker exists
			// on the server (to prevent subsequent attemps to subscribe/querty info).
			// If tickerCode.empty(), then the whole structure is invalid and acts as a placeholder
			// (could be used to distinguish between filled and not filled slots in a vector, indexed by tickerId)


			//////////////////////////////////////////////////////////////////////////
		public:
			//TickerRTInfo() {}

			TickerRTInfo(const char* tickr, const char* clc, mxTimestamp _ts, const TickerCfgData_t*const pTC)
				: TickerInfo(nullptr, tickr, clc), allTradesQueue(allTradesQueueInitialCapacity)
				, lastKnownDeal(_ts)
				, pTickerCfg(pTC)
				//, amiTicksTickerCode(bMakeTicks ? tickerCode + "@" + classCode : ::std::string(nullptr))
				//, amiMOTickerCode(bMakeMO ? ::std::string(amiTickerMOPfx) + amiTicksTickerCode : ::std::string(nullptr))
			{
				//resetRI();
			}

			/ *RecentInfo* resetRI()noexcept {
				::std::memset(&ri, 0, sizeof(ri));
				ri.nStructSize = sizeof(ri);
				ri.nStatus |= RI_STATUS_UPDATE | RI_STATUS_INCOMPLETE | RI_STATUS_BARSREADY | RI_STATUS_TRADE;
				::strncpy_s(ri.Name, amiTickerCode.c_str(), amiTickerCode.length());
				return &ri;
			}* /
		};*/

	protected:
		typedef ::spdlog::sinks::msvc_sink_mt outds_sink_t;

		typedef proxy::QCliWThread<self_t> qcli_t;

		typedef ::std::mutex network2ami_sync_t;
		typedef ::std::unique_lock<network2ami_sync_t> network2ami_lock_t;

		typedef utils::spinlock spinlock_t;
		typedef utils::spinlock_guard spinlock_guard_t;
		typedef utils::spinlock_guard_ex spinlock_guard_ex_t;

		//////////////////////////////////////////////////////////////////////////
		typedef ::std::uint32_t flags_t;
		typedef ::utils::atomic_flags_set<flags_t> safe_flags_t;

		static constexpr flags_t _flagsQ2Ami_ConfigureInProcess = 1;
		static constexpr flags_t _flagsQ2Ami_Running = (_flagsQ2Ami_ConfigureInProcess << 1);
		static constexpr flags_t _flagsQ2Ami_CheckTheLog = (_flagsQ2Ami_Running << 1);
		static constexpr flags_t _flagsQ2Ami_NeverDidGetQuotes = (_flagsQ2Ami_CheckTheLog << 1);

		static constexpr flags_t _flagsQ2Ami__lastUsedBit = _flagsQ2Ami_NeverDidGetQuotes;
		//////////////////////////////////////////////////////////////////////////

	public:
		//////////////////////////////////////////////////////////////////////////
		//required by QCli
		static constexpr long timeoutConnectMs = 10000;
		static constexpr long timeoutReadMs = 60000;
		static constexpr long timeoutWriteMs = 60000;
		static constexpr long heartbeatPeriodMs = 28000;
		//////////////////////////////////////////////////////////////////////////

		static constexpr auto log_default_level = ::spdlog::level::trace;
		static constexpr auto file_log_level = ::spdlog::level::debug;
		static constexpr auto log_immediate_flush_on_level = log_default_level;

		//////////////////////////////////////////////////////////////////////////

		static constexpr long timeout_Configure2queryTickerInfo_ms = 10000;

		//when quotes array is full, is is shifted by uShiftQuotesArrayOffset elements.
		static constexpr int uShiftQuotesArrayOffset = 3000;
		//static constexpr int uShiftQuotesArrayOffset = 2850;

		//////////////////////////////////////////////////////////////////////////
		
	protected:
		Cfg_t m_config;
		::std::unique_ptr<::spdlog::logger> m_Log;

		T18_COMP_SILENCE_ZERO_AS_NULLPTR;
		HWND m_hAmiBrokerWnd{ NULL };
		T18_COMP_POP;

		::std::unique_ptr<qcli_t> m_pCli;

		// for network thread - ami thread synchronization
		network2ami_sync_t m_syncMtx;
		::std::condition_variable m_syncCV;

		safe_flags_t m_flags;//thread safe
		
		State m_state{ State::NotInitialized };
				
		spinlock_t m_spinlock;//we need something to protect access to modifiable parts of m_config. It's very
							  //unlikely that two treads will use it at the same time, but still possible,
							  // so small and fast spinlock seems the best choice.

		static_assert(sizeof(proxy::prxyTsDeal::tid) == 1, "Expecting tid to have a range of [0,255] here.");
		::std::array<TickerCfgData_t*, 256> m_ptrs2TickerCfgData;
		// m_ptrs2TickerCfgData is a different thing. It's indexed by tickerid, assigned by the server, 
		// and therefore it's preallocated from the start.
		// its members points to config members. m_ptrs2TickerCfgData can only be used/updated from the network thread. 

		//must always be empty except for the duration of configure(), so no deinitialization on db unloading required
		::std::vector<TickerInfo> m_queryTickerInfo;
		
		::std::vector<TickerCfgData_t*> m_rti4Update;

		//////////////////////////////////////////////////////////////////////////
	public:
		~Q2Ami() {
			_shutdownCli();

			::spdlog::drop_all();
			m_Log.reset();
		}
		Q2Ami() {
			_cleanPtrs2TickerCfgData();
			_init_outds_logger();
		}

	protected:
		void _shutdownCli() {
			m_Log->info("Performing client shutdown from state '{}'", stateName(m_state));
			m_state = State::NotInitialized;
			m_flags.clear<_flagsQ2Ami_Running | _flagsQ2Ami_CheckTheLog>();
			m_pCli.reset();

			{
				spinlock_guard_t g(m_spinlock);
				_cleanPtrs2TickerCfgData();
			}
			m_rti4Update.clear();

			m_config.logDealsStorageUseCount(*m_Log.get());
			m_config.clearAll();

			T18_COMP_SILENCE_ZERO_AS_NULLPTR;
			m_hAmiBrokerWnd = NULL;//must be cleared at the very end of cleanup procedure
			T18_COMP_POP;
		}

		bool _loadCfgAndConnect(const char*const pszDatabasePath) {
			auto r = m_config.readFromPath(*m_Log.get(), pszDatabasePath);
			if (r) {
				m_Log->info("Config of DB '{}' has been loaded", pszDatabasePath);

				m_rti4Update.reserve(m_config.tickersCount());

				m_flags.set<_flagsQ2Ami_Running | _flagsQ2Ami_NeverDidGetQuotes>();

				m_state = State::Connecting;
				m_pCli = ::std::make_unique<qcli_t>(*this, m_config.ServerIP().c_str(), m_config.ServerPort());
			} else {
				//an error must already be logged
				m_state = State::Err_configLoad;
			}
			return r;
		}

		bool _onDbLoad(PluginNotification *const pn) {
			T18_ASSERT(pn);
			if (_isDbLoaded()) {
				m_Log->info("DB {} load requested while prev DB was not unloaded. Performing unload first", pn->pszDatabasePath);
				_onDbUnload(pn);
			}

			_init_full_logger(pn->pszDatabasePath);
			const auto r = _loadCfgAndConnect(pn->pszDatabasePath);
			if (r) m_hAmiBrokerWnd = pn->hMainWnd;
			return r;
		}

		bool _onDbUnload(PluginNotification *const pn) {
			T18_ASSERT(pn);
			T18_ASSERT(_isDbLoaded());

			m_Log->info("DB '{}' unloading, but will use this log until new DB is loaded", pn->pszDatabasePath);

			_shutdownCli();
			return true;
		}

		bool _isDbLoaded()const noexcept {
			const auto r = static_cast<bool>(m_hAmiBrokerWnd);
			T18_ASSERT(!(r^static_cast<bool>(m_pCli)));
			return r;
		}
		
		void _cleanPtrs2TickerCfgData()noexcept {
			::std::fill(m_ptrs2TickerCfgData.begin(), m_ptrs2TickerCfgData.end(), nullptr);
		}

		void _init_outds_logger() {
			//log initialization
			try {
				// Create basic file logger (not rotated)
				auto outds_logger = ::std::make_shared<outds_sink_t>();
				outds_logger->set_level(log_default_level);
				m_Log = ::std::make_unique<::spdlog::logger>("my", outds_logger);
				m_Log->set_level(log_default_level);

				m_Log->debug("outds initialized");

				//::MessageBoxA(NULL, "Init","Capt", MB_OK);
			} catch (const ::spdlog::spdlog_ex& ex) {
				::OutputDebugStringA("Failed to initialize outds logger, exception data:");
				::OutputDebugStringA(ex.what());
			}
		}

		void _init_full_logger(const char*const pszDbPath) {
			T18_ASSERT(pszDbPath);
			if(m_Log) m_Log->trace("Trying to create file logger for path '{}'", pszDbPath);

			try {
				::std::shared_ptr<::spdlog::sinks::sink> outds_logger;
				if (m_Log && m_Log->sinks().size()) {
					outds_logger = m_Log->sinks().front();
				} else {
					outds_logger = ::std::make_shared<outds_sink_t>();
					outds_logger->set_level(log_default_level);
				}
				
				::std::string fpath;
				fpath.reserve(::std::strlen(pszDbPath) + 12); //strlen not an issue here
				fpath += pszDbPath;
				fpath += "/logs.txt";
				auto file_sink = ::std::make_shared<::spdlog::sinks::rotating_file_sink_mt>(fpath.c_str(), 1024*64, 3);
				file_sink->set_level(file_log_level);

				m_Log = ::std::make_unique<::spdlog::logger>("my", ::spdlog::sinks_init_list{ outds_logger , file_sink });
				m_Log->set_level(log_default_level);
				m_Log->flush_on(log_default_level);

				m_Log->debug("Complete logging initialized!");
			} catch (const ::spdlog::spdlog_ex& ex) {
				::OutputDebugStringA("Failed to initialize file logger, exception data:");
				::OutputDebugStringA(ex.what());

				_init_outds_logger();
			}
		}

	public:
		//QCli interface functions
		auto& getLog()noexcept { return *m_Log.get(); }

		void hndQuikConnectionState(const bool bConnected) {
			if (m_state != State::Connected) {
				if (bConnected) m_Log->info("QUIK's connection to broker's server was restored. Prev state was '{}'", stateName(m_state));
			} else if (m_state != State::QuikServerDisconnected){
				if (!bConnected) m_Log->warn("QUIK has lost connection to broker's server! Prev state was '{}'", stateName(m_state));
			}
			m_state = bConnected ? State::Connected : State::QuikServerDisconnected;
		}

		void hndConnectionState(const bool bConnected) {
			if (bConnected) {
				m_Log->info("Connected to t18qsrv from state '{}'", stateName(m_state));
				m_state = State::Connected;
			} else {
				if (State::NotInitialized != m_state) {
					m_Log->critical("Connection to t18qsrv failed from state '{}'!", stateName(m_state));
					m_state = State::Err_ConnectionFailed;
				}
			}
		}
		void hndRequestFailed(const char*const pStr){
			//#todo
			m_state = State::SomeRequestFailed;//#WARNING will soon be overriden in hndQuikConnectionState(),
			// but for now it's better than nothing
			m_Log->error("Some request failed: {}", pStr);
		}

	public:
		int Ami_HandleNotify(PluginNotification* const pn) {
			T18_ASSERT(pn);
			int r = 1;
			if (pn->nReason & REASON_DATABASE_LOADED) {
				r = _onDbLoad(pn);
			}else if (pn->nReason & REASON_DATABASE_UNLOADED) {
				r = _onDbUnload(pn);
			} /*else if (pn->nReason & REASON_STATUS_RMBCLICK) {
			}*/
			return r;
		}
		static constexpr int Ami_symLimit()noexcept { return 16; }

		//////////////////////////////////////////////////////////////////////////
	protected:
		void _notifyAmi(const TickerCfgData_t*const pTCD)const noexcept {
			for (const auto& up : pTCD->modesList) {
				T18_ASSERT(up);
				::PostMessage(m_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, reinterpret_cast<WPARAM>(up->amiName.c_str()), 0);
			}

			//::PostMessage(m_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE
			//	, reinterpret_cast<WPARAM>(bMO ? pTI->amiMOTickerCode.c_str() : pTI->amiTicksTickerCode.c_str()), 0);

				//, , reinterpret_cast<LPARAM>(pTI->resetRI()));

			//SendMessage( g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, (WPRAM) Ticker, (LPARAM) &recentInfoStructureForGivenTicker );
			//  Ticker is a string (char *) and the other parameter is RecentInfo struct documented in ADK.
			/*
			 *https://forum.amibroker.com/t/chart-is-not-updating-in-getquotesex/9500/4
			 You must send it the way it is shown above, with proper arguments in WPARAM and LPARAM fields. Also RecentInfo structure (which must be separate for every symbol) must be populated properly as shown in a number of examples in the ADK. Study QT sample plugin. It has all the code already there. Copy-paste. It is that easy. You need to fill nDateUpdate, nTimeUpdate, nStatus fields and all the others. Especially nStatus is of paramount importance. On new trade you must specify at minimum RI_STATUS_UPDATE | RI_STATUS_TRADE | RI_STATUS_BARSREADY You have to fill it properly as shown in sample codes. The sample is working so just copy it.

			 // excerpt from Plugin.cpp in QT sample code:
			 ri->nDateChange = 10000 * year + 100 * month + day;
			 ri->nTimeChange = 10000 * hour + 100 * minute + second;

			 if( ri->fLast != fOldLast || ri->fAsk != fOldAsk || ri->fBid != fOldBid )
			 {
			 ri->nDateUpdate = ri->nDateChange;
			 ri->nTimeUpdate = ri->nTimeChange;
			 }

			 ri->nBitmap = RI_LAST | ( ri->fOpen ? RI_OPEN : 0 ) | ( ri->fHigh ? RI_HIGHLOW : 0 ) | RI_TRADEVOL | RI_52WEEK |
			 RI_TOTALVOL | RI_PREVCHANGE | RI_BID | RI_ASK | RI_DATEUPDATE | RI_DATECHANGE;
			 // STATUS FIELD MUST BE CORRECT !
			 ri->nStatus = RI_STATUS_UPDATE | RI_STATUS_BIDASK | RI_STATUS_TRADE | RI_STATUS_BARSREADY;

			 /////////////////////////////
			 your plugin needs to implement flagging symbol as “incomplete” while the backfill is NOT completed.
			To mark symbol as incomplete you need to set this flag: RI_STATUS_INCOMPLETE in the nStatus field of RecentInfo structure.
			struct RecentInfo *ri = GetRecentInfoForSymbol();
			ri->nStatus |= RI_STATUS_INCOMPLETE; // set the flag, backfill is NOT complete
			....
			ri->nStatus &= ~RI_STATUS_INCOMPLETE; // clear the flag, backfill is complete

			 **/
		}

		

		//#TODO 
		int _doGetQuotes(TickerCfgData_t* pTCD, convBase_t*const pModeConv, int nLastValid, const int nSize, Quotation*const pQuotes){
			T18_ASSERT(nLastValid < nSize && nLastValid >= -1);

			if (UNLIKELY(uShiftQuotesArrayOffset >= nSize)) {
				m_Log->critical("Too small quotes array size={}. Make it (much) more than {} for optimal performance. Aborting"
					, nSize, uShiftQuotesArrayOffset);
				m_flags.set<_flagsQ2Ami_CheckTheLog>();
				return nLastValid + 1;
			}
			
			size_t curDealsCnt;
			const auto& dealsVec = pTCD->rawDeals;
			auto nextDealIdx = pModeConv->nextDealToProcess;
			T18_ASSERT(pTCD->eTI.isValid());
			const auto& eTI = pTCD->eTI;

			T18_DEBUG_ONLY(size_t _capac);
			dealsLock_guard_ex_t dlg(pTCD->rawDealsLock);
			curDealsCnt = dealsVec.size();
			T18_DEBUG_ONLY(_capac = dealsVec.capacity());
			dlg.unlock();
			T18_ASSERT(_capac > 0 || !"WTF? Deals vector must already be initialized!");

			if (LIKELY(nextDealIdx < curDealsCnt)) {
				proxy::prxyTsDeal tsd;

				T18_DEBUG_ONLY(mxTimestamp prevTs);
				T18_DEBUG_ONLY(int prevNLV{ nLastValid });
				T18_DEBUG_ONLY(bool bFirst{ true });
				T18_DEBUG_ONLY(bool bJustMoved{ false });
				const auto maxLastValid = nSize - 1;

				while (nextDealIdx < curDealsCnt) {
					dlg.lock();//unfortunately, we MUST acquire lock because in case vector::push_back(), called in parallel thread,
							   //change the vector capacity, all iterators/pointers will be invalidated.
					tsd = dealsVec[nextDealIdx++];
					dlg.unlock();

					//checking if we are to shift ami's array
					if (UNLIKELY(nLastValid >= maxLastValid)) {
						//we have to shift quotes array uShiftQuotesArrayOffset elements back
						T18_ASSERT(nLastValid == maxLastValid);
						nLastValid -= uShiftQuotesArrayOffset;
						T18_DEBUG_ONLY(prevNLV -= uShiftQuotesArrayOffset);
						::std::memmove(pQuotes, &pQuotes[uShiftQuotesArrayOffset], sizeof(*pQuotes)*static_cast<unsigned>(nLastValid + 1));
						T18_DEBUG_ONLY(bJustMoved = true);
					} else T18_DEBUG_ONLY(bJustMoved = false);

					//for the debug build we must make sure the timestamps are sequential
				#ifdef T18_DEBUG
					//timestamps MUST differ from bar to bar. convBase::processDeal() MUST enqueue quotes with different
					// timestamps, because we can't make them different here (prevTs at first is initialized from already
					// updated version, written to quotes in previous attempt.
					Quotation& curQ = pQuotes[nLastValid];//just an address calculation, so don't care about nLastValid
					if (UNLIKELY(bFirst)) {
						bFirst = false;
						if (UNLIKELY(nLastValid < 0)) {
							prevTs = mxTimestamp(tag_mxTimestamp());
						} else {
							prevTs = AmiDate2Timestamp(curQ.DateTime);
						}
						prevNLV = nLastValid;
						m_Log->trace("_doGetQuotes {} (first) nLastValid={}, prevTs={}, orig nextDealIdx={}"
							, pModeConv->amiName, nLastValid, prevTs.to_string(), nextDealIdx - 1);
					} else if (prevNLV != nLastValid) {
						T18_ASSERT(nLastValid >= 0 && nLastValid < nSize);
						auto curTs = AmiDate2Timestamp(curQ.DateTime);

						//m_Log->trace("_doGetQuotes {} (next) nLastValid={}, curTs={}, orig nextDealIdx={}"
						//	, pModeConv->amiName, pTCD->tickerName, nLastValid, curTs.to_string(), nextDealIdx - 1);

						if (curTs <= prevTs) {
							char _buf[1024];
							sprintf_s(_buf, "_doGetQuotes - Invalid time ticker=%s. bJustMoved=%d, nLastValid=%d, prevNLV=%d, curTs=%s, prevTs=%s"
								, pModeConv->amiName.c_str(), bJustMoved ? 1 : 0, nLastValid, prevNLV, curTs.to_string().c_str(), prevTs.to_string().c_str());

							m_Log->critical(_buf);
							m_flags.set<_flagsQ2Ami_CheckTheLog>();
							//T18_ASSERT(!"_doGetQuotes - Invalid time!");
							T18_COMP_SILENCE_ZERO_AS_NULLPTR;
							::MessageBox(NULL, _buf, "_doGetQuotes - Invalid time!", MB_OK | MB_ICONERROR);
							T18_COMP_POP;
						}
						prevNLV = nLastValid;
						prevTs = curTs;
					}
				#endif

					//processing the deal.
					//nLastValid = pModeConv->processDeal(tsd, eTI, pQuotes, nLastValid, nSize);
					// If converter requires more available slots than allowed by nLastValid & nSize, it should update as much as it can
					// changing nLastValid value and then return number of required free slots to finish parsing tsd. It'll be called again.
					int convRet;
					T18_DEBUG_ONLY(auto dbg_old_nLastValid = nLastValid);
					while ( UNLIKELY((convRet = pModeConv->processDeal(tsd, eTI, pQuotes, nLastValid, nSize)) > 0) ) {
						T18_ASSERT(dbg_old_nLastValid <= nLastValid && nLastValid <= nSize);
						const auto shiftOffs = ::std::min(convRet + uShiftQuotesArrayOffset, nLastValid );
						nLastValid -= shiftOffs;
						T18_DEBUG_ONLY(prevNLV -= shiftOffs);
						::std::memmove(pQuotes, &pQuotes[shiftOffs], sizeof(*pQuotes)*static_cast<unsigned>(nLastValid + 1));
					}
					T18_ASSERT(dbg_old_nLastValid <= nLastValid && nLastValid <= nSize);

					dlg.lock();
					curDealsCnt = dealsVec.size();
					dlg.unlock();
				}
				pModeConv->nextDealToProcess = nextDealIdx;
			}
			return nLastValid + 1;
		}
	public:

		void hndAllTrades(const ::t18::proxy::prxyTsDeal* pTrades, const size_t cnt) {
			if (!m_flags.isSet<_flagsQ2Ami_Running>()) return;

		#ifdef T18_DEBUG
			//m_Log->trace("Got {} AllTrades packets", cnt);
		#endif

			T18_ASSERT(m_rti4Update.empty());

			for (size_t i = 0; i < cnt; ++i) {
				const auto& tsd = pTrades[i];

				auto pTCD = m_ptrs2TickerCfgData[tsd.tid];
				//there should never be a race condition accessing m_ptrs2TickerCfgData, because it's modification
				// from the main thread happens only in _shutdownCli(), but either the Running flag
				// was cleared and we were never able to entered here, or the function worked until the end and the network thread
				// was finished during _shutdownCli(), and m_ptrs2TickerCfgData is still valid.

				if (LIKELY(pTCD)) {
					T18_ASSERT(pTCD->rawDeals.capacity() > 0);//seems to be fine here without using syncronization
					
					//checking the time of the deal 
					if (pTCD->timeSuits(tsd.ts.Time())) {

						if (!pTCD->eTI.isDealNumOffsetSpecified()) {
							//we MUST set deal number offset based on the first - i.e. current deal
							pTCD->eTI.setDealNumOffset(tsd.dealNum);
						}

						{
							//we MUST acquire lock before accessing, especially for modification rawDeals vector
							dealsLock_guard_t dlg(pTCD->rawDealsLock);
							pTCD->rawDeals.push_back(tsd);
						}
						if (!::std::any_of(m_rti4Update.begin(), m_rti4Update.end(), [pTCD](const auto v) {return v == pTCD; })) {
							m_rti4Update.emplace_back(pTCD);
						}
					}//else skipping
				} else {
					//this actually should never happen
					m_Log->critical("WTF? Got allTrades message for tid={} we know nothing about!", tsd.tid);
				}
			}

			//finally we must inform Amibroker that there's some new data
			for (const auto v : m_rti4Update) {

			/*#ifdef T18_DEBUG
				size_t rds;
				{
					dealsLock_guard_t dlg(v->rawDealsLock);
					rds = v->rawDeals.size();
				}
				for (auto& conv : v->modesList) {
					m_Log->trace("{} size {}, mode {} processed {}", v->tickerName, rds, conv->amiName, conv->nextDealToProcess);
				}
			#endif
*/

				_notifyAmi(v);
			}
			m_rti4Update.clear();
		}

		//called from network thread
		void hndSubscribeAllTradesResult(const ::t18::proxy::prxyTickerInfo* const pPTI
			, const char*const pTickerName, const char*const pClassName)
		{
			if (!m_flags.isSet<_flagsQ2Ami_Running>()) return;

			auto pCfgInfo = m_config.find(pTickerName, pClassName);
			if (LIKELY(pCfgInfo)) {

				//we MUST hold lock, while accessing mt-related members
				spinlock_guard_ex_t lk(m_spinlock);
				const bool bSubsIssued = pCfgInfo->unsafe_subscribeWasIssued();
				const bool bSubsOk = pCfgInfo->unsafe_subscribeWasSuccessfull();
				//lk.unlock(); //now there's no code that may modify pti/eTI in other thread, but better not make it possible entirely

				if (LIKELY(bSubsIssued)) {
					if (LIKELY(pPTI)) {
						//OK, saving the obtained data and pointer for fast access
						T18_ASSERT(pPTI->isValid());

						//#TODO or #NOTE or #BUGBUG - server might update some data stored in proxy::prxyTickerInfo pti/eTI variable
						// (for example, change lot size for a next session). We need a mechanism to update that info here

						pCfgInfo->eTI.setPti(pPTI);
						T18_ASSERT(pPTI->tid < m_ptrs2TickerCfgData.size());
						T18_ASSERT(!m_ptrs2TickerCfgData[pPTI->tid] || m_ptrs2TickerCfgData[pPTI->tid] == pCfgInfo);						
						m_ptrs2TickerCfgData[pPTI->tid] = pCfgInfo;
						lk.unlock();

						//querying rawDeals state under protection (should not be necessary, however, just for a case)
						dealsLock_guard_ex_t dlg(pCfgInfo->rawDealsLock);
						const auto s = pCfgInfo->rawDeals.size();
						const auto cap = pCfgInfo->rawDeals.capacity();
						dlg.unlock();

						T18_ASSERT(cap > 0);

						m_Log->info("Ticker {}@{} subscribed with tid={}. Raw deals capacity={}, already used={}", pTickerName, pClassName
							, pPTI->tid, cap, s);
					} else {
						//no such ticker on the server. Changing the "flag"
						//pCfgInfo->pti = proxy::prxyTickerInfo::createInvalid();
						pCfgInfo->eTI.reset();
						lk.unlock();

						//and freeing rawDeals memory.
						{
							dealsLock_guard_t dlg(pCfgInfo->rawDealsLock);
							pCfgInfo->rawDeals.clear();
							pCfgInfo->rawDeals.shrink_to_fit();
						}

						m_Log->critical("hndSubscribeAllTradesResult: no such ticker {}@{} on the server!", pTickerName, pClassName);
						m_flags.set<_flagsQ2Ami_CheckTheLog>();
					}
				} else {
					lk.unlock();

					//freeing rawDeals memory
					{
						dealsLock_guard_t dlg(pCfgInfo->rawDealsLock);
						pCfgInfo->rawDeals.clear();
						pCfgInfo->rawDeals.shrink_to_fit();
					}

					m_Log->critical("hndSubscribeAllTradesResult: WTF? never issued subscription request for {}@{}"
						, pTickerName, pClassName);
					m_flags.set<_flagsQ2Ami_CheckTheLog>();
				}
				if (UNLIKELY(bSubsOk)) {
					m_Log->warn("hndSubscribeAllTradesResult: WTF? already subscribed to {}@{}. Continuing.."
						, pTickerName, pClassName);
				}
			} else {
				//no such ticker in the list, so just issue warning and do nothing else
				m_Log->critical("hndSubscribeAllTradesResult: unknown ticker {}@{}", pTickerName, pClassName);
				m_flags.set<_flagsQ2Ami_CheckTheLog>();
			}
		}

		int Ami_GetQuotesEx(const char*const pszTicker, const int nPeriodicity, int nLastValid
			, const int nSize, Quotation *pQuotes, GQEContext */*pContext*/)
		{
			T18_ASSERT(_isDbLoaded());
			T18_ASSERT(nLastValid < nSize && nLastValid >= -1);
			int ret = nLastValid + 1;

			if (UNLIKELY(m_flags.isSet<_flagsQ2Ami_NeverDidGetQuotes>())) {
				m_flags.clear<_flagsQ2Ami_NeverDidGetQuotes>();
				m_Log->trace("Skipping first call to GetQuotesEx, ticker={}", pszTicker);
				return ret;
			}
			
			if (UNLIKELY(!pszTicker || !Ami_IsTimeBaseOk(nPeriodicity) || !pQuotes || nLastValid >= nSize)) {
				m_Log->warn("Invalid call to GetQuotesEx(btw, per={}), ignoring...", nPeriodicity);
				return ret;
			}
						
			_Q2Ami::convBase* pModeConv;
			const ::std::string* psTicker;
			const ::std::string* psClass;
			auto* pCfgInfo = m_config.findByAmiTicker(*m_Log.get(), pszTicker, &pModeConv, &psTicker, &psClass);
			if (LIKELY(pCfgInfo)) {
				T18_ASSERT(pszTicker && psClass && pModeConv);
				//we MUST hold lock, while accessing mt-related members
				bool bSubsIssued, bSubsOk;
				{
					spinlock_guard_t lk(m_spinlock);
					bSubsIssued = pCfgInfo->unsafe_subscribeWasIssued();
					bSubsOk = pCfgInfo->unsafe_subscribeWasSuccessfull();
				}

				if (LIKELY(bSubsIssued)) {
					//checking if the subscription was successfull
					if (LIKELY(bSubsOk)) {
						//auto*const pModeConv = pCfgInfo->modesList.findMode(modeId, pTickerMode);
						if (LIKELY(pModeConv)) {
							//it's perfectly valid ticker, already subscribed to trades. Going to process new quotes, if we're
							//connected or if there're some unprocessed data left
							bool bLeftUnprocessed;
							const bool bConnected = State::Connected == m_state;
							if (!bConnected) {
								size_t curDealsCnt;
								{
									dealsLock_guard_t dlg(pCfgInfo->rawDealsLock);
									curDealsCnt = pCfgInfo->rawDeals.size();
								}
								bLeftUnprocessed = curDealsCnt > pModeConv->nextDealToProcess;
							} else bLeftUnprocessed = false;

							if (LIKELY(bConnected || bLeftUnprocessed)) {
								//but first we must check if today's data were really removed from quotes array
								if (UNLIKELY(pModeConv->nextDealToProcess == 0 && nLastValid >= 0)) { //for first call only	
									auto lkd = AmiDate2Timestamp(pQuotes[nLastValid].DateTime);
									if (lkd.Date() >= mxDate::now()) {//doing it for current day only
										lkd.set_StartOfDay();

										const auto curNLV = nLastValid;
										const Quotation* pLQ;
										//also we MUST shift nLastValid to previous day's last quote
										do {
											pLQ = &pQuotes[--nLastValid];//not a real memory access, just computation of an address, therefore is safe
										} while (nLastValid >= 0 && (lkd < AmiDate2Timestamp(pLQ->DateTime)));

										m_Log->debug("Before first call to _doGetQuotes({}) had to shrink array from {} to {} ({} elements, {} is the last)"
											, pszTicker, curNLV, nLastValid, (curNLV - nLastValid)
											, nLastValid >= 0 ? AmiDate2Timestamp(pLQ->DateTime).to_string() : lkd.to_string());
									}
								}

								ret = _doGetQuotes(pCfgInfo, pModeConv, nLastValid, nSize, pQuotes);
							} else {
								m_Log->warn("Server disconnected, can't serve _doGetQuotes for {}", pszTicker);
							}							
						}
					} else {
						//nothing can be done here.
					}
				} else {
					//we must issue subscription order here if we're connected to the server
					if (State::Connected != m_state) {
						m_Log->warn("Failing subscription in GetQuotesEx() for {}, because of disconnected state", pszTicker);
					} else {
						//doing subscription
						//first we must find the latest known timestamp
						mxTimestamp lkd;
						const Quotation* pLQ;
						if (nLastValid < 0) {
							pLQ = nullptr;
							lkd = mxTimestamp(tag_mxTimestamp());
						} else {
							pLQ = &pQuotes[nLastValid];
							lkd = AmiDate2Timestamp(pLQ->DateTime);
							//we can't query a last known quote for other ami tickers, that were derived from pTicker@pClass
							//therefore we had to stick to the start of session/day to make sure that the first obtained quote
							// is suitable for each converter.
							if (lkd.Date() >= mxDate::now()) {//doing it for current day only
								lkd.set_StartOfDay();

								const auto curNLV = nLastValid;
								//also we MUST shift nLastValid to previous day's last quote
								do {
									pLQ = &pQuotes[--nLastValid];//not a real memory access, just computation of an address, therefore is safe
								} while (nLastValid >= 0 && (lkd < AmiDate2Timestamp(pLQ->DateTime)));

								if (nLastValid < 0) pLQ = nullptr;

								m_Log->debug("Before subscribing set lastDeal to {} and shrinked array from {} to {} ({} elements, {} is the last)"
									, lkd.to_string(), curNLV, nLastValid, (curNLV - nLastValid)
									, pLQ ? AmiDate2Timestamp(pLQ->DateTime).to_string() : lkd.to_string());

								ret = nLastValid + 1;
							}
						}
						T18_ASSERT(!lkd.empty() && ((nLastValid < 0 && !pLQ) || (lkd > mxTimestamp(tag_mxTimestamp()) && pLQ)));

						m_Log->debug("before subscribeAllTrades, nLastValid={}, lkd={}", nLastValid, lkd.to_string());

						char req[2 * m_config.maxStringCodeLen + 3 + 8 * 2 + 1];
						T18_ASSERT(pszTicker && psClass);
						const int n = m_pCli->makeAllTradesRequest(req, psTicker->c_str(), psClass->c_str(), lkd);
						if (LIKELY(n > 0)) {
							T18_ASSERT(static_cast<int>(::std::strlen(req)) == n);//strlen not an issue here

							//updating Ticker data
							{
								spinlock_guard_t lk(m_spinlock);
								pCfgInfo->lastKnownDeal = lkd;

								for (const auto& up : pCfgInfo->modesList) {
									T18_ASSERT(up);
									up->setPrevQuot(lkd, pLQ);
								}
							}

							//preparing rawDeals
							T18_ASSERT(pCfgInfo->initRawDealsCapacity > 0);
							{
								dealsLock_guard_t dlg(pCfgInfo->rawDealsLock);
								pCfgInfo->rawDeals.reserve(pCfgInfo->initRawDealsCapacity);
							}
							
							m_Log->info("subscribeAllTrades: {}.\nInitial raw deals capacity is {}", req, pCfgInfo->initRawDealsCapacity);

							//making request and asynchronously waiting for the results
							m_pCli->post_packet(proxy::ProtoCli2Srv::subscribeAllTrades, req);
						} else {
							m_Log->critical("Failed to create subscribeAllTrades request for {} @ {}", pszTicker, lkd.to_string());
						}
					}
				}
			}
			return ret;
		}
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		void hndQueryTickerInfoResult(const ::t18::proxy::prxyTickerInfo* const pPTI
			, const char*const pTickerName, const char*const pClassName)
		{
			if (!m_flags.isSet<_flagsQ2Ami_ConfigureInProcess>()) {
				m_Log->warn("WTF? in hndQueryTickerInfoResult while Configure() is not in process!");
				return;
			}

			{
				network2ami_lock_t lk(m_syncMtx);
				T18_ASSERT(m_queryTickerInfo.capacity() > 0);//some sanity checks
				m_queryTickerInfo.emplace_back(pPTI, pTickerName, pClassName);
			}

			m_syncCV.notify_one();
		}
		bool Ami_Configure(const char*const /*pszPath*/, InfoSite*const pSite) {
			T18_ASSERT(pSite);
			if (!pSite || pSite->nStructSize < static_cast<int>(sizeof(InfoSite)) || !pSite->AddStockNew) {
				m_Log->critical("Invalid pSite pointer passed to Ami_Configure");
				return false;
			}
			T18_ASSERT(_isDbLoaded());

			//_flagsQ2Ami_ConfigureInProcess is updated ONLY from this thread (though it may be read from other thread),
			//so we may skip lock acquiring here
			if (m_flags.isSet<_flagsQ2Ami_ConfigureInProcess>()) {
				m_Log->warn("Configure() is in process, second call won't help.");
				T18_COMP_SILENCE_ZERO_AS_NULLPTR;
				::MessageBoxA(NULL, "Configure() is in process, second call won't help.", "Error", MB_OK | MB_ICONWARNING);
				T18_COMP_POP;
				return false;
			}

			if (State::Connected != m_state) {
				T18_COMP_SILENCE_ZERO_AS_NULLPTR;
				::MessageBoxA(NULL, "Please, connect to the server first", "Error", MB_OK | MB_ICONERROR);
				T18_COMP_POP;
				return false;
			}

			// it seems that this function is the only "official&valid" way to obtain struct InfoSite, which
			// provides a means to setup tickers & tickers properties. So we first must issue a request to the t18qsrv
			// to obtain properties of tickers listed in config, and on receiving the answer we must update Amibroker's
			// internal data via pointers store in struct InfoSite
			auto tickrsList = m_config.queryTickersList();
			const size_t tc = m_config.tickersCount();

			if (tc <= 0) {
				m_Log->warn("Config is empty, nothing to configure!");
				T18_COMP_SILENCE_ZERO_AS_NULLPTR;
				::MessageBoxA(NULL, "Config is empty, nothing to configure!", "Error", MB_OK | MB_ICONWARNING);
				T18_COMP_POP;
				return false;
			}

			m_Log->trace("Going to issue synchronous queryTickerInfo using timeout of {}s..."
				, static_cast<double>(timeout_Configure2queryTickerInfo_ms) / 1000);

			m_flags.set<_flagsQ2Ami_ConfigureInProcess>();

			//we will query/update multithread variables, so we must protect from the race condition
			network2ami_lock_t lk(m_syncMtx);
			T18_ASSERT(m_queryTickerInfo.empty());
			m_queryTickerInfo.reserve(tc);
			lk.unlock();
			
			//sending the request
			m_pCli->post_packet(proxy::ProtoCli2Srv::queryTickerInfo, ::std::move(tickrsList));

			//waiting for request completion with timeout
			lk.lock();
			m_syncCV.wait_for(lk, ::std::chrono::milliseconds(timeout_Configure2queryTickerInfo_ms)
				, [&qti = m_queryTickerInfo, tc]()
			{
				return tc == qti.size();
			});
			auto _tc = m_queryTickerInfo.size();
			//releasing the lock here giving the network thread an opportunity to get&fill some delayed data into the m_queryTickerInfo
			lk.unlock();

			if (!_tc) {
				m_flags.clear<_flagsQ2Ami_ConfigureInProcess>();

				lk.lock();
				m_queryTickerInfo.clear();
				m_queryTickerInfo.shrink_to_fit();//non binding, therefore we must use flags, instead of checking vector's capacity
				lk.unlock();

				T18_COMP_SILENCE_ZERO_AS_NULLPTR;
				::MessageBoxA(NULL, "Either timeout happened or server really returned empty data for the request.\n"
					"Check logs on both sides.\n"
					, "Timeout", MB_OK | MB_ICONERROR);
				T18_COMP_POP;
				return false;
			}

			T18_ASSERT(_tc <= tc);
			 
			//examining the results
			::std::string report, tickr;
			report.reserve(m_config.tickerModesCount() * 128);
			//tickr is used for logging only
			tickr.reserve(2* m_config.maxStringCodeLen + 3);

			//TickerInfo ti;//we'll copy here element of m_queryTickerInfo to release the lock AFAP. Actual performance of
			// this function doesn't really matter
			size_t nAdded = 0;
			for (_tc = 0; _tc < tc; ++_tc) {
				lk.lock();
				if (_tc >= m_queryTickerInfo.size()) {
					lk.unlock();
					break;
				}
				auto ti{ m_queryTickerInfo[_tc] };
				lk.unlock();

				tickr.clear();
				tickr += ti.tickerCode; tickr += "@"; tickr += ti.classCode;

				//there's no need to update config because the db will be reloaded after this function ends, therefore const
				const auto pTCfg = m_config.find(ti.tickerCode, ti.classCode);
				if (LIKELY(pTCfg)) {
					//#TODO or #NOTE or #BUGBUG - server might update some data stored in proxy::prxyTickerInfo pti variable
					// (for example, change lot size for a next session). We need a mechanism to update that info here
					if (ti.pti.isValid()) {
						T18_ASSERT(!pTCfg->modesList.empty());

						const auto strTickerProps = ti.pti.to_string();
						
						//walking over each mode and adding corresponding tickers mode as a separate ticker
						for (const auto& up : pTCfg->modesList) {
							T18_ASSERT(up);
							const auto& amiName = up->amiName;

							m_Log->trace("Configuring ticker {}", amiName);
							auto pTickerData = pSite->AddStockNew(amiName.c_str());
							if (LIKELY(pTickerData)) {
								pTickerData->Decimals = ti.pti.precision;
								pTickerData->RoundLotSize = ti.pti.lotSize;
								pTickerData->TickSize = static_cast<decltype(pTickerData->TickSize)>(ti.pti.minStepSize);
								//pTickerData->Flags |= SI_MOREFLAGS_FUTURES;//SI_MOREFLAGS_FUTURES is undefined. Great job!

								m_Log->info("Ticker={} added with props {}", amiName, strTickerProps);
								report += amiName; report += " added: "; report += strTickerProps; report += "\n";

								++nAdded;
							} else {
								m_Log->critical("Failed to add {} to Ami!", amiName);
								report += "Failed to add "; report += amiName; report += " to Ami!\n";
							}
						}
					} else {
						m_Log->critical("Ticker={} was not found on server!", tickr);
						report += "Ticker "; report += tickr;
						report += " not found! Update config file!\n";
					}
				} else {
					m_Log->critical("WTF? Unknown ticker={}. Ignoring it", tickr);
					report += "Got ticker "; report += tickr;
					report += " that is not in the config! Ignored.\n";
				}
			}
			m_flags.clear<_flagsQ2Ami_ConfigureInProcess>();

			lk.lock();
			m_queryTickerInfo.clear();
			m_queryTickerInfo.shrink_to_fit();//non binding, therefore we must use flags, instead of checking vector's capacity
			lk.unlock();

			if (nAdded < m_config.tickerModesCount()) {
				m_Log->warn("Only {} tickers out of {} possible were added during Ami_Configure. " \
					"You may also want to increase timeout_Configure2queryTickerInfo_ms={}"
					, nAdded, m_config.tickerModesCount(), timeout_Configure2queryTickerInfo_ms);
				report += "Only "; report += ::std::to_string(nAdded); report += " tickers out of ";
				report += ::std::to_string(m_config.tickerModesCount());
				report += " possible were added during Ami_Configure. Check the log!\n";
			} else {
				m_Log->info("Successfully done");
				report += "All "; report += ::std::to_string(nAdded); report += " tickers were successfully configured in Ami";
			}

			T18_COMP_SILENCE_ZERO_AS_NULLPTR;
			::MessageBoxA(NULL, report.c_str(), "Configuration report", MB_OK);
			T18_COMP_POP;

			return nAdded>0;
		}
		//////////////////////////////////////////////////////////////////////////

		/*RecentInfo * Ami_GetRecentInfo(const char*const pszTicker) {
			auto pTI = _getTickerRTInfo(pszTicker);
			RecentInfo* r;
			if (pTI) {
				r = pTI->resetRI();
			} else {
				m_Log->warn("Ami_GetRecentInfo: No runtime info for {}", pszTicker);
				r = nullptr;
			}
			return r;
		}*/

		//////////////////////////////////////////////////////////////////////////
		void Ami_GetStatus(PluginStatus *const pStatus) {
			T18_ASSERT(pStatus);
			T18_ASSERT(_isDbLoaded());
			static constexpr int sCodeOK = 0;
			static constexpr int sCodeWARN = 0x10000000;
			static constexpr int sCodeMinorERR = 0x20000000;
			static constexpr int sCodeERROR = 0x30000000;

			static constexpr COLORREF clrCodeOK = RGB(0, 255, 0);
			static constexpr COLORREF clrCodeWARN = RGB(255, 255, 0);
			static constexpr COLORREF clrCodeMinorERR = RGB(255, 0, 0);
			static constexpr COLORREF clrCodeERROR = RGB(192, 0, 192);

			switch (m_state) {
			case State::NotInitialized:
				pStatus->nStatusCode = sCodeWARN;
				pStatus->clrStatusColor = clrCodeWARN;
				strcpy_s(pStatus->szShortMessage, "NotInitialized");
				strcpy_s(pStatus->szLongMessage, "Not initialized, waiting DB to load");
				break;

			case State::Err_configLoad:
				pStatus->nStatusCode = sCodeERROR;
				pStatus->clrStatusColor = clrCodeERROR;
				strcpy_s(pStatus->szShortMessage, "CfgFailed");
				strcpy_s(pStatus->szLongMessage, "Config loading failed. Check the config and log at DB directory");
				break;

			case State::Connecting:
				pStatus->nStatusCode = sCodeWARN;
				pStatus->clrStatusColor = clrCodeWARN;
				strcpy_s(pStatus->szShortMessage, "Connecting...");
				strcpy_s(pStatus->szLongMessage, "Connecting to t18qsrv proxy...");
				break;

			case State::Err_ConnectionFailed:
				pStatus->nStatusCode = sCodeERROR;
				pStatus->clrStatusColor = clrCodeERROR;
				strcpy_s(pStatus->szShortMessage, "ConFailed");
				strcpy_s(pStatus->szLongMessage, "Connection failed. Check the config and log at DB directory");
				break;

			case State::Connected:
				pStatus->nStatusCode = sCodeOK;
				pStatus->clrStatusColor = clrCodeOK;
				strcpy_s(pStatus->szShortMessage, "Ok");
				strcpy_s(pStatus->szLongMessage, "Connection succeeded, working as expected");
				break;

			case State::QuikServerDisconnected:
				pStatus->nStatusCode = sCodeWARN;
				pStatus->clrStatusColor = clrCodeWARN;
				strcpy_s(pStatus->szShortMessage, "QUIKs server is disconnected!");
				strcpy_s(pStatus->szLongMessage, "QUIK is not connected to the broker's server. No data will be updated until connection would have been restored.");
				break;

			case State::SomeRequestFailed:
				pStatus->nStatusCode = sCodeMinorERR;
				pStatus->clrStatusColor = clrCodeMinorERR;
				strcpy_s(pStatus->szShortMessage, "ReqFailed");
				strcpy_s(pStatus->szLongMessage, "Some request to t18qsrv proxy failed. Check the log for details. Can continue to work.");
				break;

			/*case State::Timeout:
				pStatus->nStatusCode = sCodeERROR;
				pStatus->clrStatusColor = clrCodeERROR;
				strcpy_s(pStatus->szShortMessage, "Network timeout");
				strcpy_s(pStatus->szLongMessage, "Some network operation timeout. Check the config and log at DB directory");
				break;*/
			}

			if (m_flags.isSet<_flagsQ2Ami_CheckTheLog>()) {
				pStatus->clrStatusColor /= 3;
				pStatus->clrStatusColor *= 2;
				strcat_s(pStatus->szShortMessage, "!LOG");
				strcat_s(pStatus->szLongMessage, " !SEE LOG!");
			}
		}

		static bool Ami_IsTimeBaseOk(const int nTimeBase)noexcept{
			return 0 == nTimeBase ? 1 : 0;
		}
	};

}