#ifndef HP3478TAB_H
#define HP3478TAB_H

#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QLabel>


class hp3478Tab : public QWidget
{
    Q_OBJECT
public:
    explicit hp3478Tab(int iConfiguration, QWidget *parent = 0);
    void restoreSettings();
    void saveSettings();

signals:

public slots:
    void onMeasureIntervalEdit_textChanged(const QString &arg1);

protected:
    void setToolTips();
    void setCaptions();
    void initUI();
    void connectSignals();
    bool isIntervalValid(double interval);

public:
    double dInterval;

private:
    // Limit Values
    const double intervalMin;
    const double intervalMax;

    // QLineEdit styles
    QString sNormalStyle;
    QString sErrorStyle;

    // UI Elements
    QLineEdit    MeasureIntervalEdit;

    int          myConfiguration;
};

#endif // HP3478TAB_H
