/* SPDX-License-Identifier: GPL-2.0 */

#ifndef LAN969X_MAIN
#define LAN969X_MAIN

void lan969x_otp_init(void);
bool lan969x_otp_get_mac(u8 *mac);
bool lan969x_otp_get_pcb(u16 *pcb);
bool lan969x_otp_get_mac_count(char *mac_count);
bool lan969x_otp_get_partid(char *part);
bool lan969x_otp_get_serial_number(char *serial);

#endif
