#include <QtCore>
#include <QtWidgets>
#include <QtNetwork>

#include "snake_generated.h"

using namespace SnakeGame;

#define SETTINGS_NAME "losh-slitherio-client"

class LoginForm : public QWidget
{
	Q_OBJECT
	public:
		LoginForm();
		QLineEdit *server, *login, *password, *field;
		QCheckBox *needNoSendPos;
	public slots:
		void start();
};

class GameForm;

class GameWidget : public QWidget
{
	Q_OBJECT
	public:
		GameWidget();
		void paintEvent(QPaintEvent *e) override;
		void mousePressEvent(QMouseEvent *e) override;
		void mouseReleaseEvent(QMouseEvent *e) override;
		void mouseMoveEvent(QMouseEvent *e) override;
		void keyPressEvent(QKeyEvent *e) override;
		QByteArray fieldBuf;
		QTransform trn;
		int playerId;
		int snakeId;
		double w = 0;
		bool boost = false;
		double ratio = 5;
		QPointF currentMousePosition, currentHeadPosition;
		QSize sizeHint() const override
		{
			return QSize(800, 800);
		}
};

class GameForm : public QWidget
{
	Q_OBJECT
	public:
		GameForm(const QString& s, const QString& l, const QString& p, const QString& f);
		QTcpSocket *sock;
		QByteArray sizeBuf, bodyBuf;
		GameWidget *gw;
		void error(const QString& text);
		bool needSendPos;

	public slots:
		void sockReadyRead();
		void sockError();
		void sockDisconnected();

	private:
		void sendPackage(const flatbuffers::FlatBufferBuilder& fbb);
		void processMessage(const QByteArray& message);
		void processWelcome(const Welcome* pkg);
		void processError(const Error* pkg);
		void sendPos();
		void updateInfo();

		QLabel *head, *direction, *w, *snakeid, *playerid;

	friend class GameWidget;
};

GameWidget::GameWidget()
{
	setMouseTracking(true);
}

LoginForm::LoginForm()
{
	setWindowTitle("Snake login");
	QSettings settings(SETTINGS_NAME);
	QFormLayout *l = new QFormLayout;
	server = new QLineEdit(settings.value("server").toString());
	l->addRow("Server", server);
	login = new QLineEdit(settings.value("login").toString());
	l->addRow("Login", login);
	password = new QLineEdit(settings.value("password").toString());
	password->setEchoMode(QLineEdit::Password);
	l->addRow("Password", password);
	field = new QLineEdit(settings.value("field").toString());
	l->addRow("Field", field);
	needNoSendPos = new QCheckBox("Don't send position");
	l->addRow(needNoSendPos);
	setLayout(l);
	QPushButton *start = new QPushButton("Start");
	l->addRow(start);
	QObject::connect(start, &QPushButton::clicked, this, &LoginForm::start);
}

void LoginForm::start()
{
	QSettings settings(SETTINGS_NAME);
	settings.setValue("server", server->text());
	settings.setValue("login", login->text());
	settings.setValue("password", password->text());
	settings.setValue("field", field->text());
	auto x = new GameForm(server->text(), login->text(), password->text(), field->text());
	if (needNoSendPos->isChecked())
	{
		x->needSendPos = false;
	}
	x->show();
	deleteLater();
}

GameForm::GameForm(const QString& s, const QString& _l, const QString& p, const QString& f):
	needSendPos(true)
{
	sock = new QTcpSocket;
	QObject::connect(sock, &QTcpSocket::readyRead, this, &GameForm::sockReadyRead);
	QObject::connect(sock, &QTcpSocket::connected, [this, _l, p, f]()
		{
			qDebug() << "Connected!";
			flatbuffers::FlatBufferBuilder fbb;
			auto login = fbb.CreateString(_l.toStdString());
			auto password = fbb.CreateString(p.toStdString());
			auto w = CreateLogin(fbb, login, password, f.toInt());
			auto pkg = CreatePackage(fbb, PackageType_Login, w.Union());
			FinishPackageBuffer(fbb, pkg);
			sendPackage(fbb);
		});

	QObject::connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(sockError()));
	QObject::connect(sock, &QTcpSocket::disconnected, this, &GameForm::sockDisconnected);

	QStringList hostParts = s.split(":");
	sock->connectToHost(hostParts.value(0), hostParts.value(1).toInt());	

	QHBoxLayout *l = new QHBoxLayout;
	gw = new GameWidget;
	l->addWidget(gw);
	QFormLayout *fl = new QFormLayout;
	fl->addRow("Head", head = new QLabel);
	fl->addRow("Direction", direction = new QLabel);
	fl->addRow("W", w = new QLabel);
	fl->addRow("Player", playerid = new QLabel);
	fl->addRow("SnakeId", snakeid = new QLabel);
	l->addLayout(fl);
	setLayout(l);
	gw->setFocus();
}

void GameForm::sendPackage(const flatbuffers::FlatBufferBuilder& fbb)
{
	char size[4];
	qToBigEndian<int32_t>(fbb.GetSize(), reinterpret_cast<uchar*>(size));
	sock->write(size, sizeof(size));
	sock->write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
}

