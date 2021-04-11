The library is built for sir01b system. Some bugs may still exist.

Newest tests:
1.	Transfer on Intel USB 3.0 Host will fail occasionally. After pipe reset it'll come back to life. This issue is not witnessed on VIA Hub.
	Thus the recommended method is to use VIA's hub after Intel USB Host for best reliability.
2.	Device is automatically initialized according to the ID code. For invalid ID code, a valid code is required to be initialized ahead.
	The code is stored at 0x3C0000, 8 bytes. The first DWORD is magic code 0x7C495608, and the second DWORD takes 0x3A000003 as MT9J003PA,
	0x3B000003 as MT9J003JA, 0x5F000001 as V5000A, 0x5F000002 as NSC1105, 0x2A000001 as MT9F002. Also note that bit [7:4] of the second DWORD 
	defines whether to use reduced sync stream or traditional buffered transfer. Bit [11:8] indicate camera type and its index. Currently 
	bit [11:10] is mapped to iris, face, hdr, wdr types of cameras, and [9:8] indicate its index of each category.
		0x3A0000X3		MT9J003PA, DDR2		0x3A0000X3		MT9J003PA, RSYNC
		0x3B0000X3		MT9J003JA, DDR2		0x3A0001X3		MT9J003PA, RSYNC, the second in the category.
		0x2A0000X1		MT9F002
		0x5F0000X1		V5000A
		0x5F0000X2		NSC1105
		0x1A0000X6		OV13850
		0x1A0001X1		OPT9221
	The device index map is listed below:
		00 00			6		Iris Top
		00 01			7		Iris Bottom
		01 00			2		Face
		10 00			4		HDR Top
		10 01			5		HDR Bottom
		11 00			3		WDR

	If a device contains no magic code, then after device manual initialization, a valid code is needed to be burnt. However external logic 
	can still manually initialize the device without any limitation.



