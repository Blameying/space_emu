import sys
import os
from PyQt5.QtWidgets import (QApplication, QMainWindow,  QWidget, QHBoxLayout,
                             QVBoxLayout, QAction, QLabel, QScrollArea,
                             QPushButton, QTableWidget, QHeaderView,
                             QTabWidget, QListWidget, QLineEdit, QPlainTextEdit,
                             QTableWidgetItem, QDialog, QComboBox, QStackedWidget,
                             QFormLayout, QFileDialog, QMessageBox)
from PyQt5.QtGui import QRegExpValidator
from PyQt5 import QtCore
from pyqterm import TerminalWidget
from pyqterm.procinfo import ProcessInfo
from ipc import ShareMemory, CpuStateStruct, TemuStateStruct
import atexit
import time
import json

esc = '\x03'

qmenu_stylesheet = """
QMenuBar {
    background-color: white;
    border: 1px solid #3d9ad1;
}
"""

mainwindow_stylesheet = """
QMainWindow {
    background-color:#F3F6EC;
}

QToolTip {
    background-color: white;
    color: black;
    border: black solid 1px;
}
"""

dialog_stylesheet = """
QDialog {
    background-color:#F3F6EC;
}
"""
emulator = "temu"

cpuState = CpuStateStruct()
temuState = TemuStateStruct()
cpu_reg_list = ['pc', 'zero', 'ra', 'sp', 'gp', 'tp', 't0', 't1', 't2', 's0_fp', 's1', 'a0', 'a1', 'a2',
            'a3', 'a4', 'a5', 'a6', 'a7', 's2', 's3', 's4', 's5', 's6', 's7', 's8', 's9', 's10', 's11',
            't3', 't4', 't5', 't6', 'ft0', 'ft1', 'ft2', 'ft3', 'ft4', 'ft5', 'ft6', 'ft7', 'fs0',
            'fs1', 'fa0', 'fa1', 'fa2', 'fa3', 'fa4', 'fa5', 'fa6', 'fa7', 'fs2', 'fs3', 'fs4', 'fs5',
            'fs6', 'fs7', 'fs8', 'fs9', 'fs10', 'fs11', 'ft8', 'ft9', 'ft10', 'ft11', 'fflags', 'frm',
            'xlen', 'power_down_flag', 'priv', 'fs', 'mxl', 'cycles', 'rtc_start_time', 'mtimecmp',
            'plic_pending_irq', 'plic_served_irq', 'htif_tohost', 'htif_fromhost',
            'pending_exception', 'pending_tval', 'mstatus', 'mtvec', 'mscratch', 'mepc', 'mcause',
            'mtval', 'mhartid', 'misa', 'mie', 'mip', 'medeleg', 'mideleg', 'mcounteren', 'stvec',
            'sscratch', 'sepc', 'scause', 'stval', 'satp', 'scounteren', 'load_res'];

