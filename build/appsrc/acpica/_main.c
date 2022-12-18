
#include <kprintf.h>
#include <fcntl.h>
#include <vfs.h>
#include <string.h>
#include <heap.h>
#include <virtual.h>
#include <physical.h>
#include <thread.h>
#include <cpu.h>
#include <vnode.h>
#include <spinlock.h>
#include <string.h>
#include <synch.h>
#include <panic.h>
#include <sys/stat.h>
#include <machine/portio.h>
#include <machine/interrupt.h>

#include "acpi.h"

ACPI_STATUS AcpiOsInitialize()
{
    kprintf("AcpiOsInitialize\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate()
{
    kprintf("AcpiOsTerminate\n");
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    kprintf("AcpiOsGetRootPointer\n");
    ACPI_PHYSICAL_ADDRESS Ret = 0;
    AcpiFindRootPointer(&Ret);
    return Ret;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* PredefinedObject, ACPI_STRING* NewValue)
{    
    kprintf("AcpiOsPredefinedOverride\n");

    if (!PredefinedObject || !NewValue) {
        return AE_BAD_PARAMETER;
    }

    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_TABLE_HEADER** NewTable)
{
    kprintf("AcpiOsTableOverride\n");

    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_PHYSICAL_ADDRESS* NewAddress, UINT32* NewTableLength)
{
    kprintf("AcpiOsPhysicalTableOverride\n");

    *NewAddress = 0;
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width)
{
    kprintf("AcpiOsReadPort\n");

    if (Width == 8) *Value = inb(Address);
    else if (Width == 16) *Value = inw(Address);
    else if (Width == 32) *Value = inl(Address);
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    kprintf("AcpiOsWritePort\n");

    if (Width == 8) outb(Address, (uint8_t) Value);
    else if (Width == 16) outw(Address, (uint16_t) Value);
    else if (Width == 32) outl(Address, Value);
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

UINT64 AcpiOsGetTimer(void)
{
    kprintf("AcpiOsGetTimer\n");

    /* returns it in 100 nanosecond units */
    return get_time_since_boot() / 100;
}

void AcpiOsWaitEventsComplete(void)
{
    panic("acpi: AcpiOsWaitEventsComplete");
}

//https://github.com/no92/vineyard/blob/dev/kernel/driver/acpi/osl/pci.c
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64* Value, UINT32 Width)
{
    kprintf("AcpiOsReadPciConfiguration\n");

    uint32_t regAligned = Register & ~0x03U;
    uint32_t offset = Register & 0x03U;
    uint32_t addr = (uint32_t) ((uint32_t) (PciId->Bus << 16) | (uint32_t) (PciId->Device << 11) | (uint32_t) (PciId->Function << 8) | ((uint32_t) 0x80000000) | regAligned);

    outl(0xCF8, addr);

    uint32_t ret = inl(0xCFC);

    void* res = (char*) &ret + offset;
    size_t count = Width >> 3;
    *Value = 0;

    memcpy(Value, res, count);

    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64 Value, UINT32 Width)
{
    kprintf("AcpiOsWritePciConfiguration\n");

    uint32_t addr = (uint32_t) ((uint32_t) (PciId->Bus << 16) | (uint32_t) (PciId->Device << 11) | (uint32_t) (PciId->Function << 8) | ((uint32_t) 0x80000000));
    addr += Register;

    outl(0xCF8, addr);

    switch (Width) {
    case 8:
        outb(0xCFC, Value & 0xFF);
        break;
    case 16:
        outw(0xCFC, Value & 0xFFFF);
        break;
    case 32:
        outl(0xCFC, Value & 0xFFFFFFFF);
        break;
    default:
        panic("acpi: AcpiOsWritePciConfiguration bad width!");
        break;
    }

    return AE_OK;

}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void* Info)
{
    kprintf("AcpiOsSignal\n");

    switch (Function) {
    case ACPI_SIGNAL_FATAL:
        panic("acpi: AML fatal opcode");
        break;
    case ACPI_SIGNAL_BREAKPOINT:
        panic("acpi: AML breakpoint\n");
        break;
    default:
        panic("acpi: AcpiOsSignal");
        return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64* Value, UINT32 Width)
{
    panic("acpi: TODO: read memory...");
    if (Width == 8) {
        uint8_t* a = (uint8_t*) (size_t) Address;
        *Value = *a;
    }
    else if (Width == 16) {
        uint16_t* a = (uint16_t*) (size_t) Address;
        *Value = *a;
    }
    else if (Width == 32) {
        uint32_t* a = (uint32_t*) (size_t) Address;
        *Value = *a;
    }
    else if (Width == 64) {
        uint64_t* a = (uint64_t*) (size_t) Address;
        *Value = *a;
    }
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
    kprintf("AcpiOsEnterSleep\n");

    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    panic("acpi: TODO: write memory...");

    if (Width == 8) {
        uint8_t* a = (uint8_t*) (size_t) Address;
        *a = Value;
    }
    else if (Width == 16) {
        uint16_t* a = (uint16_t*) (size_t) Address;
        *a = Value;
    }
    else if (Width == 32) {
        uint32_t* a = (uint32_t*) (size_t) Address;
        *a = Value;
    }
    else if (Width == 64) {
        uint64_t* a = (uint64_t*) (size_t) Address;
        *a = Value;
    }
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{		
    kprintf("AcpiOsMapMemory 0x%X 0x%X\n", (uint32_t) PhysicalAddress, (uint32_t) Length);

    size_t virt = virt_allocate_unbacked_krnl_region(Length);
    size_t pages = virt_bytes_to_pages(Length);

    for (size_t i = 0; i < pages; ++i) {
        vas_map(vas_get_current_vas(), (PhysicalAddress & ~0xFFF) + i * 4096, virt + i * 4096, VAS_FLAG_LOCKED);
    }

    vas_flush_tlb();

    return (void*) (size_t) (virt | (PhysicalAddress & 0xFFF));
}

void AcpiOsUnmapMemory(void* where, ACPI_SIZE length)
{
    kprintf("ignoring ACPICA request to unmap memory...\n");
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void* LogicalAddress, ACPI_PHYSICAL_ADDRESS* PhysicalAddress)
{
    kprintf("AcpiOsGetPhysicalAddress\n");

    *PhysicalAddress = vas_virtual_to_physical(vas_get_current_vas(), (size_t) LogicalAddress);
    return AE_OK;
}

void* AcpiOsAllocate(ACPI_SIZE Size)
{
    void* v = malloc(Size);
    return v;
}

void AcpiOsFree(void* Memory)
{
    free(Memory);
}

BOOLEAN AcpiOsReadable(void* Memory, ACPI_SIZE Length)
{
    kprintf("AcpiOsReadable 0x%X\n", Memory);
    return true;
}

BOOLEAN AcpiOsWritable(void* Memory, ACPI_SIZE Length)
{
    kprintf("AcpiOsWritable 0x%X\n", Memory);
    return true;
}

ACPI_THREAD_ID AcpiOsGetThreadId()
{
    return current_cpu->current_thread->thread_id;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void* Context)
{
    panic("TODO: PUT CONTEXT ON THE STACK AcpiOsExecute");
    thread_create(Function, Context, vas_get_current_vas());
    return AE_OK;
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    kprintf("AcpiOsSleep\n");

    thread_nano_sleep(Milliseconds * 1000 * 1000);
}

void AcpiOsStall(UINT32 Microseconds)
{
    kprintf("AcpiOsStall\n");

    uint64_t end = get_time_since_boot() + Microseconds * 1000 + 1;
    while (get_time_since_boot() < end) {
        ;
    }
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE* OutHandle)
{
    struct semaphore* sem = semaphore_create(MaxUnits);
    semaphore_set_count(sem, MaxUnits - InitialUnits);
    
    *OutHandle = (void*) sem;

    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    semaphore_destory((struct semaphore*) Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if (Units > 1) {
        kprintf("AcpiOsWaitSemaphore units > 1\n");
        return AE_SUPPORT;
    }
    
    if (Timeout == 0xFFFF) {
        semaphore_acquire((struct semaphore*) Handle);

    } else {
        uint64_t startNano = get_time_since_boot();
        uint64_t extra = ((uint32_t) Timeout) * 1000000;

        while (get_time_since_boot() < startNano + extra) {
            int success = semaphore_try_acquire((struct semaphore*) Handle);
            if (success == 0) {
                kprintf("AcpiOsWaitSemaphore done B\n");
                return AE_OK;
            }
        }

        return AE_TIME;
    }
    
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    if (Units > 1) {
        kprintf("AcpiOsSignalSemaphore units > 1\n");
        return AE_SUPPORT;
    }

    semaphore_release((struct semaphore*) Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle)
{
    kprintf("AcpiOsCreateLock\n");

    struct spinlock* lock = malloc(sizeof(struct spinlock));
    spinlock_init(lock, "acpica lock");

    *OutHandle = (ACPI_SPINLOCK*) lock;

    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_HANDLE Handle)
{
    free(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    spinlock_acquire((struct spinlock*) Handle);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    spinlock_release((struct spinlock*) Handle);
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void* Context)
{
    panic("acpi: AcpiOsInstallInterruptHandler - how are we going to do context??");
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
    panic("acpi: AcpiOsRemoveInterruptHandler");
    return AE_OK;
}

void AcpiOsVprintf(const char* format, va_list list)
{

}

void AcpiOsPrintf(const char* format, ...)
{
    
}

void _driver_entry_point() {
    kprintf("Lol, ACPICA.SYS loaded\n");
	kprintf("A 0x%X\n", AcpiInitializeSubsystem);

    ACPI_STATUS a = AcpiInitializeSubsystem();
	kprintf("A ");
    if (ACPI_FAILURE(a)) panic("FAILURE AcpiInitializeSubsystem");
    kprintf("A ");

	a = AcpiInitializeTables(NULL, 16, true);
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiInitializeTables");
    kprintf("A ");

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiInstallAddressSpaceHandler ACPI_ADR_SPACE_SYSTEM_MEMORY");
    kprintf("A ");

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiInstallAddressSpaceHandler ACPI_ADR_SPACE_SYSTEM_IO");
    kprintf("A ");

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiInstallAddressSpaceHandler ACPI_ADR_SPACE_PCI_CONFIG");
    kprintf("A ");

	a = AcpiLoadTables();
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiLoadTables");
    kprintf("A ");

	a = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiEnableSubsystem");
    kprintf("A ");

	a = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(a)) panic("FAILURE AcpiInitializeObjects");
    kprintf("A ");

}