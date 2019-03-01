#include "myscanner.h"
#include "ui_myscanner.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>

using namespace kso;

#define MAX_SCAN_NUM 10

MyScanner::MyScanner(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MyScanner)
{
    ui->setupUi(this);
	m_sane = NULL;
	m_bGetDevice = false;
	m_bOpenStatus = false;
	m_deviceNum = 0;
	m_setDialog = NULL;
	m_bAutoAdapt = false;
}

MyScanner::~MyScanner()
{
	delete ui;
}

void MyScanner::createSetDialog()
{
	if (m_setDialog == NULL)
	{
		m_setDialog = new MyScanDialog(NULL);
		connect(m_setDialog, SIGNAL(sigStartScan()), this, SLOT(slotSaneStartScan()));
		connect(m_setDialog, SIGNAL(sigGetOptionInfo()), this, SLOT(slotGetOptionInfo()));
		connect(m_setDialog, SIGNAL(sigSetSource()), this, SLOT(slotSetSource()));

		m_setDialog->initDialog(m_sane->getSourceList(m_saneHandle),
								m_sane->getModeList(m_saneHandle),
								m_sane->getScanAreaList(m_saneHandle),
								m_sane->getResolutionVector(m_saneHandle));

		SANE_Char sourceStr[50]= {0};
		SANE_Char modeStr[50]= {0};
		SANE_Char scanAreaStr[50]= {0};
		SANE_Word resolution = 0;
		m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getSourceOptionIndex(m_saneHandle), sourceStr);
		m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getModeOptionIndex(m_saneHandle), modeStr);
		m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getScanAreaOptionIndex(m_saneHandle), scanAreaStr);
		m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getResolutionOptionIndex(m_saneHandle), &resolution);

		m_setDialog->setCurrentOption(QString(sourceStr), QString(modeStr), QString(scanAreaStr), QString::number(resolution));

		m_setDialog->setWindowModality(Qt::ApplicationModal);
		m_setDialog->show();
	}
	else
	{
		if (!m_setDialog->isVisible())
			m_setDialog->show();
	}
}

void MyScanner::closeSetDialog()
{
	if (m_setDialog)
	{
		delete m_setDialog;
		m_setDialog = NULL;
	}
}

void MyScanner::initScannerList()
{
	//获取扫描仪设备列表并将耗时的sane_get_devices（）放入线程中
	m_deviceList = NULL;
	ui->textEdit->append(QString(tr("获取设备信息以及调试信息 ... ...\n")));
	if (m_sane == NULL)
		m_sane = new Sane;
	connect(m_sane, SIGNAL(sigGetDeviceDone()), this, SLOT(slotGetDeviceDone()));
	m_thread = new QThread;
	connect(m_thread, SIGNAL(finished()), m_thread, SLOT(deleteLater()));
	m_thread->start();
	m_sane->moveToThread(m_thread);
	QTimer::singleShot(0, m_sane, SLOT(get_devices()));//使用发送信号触发m_sane的槽函数获取设备列表，使它在线程中运行

	//显示进度条对话框
	m_progressDialog = new QProgressDialog(QString("Scaning Current PC Linked Scanner ... ..."), QString(), 0, 30, NULL, Qt::WindowStaysOnTopHint);
	m_progressDialog->setWindowModality(Qt::ApplicationModal);
	m_progressDialog->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::WindowMinimizeButtonHint);
	m_progressDialog->setMaximum(30);//最多等候30秒
	m_progressDialog->setValue(0);
	m_timer.setInterval(1000);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(progressValueAdd()));
	m_timer.start();
	m_progressDialog->show();
	m_progressDialog->exec();

	//获取到设备信息列表，并将内容添加到ListWidget显示
	m_deviceList = m_sane->getDevicesList();
	ui->listWidget->clear();
	for (int i = 0; m_deviceList[i] != 0L; i++)
	{
		ui->textEdit->append(QString(m_deviceList[i]->name));
		ui->listWidget->addItem(QString(m_deviceList[i]->name));
		m_deviceNum++;
	}
}

void MyScanner::on_pushButton_stop_clicked()
{
	closeSetDialog();
	if (m_bOpenStatus)
	{
		ui->textEdit->append("Close the device");
		m_sane->close_device(m_saneHandle);
		m_saneHandle = NULL;
	}
	if(m_sane != NULL)
	{
		delete m_sane;
		m_sane = NULL;
	}
}

