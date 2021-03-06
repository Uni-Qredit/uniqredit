// Copyright (c) 2011-2015 The Uniqredit Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/uniqredit-config.h"
#endif

#include "uniqreditgui.h"

#include "uniqreditunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "platformstyle.h"
#include "rpcconsole.h"
#include "utilitydialog.h"

#ifdef ENABLE_WALLET
#include "walletframe.h"
#include "walletmodel.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "ui_interface.h"
#include "util.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QFile>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
//#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const std::string UniqreditGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif 
;

const QString UniqreditGUI::DEFAULT_WALLET = "~Default";

UniqreditGUI::UniqreditGUI(const PlatformStyle *platformStyle, const NetworkStyle *networkStyle, QWidget *parent) :
    QMainWindow(parent),
    clientModel(0),
    walletFrame(0),
    //unitDisplayControl(0),
    labelEncryptionIcon(0),
    labelConnectionsIcon(0),
    labelBlocksIcon(0),
    progressBarLabel(0),
    progressBar(0),
    progressDialog(0),
    appMenuBar(0),
    overviewAction(0),
    historyAction(0),
    quitAction(0),
    sendCoinsAction(0),
    sendCoinsMenuAction(0),
    usedSendingAddressesAction(0),
    usedReceivingAddressesAction(0),
    signMessageAction(0),
    verifyMessageAction(0),
    aboutAction(0),
    receiveCoinsAction(0),
    receiveCoinsMenuAction(0),
    optionsAction(0),
    toggleHideAction(0),
    encryptWalletAction(0),
    backupWalletAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    openRPCConsoleAction(0),
    openAction(0),
    showHelpMessageAction(0),
    trayIcon(0),
    trayIconMenu(0),
    notificator(0),
    rpcConsole(0),
    helpMessageDialog(0),
    prevBlocks(0),
    spinnerFrame(0),
    platformStyle(platformStyle)
{
    setFixedSize(850, 650);
    setWindowFlags(Qt::FramelessWindowHint);
    GUIUtil::restoreWindowGeometry("nWindow", QSize(850, 650), this);

    // load stylesheet
    QFile qss(":css/dyno");
    qss.open(QFile::ReadOnly);
    qApp->setStyleSheet(qss.readAll());
    qss.close();

    QString windowTitle = tr(PACKAGE_NAME) + " - ";
#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET
    if(enableWallet)
    {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    //windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowIcon(networkStyle->getTrayAndWindowIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(platformStyle, 0);
    helpMessageDialog = new HelpMessageDialog(this, false);
#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the centralish widget */
        walletFrame = new WalletFrame(platformStyle, this);
        //walletFrame = new WalletFrame(this);
        //setCentralWidget(walletFrame);
        walletFrame->setFixedWidth(850);
        walletFrame->setFixedHeight(390);
        walletFrame->move(0,165);        
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Header UI elements

    // logo - we'll make it a button that leads back to the overviewpage menu
    Logo = new QPushButton(this);
    Logo->move(10, 30);
    Logo->setFixedWidth(250);
    Logo->setFixedHeight(80);
    Logo->setObjectName("Logo");
    connect(Logo, SIGNAL(clicked()), this, SLOT(gotoOverviewPage()));

    // balance frame
    bframe = new QFrame(this);
    bframe->move(250, 30);
    bframe->setFixedWidth(590);
    bframe->setFixedHeight(80);
    bframe->setObjectName("bframe");    

    // available balance label
    //labelBalance = new QLabel(bframe);
    //labelBalance->move(10, 10);
    //labelBalance->setFixedWidth(520);
    //labelBalance->setFixedHeight(30);
    //labelBalance->setText("Available Balance:");
    //labelBalance->setObjectName("labelBalance");    

    // balance label - semi retired
    labelHeaderBalance = new QLabel(bframe);
    labelHeaderBalance->move(1, 1);
    labelHeaderBalance->setFixedWidth(1);
    labelHeaderBalance->setFixedHeight(1);
    labelHeaderBalance->setText("0.00");
    labelHeaderBalance->setObjectName("labelHeaderBalance");

    // fancy new html label with smaller post-decimal-point digits
    labelSplit = new QLabel(bframe);
    labelSplit->move (10, 25);
    labelSplit->setFixedWidth(520);
    labelSplit->setFixedHeight(30);
    labelSplit->sizeHint();
    labelSplit->setAlignment(Qt::AlignCenter);
    labelSplit->setText("0.00000000");
    labelSplit->setObjectName("labelSplit");
    labelSplit->setToolTip("Available Balance");   

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    //createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // create bottom 'toolbar'...
    QWidget *toolbar2 = new QWidget(this);
    toolbar2->setFixedHeight(30);
    toolbar2->setFixedWidth(830);
    toolbar2->move(10, 620);
    toolbar2->setObjectName("toolbar2");
    
    // ...add encryption, connections and blocks icons
    labelEncryptionIcon = new QLabel(toolbar2);
    labelEncryptionIcon->setObjectName("labelEncryptionIcon");
    labelEncryptionIcon->setFixedHeight(20);
    labelEncryptionIcon->setFixedWidth(20);
    labelEncryptionIcon->move(80, 0);
    labelConnectionsIcon = new QLabel(toolbar2);
    labelConnectionsIcon->setPixmap(QIcon(":/icons/connect_0").pixmap(18, 18));
    labelConnectionsIcon->setObjectName("labelConnectionsIcon");
    labelConnectionsIcon->setFixedHeight(20);
    labelConnectionsIcon->setFixedWidth(20);
    labelConnectionsIcon->move(0, 0);
    labelBlocksIcon = new QLabel(toolbar2);
    labelBlocksIcon->setPixmap(QIcon(":/icons/connect0s").pixmap(18, 18)); //Initialize with 'searching' icon so people with slow connections see something
    labelBlocksIcon->setToolTip("Looking for more network connections");
    labelBlocksIcon->setObjectName("labelBlocksIcon");
    labelBlocksIcon->setFixedHeight(20);
    labelBlocksIcon->setFixedWidth(20);
    labelBlocksIcon->move(40, 0);
    
    QPushButton *aboutButton = new QPushButton(toolbar2);
    //aboutButton->setStyleSheet("padding: none; border: none; background-color: #232323; background-image: url(':/icons/about'); background-repeat: none; background-position: center;");
    aboutButton->setFixedHeight(30);
    aboutButton->setFixedWidth(30);
    aboutButton->move(720, 0);
    aboutButton->setToolTip("About Uniqredit");
    aboutButton->setObjectName("aboutButton");
    connect(aboutButton, SIGNAL(clicked()), this, SLOT(aboutClicked()));
    
    QPushButton *hideButton = new QPushButton(toolbar2);
    //hideButton->setStyleSheet("padding: none; border: none; background-color: #232323; background-image: url(':/icons/hide'); background-repeat: none; background-position: center;");
    hideButton->setFixedHeight(30);
    hideButton->setFixedWidth(30);
    hideButton->move(760, 0);
    hideButton->setToolTip("Minimise");
    hideButton->setObjectName("hideButton");
    connect(hideButton, SIGNAL(clicked()), this, SLOT(toggleHidden()));

    QPushButton *quitButton = new QPushButton(toolbar2);
    //quitButton->setStyleSheet("padding: none; border: none; background-color: #232323;  background-image: url(':/icons/quit'); background-repeat: none; background-position: center;");
    quitButton->setFixedHeight(30);
    quitButton->setFixedWidth(30);
    quitButton->move(800, 0);
    quitButton->setToolTip("Exit");
    quitButton->setObjectName("quitButton");
    connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));
    
    // Progress bar and label for blocks download (these will float if called since we haven't added them to anything else)
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();
}

