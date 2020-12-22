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

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <forward_list>

#include "../t18/t18/utils/spinlock.h"

#include "q2ami_convs.h"

namespace t18 {

	namespace _Q2Ami {

		//this structure contains information about single ticker data (read from the config and from the server)
		class TickerCfgData {
		public:
			typedef ModesVector modesVector_t;
			typedef utils::spinlock dealsLock_t;
			typedef utils::spinlock_guard_ex dealsLock_guard_ex_t;
			typedef utils::spinlock_guard dealsLock_guard_t;

		public:
			const ::std::string tickerName;

			const mxTime removeTimeBefore;
			const mxTime removeTimeInclAfter;

			const modesVector_t modesList;

			const size_t initRawDealsCapacity;

			//////////////////////////////////////////////////////////////////////////
			//The following vars may be accessed from the network thread and therefore MUST have mt-protection
			// Member functions with unsafe_* prefix MUST be executed from under the lock
			// (or some other form of protection must be used)
			
			//#TODO or #NOTE or #BUGBUG - server might update some data stored in proxy::prxyTickerInfo
			// (for example, change lot size for a next session). We need a mechanism to update that info here
			//proxy::prxyTickerInfo pti;//invalid state means no ticker available on the server
									  // provided a subscription request was issued
			extTickerInfo eTI; //!isPtiValid() means that no ticker available on the server (provided a subscription request was issued)
			
			mxTimestamp lastKnownDeal;//non empty value means a subscribe request was already issued for the ticker

			//vector of deals as received from t18qsrv. It's populated at network thread and are used by ticker's modes
			// during execution of GetQuotesEx() in ami's thread.
			::std::vector<proxy::prxyTsDeal> rawDeals;
			mutable dealsLock_t rawDealsLock;//are used to update rawDeals and query rawDeals size. Individual elements of
			//rawDeals MUST also be accessed with syncronization, because though once created they are never modified from different
			//threads, they can be moved during vector extension

			//////////////////////////////////////////////////////////////////////////

		public:
			TickerCfgData(const char* p, mxTime fB, mxTime fA, modesVector_t&& mv, size_t expectedRawDeals)
				: tickerName(p)
				, removeTimeBefore(fB), removeTimeInclAfter(fA)
				, modesList(::std::move(mv))
				, initRawDealsCapacity(expectedRawDeals)
				//, pti(proxy::prxyTickerInfo::createInvalid())
			{
				T18_ASSERT(lastKnownDeal.empty());
				T18_ASSERT(expectedRawDeals > 0);
				//rawDeals.reserve(expectedRawDeals);
			}

			bool unsafe_subscribeWasIssued()const noexcept { return !lastKnownDeal.empty(); }
			//bool unsafe_subscribeWasSuccessfull()const noexcept { return pti.isValid(); }
			bool unsafe_subscribeWasSuccessfull()const noexcept { return eTI.isPtiValid(); }

			bool timeSuits(const mxTime t)const noexcept {
				T18_ASSERT(!t.empty());
				return (removeTimeBefore.empty() || (removeTimeBefore <= t))
					&& (removeTimeInclAfter.empty() || (t < removeTimeInclAfter));
			}

			//expecting it be called when network thread is shutdown, therefore it should run in singlethreaded context
			void onResetConnection() {
				for (const auto& up : modesList) {
					T18_ASSERT(up);
					up->_resetConnection();
				}
				//pti = proxy::prxyTickerInfo::createInvalid();
				eTI.reset();
				lastKnownDeal.clear();
			}
		};

		struct Cfg {
		public:
			typedef convBase convBase_t;
			typedef ModesCreator ModesCreator_t;
			typedef TickerCfgData TickerCfgData_t;
			typedef typename TickerCfgData_t::modesVector_t modesVector_t;