void MyScanner::on_pushButton_start_clicked()
{
	do
	{
		SANE_Status sane_status = SANE_STATUS_GOOD;
		if (!m_bOpenStatus)
		{
			// 3.打开指定设备
			if(ui->listWidget->currentItem() != NULL)
				ui->textEdit->append(QString("\nOpening Device ... %1 ...\n").arg(ui->listWidget->currentItem()->text()));

			if (m_deviceNum == 0)
				ui->textEdit->append("No device connected\n");
			else
				ui->textEdit->append("Device is not NULL \n");

			if(ui->listWidget->currentItem() != NULL)
			{
				int i = 0;
				for( ; m_deviceList[i] != 0L; i++)
				{
					if(QString(m_deviceList[i]->name) == ui->listWidget->currentItem()->text())
					{
						if (!(sane_status = m_sane->open_device(const_cast<SANE_Device *>(m_deviceList[i]), &m_saneHandle)))
						{
							m_bOpenStatus = true;
							ui->textEdit->append(QString("Open Sane Device %1 Success! \n").arg((char *)m_deviceList[0]->name));
						}
						else
						{
							ui->textEdit->append("Open Sane Device Failed! \n");
							break;
						}
					}
				}
			}
		}

		if(ui->listWidget->currentItem() != NULL)
			createSetDialog();
		else
			QMessageBox::about(NULL, "Warnig", "Please Select The Scanner Which You Will Open");

	} while(0);
}

void MyScanner::progressValueAdd()
{
	static int i=0;
	m_progressDialog->setValue(++i);
	QMutex mutex;
	QMutexLocker locker(&mutex);
	if(m_bGetDevice || i == 30)//最多等30秒
	{
		m_progressDialog->setValue(30);
		i = 0;
		m_timer.stop();
		if (m_thread->isRunning())
			m_thread->quit();
		m_thread->wait();
		m_progressDialog->close();//关闭进度条
	}
}

void MyScanner::slotSaneStartScan()
{
	qDebug() << "void MyScanner::slotSaneStartScan()";
	do
	{
		SANE_Status sane_status = SANE_STATUS_GOOD;
		if (!m_bOpenStatus)
		{
			// 3. open a device
			if(ui->listWidget->currentItem() != NULL)
				ui->textEdit->append(QString("\nOpening Device ... %1 ...\n").arg(ui->listWidget->currentItem()->text()));

			if (m_deviceNum == 0)
			{
				ui->textEdit->append("No device connected\n");
				break;
			}
			else
			{
				ui->textEdit->append("Device is not NULL \n");
			}

			if(ui->listWidget->currentItem() != NULL)
			{
				if (!(sane_status = m_sane->open_device(/*const_cast<SANE_Device *>(m_deviceList[0])*/
														(SANE_Device *)(ui->listWidget->currentItem()->text().toLocal8Bit().data()),
														&m_saneHandle)))
				{
					m_bOpenStatus = true;
					ui->textEdit->append(QString("Open Sane Device %1 Success! \n").arg((char *)m_deviceList[0]));
				}
				else
				{
					ui->textEdit->append("Open Sane Device Failed! \n");
					break;
				}
			}
			else
			{
				QMessageBox::about(NULL, "Warnig", "Please Select The Scanner Which You Will OPen");
			}
		}

		ui->textEdit->append("Scanning...\n ");

		//sane_start
		m_sane->start_sane(m_saneHandle);

		// 4. start scanning
		for (int i=0; i < MAX_SCAN_NUM && !sane_status; i++)
		{
			m_setDialog->setProBar(0);
			m_setDialog->update();
			QElapsedTimer t;t.start();
			QString tempFileStr = QDir::tempPath() + QDir::separator() +"Test"+ QString::number(QDateTime::currentMSecsSinceEpoch());
			m_setDialog->setProBar(50);
			sane_status = m_sane->start_read_while(tempFileStr.toLocal8Bit().data());
			qDebug() << "do_scan(fileName) " << t.elapsed();
			m_setDialog->setProBar(100);
			m_setDialog->update();
		}

		m_sane->cancle_scan(m_saneHandle);
	} while(0);

	m_setDialog->setProBar(0);
	m_setDialog->update();
}

void MyScanner::slotGetOptionInfo()
{
	qDebug() << "void MyScanner::slotGetOptionInfo()";

	if (m_saneHandle == (void *)NULL)
	{
		ui->textEdit->append("The m_saneHandle is null!!!");
	}
	else
	{
		int sum = m_sane->getOptionCount(m_saneHandle);
//		ui->textEdit->append(QString("The Current Scanner Support Option Number : ") +
//							 QString::number(sum));

//		int sourceIndex = m_sane->getSourceOptionIndex(m_saneHandle);
//		ui->textEdit->append(QString("The Current Scanner Source Index Number : ") +
//							 QString::number(sourceIndex));
//		ui->textEdit->append(QString("The Scanner Source Index Title : \n") +
//							 QString(m_sane->getOptionTitle(m_saneHandle, sourceIndex)));

//		for (int i=0; i<sum+1; i++)
//		{
//			ui->textEdit->append(QString("The Title : ") +
//								 QString(m_sane->getOptionTitle(m_saneHandle, i)) +
//								 QString("\nDESC:") +
//								 QString(m_sane->getOptionDescription(m_saneHandle, i)));
//		}
	}
	SANE_Char sourceStr[50]= {0};
	SANE_Char modeStr[50]= {0};
	SANE_Char scanAreaStr[50]= {0};
	SANE_Word resolution = 0;
	m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getSourceOptionIndex(m_saneHandle), sourceStr);
	m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getModeOptionIndex(m_saneHandle), modeStr);
	m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getScanAreaOptionIndex(m_saneHandle), scanAreaStr);
	m_sane->getCurrentOptionInfo(m_saneHandle, m_sane->getResolutionOptionIndex(m_saneHandle), &resolution);
	qDebug() << resolution;
}