temu_reg_list = ['pc', 'zero', 'ra', 'sp', 'gp', 'tp', 't0', 't1', 't2', 's0_fp', 's1', 'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 's2', 's3', 's4', 's5', 's6', 's7', 's8', 's9', 's10', 's11', 't3', 't4', 't5', 't6', 'ft0', 'ft1', 'ft2', 'ft3', 'ft4', 'ft5', 'ft6', 'ft7', 'fs0', 'fs1', 'fa0', 'fa1', 'fa2', 'fa3', 'fa4', 'fa5', 'fa6', 'fa7', 'fs2', 'fs3', 'fs4', 'fs5', 'fs6', 'fs7', 'fs8', 'fs9', 'fs10', 'fs11', 'ft8', 'ft9', 'ft10', 'ft11', 'fflags', 'frm', 'xlen', 'priv', 'fs', 'mxl', 'cycles', 'insn_counter', 'power_down_flag', 'pending_exception', 'pending_tval', 'mstatus', 'mtvec', 'mscratch', 'mepc', 'mcause', 'mtval', 'mhartid', 'misa', 'mie', 'mip', 'medeleg', 'mideleg', 'mcounteren', 'stvec', 'sscratch', 'sepc', 'scause', 'stval', 'satp', 'scounteren', 'load_res']
visitor = {}

stopG = False
load_files = {}

class TabbedTerminal(QTabWidget):

    def __init__(self, parent=None):
        super(TabbedTerminal, self).__init__(parent)
        self.proc_info = ProcessInfo()
        self.setTabPosition(QTabWidget.South)
        self._terms = []
        self.tabCloseRequested[int].connect(self._on_close_request)
        self.currentChanged[int].connect(self._on_current_changed)
        QtCore.QTimer.singleShot(0, self.new_terminal)  # create lazy on idle
        self.startTimer(1000)

    def _on_close_request(self, idx):
        term = self.widget(idx)
        term.stop()

    def _on_current_changed(self, idx):
        term = self.widget(idx)
        self._update_title(term)

    def new_terminal(self):
        term = TerminalWidget(self, command="/usr/bin/zsh",
                              font_name="Dejavu Sans Mono Bold",
                              font_size=25)
        term.session_closed.connect(self._on_session_closed)
        self.addTab(term, "Terminal")
        self._terms.append(term)
        self.setCurrentWidget(term)
        term.setFocus()

    def timerEvent(self, event):
        self._update_title(self.currentWidget())

    def _update_title(self, term):
        if term is None:
            self.setWindowTitle("Terminal")
            return
        idx = self.indexOf(term)
        pid = term.pid()
        self.proc_info.update()
        child_pids = [pid] + self.proc_info.all_children(pid)
        for pid in reversed(child_pids):
            cwd = self.proc_info.cwd(pid)
            if cwd:
                break
        try:
            cmd = self.proc_info.commands[pid]
            title = "%s: %s" % (os.path.basename(cwd), cmd)
        except:
            title = "Terminal"
        self.setTabText(idx, title)
        self.setWindowTitle(title)

    def _on_session_closed(self):
        term = self.sender()
        try:
            self._terms.remove(term)
        except:
            pass
        self.removeTab(self.indexOf(term))
        widget = self.currentWidget()
        if widget:
            widget.setFocus()
        if self.count() == 0:
            self.new_terminal()


class SignalPackage(QtCore.QObject):
    sig_loaded_files = QtCore.pyqtSignal()

    def __init__(self):
        super().__init__()


sinalPackage = SignalPackage()


class DataFetchQThread(QtCore.QThread):
    signal = QtCore.pyqtSignal()
    con_signal = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)

    def run(self):
        global visitor
        shm = visitor['shm']
        state = None
        global cpuState
        global temuState
        global emulator
        if (emulator == "space"):
            state = cpuState
        else:
            state = temuState

        while True:
            if stopG:
                print("thread break")
                break
            if (shm.open()):
                shm.read(state)
                self.signal.emit()
                self.msleep(100)
            else:
                self.msleep(1000)


