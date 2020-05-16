from ctypes import *
import sys

SHM_KEY = 718
SHM_SIZE = 1024 * 8

class IrqSignalStruct(Structure):
    _pack_ = 1
    _fields_ = [('set_irq', c_void_p),
                ('opaque', c_void_p),
                ('irq_num', c_int)]


class CpuStateStruct(Structure):
    _pack_ = 1
    _fields_ = [('pc', c_uint64),
                ('zero', c_uint64),
                ('ra', c_uint64),
                ('sp', c_uint64),
                ('gp', c_uint64),
                ('tp', c_uint64),
                ('t0', c_uint64),
                ('t1', c_uint64),
                ('t2', c_uint64),
                ('s0_fp', c_uint64),
                ('s1', c_uint64),
                ('a0', c_uint64),
                ('a1', c_uint64),
                ('a2', c_uint64),
                ('a3', c_uint64),
                ('a4', c_uint64),
                ('a5', c_uint64),
                ('a6', c_uint64),
                ('a7', c_uint64),
                ('s2', c_uint64),
                ('s3', c_uint64),
                ('s4', c_uint64),
                ('s5', c_uint64),
                ('s6', c_uint64),
                ('s7', c_uint64),
                ('s8', c_uint64),
                ('s9', c_uint64),
                ('s10', c_uint64),
                ('s11', c_uint64),
                ('t3', c_uint64),
                ('t4', c_uint64),
                ('t5', c_uint64),
                ('t6', c_uint64),
                ('ft0', c_uint64),
                ('ft1', c_uint64),
                ('ft2', c_uint64),
                ('ft3', c_uint64),
                ('ft4', c_uint64),
                ('ft5', c_uint64),
                ('ft6', c_uint64),
                ('ft7', c_uint64),
                ('fs0', c_uint64),
                ('fs1', c_uint64),
                ('fa0', c_uint64),
                ('fa1', c_uint64),
                ('fa2', c_uint64),
                ('fa3', c_uint64),
                ('fa4', c_uint64),
                ('fa5', c_uint64),
                ('fa6', c_uint64),
                ('fa7', c_uint64),
                ('fs2', c_uint64),
                ('fs3', c_uint64),
                ('fs4', c_uint64),
                ('fs5', c_uint64),
                ('fs6', c_uint64),
                ('fs7', c_uint64),
                ('fs8', c_uint64),
                ('fs9', c_uint64),
                ('fs10', c_uint64),
                ('fs11', c_uint64),
                ('ft8', c_uint64),
                ('ft9', c_uint64),
                ('ft10', c_uint64),
                ('ft11', c_uint64),
                ('fflags', c_uint32),
                ('frm', c_uint8),
                ('xlen', c_uint8),
                ('power_down_flag', c_uint8),
                ('priv', c_uint8),
                ('fs', c_uint8),
                ('mxl', c_uint8),
                ('cycles', c_uint64),
                ('rtc_start_time', c_uint64),
                ('mtimecmp', c_uint64),
                ('plic_pending_irq', c_uint32),
                ('plic_served_irq', c_uint32),
                ('plic_irq', IrqSignalStruct * 32),
                ('htif_tohost', c_uint64),
                ('htif_fromhost', c_uint64),
                ('pending_exception', c_int),
                ('pending_tval', c_uint64),
                ('mstatus', c_uint64),
                ('mtvec', c_uint64),
                ('mscratch', c_uint64),
                ('mepc', c_uint64),
                ('mcause', c_uint64),
                ('mtval', c_uint64),
                ('mhartid', c_uint64),
                ('misa', c_uint32),
                ('mie', c_uint32),
                ('mip', c_uint32),
                ('medeleg', c_uint32),
                ('mideleg', c_uint32),
                ('mcounteren', c_uint32),
                ('stvec', c_uint64),
                ('sscratch', c_uint64),
                ('sepc', c_uint64),
                ('scause', c_uint64),
                ('stval', c_uint64),
                ('satp', c_uint64),
                ('scounteren', c_uint64),
                ('load_res', c_uint64)]