			typedef typename TickerCfgData_t::dealsLock_t dealsLock_t;
			typedef typename TickerCfgData_t::dealsLock_guard_ex_t dealsLock_guard_ex_t;
			typedef typename TickerCfgData_t::dealsLock_guard_t dealsLock_guard_t;

		protected:
			typedef ::std::forward_list<TickerCfgData_t> TickersList_t;

			static constexpr int _defaultExpDailyDealsCount = 1000;

			::std::map<::std::string, TickersList_t> classTickersList;
			::std::string serverIp;
			unsigned _tickersCnt, _totalModesTickersCount;
			::std::uint16_t serverPort{ 0 };

		public:
			const auto& ServerIP()const noexcept { return serverIp; }
			auto ServerPort()const noexcept { return serverPort; }
			size_t tickersCount()const noexcept { return _tickersCnt; }
			size_t tickerModesCount()const noexcept { return _totalModesTickersCount; }

			bool isValid()const noexcept {
				return serverPort > 1000 && !serverIp.empty() && _tickersCnt > 0 && _totalModesTickersCount > 0
					&& !classTickersList.empty() && !classTickersList.begin()->first.empty()
					&& !classTickersList.begin()->second.empty() && !classTickersList.begin()->second.front().tickerName.empty();
			}

			static ::std::string makeCfgFileName(const char* pszPath) {
				T18_ASSERT(pszPath);
				::std::string fpath;
				fpath.reserve(::std::strlen(pszPath) + 16);//strlen not an issue here
				fpath += pszPath;
				fpath += "/cfg.txt";
				return fpath;
			}

		protected:
			static ::std::string _defConfig() {
				char _buf[2048];
				sprintf_s(_buf, "# default config, edit as necessary\n\n"
					"# server's ip&port address:\n"
					"serverIp = 111.222.113.224\n"
					"serverPort = %u\n\n"
					"# specify category of tickers to fetch using classCode as [section name]\n"
					"# On MOEX.com the TQBR code is used for the stock market section and the SPBFUT for the derivatives market\n"
					"# QJSIM is used in a QUIK Junior (QUIK's demo) program to address simulated data for stock market\n"
					"[TQBR]\n\n"
					"# tickers is a comma separated list of tickers codes for the class\n"
					"tickers = GAZP,SBER\n\n"
					"# sessionStart and sessionEnd variables serves as default values for ticker time filters.\n"
					"# Each may be overridden with corresponding <ticker>_sessionStart and <ticker>_sessionEnd.\n"
					"# set to -1 to disable filtering\n"
					"sessionStart = 100000\n"
					"sessionEnd = 184000\n\n"
					"# defModes can be overridden for each ticker with <ticker>_modes\n"
					"defModes = ticks\n\n"
					//"# Individual setting for a mode, that supports options, can be specified using format\n"	//not tested yet, probably even not completely supported yet.
					//"# <ticker>_<mode><i>_<option> (where <i> is an index of the mode in the <tickers>_modes list)\n\n" // so better don't expect anything good from this feature
					"# ExpDailyDealsCount is a daily expected number of deals for a ticker\n"
					"defExpDailyDealsCount = 50000\n\n"
					, static_cast<unsigned>(proxy::defaultServerTcpPort));
				return ::std::string(_buf);
			}

			static mxTime _parseTime(int mlTime)noexcept {
				if (mlTime >= 0) {
					int h, m, s;
					if (milTime::untie_miltime_s(static_cast<time_ult>(mlTime), h, m, s)) {
						return mxTime(h, m, s);
					}
				}
				return mxTime();
			}

		public:
			void clearAll() {
				serverIp.clear();
				serverPort = 0;
				classTickersList.clear();
				_tickersCnt = _totalModesTickersCount = 0;
			}