void MyScanner::slotSetSource()
{
	qDebug() << "void MyScanner::slotSetSource()";
	QString sourceStr;
	QString modeStr;
	QString scanAreaStr;
	SANE_Word resolution = 0;
	sourceStr = m_setDialog->getSourceOption();
	modeStr = m_setDialog->getModeOption();
	scanAreaStr = m_setDialog->getAreaOption();
	resolution = m_setDialog->getResolutionOption().toInt();
	m_sane->setOptionInfo(m_saneHandle, m_sane->getSourceOptionIndex(m_saneHandle), sourceStr.toLocal8Bit().data(), m_bAutoAdapt);
	m_sane->setOptionInfo(m_saneHandle, m_sane->getModeOptionIndex(m_saneHandle), modeStr.toLocal8Bit().data(), m_bAutoAdapt);
	m_sane->setOptionInfo(m_saneHandle, m_sane->getScanAreaOptionIndex(m_saneHandle), scanAreaStr.toLocal8Bit().data(), m_bAutoAdapt);
	m_sane->setOptionInfo(m_saneHandle, m_sane->getResolutionOptionIndex(m_saneHandle), &resolution, m_bAutoAdapt);
}

void MyScanner::slotGetDeviceDone()
{
	QMutex mutex;
	QMutexLocker locker(&mutex);
	m_bGetDevice = true;
}

void MyScanner::on_pushButton_clicked()
{
	ui->textEdit->clear();
}


#ifdef __cplusplus
extern "C" {
#endif

#define STRIP_HEIGHT	256
static SANE_Handle device = NULL;
static int verbose;
static int progress = 0;
static SANE_Byte *buffer;
static size_t buffer_size;
static const char *prog_name;

static void
auth_callback (SANE_String_Const resource,
		   SANE_Char * username, SANE_Char * password)
{
}

#ifdef __cplusplus
}
#endif

void Sane::write_pnm_header (SANE_Frame format, int width, int height, int depth, FILE *ofp)
{
	switch (format)
	{
		case SANE_FRAME_RED:
		case SANE_FRAME_GREEN:
		case SANE_FRAME_BLUE:
		case SANE_FRAME_RGB:
			fprintf (ofp, "P6\n# SANE data follows\n%d %d\n%d\n", width, height, (depth <= 8) ? 255 : 65535);
			break;
		default:
			if (depth == 1)
				fprintf (ofp, "P4\n# SANE data follows\n%d %d\n", width, height);
			else
				fprintf (ofp, "P5\n# SANE data follows\n%d %d\n%d\n", width, height,(depth <= 8) ? 255 : 65535);
			break;
	}
}

void * Sane::advance (Image * image)
{
  if (++image->x >= image->width)
	{
	  image->x = 0;
	  if (++image->y >= image->height || !image->data)
	{
	  size_t old_size = 0, new_size;

	  if (image->data)
		old_size = image->height * image->width;

	  image->height += STRIP_HEIGHT;
	  new_size = image->height * image->width;

	  if (image->data)
		image->data = (uint8_t *)realloc (image->data, new_size);
	  else
		image->data = (uint8_t *)malloc (new_size);
	  if (image->data)
		memset (image->data + old_size, 0, new_size - old_size);
	}
	}
  if (!image->data)
	fprintf (stderr, "%s: can't allocate image buffer (%dx%d)\n",
		 prog_name, image->width, image->height);
  return image->data;
}

