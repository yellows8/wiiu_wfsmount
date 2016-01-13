This is a FUSE tool for accessing a mounted plaintext image for Wii U USB-storage.

For now this just allows read-access to a decrypted image, decrypted in chunks of 0x2000-bytes with iv=0(hence the first 0x10-bytes of some blocks may be junk).

