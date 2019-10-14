////////////////////////////////////////////////////
// Plugin.cpp
// Standard implementation file for all AmiBroker plug-ins
//
///////////////////////////////////////////////////////////////////////
// Copyright (c) 2001-2009 AmiBroker.com. All rights reserved. 
//
// Users and possessors of this source code are hereby granted a nonexclusive, 
// royalty-free copyright license to use this code in individual and commercial software.
//
// AMIBROKER.COM MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE CODE FOR ANY PURPOSE. 
// IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND. 
// AMIBROKER.COM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOURCE CODE, 
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
// IN NO EVENT SHALL AMIBROKER.COM BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, OR 
// CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.
// 
// Any use of this source code must include the above notice, 
// in the user documentation and internal comments to the code.
///////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "q2ami.h"

// These are the only two lines you need to change
#define PLUGIN_NAME "My QUIK2Ami"
#define VENDOR_NAME "My company name here"
#define PLUGIN_VERSION 10000
#define PLUGIN_ID PIDCODE( 'Q', '2', 'A', 'B')

#define THIS_PLUGIN_TYPE PLUGIN_TYPE_DATA

////////////////////////////////////////
// Data section
////////////////////////////////////////
static struct PluginInfo oPluginInfo =
{
		sizeof( struct PluginInfo ),
		THIS_PLUGIN_TYPE,		
		PLUGIN_VERSION,
		PLUGIN_ID,
		PLUGIN_NAME,
		VENDOR_NAME,
		0,
		387000
};

///////////////////////////////////////////////////////////
// Basic plug-in interface functions exported by DLL
///////////////////////////////////////////////////////////

PLUGINAPI int GetPluginInfo( struct PluginInfo *pInfo ) { 
	*pInfo = oPluginInfo;
	return TRUE;
}
//////////////////////////////////////////////////////////////////////////

T18_COMP_SILENCE_EXIT_DESTRUCTOR;
T18_COMP_SILENCE_REQ_GLOBAL_CONSTR;
static ::std::unique_ptr<::t18::Q2Ami> gQ2Ami;
T18_COMP_POP;

PLUGINAPI int Init() { 
	//AFX_MANAGE_STATE( AfxGetStaticModuleState() );
	T18_ASSERT(!gQ2Ami);
	gQ2Ami = ::std::make_unique<::t18::Q2Ami>();	
	return 1;
}	 

PLUGINAPI int Release() { 
	//AFX_MANAGE_STATE( AfxGetStaticModuleState() );
	T18_ASSERT(gQ2Ami);
	gQ2Ami.reset();
	return 1;
} 

////////////////////////////////////////
// GetSymbolLimit function is called by AmiBroker
// to find out the maximum number of real-time streaming symbols
// that can be displayed in the real-time quote window.
///////////////////////////////////////
PLUGINAPI int GetSymbolLimit(void) {
	T18_ASSERT(gQ2Ami);
	return gQ2Ami->Ami_symLimit();
}

//////////////////////////////////////////////////////////////////////////
// Notify() is called by AmiBroker
// in the various circumstances including:
// 1. database is loaded/unloaded
// 2. right mouse button is clicked in the 
//    plugin status area of AmiBroker's status bar.
//
// The implementation may provide special handling of such events.
PLUGINAPI int Notify(struct PluginNotification *pn) { 
	//AFX_MANAGE_STATE( AfxGetStaticModuleState() );
	if (!pn) return 0;
	T18_ASSERT(gQ2Ami);
	return gQ2Ami->Ami_HandleNotify(pn);
}	 

// nLastValid is an INDEX of last valid element. Seems to be set to -1 when no data available.
// GetQuotesEx() returns the COUNT of valid elements in the arrays
PLUGINAPI int GetQuotesEx( LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation *pQuotes, GQEContext *pContext  )
{
	//AFX_MANAGE_STATE( AfxGetStaticModuleState() );
	T18_ASSERT(gQ2Ami);
	return gQ2Ami->Ami_GetQuotesEx(pszTicker, nPeriodicity, nLastValid, nSize, pQuotes, pContext);
}

/////////////////////////////
// Configure function is called when user clicks "Configure" button in the File->Database Settings dialog
PLUGINAPI int Configure( LPCTSTR pszPath, struct InfoSite *pSite ) {
	//AFX_MANAGE_STATE( AfxGetStaticModuleState() );
	if (!pszPath || !pSite)return 0;
	T18_ASSERT(gQ2Ami);
	return gQ2Ami->Ami_Configure(pszPath, pSite);
}

/*
PLUGINAPI AmiVar GetExtraData( LPCTSTR pszTicker, LPCTSTR pszName, int nArraySize, int nPeriodicity
	, void* (*pfAlloc)(unsigned int nSize) )
{
	// default implementation does nothing

	AmiVar var;

	var.type = VAR_NONE;
	var.val = 0;

	return var;
}*/

PLUGINAPI int SetTimeBase( int nTimeBase ) {
	T18_ASSERT(gQ2Ami);
	return gQ2Ami->Ami_IsTimeBaseOk(nTimeBase);
}

///////////////////////////////////////
// GetRecentInfo function is called by real-time quote window to retrieve the most
// recent quote and other data see the definition of RecentInfo structure in the Plugin.h file
///////////////////////////////////////
/*
PLUGINAPI RecentInfo * GetRecentInfo(LPCTSTR pszTicker) {
	T18_ASSERT(gQ2Ami);
	return gQ2Ami->Ami_GetRecentInfo(pszTicker);
}*/


////////////////////////////////////////
// GetStatus function is called periodically (in on-idle processing) to retrieve the status of the plugin
// Returned status information (see PluginStatus structure definition) contains numeric status code	as well as short and long
// text descriptions of status.
//
// The highest nibble (4-bit part) of nStatus code represents type of status:
// 0 - OK, 1 - WARNING, 2 - MINOR ERROR, 3 - SEVERE ERROR
// that translate to color of status area:
// 0 - green, 1 - yellow, 2 - red, 3 - violet 
PLUGINAPI int GetStatus(struct PluginStatus *status) {
	if (status) {
		T18_ASSERT(gQ2Ami);
		gQ2Ami->Ami_GetStatus(status);
	}
	return 1;
}