class TemuStateStruct(Structure):
    _fields_ = [('class_ptr', c_void_p),
                ('pc', c_uint64),
                ('zero', c_uint64),
                ('ra', c_uint64),
                ('sp', c_uint64),
                ('gp', c_uint64),
                ('tp', c_uint64),
                ('t0', c_uint64),
                ('t1', c_uint64),
                ('t2', c_uint64),
                ('s0_fp', c_uint64),
                ('s1', c_uint64),
                ('a0', c_uint64),
                ('a1', c_uint64),
                ('a2', c_uint64),
                ('a3', c_uint64),
                ('a4', c_uint64),
                ('a5', c_uint64),
                ('a6', c_uint64),
                ('a7', c_uint64),
                ('s2', c_uint64),
                ('s3', c_uint64),
                ('s4', c_uint64),
                ('s5', c_uint64),
                ('s6', c_uint64),
                ('s7', c_uint64),
                ('s8', c_uint64),
                ('s9', c_uint64),
                ('s10', c_uint64),
                ('s11', c_uint64),
                ('t3', c_uint64),
                ('t4', c_uint64),
                ('t5', c_uint64),
                ('t6', c_uint64),
                ('ft0', c_uint64),
                ('ft1', c_uint64),
                ('ft2', c_uint64),
                ('ft3', c_uint64),
                ('ft4', c_uint64),
                ('ft5', c_uint64),
                ('ft6', c_uint64),
                ('ft7', c_uint64),
                ('fs0', c_uint64),
                ('fs1', c_uint64),
                ('fa0', c_uint64),
                ('fa1', c_uint64),
                ('fa2', c_uint64),
                ('fa3', c_uint64),
                ('fa4', c_uint64),
                ('fa5', c_uint64),
                ('fa6', c_uint64),
                ('fa7', c_uint64),
                ('fs2', c_uint64),
                ('fs3', c_uint64),
                ('fs4', c_uint64),
                ('fs5', c_uint64),
                ('fs6', c_uint64),
                ('fs7', c_uint64),
                ('fs8', c_uint64),
                ('fs9', c_uint64),
                ('fs10', c_uint64),
                ('fs11', c_uint64),
                ('ft8', c_uint64),
                ('ft9', c_uint64),
                ('ft10', c_uint64),
                ('ft11', c_uint64),
                ('fflags', c_uint32),
                ('frm', c_uint8),
                ('xlen', c_uint8),
                ('priv', c_uint8),
                ('fs', c_uint8),
                ('mxl', c_uint8),
                ('cycles', c_uint32),
                ('insn_counter', c_uint64),
                ('power_down_flag', c_uint8),
                ('pending_exception', c_int),
                ('pending_tval', c_uint64),
                ('mstatus', c_uint64),
                ('mtvec', c_uint64),
                ('mscratch', c_uint64),
                ('mepc', c_uint64),
                ('mcause', c_uint64),
                ('mtval', c_uint64),
                ('mhartid', c_uint64),
                ('misa', c_uint32),
                ('mie', c_uint32),
                ('mip', c_uint32),
                ('medeleg', c_uint32),
                ('mideleg', c_uint32),
                ('mcounteren', c_uint32),
                ('stvec', c_uint64),
                ('sscratch', c_uint64),
                ('sepc', c_uint64),
                ('scause', c_uint64),
                ('stval', c_uint64),
                ('satp', c_uint64),
                ('scounteren', c_uint64),
                ('load_res', c_uint64)]

class ShareMemory:
    def __init__(self):
        self.opened = False
        libc_so = {'darwin': 'libc.dylib', 'linux': ""}[sys.platform]
        try:
            libc = CDLL(libc_so, use_errno=True, use_last_error=True)
        except:
            print("load libc error");
            return False

        self.shmget = libc.shmget
        self.shmget.restype = c_int
        self.shmget.argtypes = (c_int, c_size_t, c_int)

        self.shmat = libc.shmat
        self.shmat.restype = c_void_p
        self.shmat.argtypes = (c_int, c_void_p, c_int)

        self.shmdt = libc.shmdt
        self.shmdt.restype = c_int
        self.shmdt.argtypes = (c_void_p,)

        self.memcpy = libc.memcpy
        self.memcpy.restype = c_void_p
        self.memcpy.argtypes = (c_void_p, c_void_p, c_uint)

    def read(self, cpu_state):
        if not self.opened:
            return False
        value = c_int()
        while(True):
            self.memcpy(byref(value), self.addr, sizeof(value))
            if (value.value == 0):
                value.value = 1
                self.memcpy(self.addr, byref(value), sizeof(value))
                break
            elif (value.value == 3):
                self.opened = False
                if (self.addr):
                    self.shmdt(self.addr)
                return False
        self.memcpy(byref(cpu_state), self.addr + sizeof(value), sizeof(cpu_state))
        value.value = 0
        self.memcpy(self.addr, byref(value), sizeof(value))
        return True

    def enablePause(self):
        if not self.opened:
            return False
        pause_addr = self.addr + 1024
        enable_flag = c_int()
        counter = c_int()
        locker = c_int()
        while(True):
            self.memcpy(byref(locker), pause_addr + sizeof(enable_flag) * 2, sizeof(locker))
            if (locker.value == 0):
                locker.value = 1
                self.memcpy(pause_addr +  sizeof(enable_flag) * 2, byref(locker),
                            sizeof(locker))
                break
        enable_flag.value = 1
        counter.value = 0
        self.memcpy(pause_addr, byref(enable_flag), sizeof(enable_flag))
        self.memcpy(pause_addr + sizeof(enable_flag), byref(counter), sizeof(counter))
        locker.value = 0
        self.memcpy(pause_addr +  sizeof(enable_flag) * 2, byref(locker),
                    sizeof(locker))
        return True
        
    def disablePause(self):
        if not self.opened:
            return False
        pause_addr = self.addr + 1024
        enable_flag = c_int()
        enable_flag.value = 0
        self.memcpy(pause_addr, byref(enable_flag), sizeof(enable_flag))
        return True
    
    def stepNext(self):
        if not self.opened:
            return False
        pause_addr = self.addr + 1024
        counter = c_int()
        locker = c_int()
        while(True):
            self.memcpy(byref(locker), pause_addr + sizeof(counter) * 2, sizeof(locker))
            if (locker.value == 0):
                locker.value = 1
                self.memcpy(pause_addr +  sizeof(counter) * 2, byref(locker),
                            sizeof(locker))
                break
        counter.value = 1
        self.memcpy(pause_addr + sizeof(counter), byref(counter), sizeof(counter))
        locker.value = 0
        self.memcpy(pause_addr +  sizeof(counter) * 2, byref(locker),
                    sizeof(locker))


    def isOpen(self):
        return self.opened;

    def open(self):
        if self.opened:
            return True

        self.shmid = self.shmget(SHM_KEY, SHM_SIZE, 0o666)
        self.addr = None
        if (self.shmid < 0):
            return False
        else:
            self.addr = self.shmat(self.shmid, None, 0)
            if (self.addr < 0):
                return False
        self.opened = True
        return True

    def __del__(self):
        if self.addr:
            self.shmdt(self.addr)

