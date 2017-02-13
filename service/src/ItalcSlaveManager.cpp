/*
 * ItalcSlaveManager.cpp - ItalcSlaveManager which manages (GUI) slave apps
 *
 * Copyright (c) 2010-2017 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
 * Copyright (c) 2010 Univention GmbH
 *
 * This file is part of iTALC - http://italc.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTime>

#include "DesktopAccessPermission.h"
#include "ItalcConfiguration.h"
#include "ItalcCore.h"
#include "ItalcSlaveManager.h"
#include "LocalSystem.h"
#include "ScreenLockSlaveLauncher.h"

const Ipc::Id ItalcSlaveManager::IdCoreServer = "CoreServer";
const Ipc::Id ItalcSlaveManager::IdAccessDialog = "AccessDialog";
const Ipc::Id ItalcSlaveManager::IdDemoClient = "DemoClient";
const Ipc::Id ItalcSlaveManager::IdDemoServer = "DemoServer";
const Ipc::Id ItalcSlaveManager::IdScreenLock = "ScreenLock";
const Ipc::Id ItalcSlaveManager::IdInputLock = "InputLock";

const Ipc::Command ItalcSlaveManager::AccessDialog::Ask = "Ask";
const Ipc::Argument ItalcSlaveManager::AccessDialog::User = "User";
const Ipc::Argument ItalcSlaveManager::AccessDialog::Host = "Host";
const Ipc::Argument ItalcSlaveManager::AccessDialog::ChoiceFlags = "ChoiceFlags";
const Ipc::Command ItalcSlaveManager::AccessDialog::ReportChoice = "ReportChoice";

const Ipc::Command ItalcSlaveManager::DemoClient::StartDemo = "StartDemo";
const Ipc::Argument ItalcSlaveManager::DemoClient::MasterHost = "MasterHost";
const Ipc::Argument ItalcSlaveManager::DemoClient::FullScreen = "FullScreen";

const Ipc::Command ItalcSlaveManager::DemoServer::StartDemoServer = "StartDemoServer";
const Ipc::Argument ItalcSlaveManager::DemoServer::SourcePort = "SourcePort";
const Ipc::Argument ItalcSlaveManager::DemoServer::DestinationPort = "DestinationPort";
const Ipc::Argument ItalcSlaveManager::DemoServer::CommonSecret = "CommonSecret";

const Ipc::Command ItalcSlaveManager::DemoServer::UpdateAllowedHosts = "UpdateAllowedHosts";
const Ipc::Argument ItalcSlaveManager::DemoServer::AllowedHosts = "AllowedHosts";



ItalcSlaveManager::ItalcSlaveManager() :
	Ipc::Master( QCoreApplication::applicationFilePath() ),
	m_demoServerMaster( this )
{
}




ItalcSlaveManager::~ItalcSlaveManager()
{
}




void ItalcSlaveManager::startDemo( const QString &masterHost, bool fullscreen )
{
	// if a demo-server is started, it's likely that the demo was started
	// on master-computer as well therefore we deny starting a demo on
	// hosts on which a demo-server is running
	if( isSlaveRunning( IdDemoServer ) )
	{
		return;
	}

	Ipc::SlaveLauncher *slaveLauncher = NULL;
	if( fullscreen && ItalcCore::config->lockWithDesktopSwitching() )
	{
		slaveLauncher = new ScreenLockSlaveLauncher( applicationFilePath() );
	}
	createSlave( IdDemoClient, slaveLauncher );
	sendMessage( IdDemoClient,
					Ipc::Msg( DemoClient::StartDemo ).
						addArg( DemoClient::MasterHost, masterHost ).
						addArg( DemoClient::FullScreen, fullscreen ) );
}




void ItalcSlaveManager::stopDemo()
{
	stopSlave( IdDemoClient );
}




void ItalcSlaveManager::lockScreen()
{
	if( isSlaveRunning( IdDemoServer ) )
	{
		return;
	}

	Ipc::SlaveLauncher *slaveLauncher = NULL;
	if( ItalcCore::config->lockWithDesktopSwitching() )
	{
		slaveLauncher = new ScreenLockSlaveLauncher( applicationFilePath() );
	}
	createSlave( IdScreenLock, slaveLauncher );
}




void ItalcSlaveManager::unlockScreen()
{
	stopSlave( IdScreenLock );
}




void ItalcSlaveManager::lockInput()
{
	createSlave( IdInputLock );
}




void ItalcSlaveManager::unlockInput()
{
	stopSlave( IdInputLock );
}



int ItalcSlaveManager::execAccessDialog( const QString &user,
											const QString &host,
											int choiceFlags )
{
	m_accessDialogChoice = -1;

	createSlave( IdAccessDialog );
	sendMessage( IdAccessDialog,
					Ipc::Msg( AccessDialog::Ask ).
						addArg( AccessDialog::User , user ).
						addArg( AccessDialog::Host, host ).
						addArg( AccessDialog::ChoiceFlags, choiceFlags ) );

	// wait for answer
	QTime t;
	t.start();
	while( m_accessDialogChoice < 0 )
	{
		QCoreApplication::processEvents();
		if( t.elapsed() > 30000 )
		{
			stopSlave( IdAccessDialog );
			return DesktopAccessPermission::ChoiceNo;
		}
	}

	return m_accessDialogChoice;
}




int ItalcSlaveManager::slaveStateFlags()
{
	int s = 0;
#define GEN_SLAVE_STATE_SETTER(x)				\
			if( isSlaveRunning( Id##x ) )		\
			{									\
				s |= ItalcCore::x##Running;		\
			}
	GEN_SLAVE_STATE_SETTER(AccessDialog)
	GEN_SLAVE_STATE_SETTER(DemoServer)
	GEN_SLAVE_STATE_SETTER(DemoClient)
	GEN_SLAVE_STATE_SETTER(ScreenLock)
	GEN_SLAVE_STATE_SETTER(InputLock)

	return s;
}



void ItalcSlaveManager::createSlave( const Ipc::Id &id, Ipc::SlaveLauncher *slaveLauncher )
{
	// only launch interactive iTALC slaves (screen lock, demo, message box,
	// access dialog) if a user is logged on - prevents us from messing up logon
	// dialog on Windows
	if( !LocalSystem::User::loggedOnUser().name().isEmpty() )
	{
		Ipc::Master::createSlave( id, slaveLauncher );
	}
	else
	{
		qDebug() << "ItalcSlaveManager: not creating slave" << id
					<< "as no user is logged on.";
	}
}



bool ItalcSlaveManager::handleMessage( const Ipc::Id &slaveId, const Ipc::Msg &m )
{
	if( slaveId == IdAccessDialog )
	{
		if( m.cmd() == AccessDialog::ReportChoice )
		{
			m_accessDialogChoice = m.arg( AccessDialog::ChoiceFlags ).toInt();
			return true;
		}
	}
	return false;
}