UniqreditGUI::~UniqreditGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}

void UniqreditGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a Uniqredit address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/send"), sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and uniqredit: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/receiving_addresses"), receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
#endif // ENABLE_WALLET

    quitAction = new QAction(platformStyle->TextColorIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&About %1").arg(tr(PACKAGE_NAME)), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(tr(PACKAGE_NAME)));
    //aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(platformStyle->TextColorIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(platformStyle->TextColorIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(tr(PACKAGE_NAME)));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(platformStyle->TextColorIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your Uniqredit addresses to prove you own them"));
    verifyMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/verify"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified Uniqredit addresses"));

    openRPCConsoleAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));

    usedSendingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(platformStyle->TextColorIcon(":/icons/open"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a uniqredit: URI or payment request"));

    showHelpMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/info"), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible Uniqredit command-line options").arg(tr(PACKAGE_NAME)));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindow()));
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));

        // cryptit signal triggerred from otherPage via walletView, jesus christ #4
        connect(this, SIGNAL(enc(bool)), walletFrame, SLOT(encryptWallet(bool)));    
        
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));

        // backitup signal triggerred from otherPage via walletView, jesus christ
        connect(this, SIGNAL(backitup()), walletFrame, SLOT(backupWallet()));

        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));

        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        
        // usedsending signal triggerred from otherPage via walletView, jesus christ #2
        connect(this, SIGNAL(usedsending()), walletFrame, SLOT(usedSendingAddresses()));
        
        // usedreceiving signal triggerred from otherPage via walletView, jesus christ #3
        connect(this, SIGNAL(usedreceiving()), walletFrame, SLOT(usedReceivingAddresses()));
        
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
    }