class LoadFilesDialog(QDialog):
    def __init__(self):
        super().__init__()
        self.ui()

    def ui(self):
        vbox = QVBoxLayout()
        hbox1 = QHBoxLayout()
        hbox1Widget = QWidget(parent=self)
        self.arch_label = QLabel(parent=hbox1Widget)
        self.arch_label.setText("架构")
        self.combobox = QComboBox(parent=hbox1Widget)
        self.combobox.addItem("riscv64")
        self.combobox.setCurrentIndex(0)
        hbox1.addWidget(self.arch_label)
        hbox1.addStretch()
        hbox1.addWidget(self.combobox)
        hbox1Widget.setLayout(hbox1)
        vbox.addWidget(hbox1Widget)
        
        hbox2 = QHBoxLayout()
        hbox2Widget = QWidget(parent=self)
        self.guest_label = QLabel(parent = hbox2Widget)
        self.guest_label.setText("宿主程序")
        self.guest_combobox = QComboBox(parent=self)
        self.guest_combobox.addItems(["Linux", "Binary"])
        self.guest_combobox.setCurrentIndex(0)
        self.guest_combobox.currentIndexChanged.connect(self.switchStack)
        hbox2.addWidget(self.guest_label)
        hbox2.addStretch()
        hbox2.addWidget(self.guest_combobox)
        hbox2Widget.setLayout(hbox2)
        vbox.addWidget(hbox2Widget)

        self.stack = QStackedWidget(parent=self)
        self.buildStackWidgets(self.stack)
        self.stack.setCurrentIndex(0)
        vbox.addWidget(self.stack)

        self.input_button = QPushButton(parent=self)
        self.input_button.setText("确认")
        self.input_button.clicked.connect(self.judge)
        vbox.addWidget(self.input_button)

        self.setLayout(vbox)
        self.setStyleSheet(dialog_stylesheet)
        self.resize(500, 300)
        self.setWindowTitle("选择加载文件")

    def buildStackWidgets(self, stack):
        linux_layout1 = QFormLayout()
        self.bbl_btn = QPushButton(parent=stack)
        self.bbl_btn.setText("加载")
        self.bbl_btn.clicked.connect(lambda: self.getFileName(self.bbl_btn))
        self.kernel_btn = QPushButton(parent=stack)
        self.kernel_btn.setText("加载")
        self.kernel_btn.clicked.connect(
                lambda: self.getFileName(self.kernel_btn))
        self.rootfs_btn = QPushButton(parent=stack)
        self.rootfs_btn.setText("加载")
        self.rootfs_btn.clicked.connect(
                lambda: self.getFileName(self.rootfs_btn))
        self.cmdline_edit = QLineEdit(parent=stack)
        self.cmdline_edit.setText("console=hvc0 root=/dev/vda rw")
        linux_layout1.addRow("BBL", self.bbl_btn)
        linux_layout1.addRow("Kernel", self.kernel_btn)
        linux_layout1.addRow("RootFS", self.rootfs_btn)
        linux_layout1.addRow("cmdline", self.cmdline_edit)
        linux_widget = QWidget(parent=stack)
        linux_widget.setLayout(linux_layout1)
        stack.addWidget(linux_widget)

        bin_layout2 = QFormLayout()
        self.bin_btn = QPushButton(parent=stack)
        self.bin_btn.setText("加载")
        self.bin_btn.clicked.connect(
                lambda: self.getFileName(self.bin_btn))
        bin_layout2.addRow("binary", self.bin_btn)
        bin_widget = QWidget(parent=stack)
        bin_widget.setLayout(bin_layout2)
        stack.addWidget(bin_widget)

    def switchStack(self, i):
        self.stack.setCurrentIndex(i)

    def getFileName(self, btn):
        filename = QFileDialog.getOpenFileName(self, caption="选取文件",
                                               filter="Binarys (*.bin);;AllFiles (*.*)")
        if (filename and  filename[0] != ""):
            btn.setText(filename[0])
        else:
            btn.setText("加载")

    def judge(self):
        global load_files
        if (self.stack.currentIndex() == 0):
            if ((self.bbl_btn.text() == "加载") or
                    (self.kernel_btn.text() == "加载") or
                    (self.rootfs_btn.text() == "加载")):
                QMessageBox.information(self, "提示", "请加载全部文件",
                                        QMessageBox.Ok,
                                        QMessageBox.Ok)
                return
            else:
                stopEmulator()
                load_files['type'] = self.guest_combobox.currentText()
                load_files["bbl"] = self.bbl_btn.text()
                load_files["kernel"] = self.kernel_btn.text()
                load_files["rootfs"] = self.rootfs_btn.text()
                load_files["cmdline"] = self.cmdline_edit.text()
                self.accept()
        else:
            if (self.bin_btn.text() == "加载"):
                QMessageBox.information(self, "提示", "请加载全部文件",
                                        QMessageBox.Ok,
                                        QMessageBox.Ok)
                return
            else:
                stopEmulator()
                load_files["type"] = self.guest_combobox.currentText()
                load_files["bin"] = self.bin_btn.text()
                self.accept()


