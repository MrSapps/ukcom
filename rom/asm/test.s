    xdef exe_data_start
    section .exe_data
exe_data_start:
    incbin ".\obj\ukcom_exe.exe", 0x800
    xdef exe_data_end
exe_data_end
