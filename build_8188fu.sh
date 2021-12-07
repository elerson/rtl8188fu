dkms remove rtl8188fu/1.0 --all
dkms add .
dkms build rtl8188fu/1.0
dkms install rtl8188fu/1.0
cp ./firmware/rtl8188fufw.bin /lib/firmware/rtlwifi/