#endif // ENABLE_WALLET

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));
}


void UniqreditGUI::splitBalance()
{
    QStringList chunks = this->labelHeaderBalance->text().split(".");
    QString integer = chunks.at(0);
    QString decimal = chunks.at(1);
    QString joined = ("<span style='font-size:16pt; color:#232323;'>Available Balance: " + integer + "</span><span style='font-size:12pt; color:#232323;'>." + decimal + "</span>");
    this->labelSplit->setText(joined);
}

void UniqreditGUI::enc()
{
    if(walletFrame) encryptWalletAction->activate(QAction::Trigger);
}

void UniqreditGUI::changepw()
{
    if(walletFrame) changePassphraseAction->activate(QAction::Trigger);
}

void UniqreditGUI::emitbackitup()
{
    Q_EMIT backitup();
}

void UniqreditGUI::emitusedsending()
{
    Q_EMIT usedsending();
}

void UniqreditGUI::emitusedreceiving()
{
    Q_EMIT usedreceiving();
}

void UniqreditGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    if(walletFrame)
    {
        help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void UniqreditGUI::createToolBars()
{
    if(walletFrame)
    {
        // menu/back button
        bover = new QPushButton(this);
        bover->setFixedWidth(830);
        bover->setFixedHeight(50);
        bover->setObjectName("bover");
        bover->move(10,565);
        bover->setText(" <<< Menu <<<");
        connect(bover, SIGNAL(clicked()), this, SLOT(gotoOverviewPage()));
        bover->hide();
        
        // sendrec 'toolbar' with send and receive 'tabs'
        sendrec = new QWidget(this);
        sendrec->setFixedHeight(25);
        sendrec->setFixedWidth(830);
        sendrec->move(10, 135);
        sendrec->hide();
        
        bsendtab = new QPushButton(sendrec);
        bsendtab->setFixedHeight(25);
        bsendtab->setFixedWidth(410);
        bsendtab->move(0,0);
        bsendtab->setText("Send");
        bsendtab->setObjectName("bsendtab");
        bsendtab->setCheckable(true);
        connect(bsendtab, SIGNAL(clicked()), this, SLOT(gotoSendCoinsPage()));
        
        brectab = new QPushButton(sendrec);
        brectab->setFixedHeight(25);
        brectab->setFixedWidth(410);
        brectab->move(420,0);
        brectab->setText("Receive");
        brectab->setObjectName("brectab");
        brectab->setCheckable(true);
        connect(brectab, SIGNAL(clicked()), this, SLOT(gotoReceiveCoinsPage()));

        // p2p finance page lend / borrow toolbar
        p2p = new QWidget(this);
        p2p->setFixedHeight(25);
        p2p->setFixedWidth(830);
        p2p->move(10, 135);
        p2p->setObjectName("uands");
        p2p->hide();

        bborrow = new QPushButton(p2p);
        bborrow->setFixedHeight(25);
        bborrow->setFixedWidth(410);
        bborrow->move(0,0);
        bborrow->setText("Borrow UNIQ");
        bborrow->setObjectName("bborrow");
        bborrow->setCheckable(true);
        connect(bborrow, SIGNAL(clicked()), this, SLOT(gotoP2PPage()));
        
        blend = new QPushButton(p2p);
        blend->setFixedHeight(25);
        blend->setFixedWidth(410);
        blend->move(420,0);
        blend->setText("Lend UNIQ");
        blend->setObjectName("blend");
        blend->setCheckable(true);
        connect(blend, SIGNAL(clicked()), this, SLOT(gotoP2PLPage()));

        // utilities and settings 'toolbar'
        uands = new QWidget(this);
        uands->setFixedHeight(25);
        uands->setFixedWidth(830);
        uands->move(10, 135);
        uands->setObjectName("uands");
        uands->hide();

        bbcrstatstab = new QPushButton(uands);
        bbcrstatstab->setFixedHeight(25);
        bbcrstatstab->setFixedWidth(207);
        bbcrstatstab->move(0,0);
        bbcrstatstab->setText("UNIQ Network Stats");
        bbcrstatstab->setObjectName("bbcrstatstab");
        bbcrstatstab->setCheckable(true);
        bbcrstatstab->setObjectName("bbcrstatstab");
        connect(bbcrstatstab, SIGNAL(clicked()), this, SLOT(gotoUtilitiesPage()));

        bexplorertab = new QPushButton(uands);
        bexplorertab->setFixedHeight(25);
        bexplorertab->setFixedWidth(207);
        bexplorertab->move(208, 0);
        bexplorertab->setText("Block Explorer");
        bexplorertab->setObjectName("bexplorertab");
        bexplorertab->setCheckable(true);
        bexplorertab->setObjectName("bexplorertab");
        connect(bexplorertab, SIGNAL(clicked()), this, SLOT(gotoBlockExplorerPage()));
        
        bmarkettab = new QPushButton(uands);
        bmarkettab->setFixedHeight(25);
        bmarkettab->setFixedWidth(207);
        bmarkettab->move(416, 0);
        bmarkettab->setText("Market Data");
        bmarkettab->setObjectName("bmarkettab");
        bmarkettab->setCheckable(true);
        bmarkettab->setObjectName("bmarkettab");
        connect(bmarkettab, SIGNAL(clicked()), this, SLOT(gotoExchangeBrowserPage()));

        bothertab = new QPushButton(uands);
        bothertab->setFixedHeight(25);
        bothertab->setFixedWidth(206);
        bothertab->move(624, 0);
        bothertab->setText("Other Stuff");
        bothertab->setObjectName("bothertab");
        bothertab->setCheckable(true);
        bothertab->setObjectName("bothertab");
        connect(bothertab, SIGNAL(clicked()), this, SLOT(gotoOtherPage()));
     }
}

void UniqreditGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getLastBlockDate(), clientModel->getVerificationProgress(NULL));
        connect(clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double)), this, SLOT(setNumBlocks(int,QDateTime,double)));

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        // Show progress dialog
        connect(clientModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));

        rpcConsole->setClientModel(clientModel);
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(clientModel);
        }