def actions_load_files():
    dialog = LoadFilesDialog()
    if (dialog.exec_() == QDialog.Accepted):
        sinalPackage.sig_loaded_files.emit()


def pauseButtonClickedSlot():
    global visitor
    btn = visitor['pause_button']
    btn.setEnabled(False)
    if (btn.text() == "暂停"):
        if (visitor['shm'].enablePause()):
            btn.setText("继续")
    elif (btn.text() == "继续"):
        if (visitor['shm'].disablePause()):
            btn.setText("暂停")
    btn.setEnabled(True)


def stepButtonClickedSlot():
    global visitor
    visitor['shm'].stepNext()


def create_menu(mainwindow, app):
    menu = mainwindow.menuBar()
    menu.setStyleSheet(qmenu_stylesheet)
    options = menu.addMenu('选项')
    load_item = QAction("加载", mainwindow)
    load_item.setShortcut('Ctrl+l')
    load_item.triggered.connect(actions_load_files)

    quit_item = QAction("退出", mainwindow)
    quit_item.triggered.connect(app.quit)

    about = menu.addMenu('关于')

    options.addAction(load_item)
    options.addAction(quit_item)
    mainwindow.setMenuBar(menu)


def createStatusInfoDisplay(father, visitor):
    vbox = QVBoxLayout()
    widget = QWidget(parent=father)
    scrolarea = QScrollArea(parent=widget)

    button_run = QPushButton(parent=widget)
    button_run.setText("运行")
    button_run.clicked.connect(startEmulator)

    status = QLabel(parent=widget)
    status.setText("状态: 停止")
    status.setToolTip("当前子程序运行状态")

    fileinfo = QLabel(parent=scrolarea)
    fileinfo.setText("文件路径: 未加载")
    fileinfo.setToolTip("加载程序文件路径")
    fileinfo.setWordWrap(True)
    scrolarea.setWidget(fileinfo)

    visitor['button_run'] = button_run
    visitor['label_status'] = status
    visitor['label_fileinfo'] = fileinfo
    vbox.addWidget(button_run)
    vbox.addWidget(status)
    vbox.addWidget(scrolarea)
    widget.setLayout(vbox)
    return widget


def regCpuDataPlugIn(tablelist):
    global cpu_reg_list
    global temu_reg_list
    global cpuState
    global temuState
    global emulator
    state = None
    reg_list = None
    
    if (emulator == "space"):
        reg_list = cpu_reg_list
        state = cpuState
    else:
        reg_list = temu_reg_list
        state = temuState

    row = 0
    for name in reg_list:
        item = QTableWidgetItem("0x" + str(hex(getattr(state, name))))
        item.setFlags(item.flags() & (~QtCore.Qt.ItemIsEditable))
        tablelist.setItem(row, 1, item)
        row += 1


def createRegDisplay(father, visitor):
    reg_list = None
    global cpu_reg_list
    global temu_reg_list
    global emulator
    if (emulator == "space"):
        reg_list = cpu_reg_list
    else:
        reg_list = temu_reg_list

    tablelist = QTableWidget(parent=father)
    visitor['table_regs'] = tablelist
    tablelist.setColumnCount(2)
    tablelist.setRowCount(len(reg_list))
    tablelist.setHorizontalHeaderLabels(['寄存器', '值'])
    tablelist.horizontalHeader().setSectionResizeMode(QHeaderView.Interactive)
    row = 0
    for name in reg_list:
        item = QTableWidgetItem(name)
        item.setFlags(item.flags() & (~QtCore.Qt.ItemIsEditable))
        tablelist.setItem(row, 0, item)
        row += 1

    return tablelist


def leftArea(father, visitor):
    main_leftvbox = QVBoxLayout()
    widget = QWidget(parent=father)
    main_leftvbox.setAlignment(QtCore.Qt.AlignTop)
    main_leftvbox.addWidget(
            createStatusInfoDisplay(widget, visitor))
    main_leftvbox.addWidget(createRegDisplay(widget, visitor))
    main_leftvbox.setStretch(0, 1)
    main_leftvbox.setStretch(1, 3)
    widget.setLayout(main_leftvbox)
    return widget


