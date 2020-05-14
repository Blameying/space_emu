objects = space.o clint.o fdt.o htif.o instructions.o iomap.o	\
						memory.o plic.o regs.o virtio_interface.o virtio_block_device.o	\
						machine.o console.o softfp.o cutils.o debug.o
cc = gcc
CFLAGS = -g -Wall -DDEBUG_VIRTIO

space: $(objects)
	cc $(cflags) -o space $(objects)

regs.o: regs.h riscv_definations.h
clint.o: clint.h riscv_definations.h iomap.h regs.h
fdt.o: regs.h riscv_definations.h memory.h fdt.h
htif.o: htif.h riscv_definations.h iomap.h regs.h
instructions.o: instructions.h regs.h iomap.h softfp.h
iomap.o: riscv_definations.h iomap.h
memory.o: regs.h memory.h iomap.h riscv_definations.h
plic.o: plic.h riscv_definations.h iomap.h regs.h
debug.o: debug.h riscv_definations.h iomap.h regs.h
virtio_interface.o: virtio_interface.h virtio_block_device.h riscv_definations.h iomap.h regs.h console.h
virtio_block_device.o: virtio_block_device.h virtio_interface.h
space.o: regs.h memory.h clint.h htif.h instructions.h iomap.h plic.h fdt.h virtio_interface.h virtio_block_device.h debug.h
console.o: console.h regs.h machine.h
machine.o: machine.h
softfp.o:	softfp.h cutils.h softfp_template.h softfp_template_icvt.h
cutils.o: cutils.h

clean:
	rm *.o space