SANE_Status Sane::scan_it (FILE *ofp)
{
	int i, len, first_frame = 1, offset = 0, must_buffer = 0, hundred_percent;
	SANE_Byte min = 0xff, max = 0;
	SANE_Parameters parm;
	SANE_Status status;
	Image image = { 0, 0, 0, 0, 0 };
	static const char *format_name[] = {"gray", "RGB", "red", "green", "blue"};
	SANE_Word total_bytes = 0, expected_bytes;
	SANE_Int hang_over = -1;

	do
	{
		if(device != NULL)
		{
			SaneGetParameters sane_get_parameters = (SaneGetParameters)m_lsaneso.resolve("sane_get_parameters");
			status = sane_get_parameters (device, &parm);
		}
		else
		{
			QMessageBox::about(NULL, "Device is NULL", "Device is NULL");
			break;
		}

		if (status != SANE_STATUS_GOOD /*&& status != SANE_STATUS_JAMMED && status != SANE_STATUS_INVAL*/)
		  goto cleanup;


		if (first_frame)
		{
			switch (parm.format)
			{
				case SANE_FRAME_RED:
				case SANE_FRAME_GREEN:
				case SANE_FRAME_BLUE:
				  assert (parm.depth == 8);
				  must_buffer = 1;
				  offset = parm.format - SANE_FRAME_RED;
				  break;
				case SANE_FRAME_RGB:
				  assert ((parm.depth == 8) || (parm.depth == 16));
				case SANE_FRAME_GRAY:
					assert ((parm.depth == 1) || (parm.depth == 8) || (parm.depth == 16));
					if (parm.lines < 0)
					{
						must_buffer = 1;
						offset = 0;
					}
					else
					{
						write_pnm_header (parm.format, parm.pixels_per_line,parm.lines, parm.depth, ofp);
					}
				  break;
				default:
				  break;
			}

			if (must_buffer)
			{
				image.width = parm.bytes_per_line;
				if (parm.lines >= 0)
					image.height = parm.lines - STRIP_HEIGHT + 1;
				else
					image.height = 0;

				image.x = image.width - 1;
				image.y = -1;
				if (!advance (&image))
				{
					status = SANE_STATUS_NO_MEM;
					goto cleanup;
				}
			}
		}
		else
		{
			assert (parm.format >= SANE_FRAME_RED && parm.format <= SANE_FRAME_BLUE);
			offset = parm.format - SANE_FRAME_RED;
			image.x = image.y = 0;
		}

		hundred_percent = parm.bytes_per_line * parm.lines * ((parm.format == SANE_FRAME_RGB || parm.format == SANE_FRAME_GRAY) ? 1:3);

		while (1)
		{
			double progr;
			if(device != NULL)
			{
				SaneRead sane_read = (SaneRead)m_lsaneso.resolve("sane_read");
				status = sane_read (device, buffer, buffer_size, &len);qDebug() << "sane_read's status: " << status;
			}
			else
			{
				QMessageBox::about(NULL, "Device is NULL", "Device is NULL");
				break;
			}
			total_bytes += (SANE_Word) len;
			progr = ((total_bytes * 100.) / (double) hundred_percent);
			if (progr > 100.)
				goto cleanup;

			if (status != SANE_STATUS_GOOD /*&& status != SANE_STATUS_JAMMED && status != SANE_STATUS_INVAL*/)
			{
				if (status != SANE_STATUS_EOF)
				{
					return status;
				}
				break;
			}

			if (must_buffer)
			{
				switch (parm.format)
				{
					case SANE_FRAME_RED:
					case SANE_FRAME_GREEN:
					case SANE_FRAME_BLUE:
						for (i = 0; i < len; ++i)
						{
							image.data[offset + 3 * i] = buffer[i];
							if (!advance (&image))
							{
								status = SANE_STATUS_NO_MEM;
								goto cleanup;
							}
						}
						offset += 3 * len;
						break;
					case SANE_FRAME_RGB:
						for (i = 0; i < len; ++i)
						{
							image.data[offset + i] = buffer[i];
							if (!advance (&image))
							{
								status = SANE_STATUS_NO_MEM;
								goto cleanup;
							}
						}
						offset += len;
						break;
					case SANE_FRAME_GRAY:
						for (i = 0; i < len; ++i)
						{
							image.data[offset + i] = buffer[i];
							if (!advance (&image))
							{
								status = SANE_STATUS_NO_MEM;
								goto cleanup;
							}
						}
						offset += len;
						break;
					default:
						break;
				}
			}
			else			/* ! must_buffer */
			{
				if ((parm.depth != 16))
					fwrite (buffer, 1, len, ofp);//***
				else
				{
#if !defined(WORDS_BIGENDIAN)
					int i, start = 0;
					/* check if we have saved one byte from the last sane_read */
					if (hang_over > -1)
					{
						if (len > 0)
						{
							fwrite (buffer, 1, 1, ofp);
							buffer[0] = (SANE_Byte) hang_over;
							hang_over = -1;
							start = 1;
						}
					}
					/* now do the byte-swapping */
					for (i = start; i < (len - 1); i += 2)
					{
						unsigned char LSB;
						LSB = buffer[i];
						buffer[i] = buffer[i + 1];
						buffer[i + 1] = LSB;
					}
					/* check if we have an odd number of bytes */
					if (((len - start) % 2) != 0)
					{
						hang_over = buffer[len - 1];
						len--;
					}
#endif
					fwrite (buffer, 1, len, ofp);
				}
			}

			if (verbose && parm.depth == 8)
			{
			  for (i = 0; i < len; ++i)
				if (buffer[i] >= max)
					max = buffer[i];
				else if (buffer[i] < min)
					min = buffer[i];
			}
		}
		first_frame = 0;
	}while (!parm.last_frame);

	if (must_buffer)
	{
		image.height = image.y;
		write_pnm_header (parm.format, parm.pixels_per_line,image.height, parm.depth, ofp);

#if !defined(WORDS_BIGENDIAN)
		if (parm.depth == 16)
		{
			int i;
			for (i = 0; i < image.height * image.width; i += 2)
			{
				unsigned char LSB;
				LSB = image.data[i];
				image.data[i] = image.data[i + 1];
				image.data[i + 1] = LSB;
			}
		}
#endif
		fwrite (image.data, 1, image.height * image.width, ofp);
	}

	fflush( ofp );

cleanup:
	if (image.data)
		free (image.data);

	return status;
}

