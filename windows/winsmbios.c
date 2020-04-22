#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include "winsmbios.h"
#include "native.h"
#include "../dmidecode.h"

typedef UINT (WINAPI *GetSystemFirmwareTablePtr)(DWORD, DWORD, PVOID, DWORD BufferSize);

typedef VOID (WINAPI *RtlInitUnicodeStringPtr)(PUNICODE_STRING, PCWSTR);
typedef NTSTATUS (WINAPI *NtUnmapViewOfSectionPtr)(HANDLE, PVOID);
typedef NTSTATUS (WINAPI *NtOpenSectionPtr)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef NTSTATUS (WINAPI *NtMapViewOfSectionPtr)(HANDLE, HANDLE, PVOID *, ULONG, ULONG, PLARGE_INTEGER, PULONG, SECTION_INHERIT, ULONG, ULONG);
typedef ULONG (WINAPI *RtlNtStatusToDosErrorPtr)(NTSTATUS);

GetSystemFirmwareTablePtr myGetSystemFirmwareTable = NULL;
RtlInitUnicodeStringPtr myRtlInitUnicodeString = NULL;
NtUnmapViewOfSectionPtr myNtUnmapViewOfSection = NULL;
NtOpenSectionPtr myNtOpenSection = NULL;
NtMapViewOfSectionPtr myNtMapViewOfSection = NULL;
RtlNtStatusToDosErrorPtr myRtlNtStatusToDosError = NULL;

int LocateNtdllEntryPoints()
{
    switch(get_windows_platform()){
        case WIN_2003_VISTA:
        	if( !(myGetSystemFirmwareTable = (void *) GetProcAddress( GetModuleHandle(L"kernel32.dll"),
        			"GetSystemFirmwareTable" )) ) {
    
        	    return 0;
        	}	
        break;
        
        default:     
        	if( !(myRtlInitUnicodeString = (void *) GetProcAddress( GetModuleHandle(L"ntdll.dll"),
        			"RtlInitUnicodeString" )) ) {
        
        		return 0;
        	}
        	if( !(myNtUnmapViewOfSection = (void *) GetProcAddress( GetModuleHandle(L"ntdll.dll"),
        			"NtUnmapViewOfSection" )) ) {
        
        		return 0;
        	}
        	if( !(myNtOpenSection = (void *) GetProcAddress( GetModuleHandle(L"ntdll.dll"),
        			"NtOpenSection" )) ) {
        
        		return 0;
        	}
        	if( !(myNtMapViewOfSection = (void *) GetProcAddress( GetModuleHandle(L"ntdll.dll"),
        			"NtMapViewOfSection" )) ) {
        
        		return 0;
        	}
        	if( !(myRtlNtStatusToDosError = (void *) GetProcAddress( GetModuleHandle(L"ntdll.dll"),
        			"RtlNtStatusToDosError" )) ) {
        
        		return 0;
        	}
	
        break;
    }
    
	return 1;
}

void PrintError( char *message, NTSTATUS status )
{
    char *errMsg = NULL;

	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, myRtlNtStatusToDosError( status ),  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &errMsg, 0, NULL );

	printf("%s: %s\n", message, errMsg );

	LocalFree( errMsg );
}

static VOID UnmapPhysicalMemory(PVOID Address)
{
    NTSTATUS		status;

    status = myNtUnmapViewOfSection((HANDLE)-1, Address);
    if (!NT_SUCCESS(status))
    {
        PrintError("Unable to unmap view", status);
    }
}

