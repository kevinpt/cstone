#ifndef CONSOLE_USB_H
#define CONSOLE_USB_H

#ifdef __cplusplus
extern "C" {
#endif

bool usb_console_init(int usb_id, ConsoleConfigBasic *cfg);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_USB_H
