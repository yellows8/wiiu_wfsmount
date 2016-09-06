This is a FUSE tool for accessing a mounted plaintext image for Wii U USB-storage/eMMC. You must obtain the AES key(s) for use with this yourself(seperate for USB/eMMC). This does not implement anything more than image crypto, which is unlikely to change at this point. You could do *exactly* what this does with the openssl command for AES-CBC decryption with iv=0, however with this tool you don't need to decrypt the entire image all at once.

For now this just allows read-access to a decrypted image, decrypted in chunks of 0x2000-bytes with iv=0 via "/image.plain"(hence the first 0x10-bytes of some blocks may be off). The "/image_stream.plain" image is equivalent to decrypting the *entire* image all at once starting with iv=0 at offset 0x0, except you can read whatever size you want.

While the proper IV is known(for post-offset0 at least), this tool doesn't implement that(partly because it depends on the size of the block being accessed).

# Credits
* smea, for determining that eMMC uses wfs like usb-storage and hence the same crypto, etc.