SANE_Status Sane::do_scan(const char *fileName)
{
	SANE_Status status = SANE_STATUS_GOOD;
	FILE *ofp = NULL;
	char path[PATH_MAX];
	char part_path[PATH_MAX];
	buffer_size = (32 * 1024 * 1024);
	buffer = (SANE_Byte *)malloc (buffer_size);

	do
	{
		sprintf (path, "%s.pnm", fileName);
		strcpy (part_path, path);
		strcat (part_path, ".part");
		if (NULL == (ofp = fopen (part_path, "w")))
		{
			status = SANE_STATUS_ACCESS_DENIED;
			break;
		}

		SaneSetIoMode sane_set_io_mode = (SaneSetIoMode)m_lsaneso.resolve("sane_set_io_mode");
		qDebug() << "sane_set_io_mode(device, SANE_TRUE) : " << sane_set_io_mode(device, SANE_FALSE);

		status = scan_it (ofp);

		switch (status)
		{
			case SANE_STATUS_GOOD:
			case SANE_STATUS_EOF:
				 {
					  status = SANE_STATUS_GOOD;
					  if (!ofp || 0 != fclose(ofp))
					  {
						  status = SANE_STATUS_ACCESS_DENIED;
						  break;
					  }
					  else
					  {
						  ofp = NULL;
						  if (rename (part_path, path))
						  {
							  status = SANE_STATUS_ACCESS_DENIED;
							  break;
						  }
					  }
				  }
				  break;
			default:
				  break;
		}
	}while (0);

	if (SANE_STATUS_GOOD != status && device != NULL)
	{
		SaneCancel sane_cancel = (SaneCancel)m_lsaneso.resolve("sane_cancel");
		sane_cancel (device);
	}
	if (ofp)
	{
		fclose (ofp);
		ofp = NULL;
	}
	if (buffer)
	{
		free (buffer);
		buffer = NULL;
	}

	return status;
}

//1. 初始化 SANE
void Sane::init()
{
	SANE_Int version_code = 0;
	SANE_Status sane_status = SANE_STATUS_GOOD;
	QElapsedTimer t;t.start();
	SaneInit sane_init = (SaneInit)m_lsaneso.resolve("sane_init");
	sane_status = sane_init (&version_code, auth_callback);
	qDebug("SANE version code: %d %d Take Time %d\n", version_code, sane_status, t.elapsed());
}

//2. 获取设备列表
SANE_Status Sane::get_devices(/*const SANE_Device ***device_list*/)
{
	qDebug("Get all devices...\n");
	SANE_Status sane_status = SANE_STATUS_GOOD;
	QElapsedTimer t;
	t.start();
	SaneGetDevices sane_get_devices = (SaneGetDevices)m_lsaneso.resolve("sane_get_devices");
	sane_status = sane_get_devices (&m_deviceList, SANE_FALSE);
	SaneStrStatus sane_strstatus = (SaneStrStatus)m_lsaneso.resolve("sane_strstatus");
	qDebug("sane_get_devices status: %s %d Take Time %d\n", sane_strstatus(sane_status), sane_status,t.elapsed());
	emit sigGetDeviceDone();
	return sane_status;
}

//3. 打开指定名称设备
SANE_Status Sane::open_device(SANE_Device *device, SANE_Handle *sane_handle)
{
	SANE_Status sane_status = SANE_STATUS_GOOD;
	qDebug("Name: %s\nvendor: %s\nmodel: %s\ntype: %s\n", device->name, device->model, device->vendor, device->type);
	QElapsedTimer t;t.start();
	SaneOpen sane_open = (SaneOpen)m_lsaneso.resolve("sane_open");
	sane_status = sane_open(device->name, sane_handle);/*)*/
	SaneStrStatus sane_strstatus = (SaneStrStatus)m_lsaneso.resolve("sane_strstatus");
	qDebug("sane_open status: %s %d Take time %d\n", sane_strstatus(sane_status), sane_status,t.elapsed());

	return sane_status;
}

