#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>

#include <openssl/aes.h>

int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int wfs_getattr(const char *path, struct stat *stbuf);
int wfs_open(const char *path, struct fuse_file_info *fi);
int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int wfs_truncate(const char *path, off_t size);

static struct fuse_operations wfs_operations = {
	.getattr = wfs_getattr,
	.readdir = wfs_readdir,
	.open    = wfs_open,
	.read    = wfs_read,
	.write   = wfs_write,
	.truncate = wfs_truncate
};

FILE *fimage;
off_t image_size, image_baseoffset=0;
int image_readonly = 0;

AES_KEY image_aeskey_dec, image_aeskey_enc;

unsigned int storage_blocksize = 0x2000;

int main(int argc, char *argv[])
{
	int fargc, argi;
	int i, len;
	int fail;
	int offset_set;
	unsigned long long tmpval=0;
	char *ptr;
	char **fargv;
	size_t tmpsize;
	FILE *fkey;
	unsigned char key[0x10];
	struct stat filestat;

	if(argc<4)
	{
		printf("wiiu_wfsmount by yellows8\n");
		printf("FUSE tool for accessing a mounted plaintext image for Wii U wfs(usb/mlc).\n");
		printf("Usage:\n");
		printf("wiiu_wfsmount <imagepath> <filepath for the AES key> <mount-point + FUSE options>\n");
		printf("--imgsize=0x<size> Use the specified size for the image size instead of loading it with stat.\n");
		printf("--imgoff=0x<offset> Set the base offset(default is zero) of the actual image within the specified file/device. If stat() is sucessfully used for getting the imagesize, the imagesize is subtracted by this imgoff.\n");
		return 0;
	}

	fargc = argc - 2;
	fargv = (char **) malloc(fargc * sizeof(char *));
	fargv[0] = argv[0];

	fargc = 1;
	i = 1;
	for(argi=3; argi<argc; argi++)
	{
		if(strncmp(argv[argi], "--readonly", 10)==0)
		{
			image_readonly = 1;
		}
		else if(strncmp(argv[argi], "--imgsize=", 10)==0)
		{
			if(argv[argi][10]!='0' || argv[argi][10+1]!='x')
			{
				printf("Input value for imgsize is invalid.\n");
			}
			else
			{
				sscanf(&argv[argi][10+2], "%llx", &tmpval);
				image_size = (off_t)tmpval;
			}
		}
		else if(strncmp(argv[argi], "--imgoff=", 9)==0)
		{
			if(argv[argi][9]!='0' || argv[argi][9+1]!='x')
			{
				printf("Input value for imgoff is invalid.\n");
			}
			else
			{
				sscanf(&argv[argi][9+2], "%llx", &tmpval);
				image_baseoffset = (off_t)tmpval;
			}
		}
		else
		{
			fargv[i] = argv[argi];
			i++;
			fargc++;
		}
	}

	if(image_readonly==0)
	{
		fimage = fopen(argv[1], "r+");
	}
	else
	{
		fimage = fopen(argv[1], "rb");
	}
	if(fimage==NULL)
	{
		printf("Failed to open image: %s\n", argv[1]);
		free(fargv);
		return 1;
	}
	
	if(image_size==0)
	{
		stat(argv[1], &filestat);
		image_size = filestat.st_size;

		if(filestat.st_size)image_size-= image_baseoffset;
	}

	if(image_size==0)
	{
		printf("Invalid image size.\n");
		free(fargv);
		return 1;
	}

	printf("Using image base offset 0x%llx.\n", (unsigned long long)image_baseoffset);

	fkey = fopen(argv[2], "rb");
	if(fkey==NULL)
	{
		printf("Failed to open the key file.\n");
		free(fargv);
		return 1;
	}

	memset(key, 0, 0x10);
	tmpsize = fread(key, 1, 0x10, fkey);
	fclose(fkey);

	if(tmpsize!=0x10)
	{
		printf("Failed to read the key file.\n");
		free(fargv);
		return 1;
	}

	if (AES_set_decrypt_key(key, 128, &image_aeskey_dec) < 0)
    	{
        	printf("Failed to set AES decryption key.\n");
		free(fargv);
       	 	return 2;
    	}

	if (AES_set_encrypt_key(key, 128, &image_aeskey_enc) < 0)
    	{
        	printf("Failed to set AES encryption key.\n");
		free(fargv);
       	 	return 2;
    	}

	memset(key, 0, 0x10);

	return fuse_main(fargc, fargv, &wfs_operations);
}

int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	int partindex;
	char filename[32];

	if(strcmp(path, "/")==0)
	{
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		filler(buf, "image.plain", NULL, 0);
	}
	else
	{
		return -ENOENT;
	}

	return 0;
}

