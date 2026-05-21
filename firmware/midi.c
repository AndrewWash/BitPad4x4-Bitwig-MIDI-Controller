/* midi.c - USB-MIDI descriptors + send queue for the JJ4x4 controller.
 *
 * Two parts:
 *   1. The class-compliant USB-MIDI descriptors and the V-USB callbacks
 *      that serve them. The descriptor data is taken verbatim from the
 *      V-USB MIDI example by Martin Homuth-Rosemann (2008), which in turn
 *      follows "USB Device Class Definition for MIDI Devices 1.0",
 *      Appendix B "Simple MIDI Adapter". GNU GPL v2.
 *   2. A small RAM ring buffer of 4-byte USB-MIDI event packets, drained to
 *      the interrupt-IN endpoint by midi_task().
 */
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "usbdrv/usbdrv.h"
#include "midi.h"

/* ======================================================================
 * USB-MIDI descriptors
 * ====================================================================== */

/* B.1 Device Descriptor */
PROGMEM const char deviceDescrMIDI[18] = {
    18,                     /* bLength                          */
    USBDESCR_DEVICE,        /* bDescriptorType                  */
    0x10, 0x01,             /* bcdUSB 1.1                       */
    0,                      /* bDeviceClass: at interface level */
    0,                      /* bDeviceSubClass                  */
    0,                      /* bDeviceProtocol                  */
    8,                      /* bMaxPacketSize0                  */
    USB_CFG_VENDOR_ID,      /* idVendor  (2 bytes)              */
    USB_CFG_DEVICE_ID,      /* idProduct (2 bytes)              */
    USB_CFG_DEVICE_VERSION, /* bcdDevice (2 bytes)              */
    1,                      /* iManufacturer                    */
    2,                      /* iProduct                         */
    0,                      /* iSerialNumber                    */
    1,                      /* bNumConfigurations               */
};

/* B.2 Configuration Descriptor (+ inlined AC and MIDIStreaming descriptors) */
PROGMEM const char configDescrMIDI[101] = {
    /* Configuration descriptor */
    9,                      /* bLength                          */
    USBDESCR_CONFIG,        /* bDescriptorType                  */
    101, 0,                 /* wTotalLength                     */
    2,                      /* bNumInterfaces                   */
    1,                      /* bConfigurationValue              */
    0,                      /* iConfiguration                   */
    USBATTR_BUSPOWER,       /* bmAttributes                     */
    USB_CFG_MAX_BUS_POWER / 2, /* bMaxPower (2 mA units)        */

    /* B.3.1 Standard AudioControl (AC) interface descriptor */
    9,                      /* bLength                          */
    USBDESCR_INTERFACE,     /* bDescriptorType                  */
    0,                      /* bInterfaceNumber                 */
    0,                      /* bAlternateSetting                */
    0,                      /* bNumEndpoints                    */
    1,                      /* bInterfaceClass = AUDIO          */
    1,                      /* bInterfaceSubClass = AUDIO_CONTROL */
    0,                      /* bInterfaceProtocol               */
    0,                      /* iInterface                       */

    /* B.3.2 Class-specific AC interface descriptor */
    9,                      /* bLength                          */
    0x24,                   /* bDescriptorType = CS_INTERFACE   */
    1,                      /* bDescriptorSubtype = HEADER      */
    0x0, 0x01,              /* bcdADC                           */
    9, 0,                   /* wTotalLength                     */
    1,                      /* bInCollection                    */
    1,                      /* baInterfaceNr                    */

    /* B.4.1 Standard MIDIStreaming (MS) interface descriptor */
    9,                      /* bLength                          */
    USBDESCR_INTERFACE,     /* bDescriptorType                  */
    1,                      /* bInterfaceNumber                 */
    0,                      /* bAlternateSetting                */
    2,                      /* bNumEndpoints                    */
    1,                      /* bInterfaceClass = AUDIO          */
    3,                      /* bInterfaceSubClass = MIDISTREAMING */
    0,                      /* bInterfaceProtocol               */
    0,                      /* iInterface                       */

    /* B.4.2 Class-specific MS interface descriptor */
    7,                      /* bLength                          */
    0x24,                   /* bDescriptorType = CS_INTERFACE   */
    1,                      /* bDescriptorSubtype = MS_HEADER   */
    0x0, 0x01,              /* bcdMSC                           */
    65, 0,                  /* wTotalLength                     */

    /* B.4.3 MIDI IN Jack descriptor (embedded) */
    6, 0x24, 2, 1, 1, 0,
    /* MIDI IN Jack descriptor (external) */
    6, 0x24, 2, 2, 2, 0,

    /* B.4.4 MIDI OUT Jack descriptor (embedded) */
    9, 0x24, 3, 1, 3, 1, 2, 1, 0,
    /* MIDI OUT Jack descriptor (external) */
    9, 0x24, 3, 2, 4, 1, 1, 1, 0,

    /* B.5.1 Standard Bulk OUT endpoint descriptor (interrupt on V-USB) */
    9,                      /* bLength                          */
    USBDESCR_ENDPOINT,      /* bDescriptorType                  */
    0x01,                   /* bEndpointAddress = OUT 1         */
    3,                      /* bmAttributes = interrupt         */
    8, 0,                   /* wMaxPacketSize                   */
    10,                     /* bInterval (ms)                   */
    0, 0,                   /* bRefresh, bSyncAddress           */

    /* B.5.2 Class-specific MS bulk OUT endpoint descriptor */
    5, 0x25, 1, 1, 1,

    /* B.6.1 Standard Bulk IN endpoint descriptor (interrupt on V-USB) */
    9,                      /* bLength                          */
    USBDESCR_ENDPOINT,      /* bDescriptorType                  */
    0x81,                   /* bEndpointAddress = IN 1          */
    3,                      /* bmAttributes = interrupt         */
    8, 0,                   /* wMaxPacketSize                   */
    10,                     /* bInterval (ms)                   */
    0, 0,                   /* bRefresh, bSyncAddress           */

    /* B.6.2 Class-specific MS bulk IN endpoint descriptor */
    5, 0x25, 1, 1, 3,
};

