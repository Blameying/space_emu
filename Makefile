objects = space.o clint.o fdt.o htif.o instructions.o iomap.o	\
						memory.o plic.o regs.o virtio_interface.o virtio_block_device.o
cc = gcc

space: $(objects)
	cc -o space -g $(objects)

regs.o: regs.h riscv_definations.h
clint.o: clint.h riscv_definations.h iomap.h regs.h
fdt.o: regs.h riscv_definations.h memory.h fdt.h
htif.o: htif.h riscv_definations.h iomap.h regs.h
instructions.o: instructions.h regs.h iomap.h
iomap.o: riscv_definations.h iomap.h
memory.o: regs.h memory.h iomap.h riscv_definations.h
plic.o: plic.h riscv_definations.h iomap.h regs.h
virtio_interface.o: virtio_interface.h virtio_block_device.h riscv_definations.h iomap.h regs.h
virtio_block_device.o: virtio_block_device.h virtio_interface.h
space.o: regs.h memory.h clint.h htif.h instructions.h iomap.h plic.h fdt.h virtio_interface.h virtio_block_device.h

clean:
	rm *.o space