static BOOLEAN MapPhysicalMemory(HANDLE PhysicalMemory, PVOID Address, PDWORD Length, PVOID VirtualAddress)
{
    NTSTATUS ntStatus;
    PHYSICAL_ADDRESS viewBase;
    char error[256];

    DWORD_PTR *addrCasted = Address;
    
    viewBase.QuadPart = (ULONGLONG)(*addrCasted);

    ntStatus = myNtMapViewOfSection(PhysicalMemory, (HANDLE)-1, VirtualAddress, 0L, *Length, &viewBase, Length, ViewShare, 0, PAGE_READONLY);

    if (!NT_SUCCESS(ntStatus)) {

        printf("NtMapViewOfSection failed\n");

        PrintError(error, ntStatus);

        return FALSE;
    }

#ifndef _WIN64
    *addrCasted = viewBase.LowPart;
#else
    *addrCasted = viewBase.QuadPart;
#endif

    return TRUE;
}

static HANDLE OpenPhysicalMemory()
{
    NTSTATUS		status;
    HANDLE			physmem;
    UNICODE_STRING	physmemString;
    OBJECT_ATTRIBUTES attributes;
    WCHAR			physmemName[] = L"\\device\\physicalmemory";

    myRtlInitUnicodeString(&physmemString, physmemName);

    InitializeObjectAttributes(&attributes, &physmemString, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = myNtOpenSection(&physmem, SECTION_MAP_READ, &attributes);

    if (!NT_SUCCESS(status))
    {
        PrintError("Could not open \\device\\physicalmemory", status);
        return NULL;
    }

    return physmem;
}

void *mem_chunk_win(void* base, u32 len){
	void *p;
    u32 mmoffset;
	SYSTEM_INFO sysinfo;
	HANDLE	physmem;
	DWORD length;
    PVOID paddress, vaddress;

	if( !LocateNtdllEntryPoints() ) {

		printf("Unable to locate NTDLL entry points.\n\n");
		return NULL;
	}
    	
	if( !(physmem = OpenPhysicalMemory())) {
		return NULL;
	}

	GetSystemInfo(&sysinfo);
	mmoffset = (DWORD_PTR)base % sysinfo.dwPageSize;
	len += mmoffset;
    
	paddress = base;
	length = (DWORD)len;

	if(!MapPhysicalMemory( physmem, &paddress, &length, &vaddress )){
        return NULL;
	}
    
	if((p=malloc(length))==NULL){
		return NULL;
	}
        
	memcpy(p, (u8 *)vaddress + mmoffset, length - mmoffset); 

	UnmapPhysicalMemory( vaddress );  

	CloseHandle( physmem );	
	
	return p;
}

int count_smbios_structures(const u8 *buff, u32 len){

    int icount = 0;//counts the structures
    u8 *offset = (void *)buff;//points to the actual address in the buff that's been checked
    struct dmi_header *header = NULL;//header of the struct been read to get the length to increase the offset
    
    //searches structures on the whole SMBIOS Table
    while(offset  < (buff + len)){
        //get the header to read the length and to increase the offset
        header = (struct dmi_header *)offset;                        
        offset += header->length;
        
        icount++;
        
        /*
         * increases the offset to point to the next header that's
         * after the strings at the end of the structure.
         */
        while( (*(WORD *)offset != 0)  &&  (offset < (buff + len))  ){
            offset++;
        }
        
        /*
         * Points to the next structure thas after two null BYTEs
         * at the end of the strings.
         */
        offset += 2;
    }
    
    return icount;
}

int get_windows_platform(){

    OSVERSIONINFO version;
    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&version);
   
    switch(version.dwPlatformId){        
        case VER_PLATFORM_WIN32_NT:

            if((version.dwMajorVersion >= 6) || (version.dwMajorVersion = 5 && version.dwMinorVersion >= 2)){
                return WIN_2003_VISTA;
            }else{
                return WIN_NT_2K_XP;
            }
            
        break;
        
        default:
            return WIN_UNSUPORTED;
        break;
    }
}

PRawSMBIOSData get_raw_smbios_table(void){

    void *buf = NULL;
    u32 size = 0;
    
    if(get_windows_platform() == WIN_2003_VISTA){
        size = myGetSystemFirmwareTable('RSMB', 0, buf, size);
        buf = (void *)malloc(size);
        myGetSystemFirmwareTable('RSMB', 0, buf, size);
    }

    return buf;
}            