#include "mainwindow.h"

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QTextStream>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QThread>
#include <QTime>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QTimer>




QVector<int> globalBitArray;
QVector<QVector<int>> crcList;


// 1) QString -> Bit Array
QVector<int> stringToBitArray(const QString& content) {
    QVector<int> bitArray;
    QByteArray bytes = content.toUtf8();
    for (char byte : bytes) {
        for (int i = 7; i >= 0; --i){
            bitArray.append((byte >> i) & 1);
        }
    }
    return bitArray;
}

// 2) İlk 100 biti al, geri kalanı sil
QVector<int> extractFirst100Bits(QVector<int>& bits) {
    QVector<int> first100 = bits.mid(0, 100);
    bits.erase(bits.begin(), bits.begin() + qMin(100, bits.size()));

    // Eğer 100'den azsa 0'larla tamamla
    while (first100.size() < 100) {
        first100.append(0);
    }

    return first100;
}

// 3) 0 0 + verilen sayıyı 4 byte (32 bit) + data
QVector<int> addHeader00AndNumber(int number, const QVector<int>& data) {
    QVector<int> result = {0, 0};
    for (int i = 31; i >= 0; --i)
        result.append((number >> i) & 1);
    result += data;
    return result;
}

// 4) 0 1 veya 1 0 + sayı (4 byte)
QVector<int> addHeaderAndNumberBits(int bit1, int bit2, int number) {
    QVector<int> result = {bit1, bit2};
    for (int i = 31; i >= 0; --i)
        result.append((number >> i) & 1);
    return result;
}

// 5) CRC hesapla (örnek XOR CRC-16-CCITT için)
QVector<int> calculateCRC(QVector<int> data, quint16 polynomial = 0x1021) {
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= (data[i] << 15);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ polynomial;
            else
                crc <<= 1;
        }
    }
    QVector<int> result;
    for (int i = 15; i >= 0; --i)
        result.append((crc >> i) & 1);
    return result;
}

// 6) Bit array'e başka bir bit array ekle
void appendBitArrays(QVector<int>& dest, const QVector<int>& src) {
    dest += src;
}

// 7-10) Header kontrol fonksiyonları
bool isData(const QVector<int>& bits)     { return bits.size() >= 2 && bits[0] == 0 && bits[1] == 0; }
bool isACK(const QVector<int>& bits)      { return bits.size() >= 2 && bits[0] == 0 && bits[1] == 1; }
bool isNACK(const QVector<int>& bits)     { return bits.size() >= 2 && bits[0] == 1 && bits[1] == 0; }
bool isChecksum(const QVector<int>& bits) { return bits.size() >= 2 && bits[0] == 1 && bits[1] == 1; }

// 11) CRC checksumları toplayan fonksiyon (bitwise XOR)
QVector<int> calculateChecksumFromCRCs(const QVector<QVector<int>>& crcs) {
    if (crcs.isEmpty()) return {};
    QVector<int> checksum = crcs[0];
    for (int i = 1; i < crcs.size(); ++i) {
        for (int j = 0; j < checksum.size(); ++j)
            checksum[j] ^= crcs[i][j];
    }
    return checksum;
}

// 12) İlk iki biti ayır, gerisini güncelle
QVector<int> extractFirstTwoBits(QVector<int>& bits) {
    QVector<int> firstTwo = bits.mid(0, 2);
    bits.erase(bits.begin(), bits.begin() + qMin(2, bits.size()));
    return firstTwo;
}

// 13) İlk 4 byte (32 bit) -> int, sonra sil
int extractNumberFromBits(QVector<int>& bits) {
    if (bits.size() < 32) return 0;
    int number = 0;
    for (int i = 0; i < 32; ++i) {
        number = (number << 1) | bits[i];
    }
    bits.erase(bits.begin(), bits.begin() + 32);
    return number;
}

// 14) CRC doğrulama (bit array + crc bits)
bool verifyCRC(const QVector<int>& dataWithCRC, quint16 polynomial = 0x1021) {
    QVector<int> dataOnly = dataWithCRC.mid(0, dataWithCRC.size() - 16);
    QVector<int> expectedCRC = calculateCRC(dataOnly, polynomial);
    QVector<int> actualCRC = dataWithCRC.mid(dataWithCRC.size() - 16);
    return expectedCRC == actualCRC;
}