			void logDealsStorageUseCount(::spdlog::logger& lgr)const noexcept {
				lgr.info("logDealsStorageUseCount {");
				for (const auto& e : classTickersList) {
					for (const auto& td : e.second) {
						dealsLock_guard_ex_t lk(td.rawDealsLock);
						const auto s = td.rawDeals.size();
						const auto cap = td.rawDeals.capacity();
						lk.unlock();

						lgr.info("{}@{} has used {} of {} total tick storage", e.first, td.tickerName, s, cap);
					}
				}
				lgr.info("logDealsStorageUseCount }");
			}

			bool readFromPath(::spdlog::logger& lgr, const char* pszPath) {
				clearAll();

				::std::string fpath{ makeCfgFileName(pszPath) };

				if (!::utils::myFile::exist(fpath.c_str())) {
					lgr.warn("No config file found at {}, creating new one", fpath);
					//creating and reading
					utils::myFile hF(fpath.c_str(), "w");
					auto cfg = _defConfig();
					fwrite(cfg.data(), 1, cfg.length(), hF);
				}

				//file exists, must read and parse it
				INIReader reader(fpath);
				if (reader.ParseError() != 0) {
					lgr.error("Failed to parse cfg '{}', error={}", fpath, reader.ParseError());
					return false;
				}

				serverIp = reader.Get("", "serverIp", "");
				if (serverIp.empty()) {
					lgr.error("Failed to parse serverIp");
					return false;
				}
				lgr.info("serverIp = {}", serverIp);

				auto port = reader.GetInteger("", "serverPort", 0);
				if (port < 1000 || ::std::numeric_limits<decltype(serverPort)>::max() <= port) {
					lgr.error("Invalid port={} specified! Must be in range (1000,2^16)", port);
					return false;
				}
				serverPort = static_cast<decltype(serverPort)>(port);
				lgr.info("serverPort = {}", serverPort);

				ModesCreator_t MCreator;

				const auto& classCodes = reader.Sections();
				for (const auto& ccode : classCodes) {
					if (ccode.length() <= convBase_t::maxStringCodeLen) {
						const int defSessionStart = reader.GetInteger(ccode, "sessionStart", -1);
						const int defSessionEnd = reader.GetInteger(ccode, "sessionEnd", -1);

						const ::std::string defModes = reader.Get(ccode, "defModes", "ticks");

						const int defExpDailyDealsCount = reader.GetInteger(ccode, "defExpDailyDealsCount", _defaultExpDailyDealsCount);

						::std::string tickers = reader.Get(ccode, "tickers", "");
						if (UNLIKELY(tickers.empty())) {
							if (!ccode.empty()) lgr.warn("tickers are empty for classCode={}. skipping...", ccode);
						} else {
							auto it = classTickersList.find(ccode);
							TickersList_t* pList = (classTickersList.end() == it) ? nullptr : &it->second;

							//parsing individual ticker codes from the comma-separated string
							//ugly (prior to c++17, but ok later) trick with const_cast
							char* _ctx;
							const char* pTicker = ::strtok_s(const_cast<char*>(tickers.data()), ",", &_ctx);
							while (pTicker) {
								const auto szTl = ::std::strlen(pTicker);//strlen not an issue here
								if (LIKELY(szTl <= convBase_t::maxStringCodeLen)) {
									::std::string sTicker(pTicker);

									const int tickerSessStart = reader.GetInteger(ccode, sTicker + "_sessionStart", defSessionStart);
									const int tickerSessEnd = reader.GetInteger(ccode, sTicker + "_sessionEnd", defSessionEnd);

									const int tickerExpDailyDealsCount = reader.GetInteger(ccode, sTicker + "_ExpDailyDealsCount", defExpDailyDealsCount);

									//parsing modes and creating corresponding objects
									::std::string tickerModes = reader.Get(ccode, sTicker + "_modes", defModes);
									if (UNLIKELY(tickerModes.empty())) {
										lgr.critical("Empty modes list for ticker {}@{}, skipping ticker", sTicker, ccode);
									} else {
										modesVector_t mv;
										mv.reserve(static_cast<unsigned>(::std::count(tickerModes.begin(), tickerModes.end(), ',')) + 1u);

										char* _mctx;
										const char* pMode = ::strtok_s(const_cast<char*>(tickerModes.data()), ",", &_mctx);
										while (pMode) {
											const auto pCreator = MCreator.find(pMode);
											if (LIKELY(pCreator)) {
												mv.push_back((*pCreator)(lgr, mv.size(), sTicker, ccode, reader));
												if (mv.back()) {
													++_totalModesTickersCount;
												} else {
													lgr.critical("Failed to create mode {} for ticker {}@{}. Skipping", pMode, sTicker, ccode);
													mv.pop_back();
												}
											} else {
												lgr.critical("Failed to find mode {} for ticker {}@{}. Skipping", pMode, sTicker, ccode);
											}
											pMode = ::strtok_s(nullptr, ",", &_mctx);
										}

										if (UNLIKELY(mv.empty())) {
											lgr.critical("Failed to parse modes list for ticker {}@{}, skipping ticker", sTicker, ccode);
										} else {
											if (!pList) {
												auto res = classTickersList.emplace(::std::make_pair(ccode, TickersList_t()));
												pList = &res.first->second;
											}

											pList->emplace_front(pTicker
												, _parseTime(tickerSessStart), _parseTime(tickerSessEnd), ::std::move(mv)
												, static_cast<size_t>(tickerExpDailyDealsCount > 0
													? tickerExpDailyDealsCount : _defaultExpDailyDealsCount)
											);
											++_tickersCnt;
										}
									}
								} else {
									lgr.critical("ticker={} for classCode={} is too long to be correct ({} > {}). Skipping"
										, pTicker, ccode, szTl, convBase_t::maxStringCodeLen);
								}

								pTicker = ::strtok_s(nullptr, ",", &_ctx);
							}
						}
					} else {
						lgr.critical("classCode={} is too long to be correct ({} > {}). Skipping"
							, ccode, ccode.length(), convBase_t::maxStringCodeLen);
					}
				}
				if (!isValid()) {
					lgr.error("Failed to read tickers!");
					return false;
				}

				//log what we've parsed
				if (lgr.level() <= ::spdlog::level::trace) {
					for (const auto& e : classTickersList) {
						::std::string s;
						s.reserve(_tickersCnt*(convBase_t::maxStringCodeLen + 1 + 32) + 1);
						for (const auto& td : e.second) {
							s += td.tickerName;
							s += "(cap="; s += ::std::to_string(td.rawDeals.capacity());
							s += "),";
						}
						lgr.trace("For classCode {} tickers are: {}", e.first, s);
					}
				}

				return true;
			}

			//returns tickers list in format <class-code>(<ticker-1>(?:,<ticker-i>)*)
			::std::string queryTickersList()const noexcept {
				T18_ASSERT(isValid());
				::std::string r;
				r.reserve(tickersCount() * 16);

				for (const auto& me : classTickersList) {
					T18_ASSERT(!me.first.empty() && !me.second.empty());
					r += me.first;
					r += "(";
					bool b = false;
					for (const auto& te : me.second) {
						if (b) {
							r += ",";
						} else b = true;
						T18_ASSERT(!te.tickerName.empty());
						r += te.tickerName;
					}
					r += ")";
				}
				return r;
			}

			//checks whether the passed ticker@class is in config
			//safe to call from multithreading env after config has been read
			TickerCfgData* find(const char*const tickr, const char*const clss) noexcept {
				auto it = classTickersList.find(clss);
				if (it != classTickersList.end()) {
					auto& tlist = it->second;
					for (auto& e : tlist) {
						if (tickr == e.tickerName) return &e;
					}
				}
				return nullptr;
			}
			TickerCfgData* find(const ::std::string& tickr, const ::std::string& clss) noexcept {
				return find(tickr.c_str(), clss.c_str());
			}
		};
	}

}