/* V-USB calls this for the device and configuration descriptors because
 * they are marked USB_PROP_IS_DYNAMIC in usbconfig.h. */
usbMsgLen_t usbFunctionDescriptor(usbRequest_t *rq)
{
    if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
        usbMsgPtr = (usbMsgPtr_t) deviceDescrMIDI;
        return sizeof(deviceDescrMIDI);
    }
    /* otherwise it is the configuration descriptor */
    usbMsgPtr = (usbMsgPtr_t) configDescrMIDI;
    return sizeof(configDescrMIDI);
}

/* This controller exposes no class-specific control requests and never
 * receives MIDI, so these handlers are intentionally minimal. */
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    (void) data;
    return USB_NO_MSG;
}

uchar usbFunctionRead(uchar *data, uchar len)
{
    (void) len;
    for (uchar i = 0; i < 7; i++)
        data[i] = 0;
    return 7;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
    (void) data; (void) len;
    return 1;
}

void usbFunctionWriteOut(uchar *data, uchar len)
{
    /* Host -> device MIDI is not used by this controller. */
    (void) data; (void) len;
}

/* ======================================================================
 * MIDI send queue
 *
 * Each USB-MIDI event is a 4-byte packet:
 *   [cable<<4 | CIN] [status] [data1] [data2]
 * Cable number is 0; CIN (Code Index Number) is 0x9 note-on, 0x8 note-off,
 * 0xB control change.
 * ====================================================================== */

#define MIDI_RING_SIZE 64        /* multiple of 8; holds 16 events */

static uchar    ring[MIDI_RING_SIZE];
static uint8_t  rd, wr;          /* read / write indices, always /4 */

void midi_init(void)
{
    rd = wr = 0;
}

static void queue(uchar cin, uchar status, uchar d1, uchar d2)
{
    uint8_t next = (wr + 4) % MIDI_RING_SIZE;
    if (next == rd)
        return;                  /* queue full - drop event (shouldn't happen) */
    ring[wr + 0] = cin;
    ring[wr + 1] = status;
    ring[wr + 2] = d1;
    ring[wr + 3] = d2;
    wr = next;
}

void midi_send_note_on(uint8_t ch, uint8_t note, uint8_t vel)
{
    queue(0x09, 0x90 | (ch & 0x0F), note & 0x7F, vel & 0x7F);
}

void midi_send_note_off(uint8_t ch, uint8_t note, uint8_t vel)
{
    queue(0x08, 0x80 | (ch & 0x0F), note & 0x7F, vel & 0x7F);
}

void midi_send_cc(uint8_t ch, uint8_t cc, uint8_t val)
{
    queue(0x0B, 0xB0 | (ch & 0x0F), cc & 0x7F, val & 0x7F);
}

void midi_task(void)
{
    if (rd == wr)
        return;                          /* nothing queued        */
    if (!usbInterruptIsReady())
        return;                          /* endpoint still busy   */

    /* Send one packet, or two if a second one is queued contiguously. */
    uint8_t len  = 4;
    uint8_t next = (rd + 4) % MIDI_RING_SIZE;
    if (next != wr && next != 0)
        len = 8;

    usbSetInterrupt((uchar *) &ring[rd], len);
    rd = (rd + len) % MIDI_RING_SIZE;
}
