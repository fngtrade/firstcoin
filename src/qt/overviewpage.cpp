// Copyright (c) 2011-2015 The Bitcоin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QUrl>
#include <QNetworkRequest>
#include <QtCore/QVariant>
#include <QDesktopServices>
#include <QListWidget>
#include <QListWidgetItem>
#include <QJsonValue>
#include <QDateTime>
#include <QMessageBox>
#include <QPushButton>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

class TxViewDelegate : public QAbstractItemDelegate {

    Q_OBJECT
public:
    TxViewDelegate(const PlatformStyle *_platformStyle) :
    QAbstractItemDelegate(), unit(FirstCoinUnits::BTC),
    platformStyle(_platformStyle) {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
            const QModelIndex &index) const {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2 * ypad) / 2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top() + ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top() + ypad + halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if (value.canConvert<QBrush>()) {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool()) {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top() + ypad + halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if (amount < 0) {
            foreground = COLOR_NEGATIVE;
        } else if (!confirmed) {
            foreground = COLOR_UNCONFIRMED;
        } else {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = FirstCoinUnits::formatWithUnit(unit, amount, true, FirstCoinUnits::separatorAlways);
        if (!confirmed) {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
QWidget(parent),
ui(new Ui::OverviewPage),
clientModel(0),
walletModel(0),
currentBalance(-1),
currentUnconfirmedBalance(-1),
currentImmatureBalance(-1),
currentWatchOnlyBalance(-1),
currentWatchUnconfBalance(-1),
currentWatchImmatureBalance(-1),
txdelegate(new TxViewDelegate(platformStyle)),
filter(0) {
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager();
    connect(networkManager, &QNetworkAccessManager::finished, this, &OverviewPage::onResult);
    QNetworkRequest newsReq = QNetworkRequest(QUrl("http://api.fng-trade.com/v1/news"));
    newsReq.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    networkManager->get(newsReq);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64, 64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    //news
    //    QWebEnginePage *listNews = new QWebEnginePage(this);
    //    ui->preview->setPage(listNews);
    //    ui->newsList->setUrl(QUrl("https://firstcoin.world/news-wallet.php"));

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));


    connect(ui->newsList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(newsItemClicked(QListWidgetItem*)));
}

void OverviewPage::newsItemClicked(QListWidgetItem* item) {
    for (int i = 0; i < newsJsonArray.count() - 1; i++) {
        QJsonObject subtree = newsJsonArray.at(i).toObject();
        QDateTime newsDate = QDateTime::fromString(subtree.value("publish_at").toString(), "yyyy-MM-dd hh:mm:ss"); //2017-04-27 21:32:46
        QString tmp = newsDate.toString("dd.MM.yyyy");
        if (item->text().contains(tmp)) {
            if (item->text().contains(subtree.value("title").toString())) {
                QMessageBox msgBox;
                QPushButton *gositeButton = msgBox.addButton(tr("Открыть полную версию"), QMessageBox::ActionRole);
                QPushButton *closeButton = msgBox.addButton(tr("Закрыть"), QMessageBox::YesRole);
                msgBox.setTextFormat(Qt::RichText);
                msgBox.setText(QString("<p style='text-align:center;font-size:18px;font-weight: 600;'>") + subtree.value("title").toString() + QString("</p><p style='text-align:left;font-size:14px;font-weight: 300;'> ") + subtree.value("excerpt").toString() + QString(" </p>"));
                msgBox.setInformativeText(subtree.value("publish_at").toString()); //subtree.value("title").toString() + QString(" ") + 
                msgBox.setWindowTitle(subtree.value("title").toString());
                msgBox.exec();
                if (msgBox.clickedButton() == gositeButton) {
                    QDesktopServices::openUrl(QUrl(subtree.value("view_link").toString()));
                } else if (msgBox.clickedButton() == closeButton) {
                }
                //QDesktopServices::openUrl(QUrl(subtree.value("view_link").toString()));
            }
        }
    }
}

void OverviewPage::onResult(QNetworkReply* reply) {
    // Если ошибки отсутсвуют
    if (!reply->error()) {
        // То создаём объект Json Document, считав в него все данные из ответа
        QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        // Забираем из документа корневой объект
        newsJsonArray = document.array();
        // Перебирая все элементы массива ...
        for (int i = 0; i < newsJsonArray.count() - 1; i++) {
            QJsonObject subtree = newsJsonArray.at(i).toObject();
            // ui->newsList->addItem(QString(newsJsonArray.count()));
            QString row;
            QDateTime newsDate = QDateTime::fromString(subtree.value("publish_at").toString(), "yyyy-MM-dd hh:mm:ss"); //2017-04-27 21:32:46
            row = newsDate.toString("dd.MM.yyyy ");
            row.append(subtree.value("title").toString() + QString(" ")); //51 символ если что
            ui->newsList->addItem(row);
        }
    } else {
        ui->newsList->addItem(QString("Network error"));
    }
    reply->deleteLater();
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index) {

    if (filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks() {

    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage() {

    delete ui;
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance) {

    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;
    ui->labelBalance->setText(FirstCoinUnits::formatWithUnit(unit, balance, false, FirstCoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(FirstCoinUnits::formatWithUnit(unit, unconfirmedBalance, false, FirstCoinUnits::separatorAlways));
    ui->labelImmature->setText(FirstCoinUnits::formatWithUnit(unit, immatureBalance, false, FirstCoinUnits::separatorAlways));
    ui->labelTotal->setText(FirstCoinUnits::formatWithUnit(unit, balance + unconfirmedBalance + immatureBalance, false, FirstCoinUnits::separatorAlways));
    ui->labelWatchAvailable->setText(FirstCoinUnits::formatWithUnit(unit, watchOnlyBalance, false, FirstCoinUnits::separatorAlways));
    ui->labelWatchPending->setText(FirstCoinUnits::formatWithUnit(unit, watchUnconfBalance, false, FirstCoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(FirstCoinUnits::formatWithUnit(unit, watchImmatureBalance, false, FirstCoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(FirstCoinUnits::formatWithUnit(unit, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance, false, FirstCoinUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    bool showWatchOnlyImmature = watchImmatureBalance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance
}

// show/hide watch-only labels

void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly) {
    ui->labelSpendable->setVisible(showWatchOnly); // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly); // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly); // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly); // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly); // show watch-only total balance

    if (!showWatchOnly)
        ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model) {
    this->clientModel = model;
    if (model) {
        // Show warning if this is a prerelease version

        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model) {
    this->walletModel = model;
    if (model && model->getOptionsModel()) {
        // Set up transaction list

        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("FSTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit() {
    if (walletModel && walletModel->getOptionsModel()) {

        if (currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings) {

    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow) {
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