#endif // ENABLE_WALLET
        //unitDisplayControl->setOptionsModel(clientModel->getOptionsModel());
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
    }
}

#ifdef ENABLE_WALLET
bool UniqreditGUI::addWallet(const QString& name, WalletModel *walletModel)
{
    if(!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool UniqreditGUI::setCurrentWallet(const QString& name)
{
    if(!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void UniqreditGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void UniqreditGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void UniqreditGUI::createTrayIcon(const NetworkStyle *networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(tr(PACKAGE_NAME)) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getTrayAndWindowIcon());
    trayIcon->show();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void UniqreditGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsMenuAction);
    trayIconMenu->addAction(receiveCoinsMenuAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void UniqreditGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void UniqreditGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void UniqreditGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void UniqreditGUI::showDebugWindow()
{
    rpcConsole->showNormal();
    rpcConsole->show();
    rpcConsole->raise();
    rpcConsole->activateWindow();
}

void UniqreditGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
}

void UniqreditGUI::showHelpMessageClicked()
{
    helpMessageDialog->show();
}

#ifdef ENABLE_WALLET
void UniqreditGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void UniqreditGUI::gotoOverviewPage()
{
    Logo->setStyleSheet("background-image: url(':css/logo');");
    if (walletFrame) walletFrame->gotoOverviewPage();
    bover->hide();
    sendrec->hide();
    uands->hide();
    p2p->hide();
}

void UniqreditGUI::gotoHistoryPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-history');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-history');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoHistoryPage();
    bover->show();
}

void UniqreditGUI::gotoReceiveCoinsPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-receive');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-receive');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
    bover->show();
    brectab->setChecked(true);
    bsendtab->setChecked(false);
}

void UniqreditGUI::gotoSendCoinsPage(QString addr)
{
    //Logo->setStyleSheet("background-image: url(':css/logo-send');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-send');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
    bover->show();
    sendrec->show();
    brectab->setChecked(false);
    bsendtab->setChecked(true);
}

void UniqreditGUI::gotoBidPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-bid');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-bid');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoBidPage();
    bover->show();
}

void UniqreditGUI::gotoP2PPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-p2p');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-p2p');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoP2PPage();
    p2p->show();
    bover->show();
    bborrow->setChecked(true);
    blend->setChecked(false);
}

void UniqreditGUI::gotoP2PLPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-p2p');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-p2p');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoP2PLPage();
    p2p->show();
    bover->show();
    bborrow->setChecked(false);
    blend->setChecked(true);
}

void UniqreditGUI::gotoAssetsPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-assets');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-assets');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoAssetsPage();
    bover->show();
}

void UniqreditGUI::gotoUtilitiesPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-finstats');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-finstats');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoUtilitiesPage();
    bover->show();
    uands->show();
    bbcrstatstab->setChecked(true);
    bexplorertab->setChecked(false);
    bmarkettab->setChecked(false);
    bothertab->setChecked(false);
}

