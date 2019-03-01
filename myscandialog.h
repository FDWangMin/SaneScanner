#ifndef MYSCANDIALOG_H
#define MYSCANDIALOG_H

#include <QWidget>
#include <QComboBox>
#include <QDebug>

namespace Ui {
class MyScanDialog;
}

class MyScanDialog : public QWidget
{
	Q_OBJECT

public:
	explicit MyScanDialog(QWidget *parent = 0);
	~MyScanDialog();

	void initDialog(QStringList sourceList, QStringList modeList, QStringList areaList, QVector<int> dpiList);
	void initSourceOption(QStringList);
	void initResolutionOption(QVector<int>);
	void initModeOption(QStringList);
	void initScanAreaOption(QStringList);

	void setCurrentOption(QString, QString, QString, QString);

	QString getSourceOption();
	QString getResolutionOption();
	QString getModeOption();
	QString getAreaOption();

	void setProBar(int);

private slots:
	void on_pb_Scan_clicked();

	void on_pb_getOptionInfo_clicked();

	void on_pb_setTest_clicked();

	void on_checkBox_atuo_stateChanged(int arg1);

signals:
	void sigStartScan();
	void sigGetOptionInfo();
	void sigSetSource();
	void sigAutoAdapt(bool);

private:
	Ui::MyScanDialog *ui;

	QMap<int, QString> m_sourceList;
	QMap<int, QString> m_modeList;
	QMap<int, QString> m_scanAreaList;
	QMap<int, QString> m_resolutionList;
};

#endif // MYSCANDIALOG_H
