/****************************************************************************
** Copyright (c) 2001-2004 quickfixengine.org  All rights reserved.
**
** This file is part of the QuickFIX FIX Engine
**
** This file may be distributed under the terms of the quickfixengine.org
** license as defined by quickfixengine.org and appearing in the file
** LICENSE included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.quickfixengine.org/LICENSE for licensing information.
**
** Contact ask@quickfixengine.org if any conditions of this licensing are
** not clear to you.
**
****************************************************************************/

#ifdef _MSC_VER
#include "stdafx.h"
#else
#include "config.h"
#endif
#include "CallStack.h"

#include "SocketAcceptor.h"
#include "Session.h"
#include "Settings.h"
#include "Utility.h"

namespace FIX
{
SocketAcceptor::SocketAcceptor( Application& application,
                                MessageStoreFactory& factory,
                                const SessionSettings& settings ) throw( ConfigError )
: Acceptor( application, factory, settings ),
  m_port( 0 ), m_reuseAddress(true), m_noDelay(false), m_pServer( 0 ), m_stop( false ) {}

SocketAcceptor::SocketAcceptor( Application& application,
                                MessageStoreFactory& factory,
                                const SessionSettings& settings,
                                LogFactory& logFactory ) throw( ConfigError )
: Acceptor( application, factory, settings, logFactory ),
  m_port( 0 ), m_reuseAddress(true), m_noDelay(false), m_pServer( 0 ), m_stop( false ) {}

SocketAcceptor::~SocketAcceptor()
{
  SocketConnections::iterator iter;
  for ( iter = m_connections.begin(); iter != m_connections.end(); ++iter )
    delete iter->second;
}

void SocketAcceptor::onConfigure( const SessionSettings& s )
throw ( ConfigError )
{ QF_STACK_PUSH(SocketAcceptor::onConfigure)

  m_port = ( short ) s.get().getLong( SOCKET_ACCEPT_PORT );
  if( s.get().has( SOCKET_REUSE_ADDRESS ) )
    m_reuseAddress = ( bool ) s.get().getBool( SOCKET_REUSE_ADDRESS );
  if( s.get().has( SOCKET_NODELAY ) )
    m_noDelay = ( bool ) s.get().getBool( SOCKET_NODELAY );
  QF_STACK_POP
}

void SocketAcceptor::onInitialize( const SessionSettings& s ) 
throw ( RuntimeError )
{ QF_STACK_PUSH(SocketAcceptor::onInitialize)

  try
  {
    m_pServer = new SocketServer( m_port, 1, m_reuseAddress, m_noDelay );
  }
  catch( std::exception& )
  {
    throw RuntimeError( "Unable to create, bind, or listen to port " + IntConvertor::convert(m_port) );
  }
  
  QF_STACK_POP
}

void SocketAcceptor::onStart()
{ QF_STACK_PUSH(SocketAcceptor::onStart)

  while ( !m_stop && m_pServer && m_pServer->block( *this ) ) {}

  if( !m_pServer ) 
    return;

  time_t start = 0;
  time_t now = 0;
    
  ::time( &start );
  while ( isLoggedOn() )
  {
    m_pServer->block( *this );
    if( ::time(&now) -5 >= start )
      break;
  }

  m_pServer->close();
  delete m_pServer;
  m_pServer = 0;

  QF_STACK_POP
}

bool SocketAcceptor::onPoll()
{ QF_STACK_PUSH(SocketAcceptor::onPoll)

  if( !m_pServer )
    return false;

  time_t start = 0;
  time_t now = 0;

  if( m_stop )
  {
    if( start == 0 )
      ::time( &start );
    if( !isLoggedOn() )
      return false;
    if( ::time(&now) - 5 >= start )
      return false;
  }

  m_pServer->block( *this, true );
  return true;

  QF_STACK_POP
}

void SocketAcceptor::onStop()
{ QF_STACK_PUSH(SocketAcceptor::onStop)

  m_stop = true;

  QF_STACK_POP
}

void SocketAcceptor::onConnect( SocketServer& server, int s )
{ QF_STACK_PUSH(SocketAcceptor::onConnect)

  if ( !socket_isValid( s ) ) return ;
  SocketConnections::iterator i = m_connections.find( s );
  if ( i != m_connections.end() ) return ;
  m_connections[ s ] = new SocketConnection( s, &server.getMonitor() );

  QF_STACK_POP
}

void SocketAcceptor::onData( SocketServer& server, int s )
{ QF_STACK_PUSH(SocketAcceptor::onData)

  SocketConnections::iterator i = m_connections.find( s );
  if ( i == m_connections.end() ) return ;
  SocketConnection* pSocketConnection = i->second;
  while ( pSocketConnection->read( *this, server ) ) {}

  QF_STACK_POP
}

void SocketAcceptor::onDisconnect( SocketServer&, int s )
{ QF_STACK_PUSH(SocketAcceptor::onDisconnect)

  SocketConnections::iterator i = m_connections.find( s );
  if ( i == m_connections.end() ) return ;
  SocketConnection* pSocketConnection = i->second;

  Session* pSession = pSocketConnection->getSession();
  if ( pSession ) pSession->disconnect();

  delete pSocketConnection;
  m_connections.erase( s );

  QF_STACK_POP
}

void SocketAcceptor::onError( SocketServer& ) {}

void SocketAcceptor::onTimeout( SocketServer& )
{ QF_STACK_PUSH(SocketAcceptor::onInitialize)

  SocketConnections::iterator i;
  for ( i = m_connections.begin(); i != m_connections.end(); ++i )
    i->second->onTimeout();

  QF_STACK_POP
}
}