//
SANE_Status Sane::start_sane(SANE_Handle sane_handle)
{
	SANE_Status status = SANE_STATUS_GOOD;
	device = sane_handle;
	if(device != NULL)
	{
		SaneStart sane_start = (SaneStart)m_lsaneso.resolve("sane_start");
		status = sane_start (device);qDebug() << "sane_start's status: " << status;
	}
	else
		QMessageBox::about(NULL, "Device is NULL", "Device is NULL");
	return status;
}

// Start scanning
SANE_Status Sane::start_read_while(SANE_String_Const fileName)
{
	QElapsedTimer t;t.start();
//    SANE_Status sane_status = SANE_STATUS_GOOD;
	return do_scan(fileName);
	qDebug() << "do_scan(fileName) " << t.elapsed();
}

// Cancel scanning
void Sane::cancle_scan(SANE_Handle sane_handle)
{
	SaneCancel sane_cancel = (SaneCancel)m_lsaneso.resolve("sane_cancel");
	sane_cancel(sane_handle);
}

// Close SANE device
void Sane::close_device(SANE_Handle sane_handle)
{
	SaneClose sane_close = (SaneClose)m_lsaneso.resolve("sane_close");
	sane_close(sane_handle);
}

// Release SANE resources
void Sane::exit()
{
	SaneExit sane_exit = (SaneExit)m_lsaneso.resolve("sane_exit");
	sane_exit();
}

//
QString Sane::pnm2png(QString path)
{
	QString imageFromScanner = QString("/tmp/tempfile%1").arg(QTime::currentTime().toString("hhmmss"));
	QImage pnm(path);
	QImage png;
	png = pnm.copy(0, 0, 2479, 3508);//A4 size
	png.save(imageFromScanner, "png");
	return imageFromScanner;
}

const SANE_Device ** Sane::getDevicesList()
{
	return m_deviceList;
}

bool Sane::isOptionSettable(SANE_Handle hd, int num)
{
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor sgod = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc=sgod(/*mDeviceHandle*/hd,num);
	if (option_desc)
		if (SANE_OPTION_IS_SETTABLE(option_desc->cap) == SANE_TRUE)
			return true;
	return false;
}

int Sane::getOptionCount(SANE_Handle hd)
{
	SANE_Int count;
	SANE_Status status;
	SaneControlOption sco = (SaneControlOption)m_lsaneso.resolve("sane_control_option");
	status = sco(hd, 0, SANE_ACTION_GET_VALUE, (void *)&count, (SANE_Int *)NULL);
	if (status == SANE_STATUS_GOOD)
	{
		m_optionNumber = count;
		return (int)count;
	}
	return -1;
}

QString Sane::getOptionTitle(SANE_Handle hd, int num)
{
	QString qs;
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor sgod = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc = sgod(hd,num);
	if (option_desc)
		qs = QString(option_desc->title);
	return qs;
}

QString Sane::getOptionDescription(SANE_Handle hd, int num)
{
	QString qs;
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor sgod = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc=sgod(hd,num);
	if (option_desc != NULL)
		qs = option_desc->desc;//qApp->translate("QScanner",option_desc->desc);
	return qs;
}

int Sane::getSourceOptionIndex(SANE_Handle hd)
{
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
//	int i;
//	i=0;//0 is No Option
	const SANE_Option_Descriptor *option_desc;
	for (int i = 1;i<m_optionNumber;i++)
	{
		option_desc=optionInfo(hd,i);
		if (option_desc)
			if ((QString(option_desc->name)==SANE_NAME_SCAN_SOURCE) &&
					(isOptionSettable(hd, i) == true))
				return i;
	}
	return 0;
}

int Sane::getResolutionOptionIndex(SANE_Handle hd)
{
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");

	const SANE_Option_Descriptor *option_desc;
	for (int i = 1;i<m_optionNumber;i++)
	{
		option_desc=optionInfo(hd,i);
		if (option_desc)
			if ((QString(option_desc->name)==SANE_NAME_SCAN_RESOLUTION) &&
					(isOptionSettable(hd, i) == true))
				return i;
	}
	return 0;
}

int Sane::getModeOptionIndex(SANE_Handle hd)
{
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");

	const SANE_Option_Descriptor *option_desc;
	for (int i = 1;i<m_optionNumber;i++)
	{
		option_desc=optionInfo(hd,i);
		if (option_desc)
			if ((QString(option_desc->name)==SANE_NAME_SCAN_MODE) &&
					(isOptionSettable(hd, i) == true))
				return i;
	}
	return 0;
}