// 15) Bit arrayi stringe çevirme
QString bitArrayToString(const QVector<int>& bits) {
    QString result;
    for (int bit : bits) {
        result += QString::number(bit); // 0 veya 1
    }
    return result;
}
// 16) Bit arrayi hex stringe çevirme
QString bitArrayToHexString(const QVector<int>& bits) {
    QByteArray byteArray;

    for (int i = 0; i < bits.size(); i += 8) {
        quint8 byte = 0;
        for (int j = 0; j < 8 && (i + j) < bits.size(); ++j) {
            byte = (byte << 1) | bits[i + j];
        }
        byte <<= (8 - (qMin(8, bits.size() - i))); // Eksik bit varsa sola kaydır
        byteArray.append(byte);
    }

    return byteArray.toHex().toUpper();
}
//17
void wait(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, SLOT(quit()));
    loop.exec();
}
//18
QString nextFrame(QVector<int>& gidenFrame, int& frameNumber){
    frameNumber++;
    gidenFrame.clear();
    QVector<int> dataArray = extractFirst100Bits(globalBitArray);
    QVector<int> header = addHeaderAndNumberBits(0,0,frameNumber);
    QVector<int> crc = calculateCRC(dataArray);
    crcList.append(crc);
    gidenFrame.append(header);
    gidenFrame.append(dataArray);
    gidenFrame.append(crc);

    QString value = "00-";
    value += std::to_string(frameNumber);
    value += "-";
    value += bitArrayToString(dataArray);
    value += "-";
    value += bitArrayToString(crc);
    return value;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    srand(QTime::currentTime().msec());

    QWidget window;
    window.setWindowTitle("Veri İletişimi ve Bilgisayar Ağları Ödevi");
    window.resize(1000, 800);

    QVBoxLayout *mainLayout = new QVBoxLayout();

    QLabel *titleLabel = new QLabel("Veri İletişimi ve Bilgisayar Ağları Ödevi");
    titleLabel->setFont(QFont("Arial", 20, QFont::Bold));
    mainLayout->addWidget(titleLabel);

    QHBoxLayout *topRow = new QHBoxLayout();

    QVBoxLayout *teamLayout = new QVBoxLayout();
    teamLayout->addWidget(new QLabel("Yasin Aktaş\nRubar Gündüz\nFatih Öztürk\nBerkay Gözübüyük"));

    QVBoxLayout *buttonLayout = new QVBoxLayout();
    QPushButton *readButton = new QPushButton("Dat Dosyasını Oku");
    QPushButton *startButton = new QPushButton("İletişimi Başlat");
    QPushButton *finishAllButton = new QPushButton("Tamamını Bitir");
    buttonLayout->addWidget(readButton);
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(finishAllButton);

    QVBoxLayout *checksumLayout = new QVBoxLayout();
    QLabel *checksumTitle = new QLabel("CHECKSUM TABLOSU");
    checksumTitle->setAlignment(Qt::AlignCenter);
    checksumTitle->setFont(QFont("Arial", 11, QFont::Bold));

    QLabel *checksumValue = new QLabel("Değer: -");
    checksumValue->setAlignment(Qt::AlignCenter);
    checksumValue->setFont(QFont("Courier New", 10));

    checksumLayout->addWidget(checksumTitle);
    checksumLayout->addWidget(checksumValue);
    checksumLayout->addStretch();

    QHBoxLayout *buttonAndChecksum = new QHBoxLayout();
    buttonAndChecksum->addLayout(buttonLayout);
    buttonAndChecksum->addSpacing(30);
    buttonAndChecksum->addLayout(checksumLayout);
    buttonAndChecksum->addStretch();

    topRow->addLayout(teamLayout);
    topRow->addSpacing(60);
    topRow->addLayout(buttonAndChecksum);
    topRow->addStretch();

    mainLayout->addLayout(topRow);

    QLabel *infoLabel = new QLabel("Durum bilgisi burada gösterilecek.");
    infoLabel->setWordWrap(true);

    QTableWidget *table = new QTableWidget();
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Gönderilen Data", "→ CRC", "→ Alınan Data", "→ ACK/NACK"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFont(QFont("Courier New", 10));
    table->setFixedHeight(600);
    table->setFixedWidth(1000);

    bool *skipDelays = new bool(false);

    QObject::connect(finishAllButton, &QPushButton::clicked, [&]() {
        *skipDelays = true;
    });

    QObject::connect(readButton, &QPushButton::clicked, [&]() {
        globalBitArray.clear();
        crcList.clear();
        table->setRowCount(0);
        checksumValue->setText("Değer: -");

        QString filePath = QFileDialog::getOpenFileName(&window, "DAT Dosyası Seç", "", "DAT Dosyaları (*.dat)");
        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString content = in.readAll();
                file.close();
                globalBitArray = stringToBitArray(content);
                infoLabel->setText("Bit dizisi oluşturuldu. Bit sayısı: " + QString::number(globalBitArray.size()));
            } else {
                infoLabel->setText("Dosya açılamadı.");
            }
        }
    });

    QObject::connect(startButton, &QPushButton::clicked, [&]() {
        QVector<int> gidenFrame, gelenFrame, ackFrame;
        int gidenFrameNumber = -1, gelenFrameNumber = -1;
        bool resend = false;

        while (globalBitArray.size() > 0 || resend || !isACK(ackFrame)) {
            if ((isACK(ackFrame) || ackFrame.isEmpty()) && !resend) {
                nextFrame(gidenFrame, gidenFrameNumber);
            }

            int row = table->rowCount();
            table->insertRow(row);

            QString gidenStr = "Frame " + QString::number(gidenFrameNumber) + ": " +
                               bitArrayToString(gidenFrame.mid(34, gidenFrame.size() - 50));
            QPlainTextEdit *gidenText = new QPlainTextEdit(gidenStr);
            gidenText->setReadOnly(true);
            gidenText->setFixedHeight(50);
            gidenText->setLineWrapMode(QPlainTextEdit::NoWrap);
            gidenText->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            gidenText->setStyleSheet("background-color: #D0E9FF;");
            table->setCellWidget(row, 0, gidenText);

            QVector<int> crcBits = gidenFrame.mid(gidenFrame.size() - 16);
            QPlainTextEdit *crcText = new QPlainTextEdit(bitArrayToString(crcBits));
            crcText->setReadOnly(true);
            crcText->setFixedHeight(50);
            crcText->setLineWrapMode(QPlainTextEdit::NoWrap);
            crcText->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            crcText->setStyleSheet("background-color: #D0E9FF;");
            table->setCellWidget(row, 1, crcText);

            QPlainTextEdit *alinanEdit = new QPlainTextEdit("Alınıyor...");
            alinanEdit->setReadOnly(true);
            alinanEdit->setFixedHeight(50);
            alinanEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
            alinanEdit->setStyleSheet("background-color: #FFF9C4;");
            table->setCellWidget(row, 2, alinanEdit);

            QPlainTextEdit *ackText = new QPlainTextEdit("");
            ackText->setReadOnly(true);
            ackText->setFixedHeight(50);
            ackText->setLineWrapMode(QPlainTextEdit::NoWrap);
            table->setCellWidget(row, 3, ackText);

            if (!(*skipDelays)) wait(1500);

            int r = rand() % 100;
            QString gelenStr;
            resend = false;

            if (r < 10) {
                gelenFrame.clear();
                gelenStr = "Frame kayboldu. – Tekrar gönderilecek.";
                resend = true;
            } else if (r < 30) {
                gelenFrame = gidenFrame;
                int index = (rand() % 100) + 34;
                gelenFrame[index] = 1 - gelenFrame[index];
                gelenStr = "Frame bozuldu. – Tekrar gönderilecek.";
                resend = true;
            } else {
                gelenFrame = gidenFrame;
                gelenStr = "Frame " + QString::number(gidenFrameNumber) + ": " +
                           bitArrayToString(gelenFrame.mid(34, gelenFrame.size() - 50));
            }

            QPlainTextEdit *gelenText = new QPlainTextEdit(gelenStr);
            gelenText->setReadOnly(true);
            gelenText->setFixedHeight(50);
            gelenText->setLineWrapMode(QPlainTextEdit::NoWrap);
            gelenText->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            gelenText->setStyleSheet("background-color: #FFF9C4;");
            table->setCellWidget(row, 2, gelenText);

            //if (!(*skipDelays)) wait(500);

            QString ackType = "ACK";
            r = rand() % 100;
            if (isData(gelenFrame)) {
                extractFirstTwoBits(gelenFrame);
                int gelenNo = extractNumberFromBits(gelenFrame);
                if (r < 15) {
                    ackFrame.clear();
                    ackType = "ACK yolda kayboldu";
                    resend = true;
                } else {
                    if (verifyCRC(gelenFrame)) {
                        gelenFrameNumber++;
                        ackFrame = addHeaderAndNumberBits(0, 1, gelenNo);
                        ackType = "ACK";
                    } else {
                        ackFrame = addHeaderAndNumberBits(1, 0, gelenNo);
                        ackType = "NACK – Tekrar gönderilecek.";
                        resend = true;
                    }
                }
            } else {
                ackType = "ACK/NACK oluşmadı – Tekrar gönderilecek.";
                resend = true;
            }

            ackText->setPlainText("Gönderiliyor... (" + ackType + ")");
            ackText->setStyleSheet("background-color: #E0E0E0;");
            if (!(*skipDelays)) wait(500);
            ackText->setPlainText(ackType);
            bool isNegative = ackType.contains("NACK") || ackType.contains("yolda kayboldu") || ackType.contains("oluşmadı");
            ackText->setStyleSheet(isNegative ? "background-color: #FFCDD2;" : "background-color: #C8E6C9;");
        }

        QVector<int> checksum = calculateChecksumFromCRCs(crcList);
        QString sumStr = bitArrayToHexString(checksum);
        checksumValue->setText("Değer: " + sumStr);
    });

    mainLayout->addWidget(table);
    mainLayout->addWidget(infoLabel);
    window.setLayout(mainLayout);
    window.show();
    return app.exec();
}

