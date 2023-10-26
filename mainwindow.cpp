#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdint.h>

#include "pmd.h"
#include "usb-1408FS.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    int flag;
    signed short svalue;
    uint8_t channel, gain;
    int count;
    int temp;
    time_t startTime, endTime;

    ui->setupUi(this);

    libusb_device_handle *udev = NULL;

    int ret = libusb_init(NULL);
    if (ret < 0) {
        perror("libusb_init: Failed to initialize libusb");
        exit(1);
    }

    if ((udev = usb_device_find_USB_MCC(USB1408FS_PID, NULL))) {
        printf("USB-1408FS Device is found!\n");
    }
    else {
        printf("No USB-1408FS found.\n");
        exit(0);
    }

    init_USB1408FS(udev);

    printf("Connect pin 1 - pin 21  and pin 2 - pin 3\n");
    printf("Select channel [0-3]: ");
    scanf("%d", &temp);
    if((temp < 0) || (temp > 3))
        return;
    channel = (uint8_t) temp;
    printf("\t\t1. +/- 20.V\n");
    printf("\t\t2. +/- 10.V\n");
    printf("\t\t3. +/- 5.V\n");
    printf("\t\t4. +/- 4.V\n");
    printf("\t\t5. +/- 2.5V\n");
    printf("\t\t6. +/- 2.0V\n");
    printf("\t\t7. +/- 1.25V\n");
    printf("\t\t8. +/- 1.0V\n");
    printf("Select gain: [1-8]\n");
    scanf("%d", &temp);
    switch(temp) {
    case 1: gain = BP_20_00V;
        break;
    case 2: gain = BP_10_00V;
        break;
    case 3: gain = BP_5_00V;
        break;
    case 4: gain = BP_4_00V;
        break;
    case 5: gain = BP_2_50V;
        break;
    case 6: gain = BP_2_00V;
        break;
    case 7: gain = BP_1_25V;
        break;
    case 8: gain = BP_1_00V;
        break;
    default:
        break;
    }
    flag = fcntl(fileno(stdin), F_GETFL);
    fcntl(0, F_SETFL, flag | O_NONBLOCK);
    do {
        usbDOut_USB1408FS(udev, DIO_PORTA, 0);
        sleep(1);
        svalue = usbAIn_USB1408FS(udev, channel, gain);
        printf("Channel: %d: value = %#hx, %.2fV\n",
               channel, svalue, volts_1408FS(gain, svalue));
        usbDOut_USB1408FS(udev, DIO_PORTA, 0x1);
        sleep(1);
        svalue = usbAIn_USB1408FS(udev, channel, gain);
        printf("Channel: %d: value = %#hx, %.2fV\n",
               channel, svalue, volts_1408FS(gain, svalue));
    } while (!isalpha(getchar()));

    fcntl(fileno(stdin), F_SETFL, flag);
    printf("Doing a timing test.  Please wait ...\n");
    time(&startTime);
    for(count=0; count<500; count++) {
        svalue = usbAIn_USB1408FS(udev, channel, gain);
    }
    time(&endTime);
    printf("Sampling speed is %.0f Hz.\n", 500./(endTime - startTime));
}


MainWindow::~MainWindow() {
    delete ui;
}

