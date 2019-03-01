#ifndef MYSCANNER_H
#define MYSCANNER_H

#include <QWidget>
#include <QLibrary>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QTime>
#include <QThread>
#include <QTimer>
#include <QProgressDialog>
#include <QDebug>

#include "sane/sanei.h"
#include "sane/saneopts.h"

#include <myscandialog.h>

typedef struct
{
  unsigned char *data;
  int width;
  int height;
  int x;
  int y;
}
Image;

namespace Ui {
class MyScanner;
}

namespace kso {
typedef SANE_Status (* SaneInit) (SANE_Int *, SANE_Auth_Callback);
typedef SANE_Status (* SaneGetDevices) (const SANE_Device ***, SANE_Bool);
typedef SANE_Status (* SaneOpen) (SANE_String_Const, SANE_Handle *);

//extern const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle handle, SANE_Int option);
typedef const SANE_Option_Descriptor* (* SaneGetOptionDescriptor) (SANE_Handle , SANE_Int );

typedef SANE_Status (* SaneControlOption) (SANE_Handle, SANE_Int, SANE_Action, void*, SANE_Int *);

typedef SANE_Status (* SaneStart) (SANE_Handle);
typedef SANE_Status (* SaneGetParameters) (SANE_Handle, SANE_Parameters *);

typedef SANE_Status (* SaneRead) (SANE_Handle, SANE_Byte *, SANE_Int, SANE_Int *);
typedef SANE_String_Const (* SaneStrStatus) (SANE_Status status);

//extern SANE_Status sane_set_io_mode (SANE_Handle handle,
//				     SANE_Bool non_blocking);
//extern SANE_Status sane_get_select_fd (SANE_Handle handle,
//				       SANE_Int * fd);

typedef SANE_Status (* SaneSetIoMode) (SANE_Handle, SANE_Bool);

typedef void (* SaneCancel) (SANE_Handle);
typedef void (* SaneClose) (SANE_Handle);
typedef void (* SaneExit) (void);
}

class Sane;
class MyScanner : public QWidget
{
    Q_OBJECT

public:
    explicit MyScanner(QWidget *parent = 0);
    ~MyScanner();
	void createSetDialog();
	void closeSetDialog();

	void initScannerList();

private slots:
    void on_pushButton_clicked();

	void on_pushButton_stop_clicked();

	void on_pushButton_start_clicked();

public slots:
	void progressValueAdd();
	void slotSaneStartScan();
	void slotGetOptionInfo();
	void slotSetSource();
	void slotGetDeviceDone();

private:
    Ui::MyScanner *ui;
	QThread *m_thread;
    Sane *m_sane;
	MyScanDialog *m_setDialog;
	QProgressDialog *m_progressDialog;
	QTimer m_timer;

	SANE_Handle m_saneHandle;
	const SANE_Device ** m_deviceList;

	bool m_bGetDevice;
	bool m_bOpenStatus;
	int m_deviceNum;

	bool m_bAutoAdapt;

};

class Sane : public QObject
{
	Q_OBJECT
public:
    Sane()
    {
		m_lsaneso.setFileNameAndVersion("sane", 1);
		qDebug() << "libsane Positon : " << m_lsaneso.fileName();
		if(m_lsaneso.load())
		{
//			QMessageBox::about(NULL, "libsane.so", "EXist libsane.so!\nPlease Continue Next!");
		}
		else
		{
//			QMessageBox::about(NULL, "XXXXXXXXXXX", "Dont EXist libsane.so/libsane.so.1!\nPlease install sane and make ln -s libsane.so.1\n  $: sudo apt-get install sane\n  $: sudo ln -s /usr/lib64/libsane.so.1 /usr/lib/libsane.so!");
		}
		//1. 初始化sane
		init();
    }

	~Sane() {exit();}//退出sane}

    void init();

    SANE_Status open_device(SANE_Device *device, SANE_Handle *sane_handle);
	SANE_Status start_sane(SANE_Handle sane_handle);
	SANE_Status start_read_while(SANE_String_Const fileName);
    SANE_Status do_scan(const char *fileName);
    SANE_Status scan_it (FILE *ofp);
    void *advance (Image *);
    void write_pnm_header (SANE_Frame, int, int, int, FILE *);
    void cancle_scan(SANE_Handle sane_handle);
    void close_device(SANE_Handle sane_handle);
    void exit();
	QString pnm2png(QString path);
	const SANE_Device ** getDevicesList();

	bool isOptionSettable(SANE_Handle, int);
	int getOptionCount(SANE_Handle);
	int getSourceOptionIndex(SANE_Handle hd);
	int getModeOptionIndex(SANE_Handle hd);
	int getScanAreaOptionIndex(SANE_Handle hd);
	int getResolutionOptionIndex(SANE_Handle hd);
	QStringList getSourceList(SANE_Handle hd);
	QStringList getModeList(SANE_Handle hd);
	QStringList getScanAreaList(SANE_Handle hd);
	QVector<int> getResolutionVector(SANE_Handle hd);

	QString getOptionTitle(SANE_Handle, int);
	QString getOptionDescription(SANE_Handle hd, int num);
	SANE_Constraint_Type getConstraintType(SANE_Handle, int);
	SANE_Value_Type getOptionType(SANE_Handle, int);
	int optionSize(SANE_Handle , int );
	SANE_Word getRangeMin(SANE_Handle , int );
	SANE_Word getRangeMax(SANE_Handle , int );
	QVector<SANE_Word> saneWordList(SANE_Handle , int );
	SANE_String_Const getStringListItem(SANE_Handle, int, int);
	QStringList getStringList(SANE_Handle, int);
	QList<SANE_String_Const> getStrList(SANE_Handle, int);
	void getCurrentOptionInfo(SANE_Handle, int, void *);

	SANE_Status setOptionInfo(SANE_Handle, SANE_Int, void*, bool);

signals:
	void sigGetDeviceDone();

protected slots:
	SANE_Status get_devices(/*const SANE_Device ***device_list*/);

private:
    QLibrary m_lsaneso;
	const SANE_Device ** m_deviceList;
	SANE_Handle m_deviceHandle;
	int m_optionNumber;
	bool m_isOpen;
};

#endif // MYSCANNER_H