void GameWidget::paintEvent(QPaintEvent *)
{
	if (fieldBuf.isEmpty())
	{
		return;
	}
	QPainter painter(this);
	painter.setRenderHints(QPainter::Antialiasing);
	painter.fillRect(0, 0, sizeHint().width(), sizeHint().height(), Qt::black);
	auto field = static_cast<const Field*>(GetPackage(fieldBuf.data())->pkg());
	snakeId = field->snake_id();
	w = field->w();
	/* Find head coord */
	QPointF head;
	for (auto i : *field->snakes())
	{
		if (i->player_id() == playerId && i->snake_id() == field->snake_id())
		{
			head.rx() = i->skeleton()->Get(0)->x();
			head.ry() = i->skeleton()->Get(0)->y();
			break;
		}
	}

	currentHeadPosition = head;

	trn = QTransform();
	trn.translate(sizeHint().width() / 2, sizeHint().height() / 2);
	trn.scale(ratio, ratio);
	trn.translate(-head.x(), -head.y());
	painter.setTransform(trn);

	/* Draw food */
	painter.setPen(QColor(Qt::yellow));
	QFont foodFont; foodFont.setPointSize(1);
	painter.setFont(foodFont);
	for (auto i : *field->foods())
	{
		painter.drawText(QPointF(i->p().x(), i->p().y()), QString::number(i->w(), 'f', 0));
	}

	painter.setPen(QColor(Qt::yellow));
	painter.setBrush(Qt::white);
	for (auto i : *field->snakes())
	{
		for (auto j : *i->skeleton())
		{
			painter.drawEllipse(QPointF(j->x(), j->y()), i->r(), i->r());
		}
	}
}

void GameWidget::mousePressEvent(QMouseEvent*)
{
	boost = true;
}

void GameWidget::mouseReleaseEvent(QMouseEvent*)
{
	boost = false;
}

void GameWidget::mouseMoveEvent(QMouseEvent *e)
{
	currentMousePosition = e->localPos();
	QWidget::mouseMoveEvent(e);
}

void GameWidget::keyPressEvent(QKeyEvent *e)
{
	if (e->text() == "-")
	{
		--ratio;
	}
	else if (e->text() == "=")
	{
		++ratio;
	}
	else
	{
		QWidget::keyPressEvent(e);
	}
	qBound(1.0, ratio, 10.0);
}

void GameForm::sendPos()
{
	if (!needSendPos) return;
	auto coord = gw->trn.inverted().map(gw->currentMousePosition);
	flatbuffers::FlatBufferBuilder fbb;
	auto point = Point(coord.x(), coord.y());
	auto d = CreateDirection(fbb, gw->snakeId, &point, gw->boost);
	auto pkg = CreatePackage(fbb, PackageType_Direction, d.Union());
	FinishPackageBuffer(fbb, pkg);
	sendPackage(fbb);
}

void GameForm::updateInfo()
{
	head->setText(QString("(%1, %2)").arg(gw->currentHeadPosition.x()).arg(gw->currentHeadPosition.y()));
	auto d = gw->trn.inverted().map(gw->currentMousePosition);
	direction->setText(QString("(%1, %2)").arg(d.x()).arg(d.y()));
	playerid->setText(QString::number(gw->playerId));
	snakeid->setText(QString::number(gw->snakeId));
	w->setText(QString::number(gw->w));
}

void GameForm::sockReadyRead()
{
	if (sizeBuf.size() < 4)
	{
		sizeBuf.append(sock->read(4 - sizeBuf.size()));
		bodyBuf.clear();
	}
	if (sizeBuf.size() == 4)
	{
		int len = qFromBigEndian<int32_t>(reinterpret_cast<const uchar*>(sizeBuf.data()));
		bodyBuf.append(sock->read(len - bodyBuf.size()));
		if (bodyBuf.size() == len)
		{
			processMessage(bodyBuf);
			bodyBuf.clear();
			sizeBuf.clear();
		}
	}
}

void GameForm::processMessage(const QByteArray& message)
{
	auto pkg = GetPackage(message.data());
	switch (pkg->pkg_type())
	{
		case PackageType_Welcome:
			processWelcome(static_cast<const Welcome*>(pkg->pkg()));
		break;
		case PackageType_Error:
			processError(static_cast<const Error*>(pkg->pkg()));
		break;
		case PackageType_Field:
			gw->fieldBuf = message;
			gw->repaint();
			updateInfo();
			sendPos();
		break;
		default:
			error("Unknown package type arrived: " + QString::number(pkg->pkg_type()));
	}
}

void GameForm::processWelcome(const Welcome* pkg)
{
	qDebug() << "Authenticated!";
	gw->playerId = pkg->player_id();
	qDebug() << "Player id" << gw->playerId;
}

void GameForm::processError(const Error* pkg)
{
	error(QString::fromStdString(pkg->description()->str()));
}

void GameForm::sockError()
{
	qDebug() << "Socket error" << sock->errorString();
	error("Socket error: " + sock->errorString());
}

void GameForm::sockDisconnected()
{
	qDebug() << "Socket disconnected";
	error("Socket disconnected");
}

void GameForm::error(const QString& text)
{
	QMessageBox::critical(this, "Error", text);
	deleteLater();
}

int main(int ac, char** av)
{
	QApplication app(ac, av);
	(new LoginForm)->show();
	return app.exec();
}

#include "main.moc"
