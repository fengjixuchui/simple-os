#include <interrupts.h>
#include <descriptor_tables/idt.h>
#include <devices/vga.h>
#include <extern_isrs.h>

/* This is a simple string array. It contains the message that
*  corresponds to each and every exception. We get the correct
*  message by accessing like:
*  exception_message[interrupt_number] */
const char *exception_messages[] =
{
	"Division By Zero",
	"Debug",
	"Non Maskable Interrupt",
	"Breakpoint",
	"Into Detected Overflow",
	"Out of Bounds",
	"Invalid Opcode",
	"No Coprocessor",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Bad TSS",
	"Segment Not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Unknown Interrupt",
	"Coprocessor Fault",
	"Alignment Check",
	"Machine Check"
};

interrupts::interrupt_handler interrupt_handlers[255];

void irq_install() {
	// call initialize cw1_init with future cw4
	outb(0x20, 0x11);
	io_wait();

	outb(0xA0, 0x11);
	io_wait();

	// map irq to 32..39, 40..47
	outb(0x21, 0x20);
	io_wait();

	outb(0xA1, 0x28);
	io_wait();

	// set irq line 2 for communication in primary
	outb(0x21, 0x04);
	io_wait();

	// set irq line 2 for communication in slave
	outb(0xA1, 0x02);
	io_wait();

	// set 80x86 mode in 2 pics
	outb(0x21, 0x01);
	io_wait();

	outb(0xA1, 0x01);
	io_wait();
	
	// null out data registers
	outb(0x21, 0x0);
	io_wait();
	
	outb(0xA1, 0x0);
	io_wait();
}

void interrupts::initialize() {
	memset(interrupt_handlers, 0, 255*sizeof(interrupts::interrupt_handler));

	descriptor_tables::idt::clear();

	for (word i = 0; i < 255; ++i) {
		void* addr = get_extern_address(i);
		descriptor_tables::idt::set_entry(i, addr, 0x8);
	}

	descriptor_tables::idt::flush();
	irq_install();
}

void interrupts::enable() {
	ASM_VOLATILE("sti");
}

void interrupts::add_handler(dword interrupt_code, interrupt_handler handler) {
	interrupt_handlers[interrupt_code] = handler;
}

inline bool is_irq(byte int_no) {
	return int_no >= 32 && int_no <= 48;
}

GLOBAL void isr_handler(interrupts::interrupt_frame frame) {
	
	if (interrupt_handlers[frame.int_no] != 0) {
		interrupt_handlers[frame.int_no](frame);		

		if (is_irq(frame.int_no)) {
			if (frame.int_no >= 40)
			{
				outb(0xA0, 0x20);
			}

			/* In either case, we need to send an EOI to the master
			*  interrupt controller too */
			outb(0x20, 0x20);
		}
	}
	else if (frame.int_no <= 18) {
		devices::vga::write_text(exception_messages[frame.int_no]);
		devices::vga::write_char(' ');
		while (true);
	}
	else {
		devices::vga::write_text("cannot handle interrupt ");
		devices::vga::write_number(frame.int_no);
		devices::vga::write_char(' ');
		while (true);
	}
}