def controlButtonGroup(father, visitor):
    vbox = QVBoxLayout()
    hbox = QHBoxLayout()
    ret_widget = QWidget(parent=father)
    hboxWidget = QWidget(parent=ret_widget)

    pause_button = QPushButton(parent=hboxWidget)
    pause_button.setText("暂停")
    pause_button.clicked.connect(pauseButtonClickedSlot)
    visitor['button_pause'] = pause_button

    step_button = QPushButton(parent=hboxWidget)
    step_button.setText("单步")
    step_button.clicked.connect(stepButtonClickedSlot)
    visitor['button_step'] = step_button

    hbox.addWidget(pause_button)
    hbox.addStretch()
    hbox.addWidget(step_button)
    hboxWidget.setLayout(hbox)

    condition_button = QPushButton(parent=ret_widget)
    condition_button.setText("设置中断条件")
    visitor['button_condition'] = condition_button

    label = QLabel(parent=ret_widget)
    label.setText("中断条件列表")
    breakpoint_list = QListWidget(parent=ret_widget)

    vbox.addWidget(hboxWidget)
    vbox.addWidget(condition_button)
    vbox.addWidget(label)
    vbox.addWidget(breakpoint_list)

    ret_widget.setLayout(vbox)
    return ret_widget


def memoryDumpArea(father, visitor):
    vbox = QVBoxLayout()
    hbox = QHBoxLayout()
    vboxWidget = QWidget(parent=father)
    hboxWidget = QWidget(parent=vboxWidget)

    label = QLabel(parent=vboxWidget)
    label.setText("DUMP内存")

    regex = QtCore.QRegExp('([0-9]+|0x[0-9a-fA-F]+)')
    regex_validator = QRegExpValidator(regex)
    input_address = QLineEdit(parent=hboxWidget)
    input_address.setValidator(regex_validator)
    input_address.setPlaceholderText("整数或16进制地址")
    visitor['lineEdit_address'] = input_address

    set_button = QPushButton(parent=hboxWidget)
    set_button.setText("查询")
    input_address.returnPressed.connect(lambda: set_button.click())
    visitor['button_dump'] = set_button

    hbox.addWidget(input_address)
    hbox.addWidget(set_button)
    hboxWidget.setLayout(hbox)

    memoryDataDisplay = QPlainTextEdit(parent=vboxWidget)
    memoryDataDisplay.setReadOnly(True)
    visitor['plainTextEdit_memory'] = memoryDataDisplay

    vbox.addWidget(label)
    vbox.addWidget(hboxWidget)
    vbox.addWidget(memoryDataDisplay)

    vboxWidget.setLayout(vbox)
    return vboxWidget


def rightArea(father, visitor):
    vbox = QVBoxLayout()
    vboxWidget = QWidget(father)
    vbox.addWidget(controlButtonGroup(vboxWidget, visitor))
    vbox.addWidget(memoryDumpArea(vboxWidget, visitor))
    vbox.setStretch(0, 1)
    vbox.setStretch(1, 1)
    vboxWidget.setLayout(vbox)
    return vboxWidget


def unconnect_status(visitor):
    visitor['label_status'].setText('状态: 停止')
    visitor['button_pause'].setEnabled(False)
    visitor['button_step'].setEnabled(False)
    visitor['button_condition'].setEnabled(False)
    visitor['lineEdit_address'].setEnabled(False)
    visitor['button_dump'].setEnabled(False)


def connect_status(visitor):
    visitor['label_status'].setText('状态: 运行')
    visitor['button_pause'].setEnabled(True)
    visitor['button_step'].setEnabled(True)
    visitor['button_condition'].setEnabled(True)
    visitor['lineEdit_address'].setEnabled(True)
    visitor['button_dump'].setEnabled(True)


