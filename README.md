# BNK Tools

Tools for extracting and repacking Wwise .bnk audio files.

## Tools Overview

### BNK-Unpack & BNK-Pack (Double Click)
Simple extraction tool. Just double click to run.

**How to use BNK-Unpack:**
1. Place .bnk files in the `BNK-Input` folder
2. Double click `BNK-Unpack.exe`
3. Find extracted .wem files in the `WEM` folder

**How to use BNK-Pack:**
1. Make sure original .bnk files are in `BNK-Input` folder
2. Modify .wem files in the `WEM` folder as needed
3. Double click `BNK-Pack.exe`
4. Find repacked .bnk files in the `BNK-Output` folder

### BNKTool (Command Line)
Full featured command line tool with options for extraction, packing, and pipeline mode.

**Usage:**
```bash
bnktool.exe [flags]
```

**Flags:**
- `-bi <dir>` - BNK input folder (default: BNK-Input)
- `-bo <dir>` - BNK output folder (default: BNK-Output)
- `-wo <dir>` - WEM output folder (default: WEM)
- `-wi <dir>` - WEM input folder for repacking (default: WEM)
- `-e` - Extract only
- `-p` - Pack only
- `-s` - Skip confirmation prompt
- `-no` - Quiet mode
- `-h` - Show help

## Folder Structure

```
BNK-Input/        Original .bnk files go here
BNK-Output/       Repacked .bnk files are created here
WEM/              Extracted and modified .wem files
```

## Building from Source

**Requirements:**
- Linux: `sudo pacman -S mingw-w64-gcc base-devel` (Arch) or `sudo apt install mingw-w64 build-essential` (Ubuntu/Debian)
- Windows: I have no idea.

**Compile:**
```bash
make              # Build all executables
make windows      # Build all Windows executables
make linux        # Build all Linux executables
make clean        # Remove all executables
```

## Notes

- The `data.txt` file allows you to replace WEM files with custom audio
- All tools create necessary folders automatically
- WEM files use their decimal ID as filename (e.g., 12345.wem)
