


Greetings to all. This is my OS personal project. LucidiOS is an OS that provides the user with a bash-inspired terminal, a FAT32 filesystem and a text editor (similar to the GNU nano). the kernel will have fault interrupts and IRQ interrupts, higher half and lower half virtual memory paging, DMA for file uploading, and 64 bit long mode. 

Tools I am using:
* Limine bootloader
* QEMU VM
* GDB debugger
* OSdev community
* GNUmakefile

GNUmakefile commands (make run requires QEMU):
* make - compiles and links all the files for a kernel
* make clean - deletes all current builds of the kernel
* make run - compiles and links all the files for a kernel, then runs it in QEMU

Current Progress:
As of 20th of March, LucidiOS is equipped with:
* serial printing
* linear framebuffer graphic printing
* IDT with excpetions and hardware interrupts
* I/O APIC and LAPIC (still just on BSP CPU, LucidiOS isn't SMP yet)
* GDT
* half high virtual memory paging
* 64 bit long mode

current Roadmap is detailed in architecture and design files.
                                                                               
