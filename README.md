# sdb - a debugger for x64 ELF binaries 
![image](https://github.com/danglevm/2DJavaGame/assets/84720339/97dcd99c-9733-4fa2-a236-aee0da234665)

## 📄 Overview 
Command-line debugger for native C++ code written with modern C++ (C++ 18). Has support for x64 architectures. Works on Linux or WSL

I started this project out of my love for systems programming. Frankly, I really didn't think of a practical need for this since that wasn't the priority I had in mind. Rather, I just wanted to work on and develop my skills in something I really like.

![image](https://github.com/danglevm/2DJavaGame/assets/84720339/54814ca5-0f88-41e8-bff0-80267fe03b77)

## 💾 Available features
- Software breakpoints (int3)
- Hardware breakpoints (DR0-DR7), catchpoints and watchpoints
- Custom DWARF parser
- CTest Test suite
- Symbol table parsing and traversal
- Memory manipulation and disassembly
- Line table (WIP/sort of working)

![image](https://github.com/danglevm/2DJavaGame/assets/84720339/54814ca5-0f88-41e8-bff0-80267fe03b77)

## 🛠️ To-be-added features
- Line table mapping
- Multi-threading
- Call frame parser
- Stack unwinding
- Static typing
- Source level breakpoints
- Segfault handling
- Goto error handling

## 📥 Installation
1. Click Fork from top-right corner then clone the repo into your local work directory
```
git clone https://github.com/YOUR-USERNAME/baremetal-STM32-RTOS.git
```
2. Install the required packages
```
sudo apt-get update
sudo apt-get install libreadline-dev libfmt-dev libzydis-dev catch2
```
3. Build the binary
4. Run `sdb <target>` and use `help` to list out commands


![image](https://github.com/danglevm/2DJavaGame/assets/84720339/54814ca5-0f88-41e8-bff0-80267fe03b77)
## 📷 Screenshots and Video 
<img width="2186" height="340" alt="image" src="https://github.com/user-attachments/assets/10196b60-7ebc-4fee-bad7-96d9e4c7e134" />