void UniqreditGUI::gotoBlockExplorerPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-explorer');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-explorer');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoBlockExplorerPage();
    bover->show();
    uands->show();
    bbcrstatstab->setChecked(false);
    bexplorertab->setChecked(true);
    bmarkettab->setChecked(false);
    bothertab->setChecked(false);
}

void UniqreditGUI::gotoExchangeBrowserPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-market');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-market');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoExchangeBrowserPage();
    bover->show();
    uands->show();
    bbcrstatstab->setChecked(false);
    bexplorertab->setChecked(false);
    bmarkettab->setChecked(true);
    bothertab->setChecked(false);
}

void UniqreditGUI::gotoOtherPage()
{
    //Logo->setStyleSheet("background-image: url(':css/logo-other');");
    //Logo->setStyleSheet("QPushButton{background-image: url(':css/logo-other');}" "QPushButton:hover{background-image: url(':css/logo');}");
    if (walletFrame) walletFrame->gotoOtherPage();
    bover->show();
    uands->show();
    bbcrstatstab->setChecked(false);
    bexplorertab->setChecked(false);
    bmarkettab->setChecked(false);
    bothertab->setChecked(true);
}

void UniqreditGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void UniqreditGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
#endif // ENABLE_WALLET

void UniqreditGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    //labelConnectionsIcon->setPixmap(platformStyle->SingleColorIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setPixmap(icon);
    //labelConnectionsIcon->setPixmap(icon.scaled(20, 20, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to Uniqredit network", "", count));
}

void UniqreditGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress)
{
    if(!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    //statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            progressBarLabel->setText(tr("Synchronizing with network..."));
            break;
        case BLOCK_SOURCE_DISK:
            progressBarLabel->setText(tr("Importing blocks from disk..."));
            break;
        case BLOCK_SOURCE_REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BLOCK_SOURCE_NONE:
            // Case: not Importing, not Reindexing and no network connection
            progressBarLabel->setText(tr("No block source available..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(18, 18));

#ifdef ENABLE_WALLET
        if(walletFrame)
            walletFrame->showOutOfSyncWarning(false);
#endif // ENABLE_WALLET

        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        // Represent time from last generated block in human readable text
        QString timeBehindText;
        const int HOUR_IN_SECONDS = 60*60;
        const int DAY_IN_SECONDS = 24*60*60;
        const int WEEK_IN_SECONDS = 7*24*60*60;
        const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
        if(secs < 2*DAY_IN_SECONDS)
        {
            timeBehindText = tr("%n hour(s)","",secs/HOUR_IN_SECONDS);
        }
        else if(secs < 2*WEEK_IN_SECONDS)
        {
            timeBehindText = tr("%n day(s)","",secs/DAY_IN_SECONDS);
        }
        else if(secs < YEAR_IN_SECONDS)
        {
            timeBehindText = tr("%n week(s)","",secs/WEEK_IN_SECONDS);
        }
        else
        {
            qint64 years = secs / YEAR_IN_SECONDS;
            qint64 remainder = secs % YEAR_IN_SECONDS;
            timeBehindText = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(tr("%n week(s)","", remainder/WEEK_IN_SECONDS));
        }

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(18, 18));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
            walletFrame->showOutOfSyncWarning(true);
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void UniqreditGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString strTitle = tr("Uniqredit"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    }
    else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "Uniqredit - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    }
    else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void UniqreditGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void UniqreditGUI::closeEvent(QCloseEvent *event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
    }
#endif
    QMainWindow::closeEvent(event);
}

#ifdef ENABLE_WALLET
void UniqreditGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(UniqreditUnits::formatWithUnit(unit, amount, true)) +
                  tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void UniqreditGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void UniqreditGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        Q_FOREACH(const QUrl &uri, event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool UniqreditGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool UniqreditGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void UniqreditGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_open").pixmap(18,18));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_closed").pixmap(18,18));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}
#endif // ENABLE_WALLET

void UniqreditGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void UniqreditGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void UniqreditGUI::detectShutdown()
{
    if (ShutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void UniqreditGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

static bool ThreadSafeMessageBox(UniqreditGUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret));
    return ret;
}

void UniqreditGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void UniqreditGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void UniqreditGUI::mousePressEvent(QMouseEvent *event) 
{
    m_nMouseClick_X_Coordinate = event->x();
    m_nMouseClick_Y_Coordinate = event->y();
}

void UniqreditGUI::mouseMoveEvent(QMouseEvent *event) 
{
    move(event->globalX() - m_nMouseClick_X_Coordinate, event->globalY() - m_nMouseClick_Y_Coordinate);
}

