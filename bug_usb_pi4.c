
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>




#define YOCTO_SERIAL_LEN 20
#define YOCTO_VENDORID   0x24e0
#define YOCTO_PKT_SIZE   64


#define NB_PKT_TO_TEST 15

typedef struct _yInterfaceSt {
    unsigned                vendorid;
    unsigned                deviceid;
    unsigned                ifaceno;
    char                    serial[YOCTO_SERIAL_LEN];
    libusb_device           *devref;
    libusb_device_handle    *hdl;
    unsigned                sent_pkt;
    unsigned                received_pkt;
    unsigned                rdendp;
    unsigned                wrendp;
    struct libusb_transfer  *rdTr;
    unsigned char           incomming_pkt[YOCTO_PKT_SIZE];
} yInterfaceSt;




unsigned char config_pkt[YOCTO_PKT_SIZE] = {
    0x00, 0x15, 0x09, 0x02, 0x01, 0x00, 0x01, 0x00,
    0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


unsigned char response_pkt[YOCTO_PKT_SIZE] = {
    0x00, 0x15, 0x09, 0x02, 0x01, 0x00, 0x01, 0x00,
    0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


int verbose = 0;



#define ySetErr(intro, err)  ySetErrEx(__LINE__, intro, err)

static int ySetErrEx(unsigned line, char *intro, int err)
{
    const char *msg;
    switch (err) {
    case LIBUSB_SUCCESS:
        msg = "Success (no error)";
        break;
    case LIBUSB_ERROR_IO:
        msg = "Input/output error";
        break;
    case LIBUSB_ERROR_INVALID_PARAM:
        msg = "Invalid parameter";
        break;
    case LIBUSB_ERROR_ACCESS:
        msg = "Access denied (insufficient permissions)";
        break;
    case LIBUSB_ERROR_NO_DEVICE:
        msg = "No such device (it may have been disconnected)";
        break;
    case LIBUSB_ERROR_NOT_FOUND:
        msg = "Entity not found";
        break;
    case LIBUSB_ERROR_BUSY:
        msg = "Resource busy";
        break;
    case LIBUSB_ERROR_TIMEOUT:
        msg = "Operation timed out";
        break;
    case LIBUSB_ERROR_OVERFLOW:
        msg = "Overflow";
        break;
    case LIBUSB_ERROR_PIPE:
        msg = "Pipe error";
        break;
    case LIBUSB_ERROR_INTERRUPTED:
        msg = "System call interrupted (perhaps due to signal)";
        break;
    case LIBUSB_ERROR_NO_MEM:
        msg = "Insufficient memory";
        break;
    case LIBUSB_ERROR_NOT_SUPPORTED:
        msg = "Operation not supported or unimplemented on this platform";
        break;
    default:
    case LIBUSB_ERROR_OTHER:
        msg = "Other error";
        break;
    }
    if (intro) {
        printf("%s:%s\n", intro, msg);
    } else {
        printf("LIN(%d):%s\n", line, msg);
    }
    return err;
}



// get the USB configuration descriptor
static int getDevConfig(libusb_device *dev, struct libusb_config_descriptor **config)
{
    int res = libusb_get_active_config_descriptor(dev, config);
    if (res == LIBUSB_ERROR_NOT_FOUND) {
        printf("not yet configured\n");
        if (libusb_get_config_descriptor(dev, 0, config) != 0) {
            return -1;
        }
    } else if(res != 0) {
        printf("unable to get active configuration %d\n", res);
        return -1;
    }
    return 0;

}


// get a USB string
static int getUsbStringASCII(libusb_device_handle *hdl, libusb_device *dev, unsigned desc_index, char *data, unsigned length)
{
    unsigned char buffer[512];
    unsigned l, len;
    int res, i;

    res = libusb_control_transfer(hdl, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | desc_index,
                                  0, buffer, 512, 10000);
    if (res < 0) {
        return ySetErr("Unable to get device list", res);
    }

    len = (buffer[0] - 2) / 2;
    if (len >= length) {
        len = length - 1;
    }

    for (l = 0; l < len; l++) {
        data[l] = (char) buffer[2 + (l * 2)];
    }
    data[len] = 0;
    return len;
}


// forward definition of rd_callback
static void rd_callback(struct libusb_transfer *transfer);



// fill and submit an interrupt transfer
static int submitReadPkt(yInterfaceSt *iface)
{
    int res;
    libusb_fill_interrupt_transfer( iface->rdTr,
                                    iface->hdl,
                                    iface->rdendp,
                                    (unsigned char*)iface->incomming_pkt,
                                    YOCTO_PKT_SIZE,
                                    rd_callback,
                                    iface,
                                    0);
    res = libusb_submit_transfer(iface->rdTr);
    if (res < 0) {
        return ySetErr("libusb_submit_transfer(RD) failed", res);
    }
    return 0;
}


// fill and submit an interrupt transfer
static void rd_callback(struct libusb_transfer *transfer)
{
    int res;
    yInterfaceSt *iface = (yInterfaceSt*)transfer->user_data;
    if (iface == NULL) {
        printf("- rd_callback: drop invalid ypkt rd_callback (iface is null)\n");
        return;
    }

    switch(transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        if ( transfer->actual_length == YOCTO_PKT_SIZE && memcmp(iface->incomming_pkt, response_pkt,7)==0) {
            printf("- rd_callback for %s : response %d is valid (len=%d)\n", iface->serial, iface->received_pkt, transfer->actual_length);
            iface->received_pkt++;
        } else {
            int i,j;
            printf("- rd_callback for %s : response  %d is INVAID! (len=%d)\n", iface->serial, iface->received_pkt, transfer->actual_length);
            printf("Incoming pkt:\n");
            for(i = 0; i < 8; i++) {
                for(j = 0; j < 7; j++) {
                    printf("0x%02x ",iface->incomming_pkt[i * 8 + j]);
                }
                printf("0x%02x\n",iface->incomming_pkt[i * 8 + 7]);

            }
        }
        break;
    case LIBUSB_TRANSFER_ERROR:
        printf("- rd_callback for %s : pkt error (len=%d)\n", iface->serial, transfer->actual_length);
        break;
    case LIBUSB_TRANSFER_TIMED_OUT :
        printf("- rd_callback for %s : pkt timeout\n", iface->serial);
        break;
    case LIBUSB_TRANSFER_CANCELLED:
        printf("- rd_callback for %s : pkt_cancelled (len=%d) \n", iface->serial, transfer->actual_length);
        return;
    case LIBUSB_TRANSFER_STALL:
        printf("- rd_callback for %s : pkt stall\n", iface->serial );
        res = libusb_cancel_transfer(iface->rdTr);
        printf("- rd_callback for %s : libusb_cancel_transfer returned %d\n", iface->serial, res);
        res = libusb_clear_halt(iface->hdl, iface->rdendp);
        printf("- rd_callback for %s : libusb_clear_hal returned %d\n", iface->serial, res);
        break;
    case LIBUSB_TRANSFER_NO_DEVICE:
        printf("- rd_callback for %s : no_device (len=%d)\n", iface->serial, transfer->actual_length);
        return;
    case LIBUSB_TRANSFER_OVERFLOW:
        printf("- rd_callback for %s : pkt_overflow (len=%d)\n", iface->serial, transfer->actual_length);
        return;
    default:
        printf("- rd_callback for %s : unknown state %X\n", iface->serial, transfer->status);
        return;
    }

    res = submitReadPkt(iface);
    if (res < 0) {
        printf("- rd_callback for %s : libusb_submit_transfer error %X\n", iface->serial, res);
    }

}







int main(int argc, char* argv[])
{
    int res;
    int nbifaceDetect, i;
    libusb_context      *libusb;
    const struct libusb_version *libusb_v;
    libusb_device   **list;
    ssize_t         nbdev;
    int             returnval = 0;
    unsigned             alloc_size;
    yInterfaceSt    *iface, *ifaces;
    int exit_on_error = 0;

    printf("this is a simple program that exhibit a bug in the USB stack\n");
    printf("of the Raspberry PI Zero and any Yoctopuce device\n");

    for (i = 1 ; i < argc; i++) {
        printf("%d:%s\n",i, argv[i]);
        if(strcmp(argv[i],"verbose")==0){
            printf("Enable verbose mode\n");
            verbose=1;
        }
        if(strcmp(argv[i],"exit_on_error")==0){
            printf("Enable exit_on_error mode\n");
            exit_on_error=1;
        }
    }

    // init libUSB
    res = libusb_init(&libusb);
    if(res != 0) {
        return ySetErr("Unable to start lib USB", res);
    }
    // dump actual version of the libUSB
    libusb_v = libusb_get_version();
    printf("Use libUSB v%d.%d.%d.%d\n", libusb_v->major, libusb_v->minor, libusb_v->micro, libusb_v->nano);


    printf("List all Yoctopuce devices present on this host:\n");

    nbdev = libusb_get_device_list(libusb, &list);
    if (nbdev < 0)
        return ySetErr("Unable to get device list", nbdev);

    if (verbose) {
            printf("[%d usb devices present]\n", i);
    }

    // allocate buffer for detected interfaces
    nbifaceDetect = 0;
    alloc_size = nbdev  * sizeof(yInterfaceSt);
    ifaces = (yInterfaceSt*) malloc(alloc_size);
    memset(ifaces, 0, alloc_size);
    for (i = 0; i < nbdev; i++) {
        int  res;
        struct libusb_device_descriptor desc;
        struct libusb_config_descriptor *config;
        libusb_device_handle *hdl;

        libusb_device  *dev = list[i];
        if ((res = libusb_get_device_descriptor(dev, &desc)) != 0) {
            returnval = ySetErr("Unable to get device descriptor", res);
            return -1;
        }

        if (verbose) {
            printf("[parse device %d = %X:%X]\n", i, desc.idVendor, desc.idProduct);
        }

        if (desc.idVendor != YOCTO_VENDORID) {
            continue;
        }
        if(getDevConfig(dev, &config) < 0) {
            continue;
        }
        iface = ifaces + nbifaceDetect;
        iface->vendorid = (unsigned)desc.idVendor;
        iface->deviceid = (unsigned)desc.idProduct;
        iface->ifaceno  = 0;
        iface->devref   = libusb_ref_device(dev);
        res = libusb_open(dev, &hdl);
        if (res == LIBUSB_ERROR_ACCESS) {
            printf("the user has insufficient permissions to access USB devices\n");
            return -1;
        }
        if (res != 0) {
            printf("unable to access device %X:%X\n", desc.idVendor, desc.idProduct);
            return -1;
        }

        res = getUsbStringASCII( hdl, dev, desc.iSerialNumber, iface->serial, YOCTO_SERIAL_LEN);
        if (res < 0) {
            printf("unable to get serial for device %X:%X\n", desc.idVendor, desc.idProduct);
        }
        libusb_close(hdl);
        nbifaceDetect++;
        printf(" - USB dev: %s (%X:%X:%d)\n", iface->serial, iface->vendorid, iface->deviceid, iface->ifaceno);
        libusb_free_config_descriptor(config);
    }
    libusb_free_device_list(list, 1);
    printf("%d Yoctopuce devices are present\n", nbifaceDetect);

    for (i = 0; i < nbifaceDetect; i++) {
        int p;
        yInterfaceSt *iface = ifaces + i;
        unsigned char pkt[YOCTO_PKT_SIZE];
        int res, j;
        int error;
        struct libusb_config_descriptor *config;
        const struct libusb_interface_descriptor* ifd;

        printf("\\ Test device %s (%X:%X)\n", iface->serial, iface->vendorid, iface->deviceid);

        if((res = libusb_open(iface->devref, &iface->hdl)) != 0) {
            return ySetErr("libusb_open", res);
        }

        if((res = libusb_kernel_driver_active(iface->hdl, iface->ifaceno)) < 0) {
            return ySetErr("libusb_kernel_driver_active", res);
        }
        if (res) {
            printf("- need to detach kernel driver\n");
            if((res = libusb_detach_kernel_driver(iface->hdl, iface->ifaceno)) < 0) {
                return ySetErr("libusb_detach_kernel_driver", res);
            }
        }
        if((res = libusb_claim_interface(iface->hdl, iface->ifaceno)) < 0) {
            return ySetErr("libusb_claim_interface", res);
        }

        res = getDevConfig(iface->devref, &config);
        if(res < 0) {
            return res;
        }

        ifd = &config->interface[iface->ifaceno].altsetting[0];
        for (j = 0; j < ifd->bNumEndpoints; j++) {
            if((ifd->endpoint[j].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
                iface->rdendp = ifd->endpoint[j].bEndpointAddress;
            } else {
                iface->wrendp = ifd->endpoint[j].bEndpointAddress;
            }
        }

        iface->rdTr = libusb_alloc_transfer(0);
        res = submitReadPkt(iface);
        if (res < 0) {
            return res;
        }

        // send 15 packets
        for (p = 0; p < NB_PKT_TO_TEST; p++) {
            int sleepMs = 1000;
            int transfered;
            struct timeval tv;
            memcpy(pkt, config_pkt, YOCTO_PKT_SIZE);
            printf("+ Send USB pkt no %d\n", iface->sent_pkt);
            res = libusb_interrupt_transfer(iface->hdl,
                                            iface->wrendp,
                                            pkt,
                                            YOCTO_PKT_SIZE,
                                            &transfered,
                                            5000/*5 sec*/);
            if(res < 0 || YOCTO_PKT_SIZE != transfered) {
                printf("USB pkt transmit error %d (transmitted %d / %d)\n", res, transfered, YOCTO_PKT_SIZE);
                break;
            }
            iface->sent_pkt++;
            // wait 1 second
            memset(&tv, 0, sizeof(tv));
            tv.tv_sec = 5;
            res = libusb_handle_events_timeout(libusb, &tv);
            if (res < 0) {
                return ySetErr("libusb_handle_events_timeout", res);
            }

            if (iface->sent_pkt != iface->received_pkt) {
                printf("No USB pkt received after 5 second\n");
                if (exit_on_error) {
                    printf("Exit immediately\n");
                    return 1;
                }

                break;
            }
        }

        printf("= result: %d pkt sent vs %d pkt received\n",iface->sent_pkt, iface->received_pkt);
        if (iface->sent_pkt != iface->received_pkt) {
            printf("!!! ERROR : %d packets missing !!!\n",iface->sent_pkt - iface->received_pkt);
        }
        printf("Cancel and free all pending transactions\n");
        if (iface->rdTr) {
            int count = 10;
            int res = libusb_cancel_transfer(iface->rdTr);
            if(res == 0) {
                while(count && iface->rdTr->status != LIBUSB_TRANSFER_CANCELLED) {
                    usleep(1000);
                    count--;
                }
            }
        }
        res = libusb_release_interface(iface->hdl, iface->ifaceno);
        if(res != 0 && res != LIBUSB_ERROR_NOT_FOUND && res != LIBUSB_ERROR_NO_DEVICE) {
            printf("%s libusb_release_interface error\n", iface->serial);
        }

        if (res < 0 && res != LIBUSB_ERROR_NO_DEVICE) {
            printf("%s libusb_attach_kernel_driver error\n", iface->serial);
        }
        libusb_close(iface->hdl);
        iface->hdl = NULL;

        if (iface->rdTr) {
            libusb_free_transfer(iface->rdTr);
            iface->rdTr = NULL;
        }
    }

    libusb_exit(libusb);

    return 0;
}



