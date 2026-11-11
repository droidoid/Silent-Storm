#include "stdafx.h"
#include <iostream>
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 void SetStringTableName( const string &szTable ); // CRAP
////////////////////////////////////////////////////////////////////////////////////////////////////
void __cdecl main( int argc, char* argv[] )
{
#ifdef _DEBUG
  int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	//tmpFlag |= _CRTDBG_LEAK_CHECK_DF;// | _CRTDBG_CHECK_ALWAYS_DF;
	tmpFlag = _CRTDBG_LEAK_CHECK_DF;// | _CRTDBG_CHECK_ALWAYS_DF;
  _CrtSetDbgFlag( tmpFlag );
	//_CrtSetBreakAlloc( 83 );
#endif

	string szOutput = "w:\\Complete\\game.db";
	bool bTranslate = false;
	string szServer = "a5server";
	string szConnectOverride; // VS2022 port: -connect "<ADO connection string>" (e.g. the Jet provider for Data\game.mdb)

	for ( int i = 1; i < argc; ++i )
	{
		string szCmd = argv[i];
		if ( szCmd.empty() )
			continue;
		if ( szCmd[0] != '-' )
		{
			szOutput = szCmd;
			printf( "output file: %s\n", szOutput.c_str() );
		}
		else if ( szCmd == "-translate" )
		{
			bTranslate = true;
			printf( "translate = true\n" );
		}
		else if ( szCmd == "-connect" ) // VS2022 port
		{
			if ( i < argc-1 )
				szConnectOverride = argv[++i];
			printf( "connection string = %s\n", szConnectOverride.c_str() );
		}
		else if ( szCmd == "-dbserver" )
		{
			if ( i < argc-1 )
				szServer = argv[++i];
			printf( "server = %s\n", szServer.c_str() );
		}
	}

	string szConnect = NDatabase::GetDBConnectionStr( szServer );
	if ( !szConnectOverride.empty() ) // VS2022 port
		szConnect = szConnectOverride;
	NDatabase::SetSource( szConnect.c_str() );//Provider=Microsoft.Jet.OLEDB.4.0;Data Source=w:\\Data\\Game.mdb" );//w:\\Data\\Game.mdb" );
	// read from database
	NDatabase::Import();
	NDb::BuildMapLinks( bTranslate );

	// write to proprietary format
	{
		try
		{
			CFileStream f;
			f.OpenWrite( szOutput.c_str() );
			NDatabase::Serialize( f, CStructureSaver::WRITE );
		}
		catch(...)
		{
			cout << "failed to open game.db for writing" <<endl;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
