import os
import subprocess
import glob

# Paths
arduino_path = os.path.expanduser('~/AppData/Local/Arduino15/packages/esp32/tools')
mkspiffs_exe = glob.glob(os.path.join(arduino_path, 'mkspiffs', '*', 'mkspiffs.exe'))[0]
esptool_exe = glob.glob(os.path.join(arduino_path, 'esptool_py', '*', 'esptool.exe'))[0]

data_dir = os.path.join(os.path.dirname(__file__), 'data')
spiffs_bin = os.path.join(os.path.dirname(__file__), 'spiffs.bin')

port = "COM5" # Default COM port detected for your ESP32-C3

print(f"1. Packing text files from {data_dir} into SPIFFS image...")
cmd_mk = [mkspiffs_exe, "-c", data_dir, "-b", "4096", "-p", "256", "-s", "1441792", spiffs_bin]
subprocess.run(cmd_mk, check=True)

print(f"2. Flashing SPIFFS image to ESP32 on {port} at offset 0x290000...")
cmd_esp = [esptool_exe, "--chip", "esp32c3", "--port", port, "--baud", "921600", "write_flash", "0x290000", spiffs_bin]
subprocess.run(cmd_esp, check=True)

print("SUCCESS! All books uploaded to ESP32 Flash Filesystem!")
