#ifndef WINSMBIOS_H
#define WINSMBIOS_H
 
#include "../types.h"

#define WIN_UNSUPORTED 1
#define WIN_NT_2K_XP 2
#define WIN_2003_VISTA 3

/*
 * Struct needed to get the SMBIOS table using GetSystemFirmwareTable API.
 */
typedef struct _RawSMBIOSData{
    u8	Used20CallingMethod;
    u8	SMBIOSMajorVersion;
    u8	SMBIOSMinorVersion;
    u8	DmiRevision;
    u32	Length;
    u8	SMBIOSTableData[];
} RawSMBIOSData, *PRawSMBIOSData;

int get_windows_platform(void);
RawSMBIOSData *get_raw_smbios_table(void);
int count_smbios_structures(const u8 *buff, u32 len);
void *mem_chunk_win(void* base, u32 len);

int LocateNtdllEntryPoints();

#endif /*WINSMBIOS_H*/