int Sane::getScanAreaOptionIndex(SANE_Handle hd)
{
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");

	const SANE_Option_Descriptor *option_desc;
	for (int i = 1;i<m_optionNumber;i++)
	{
		option_desc=optionInfo(hd,i);
		if (option_desc)
			if ((QString(option_desc->name)==SANE_NAME_GEOMETRY) &&
					(isOptionSettable(hd, i) == true))
				return i;
	}
	return 0;
}

QStringList Sane::getSourceList(SANE_Handle hd)
{
	return getStringList(hd, getSourceOptionIndex(hd));
}

QStringList Sane::getModeList(SANE_Handle hd)
{
	return getStringList(hd, getModeOptionIndex(hd));
}

QStringList Sane::getScanAreaList(SANE_Handle hd)
{
	return getStringList(hd, getScanAreaOptionIndex(hd));
}

QVector<int> Sane::getResolutionVector(SANE_Handle hd)
{
	return saneWordList(hd, getResolutionOptionIndex(hd));
}

SANE_Constraint_Type Sane::getConstraintType(SANE_Handle hd, int num)
{
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc = optionInfo(hd, num);
	if(option_desc)
		return option_desc->constraint_type;
	return SANE_CONSTRAINT_NONE;
}

SANE_Value_Type Sane::getOptionType(SANE_Handle hd, int num)
{
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc = optionInfo(hd, num);
	return option_desc->type;
}

int Sane::optionSize(SANE_Handle hd, int num)
{
  const SANE_Option_Descriptor *option_desc;
  SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
  option_desc = optionInfo(hd, num);
  if(option_desc)
	return option_desc->size;
  return -1;
}

