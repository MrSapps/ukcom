	opt	c+, at+, e+, n-
	section .text

; Defines the rom header, first the post boot entry point address
; the post boot ID and finally the boot TTY message.
	xref post_boot_entry_point
	dw post_boot_entry_point
	db "Licensed by Sony Computer Entertainment Inc.", 0
; Blank message
	dsb 76

; Almost a repeat of the above but contains the pre-boot entry point, additionally
; the TTY message space isn't used.
	xref pre_boot_entry_point
	dw pre_boot_entry_point
	db "Licensed by Sony Computer Entertainment Inc.", 0
; Blank message
	dsb 76