int wfs_getattr(const char *path, struct stat *stbuf)
{
	int partindex;

	memset(stbuf, 0, sizeof(struct stat));

	if(strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0445;
		stbuf->st_nlink = 2 + 1;
		return 0;
	}

	if(image_readonly==0)
	{
		stbuf->st_mode = S_IFREG | 0666;
	}
	else
	{
		stbuf->st_mode = S_IFREG | 0444;
	}

	if(strcmp(path, "/image.plain") == 0)
	{
		stbuf->st_nlink = 1;
		stbuf->st_size = image_size;
	}
	else
	{
		return -ENOENT;
	}

	return 0;
}

int wfs_open(const char *path, struct fuse_file_info *fi)
{
	int partindex;

	if(strcmp(path, "/image.plain") == 0)return 0;

	return -ENOENT;
}

int wfs_truncate(const char *path, off_t size)
{
	return wfs_open(path, NULL);
}

int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	unsigned char *tmpbuf;
	size_t transize, tmpsize, chunksize;
	size_t block_size, block_start;
	unsigned int data_start, pos;
	int i;

	unsigned char iv[0x10];

	memset(buf, 0, size);

	if(strcmp(path, "/image.plain")==0)
	{
		if(offset+size > image_size || offset > image_size)return -EINVAL;
	}
	else
	{
		return -ENOENT;
	}

	block_start = offset & ~(storage_blocksize-1);
	data_start = offset - block_start;
	block_size = data_start + size;
	block_size = (block_size + (storage_blocksize-1)) & ~(storage_blocksize-1);

	if(block_start+block_size > image_size)return -EINVAL;

	tmpbuf = (char*)malloc(block_size);
	if(tmpbuf==NULL)return -ENOMEM;
	memset(tmpbuf, 0, block_size);

	if(fseeko(fimage, image_baseoffset + block_start, SEEK_SET)==-1)
	{
		free(tmpbuf);
		return -EIO;
	}

	transize = fread(tmpbuf, 1, block_size, fimage);
	if(transize!=block_size)
	{
		free(tmpbuf);
		return -EIO;
	}

	for(pos=0; pos<block_size; pos+= storage_blocksize)
	{
		memset(iv, 0, 0x10);
		AES_cbc_encrypt(&tmpbuf[pos], &tmpbuf[pos], storage_blocksize, &image_aeskey_dec, iv, AES_DECRYPT);
	}

	memcpy(buf, (void*)&tmpbuf[data_start], size);

	free(tmpbuf);

	return size;
}

int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	char *tmpbuf;
	size_t transize, tmpsize = size, chunksize, pos=0;
	int i;
	int found;

	if(strcmp(path, "/image.plain")==0)
	{
		if(offset+size > image_size || offset > image_size)return -EINVAL;
	}
	else
	{
		return -ENOENT;
	}

	if(image_readonly)return -EACCES;

	return -EACCES;//Implement writing later.

	/*xorbuf = (char*)malloc(size);
	if(xorbuf==NULL)return -ENOMEM;
	memset(xorbuf, 0, size);

	while(tmpsize)
	{
		found = 0;
		for(partindex=0; partindex<8; partindex++)
		{
			part = &ncsd_partitions[partindex];
			if(part->enabled==0)continue;

			if(offset >= part->imageoffset && offset < part->imageoffset + part->size)
			{
				found = 1;
				break;
			}
		}

		if(!found)
		{
			free(xorbuf);
			if(pos==0)return -EINVAL;
			return pos;
		}

		fxorpad = part->fxor;

		chunksize = tmpsize;
		if(offset + chunksize > part->imageoffset + part->size)chunksize -= (part->imageoffset + part->size) - offset;

		if(fseeko(fnand, nandimage_baseoffset + offset, SEEK_SET)==-1)
		{
			free(xorbuf);
			return -EIO;
		}
		if(fseeko(fxorpad, offset - part->imageoffset, SEEK_SET)==-1)
		{
			free(xorbuf);
			return -EIO;
		}

		transize = fread(&xorbuf[pos], 1, chunksize, fxorpad);
		if(transize!=chunksize)
		{
			free(xorbuf);
			return -EIO;
		}

		for(i=0; i<chunksize; i++)xorbuf[pos+i] ^= buf[pos+i];

		transize = fwrite(&xorbuf[pos], 1, chunksize, fnand);
		if(transize!=chunksize)
		{
			free(xorbuf);
			return -EIO;
		}

		tmpsize-= chunksize;
		offset+= chunksize;
		pos+= chunksize;
	}

	free(xorbuf);

	return size;*/
}