SANE_Word Sane::getRangeMin(SANE_Handle hd, int num)
{
	QVector <SANE_Word> qa;
	SANE_Word sw;
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc=optionInfo(hd, num);
	if(option_desc)
	{
		if(option_desc->constraint_type == SANE_CONSTRAINT_RANGE)
		{
			if(option_desc->type == SANE_TYPE_FIXED)
			{
				return SANE_Word(option_desc->constraint.range->min);
			}
			else
			{
				return option_desc->constraint.range->min;
			}
		}
		else if(option_desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
		{
			qa = saneWordList(hd, num);
			sw = INT_MAX;
			for(unsigned int i=0;i<qa.size();i++)
			if(qa[i] < sw) sw = qa[i];
			if(sw < INT_MAX) return sw;
		}
	}
	return  INT_MIN;
}

SANE_Word Sane::getRangeMax(SANE_Handle hd, int num)
{
  QVector <SANE_Word> qa;
  SANE_Word sw;
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc=optionInfo(hd, num);
  if(option_desc)
  {
	if(option_desc->constraint_type == SANE_CONSTRAINT_RANGE)
	{
		if(option_desc->type == SANE_TYPE_FIXED)
		{
			return SANE_Word(option_desc->constraint.range->max);
		}
		else
		{
			return option_desc->constraint.range->max;
		}
	}
	else if(option_desc->constraint_type == SANE_CONSTRAINT_WORD_LIST)
	{
	  qa = saneWordList(hd, num);
	  sw = INT_MIN;
	  for(unsigned int i=0;i<qa.size();i++)
		if(qa[i] > sw) sw = qa[i];
	  return sw;
	}
  }
	return  INT_MIN;
}

QVector<SANE_Word> Sane::saneWordList(SANE_Handle hd, int num)
{
	int c;
	int i;
	QVector<SANE_Word> a;
	a.resize(0);
	SANE_Status status;
	status = SANE_STATUS_INVAL;

	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc=optionInfo(hd, num);
	if(option_desc)
	{
		if(getConstraintType(hd, num) == SANE_CONSTRAINT_WORD_LIST)
		{
			//first element in list holds the number of values
			c = (int)option_desc->constraint.word_list[0];
			a.resize(c);
			for(i=0;i<c;i++)
				a[i] = (SANE_Word)option_desc->constraint.word_list[i+1];
		}
	}
	return a;
}

SANE_String_Const Sane::getStringListItem(SANE_Handle hd, int index, int item)
{
  int c;
	c = 0;
	const SANE_Option_Descriptor *option_desc;
  if(item>=0)
  {
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc = optionInfo(hd,index);
	if(option_desc)
	{
	  if(option_desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
	  {
		  for(c=0;c<item;c++)
		  {
			if(!option_desc->constraint.string_list[c]) return 0L;
		}
		return option_desc->constraint.string_list[c];
	  }
	}
  }
  return 0L;
}

QStringList Sane::getStringList(SANE_Handle hd, int option)
{
	QStringList slist;
	int c;
	c = 0;
	const SANE_Option_Descriptor *option_desc;
	SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
	option_desc = optionInfo(hd, option);
	if(option_desc)
	{
		if(option_desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
		{
			while(option_desc->constraint.string_list[c])
			{
				slist.append(option_desc->constraint.string_list[c]);
				++c;
			}
		}
	}
	return slist;
}

QList<SANE_String_Const> Sane::getStrList(SANE_Handle hd, int option)
{
  QList<SANE_String_Const> slist;
  int c;
	c = 0;
	const SANE_Option_Descriptor *option_desc;
  SaneGetOptionDescriptor optionInfo = (SaneGetOptionDescriptor)m_lsaneso.resolve("sane_get_option_descriptor");
  option_desc = optionInfo(hd, option);

  if(option_desc)
  {
	if(option_desc->constraint_type == SANE_CONSTRAINT_STRING_LIST)
	{
	  while(option_desc->constraint.string_list[c])
	  {
		slist.append(option_desc->constraint.string_list[c]);
		++c;
		}
	}
  }
  for(unsigned int i=0;i<slist.count();i++)
	qDebug("getStrList: %s",(const char*)slist.at(i));

  return slist;
}

void Sane::getCurrentOptionInfo(SANE_Handle hd, int index, void *str)
{
	SaneControlOption sco = (SaneControlOption)m_lsaneso.resolve("sane_control_option");
	sco(hd, index, SANE_ACTION_GET_VALUE, str, 0);
	qDebug() <<"getCurrentOptionInfo : " << QString((char *)str);
}

SANE_Status Sane::setOptionInfo(SANE_Handle hd, SANE_Int index, void* v, bool automatic)
{
	SANE_Status status;
	SANE_Int i=0;
	void* pval;
	SANE_Int si;
	SANE_Fixed sf;
	SANE_Value_Type stype;
	QStringList strList;
	QVector<SANE_Word> intList;
	SANE_Constraint_Type ct = getConstraintType(hd, index);
	pval = v;
	stype = getOptionType(hd, index);
	if (ct == SANE_CONSTRAINT_RANGE)
	{
		qDebug() << "getRangeMin(hd, index) : " << getRangeMin(hd, index);
		qDebug() << "getRangeMax(hd, index) : " << getRangeMax(hd, index);

//		if ((stype == SANE_TYPE_INT) &&
//				(optionSize(hd, index) == sizeof(SANE_Int)))
//		{
//			if(*(SANE_Int*)v < getRangeMin(hd, index))
//			{
//				si = getRangeMin(hd, index);
//				pval = &si;
//			}
//			if(*(SANE_Int*)v > getRangeMax(hd, index))
//			{
//				si = getRangeMax(hd, index);
//				pval = &si;
//			}
//		}

//		if((stype == SANE_TYPE_FIXED) &&
//				(optionSize(hd, index) == sizeof(SANE_Fixed)))
//		{
//			if(*(SANE_Fixed*)v < getRangeMin(hd, index))
//			{
//				sf = getRangeMin(hd, index);
//				pval = &sf;
//			}
//			if(*(SANE_Fixed*)v > getRangeMax(hd, index))
//			{
//				sf = getRangeMax(hd, index);
//				pval = &sf;
//			}
//		}
	}
	else if(ct == SANE_CONSTRAINT_STRING_LIST)
	{
		qDebug() << "ct == SANE_CONSTRAINT_STRING_LIST";
		qDebug() << getStringList(hd, index);
//		strList = getStringList(hd, index);
	}
	else if(ct == SANE_CONSTRAINT_WORD_LIST)
	{
		qDebug() << "ct == SANE_CONSTRAINT_WORD_LIST";
		qDebug( ) << saneWordList(hd, index);
//		intList = saneWordList(hd, index);
	}
	else
	{
		qDebug() << "ct == SANE_CONSTRAINT_NONE";
	}

	//1.Source(one face/double face) && 3.Color(grey black White) && 7.Size("A4","A5")
	SaneControlOption sco = (SaneControlOption)m_lsaneso.resolve("sane_control_option");
//	SANE_Char saneStr[50]= {0};
//	sco(hd, index, SANE_ACTION_GET_VALUE, saneStr, 0);
//	qDebug() << "saneStr :" << saneStr << " saneStr :" << QString(saneStr);
	//2.DPI(resolution)
//	SANE_Word temp = 0;
//	sco(hd, index, SANE_ACTION_GET_VALUE, (void *)&temp, 0);
//	qDebug() << "currnet dpi : " << temp;
//	temp = intList.at(0);


	if(automatic)
		status = sco(hd, index, SANE_ACTION_SET_AUTO, 0L, &i);
	else
		status = sco(hd, index, SANE_ACTION_SET_VALUE, (void *)pval, &i);
//		status = sco(hd, index, SANE_ACTION_SET_VALUE, strList[0].toLocal8Bit().data(), &i);
//		status = sco(hd, index, SANE_ACTION_SET_VALUE, (void *)&temp, &i);
//		status = sco(hd, index, SANE_ACTION_SET_VALUE, (void *)&pval, &i);
	qDebug() << "SANE_ACTION_SET_VALUE : " << i;
	return status;
}

