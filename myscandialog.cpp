#include "myscandialog.h"
#include "ui_myscandialog.h"

MyScanDialog::MyScanDialog(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MyScanDialog)
{
	ui->setupUi(this);
}

MyScanDialog::~MyScanDialog()
{
	delete ui;
}

void MyScanDialog::initDialog(QStringList sourceList, QStringList modeList, QStringList areaList, QVector<int> dpiList)
{
	initSourceOption(sourceList);
	initModeOption(modeList);
	initScanAreaOption(areaList);
	initResolutionOption(dpiList);
	ui->pBar_scanProgress->setValue(0);
	ui->pBar_scanProgress->setMaximum(100);
}

void MyScanDialog::initSourceOption(QStringList strList)
{
	ui->cb_scanSource->addItems(strList);
	QStringList::iterator iterator = strList.begin();
	for (int i=0; iterator != strList.end(); i++,iterator++)
	{
		m_sourceList.insert(i, *iterator);
	}
	qDebug() << m_sourceList;
}

void MyScanDialog::initModeOption(QStringList strList)
{
	ui->cb_color->addItems(strList);
	QStringList::iterator iterator = strList.begin();
	for (int i=0; iterator != strList.end(); i++,iterator++)
	{
		m_modeList.insert(i, *iterator);
	}
	qDebug() << m_modeList;
}

void MyScanDialog::initScanAreaOption(QStringList strList)
{
	ui->cb_scanArea->addItems(strList);
	QStringList::iterator iterator = strList.begin();
	for (int i=0; iterator != strList.end(); i++,iterator++)
	{
		m_scanAreaList.insert(i, *iterator);
	}
	qDebug() << m_scanAreaList;
}

void MyScanDialog::initResolutionOption(QVector<int> intVec)
{
	QStringList strList;
	foreach (int var, intVec)
	{
		ui->cb_dpi->addItem(QString::number(var));
		strList.append(QString::number(var));
	}
	QStringList::iterator iterator = strList.begin();
	for (int i=0; iterator != strList.end(); i++,iterator++)
	{
		m_resolutionList.insert(i, *iterator);
	}
	qDebug() << m_resolutionList;
}

void MyScanDialog::setCurrentOption(QString source, QString mode, QString scanArea, QString dpi)
{
	qDebug() << "source:" << source << " mode:" << mode << " scanArea:" << scanArea <<  " dpi:" << dpi;
	if (ui->cb_scanSource->currentText() != source)
		ui->cb_scanSource->setCurrentIndex(m_sourceList.key(source));

	if (ui->cb_color->currentText() != mode)
		ui->cb_color->setCurrentIndex(m_modeList.key(mode));

	if (ui->cb_scanArea->currentText() != scanArea)
		ui->cb_scanArea->setCurrentIndex(m_scanAreaList.key(scanArea));

	if (ui->cb_dpi->currentText() != dpi)
		ui->cb_dpi->setCurrentIndex(m_resolutionList.key(dpi));

}

QString MyScanDialog::getSourceOption()
{
	return ui->cb_scanSource->currentText();
}

QString MyScanDialog::getResolutionOption()
{
	return ui->cb_dpi->currentText();
}

QString MyScanDialog::getModeOption()
{
	return ui->cb_color->currentText();
}

QString MyScanDialog::getAreaOption()
{
	return ui->cb_scanArea->currentText();
}

void MyScanDialog::setProBar(int v)
{
	ui->pBar_scanProgress->setValue(v);
}

void MyScanDialog::on_pb_Scan_clicked()
{
	emit sigStartScan();
}

void MyScanDialog::on_pb_getOptionInfo_clicked()
{
	emit sigGetOptionInfo();
}

void MyScanDialog::on_pb_setTest_clicked()
{
	emit sigSetSource();
}

void MyScanDialog::on_checkBox_atuo_stateChanged(int arg1)
{
	qDebug() << arg1;
	if(ui->checkBox_atuo->isChecked())
	{
		ui->cb_scanSource->setEnabled(false);
		ui->cb_color->setEnabled(false);
		ui->cb_scanArea->setEnabled(false);
		ui->cb_dpi->setEnabled(false);
		emit sigAutoAdapt(true);
	}
	else
	{
		ui->cb_scanSource->setEnabled(true);
		ui->cb_color->setEnabled(true);
		ui->cb_scanArea->setEnabled(true);
		ui->cb_dpi->setEnabled(true);
		emit sigAutoAdapt(false);
	}
}
