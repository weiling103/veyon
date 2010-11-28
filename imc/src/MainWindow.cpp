/*
 * MainWindow.cpp - implementation of MainWindow class
 *
 * Copyright (c) 2010 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
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

#include <italcconfig.h>

#ifdef ITALC_BUILD_WIN32
#include <windows.h>
#include "../ica/win32/addon/ms-logon/authSSP/vncSSP.h"
void Win32AclEditor( HWND hwnd );
#endif

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtGui/QCloseEvent>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QProgressBar>
#include <QtGui/QProgressDialog>

#include "Configuration/XmlStore.h"
#include "Configuration/UiMapping.h"

#include "KeyFileAssistant.h"
#include "FileSystemBrowser.h"
#include "ImcCore.h"
#include "ItalcConfiguration.h"
#include "LocalSystem.h"
#include "Logger.h"
#include "LogonAclSettings.h"
#include "LogonGroupEditor.h"
#include "MainWindow.h"
#include "PasswordDialog.h"

#include "ui_MainWindow.h"



MainWindow::MainWindow() :
	QMainWindow(),
	ui( new Ui::MainWindow ),
	m_configChanged( false )
{
	ui->setupUi( this );

	setWindowTitle( tr( "iTALC Management Console %1" ).arg( ITALC_VERSION ) );

	// reset all widget's values to current configuration
	reset();

	// if local configuration is incomplete, re-enable the apply button
	if( ItalcConfiguration(
			Configuration::Store::LocalBackend ).data().size() <
										ItalcCore::config->data().size() )
	{
		configurationChanged();
	}

	// connect widget signals to configuration property write methods
	FOREACH_ITALC_CONFIG_PROPERTY(CONNECT_WIDGET_TO_PROPERTY)

#define CONNECT_BUTTON_SLOT(name) \
			connect( ui->name, SIGNAL( clicked() ), this, SLOT( name() ) );

	CONNECT_BUTTON_SLOT( startService );
	CONNECT_BUTTON_SLOT( stopService );

	CONNECT_BUTTON_SLOT( openLogFileDirectory );

	CONNECT_BUTTON_SLOT( openGlobalConfig );
	CONNECT_BUTTON_SLOT( openPersonalConfig );
	CONNECT_BUTTON_SLOT( openSnapshotDirectory );

	CONNECT_BUTTON_SLOT( openPublicKeyBaseDir );
	CONNECT_BUTTON_SLOT( openPrivateKeyBaseDir );

	CONNECT_BUTTON_SLOT( launchKeyFileAssistant );
	CONNECT_BUTTON_SLOT( manageACLs );
	CONNECT_BUTTON_SLOT( testLogonAuthentication );

	connect( ui->buttonBox, SIGNAL( clicked( QAbstractButton * ) ),
				this, SLOT( resetOrApply( QAbstractButton * ) ) );

	connect( ui->actionLoadSettings, SIGNAL( triggered() ),
				this, SLOT( loadSettingsFromFile() ) );
	connect( ui->actionSaveSettings, SIGNAL( triggered() ),
				this, SLOT( saveSettingsToFile() ) );

	updateServiceControl();

	QTimer *serviceUpdateTimer = new QTimer( this );
	serviceUpdateTimer->start( 2000 );

	connect( serviceUpdateTimer, SIGNAL( timeout() ),
				this, SLOT( updateServiceControl() ) );

	connect( ItalcCore::config, SIGNAL( configurationChanged() ),
				this, SLOT( configurationChanged() ) );
}




MainWindow::~MainWindow()
{
}



void MainWindow::reset( bool onlyUI )
{
	if( onlyUI == false )
	{
		ItalcCore::config->clear();
		*ItalcCore::config += ItalcConfiguration::defaultConfiguration();
		*ItalcCore::config += ItalcConfiguration( Configuration::Store::LocalBackend );
	}

#ifdef ITALC_BUILD_WIN32
	// always make sure we do not have a LogonACL string in our config
	ItalcCore::config->removeValue( "LogonACL", "Authentication" );

	// revert LogonACL to what has been saved in the encoded logon ACL
	LogonAclSettings().setACL(
		ItalcCore::config->value( "EncodedLogonACL", "Authentication" ) );
#endif

	FOREACH_ITALC_CONFIG_PROPERTY(INIT_WIDGET_FROM_PROPERTY)

	ui->buttonBox->setEnabled( false );
	m_configChanged = false;
}




void MainWindow::apply()
{
#ifdef ITALC_BUILD_WIN32
	ItalcCore::config->setValue( "EncodedLogonACL", LogonAclSettings().acl(),
															"Authentication" );
#endif
	if( ImcCore::applyConfiguration( *ItalcCore::config ) )
	{
#ifdef ITALC_BUILD_WIN32
		if( QMessageBox::question( this, tr( "Restart iTALC Service" ),
				tr( "All settings were saved successfully. In order to take "
					"effect the iTALC service needs to be restarted. "
					"Restart it now?" ), QMessageBox::Yes | QMessageBox::No,
				QMessageBox::Yes ) == QMessageBox::Yes )
		{
			stopService();
			startService();
		}
#endif
		ui->buttonBox->setEnabled( false );
		m_configChanged = false;
	}
}




void MainWindow::configurationChanged()
{
	ui->buttonBox->setEnabled( true );
	m_configChanged = true;
}




void MainWindow::resetOrApply( QAbstractButton *btn )
{
	if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Apply )
	{
		apply();
	}
	else if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Reset )
	{
		reset();
	}
}




void MainWindow::startService()
{
	serviceControlWithProgressBar( tr( "Starting iTALC service" ), "-startservice" );
}




void MainWindow::stopService()
{
	serviceControlWithProgressBar( tr( "Stopping iTALC service" ), "-stopservice" );
}




void MainWindow::updateServiceControl()
{
	bool running = isServiceRunning();
#ifdef ITALC_BUILD_WIN32
	ui->startService->setEnabled( !running );
	ui->stopService->setEnabled( running );
#else
	ui->startService->setEnabled( false );
	ui->stopService->setEnabled( false );
#endif
	ui->serviceState->setText( running ? tr( "Running" ) : tr( "Stopped" ) );
}




void MainWindow::openLogFileDirectory()
{
	FileSystemBrowser( FileSystemBrowser::ExistingDirectory ).
												exec( ui->logFileDirectory );
}




void MainWindow::openGlobalConfig()
{
	FileSystemBrowser( FileSystemBrowser::ExistingFile ).
										exec( ui->globalConfigurationPath );
}




void MainWindow::openPersonalConfig()
{
	FileSystemBrowser( FileSystemBrowser::ExistingFile ).
										exec( ui->personalConfigurationPath );
}




void MainWindow::openSnapshotDirectory()
{
	FileSystemBrowser( FileSystemBrowser::ExistingDirectory ).
												exec( ui->snapshotDirectory );
}




void MainWindow::openPublicKeyBaseDir()
{
	FileSystemBrowser( FileSystemBrowser::ExistingDirectory ).
												exec( ui->publicKeyBaseDir );
}




void MainWindow::openPrivateKeyBaseDir()
{
	FileSystemBrowser( FileSystemBrowser::ExistingDirectory ).
												exec( ui->privateKeyBaseDir );
}




void MainWindow::loadSettingsFromFile()
{
	QString fileName = QFileDialog::getOpenFileName( this, tr( "Load settings from file" ),
											QDir::homePath(), tr( "XML files (*.xml)" ) );
	if( !fileName.isEmpty() )
	{
		// write current configuration to output file
		Configuration::XmlStore( Configuration::XmlStore::System,
										fileName ).load( ItalcCore::config );
		reset( true );
		configurationChanged();	// give user a chance to apply possible changes
	}
}




void MainWindow::saveSettingsToFile()
{
	QString fileName = QFileDialog::getSaveFileName( this, tr( "Save settings to file" ),
											QDir::homePath(), tr( "XML files (*.xml)" ) );
	if( !fileName.isEmpty() )
	{
		if( !fileName.endsWith( ".xml", Qt::CaseInsensitive ) )
		{
			fileName += ".xml";
		}

		bool configChangedPrevious = m_configChanged;

#ifdef ITALC_BUILD_WIN32
		ItalcCore::config->removeValue( "LogonACL", "Authentication" );
		ItalcCore::config->setValue( "EncodedLogonACL",
								LogonAclSettings().acl(), "Authentication" );
#endif

		// write current configuration to output file
		Configuration::XmlStore( Configuration::XmlStore::System,
										fileName ).flush( ItalcCore::config );

		m_configChanged = configChangedPrevious;
		ui->buttonBox->setEnabled( m_configChanged );
	}
}




void MainWindow::launchKeyFileAssistant()
{
	KeyFileAssistant().exec();
}




void MainWindow::manageACLs()
{
#ifdef ITALC_BUILD_WIN32
	Win32AclEditor( winId() );

	if( LogonAclSettings().acl() !=
				ItalcCore::config->value( "EncodedLogonACL", "Authentication" ) )
	{
		configurationChanged();
	}
#else
	LogonGroupEditor( this ).exec();
#endif
}




void MainWindow::testLogonAuthentication()
{
	PasswordDialog dlg( this );
	if( dlg.exec() )
	{
		bool result = false;
#ifdef ITALC_BUILD_WIN32
		result = CUPSD( dlg.username().toUtf8().constData(),
						dlg.password().toUtf8().constData(),
						"127.0.0.1" ) > 0 ?  true : false;
#endif
		if( result )
		{
			QMessageBox::information( this, tr( "Logon authentication test" ),
							tr( "Authentication with provided credentials "
								"was successful." ) );
		}
		else
		{
			QMessageBox::critical( this, tr( "Logon authentication test" ),
							tr( "Authentication with provided credentials "
								"failed!" ) );
		}
	}
}




void MainWindow::closeEvent( QCloseEvent *closeEvent )
{
	if( m_configChanged &&
			QMessageBox::question( this, tr( "Unsaved settings" ),
									tr( "There are unsaved settings. "
										"Quit anyway?" ),
									QMessageBox::Yes | QMessageBox::No ) !=
															QMessageBox::Yes )
	{
		closeEvent->ignore();
		return;
	}

	// make sure to revert the LogonACL
	reset();

	closeEvent->accept();
	QMainWindow::closeEvent( closeEvent );
}




void MainWindow::serviceControlWithProgressBar( const QString &title,
												const QString &arg )
{
	QProcess p;
	p.start( ImcCore::icaFilePath(), QStringList() << arg );
	p.waitForStarted();

	QProgressDialog pd( title, QString(), 0, 0, this );
	pd.setWindowTitle( windowTitle() );

	QProgressBar *b = new QProgressBar( &pd );
	b->setMaximum( 100 );
	b->setTextVisible( false );
	pd.setBar( b );
	b->show();
	pd.setWindowModality( Qt::WindowModal );
	pd.show();

	int j = 0;
	while( p.state() == QProcess::Running )
	{
		QApplication::processEvents();
		b->setValue( ++j % 100 );
		LocalSystem::sleep( 10 );
	}

	updateServiceControl();
}




bool MainWindow::isServiceRunning()
{
#ifdef ITALC_BUILD_WIN32
	SC_HANDLE hsrvmanager = OpenSCManager( NULL, NULL, SC_MANAGER_CONNECT );
	if( !hsrvmanager )
	{
		ilog_failed( "OpenSCManager()" );
		return false;
	}

	SC_HANDLE hservice = OpenService( hsrvmanager, "icas", SERVICE_QUERY_STATUS );
	if( !hservice )
	{
		ilog_failed( "OpenService()" );
		CloseServiceHandle( hsrvmanager );
		return false;
	}

	SERVICE_STATUS status;
	QueryServiceStatus( hservice, &status );

	CloseServiceHandle( hservice );
	CloseServiceHandle( hsrvmanager );

	return( status.dwCurrentState == SERVICE_RUNNING );
#else
	return false;
#endif
}