def loaded_status():
    global visitor
    visitor['button_run'].setEnabled(True)
    info = "文件加载信息:\n"
    if not load_files['type']:
        return
    if (load_files['type'] == "Linux"):
        info += "Type:   " + load_files['type'] + "\n"
        info += "bbl:    " + load_files['bbl'] + "\n"
        info += "kernel: " + load_files['kernel'] + "\n"
        info += "rootfs: " + load_files['rootfs'] + "\n"
        info += "cmdline:" + load_files['cmdline'] + "\n"
    else:
        info += "Type:   " + load_files['type'] + "\n"
        info += "bin:    " + load_files['bin'] + "\n"
    visitor['label_fileinfo'].setText(info)
    visitor['label_fileinfo'].adjustSize()


def unloaded_status():
    global visitor
    visitor['button_run'].setEnabled(False)
    info = "文件加载信息:\n"
    info += "未加载\n"
    visitor['label_fileinfo'].setText(info)
    visitor['label_fileinfo'].adjustSize()


def stopEmulator():
    global visitor
    if (visitor['button_run'].text() == "终止"):
        if (load_files['type']):
            if (load_files['type'] == 'Linux'):
                for term in visitor['terminal']._terms:
                    term.send('poweroff -f\n'.encode())
                while(visitor['shm'].isOpen()):
                    time.sleep(0.1)
                visitor['button_run'].setText("运行")
    unconnect_status(visitor)


def startEmulator():
    visitor['button_run'].setEnabled(False)
    if (visitor['button_run'].text() == "运行"):
        if (load_files['type']):
            if load_files['type'] == 'Linux':
                param = {'version': 1, 'machine': 'riscv64',
                         'memory_size': 128}
                param['bios'] = load_files['bbl']
                param['kernel'] = load_files['kernel']
                param['cmdline'] = load_files['cmdline']
                param['drive0'] = {'file': load_files['rootfs']}
                text = json.dumps(param)
                with open('./tmp.cfg', 'w') as fobj:
                    fobj.write(text)
                for term in visitor['terminal']._terms:
                    term.send(esc.encode())
                    term.send('./temu ./tmp.cfg\n'.encode())
                while(not visitor['shm'].open()):
                    time.sleep(2)
                visitor['button_run'].setText("终止")
                connect_status(visitor)
    else:
        stopEmulator()

    visitor['button_run'].setEnabled(True)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

    def closeEvent(self, event):
        global stopG
        stopG = True
        time.sleep(0.5)


def main_ui():
    global visitor
    app = QApplication(sys.argv)
    w = MainWindow()
    centralWidget = QWidget()

    desktop = app.desktop()
    screenRect = desktop.screenGeometry()

    w.resize(int(screenRect.width() * 0.5), int(screenRect.height() * 0.5))
    w.move(int(screenRect.width() * 0.25), int(screenRect.height() * 0.25))
    w.setWindowTitle('Space模拟器')
    w.setStyleSheet(mainwindow_stylesheet)

    create_menu(w, app)

    main_hbox = QHBoxLayout()
    main_hbox.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
    main_hbox.addWidget(leftArea(centralWidget, visitor))

    terminal = TabbedTerminal(parent=centralWidget)
    main_hbox.addWidget(terminal)
    visitor['terminal'] = terminal

    main_hbox.addWidget(rightArea(centralWidget, visitor))

    main_hbox.setStretch(0, 1)
    main_hbox.setStretch(1, 2)
    main_hbox.setStretch(2, 1)

    sinalPackage.sig_loaded_files.connect(loaded_status)
    unloaded_status()
    unconnect_status(visitor)
    centralWidget.setLayout(main_hbox)
    w.setCentralWidget(centralWidget)
    w.show()

    visitor['shm'] = ShareMemory()
    data_thread = DataFetchQThread(parent=w)
    data_thread.signal.connect(lambda: regCpuDataPlugIn(visitor['table_regs']))
    data_thread.start()

    sys.exit(app.exec_())


if __name__ == '__main__':
    main_ui()
