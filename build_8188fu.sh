dkms remove rtl8188fu/1.0 --all
dkms add ./rtl8188fu
dkms build rtl8188fu/1.0
dkms install rtl8188fu/1.0
cp ./rtl8188fu/firmware/rtl8188fufw.bin /lib/firmware/rtlwifi/
