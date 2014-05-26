/*
 * Copyright (c) 2014, Michael Müller
 * Copyright (c) 2014, Sebastian Lackner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <process/filesystem.h>
#include <process/object.h>
#include <memory/allocator.h>
#include <console/console.h>
#include <util/list.h>
#include <util/util.h>

/** \addtogroup Filesystem
 *  @{
 *	The filesystem implementation is still work in progress. It is similar to file
 *	handling on other platforms and based on named files and directory structures.
 */

struct directory *fileSystemRoot;

static void __directoryDestroy(struct object *obj);
static void __directoryShutdown(struct object *obj, UNUSED uint32_t mode);
static int32_t __directoryWrite(struct object *obj, uint8_t *buf, uint32_t length);
static int32_t __directoryRead(struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions directoryFunctions =
{
	__directoryDestroy,
	NULL, /* getMinHandle */
	__directoryShutdown,
	NULL, /* getStatus */
	NULL, /* wait */
	NULL, /* signal */
	__directoryWrite,
	__directoryRead,
	NULL, /* insert */
	NULL, /* remove */
};

static void __fileDestroy(struct object *obj);
static void __fileShutdown(struct object *obj, UNUSED uint32_t mode);
static int32_t __fileGetStatus(struct object *obj, UNUSED uint32_t mode);
static int32_t __fileWrite(struct object *obj, uint8_t *buf, uint32_t length);
static int32_t __fileRead(struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions fileFunctions =
{
	__fileDestroy,
	NULL, /* getMinHandle */
	__fileShutdown,
	__fileGetStatus,
	NULL, /* wait */
	NULL, /* signal */
	__fileWrite,
	__fileRead,
	NULL, /* insert */
	NULL, /* remove */
};

static void __openedDirectoryDestroy(struct object *obj);
static int32_t __openedDirectoryRead(struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions openedDirectoryFunctions =
{
	__openedDirectoryDestroy,
	NULL, /* getMinHandle */
	NULL, /* shutdown */
	NULL, /* getStatus */
	NULL, /* wait */
	NULL, /* signal */
	NULL, /* write */
	__openedDirectoryRead,
	NULL, /* insert */
	NULL, /* remove */
};

static void __openedFileDestroy(struct object *obj);
static void __openedFileShutdown(struct object *obj, UNUSED uint32_t mode);
static int32_t __openedFileGetStatus(struct object *obj, uint32_t mode);
static void __openedFileSignal(struct object *obj, uint32_t result);
static int32_t __openedFileWrite(struct object *obj, uint8_t *buf, uint32_t length);
static int32_t __openedFileRead(struct object *obj, uint8_t *buf, uint32_t length);

static const struct objectFunctions openedFileFunctions =
{
	__openedFileDestroy,
	NULL, /* getMinHandle */
	__openedFileShutdown,
	__openedFileGetStatus,
	NULL, /* wait */
	__openedFileSignal,
	__openedFileWrite,
	__openedFileRead,
	NULL, /* insert */
	NULL, /* remove */
};

static inline void __directoryShutdownChilds(struct directory *directory)
{
	struct file *f, *__f;
	struct directory *d, *__d;

	LL_FOR_EACH_SAFE(f, __f, &directory->files, struct file, obj.entry)
	{
		objectShutdown(f, 0);
	}

	LL_FOR_EACH_SAFE(d, __d, &directory->directories, struct directory, obj.entry)
	{
		objectShutdown(d, 0);
	}
}

static inline bool __isValidFilename(struct object *current, struct directory *parent, const char *buf, uint32_t length)
{
	struct file *cur_f;
	struct directory *cur_d;

	/* reserved names cannot be a filename */
	if (stringIsEqual(".", (char *)buf, length) || stringIsEqual("..", (char *)buf, length))
		return false;

	if (!parent)
		return true;

	/* check collision with other filenames */
	LL_FOR_EACH(cur_f, &parent->files, struct file, obj.entry)
	{
		if (&cur_f->obj != current && stringIsEqual(cur_f->name, (char *)buf, length))
			return false;
	}

	/* check collision with directory names */
	LL_FOR_EACH(cur_d, &parent->directories, struct directory, obj.entry)
	{
		if (&cur_d->obj != current && stringIsEqual(cur_d->name, (char *)buf, length))
			return false;
	}

	return true;
}

/**
 * @brief Creates a new kernel directory object
 *
 * @param parent Pointer to the parent directory
 * @param name Name of the directory or NULL if the directory doesn't have a name
 * @param nameLength Length of the name
 *
 * @return Pointer to the kernel directory object
 */
struct directory *directoryCreate(struct directory *parent, char *name, uint32_t nameLength)
{
	struct directory *d;
	char *buffer = NULL;

	/* allocate some new memory */
	if (!(d = heapAlloc(sizeof(*d))))
		return NULL;

	/* copy the name */
	if (name)
	{
		if (!(buffer = heapAlloc(nameLength + 1)))
		{
			heapFree(d);
			return NULL;
		}
		memcpy(buffer, name, nameLength);
		buffer[nameLength] = 0;
	}

	/* initialize general object info */
	__objectInit(&d->obj, &directoryFunctions);
	d->parent	= parent;
	d->name		= buffer;
	ll_init(&d->openedDirectories);

	ll_init(&d->files);
	ll_init(&d->directories);

	if (parent)
	{
		ll_add_tail(&parent->directories, &d->obj.entry);
		objectAddRef(d);
	}

	return d;
}

/**
 * @brief Destructor for kernel directory objects
 *
 * @param obj Pointer to the kernel directory object
 */
static void __directoryDestroy(struct object *obj)
{
	struct directory *d = objectContainer(obj, struct directory, &directoryFunctions);

	/* at this point there shouldn't be any parent directory anymore */
	assert(!d->parent);

	/* delete child elements (if any) */
	__directoryShutdownChilds(d);
	assert(ll_empty(&d->files) && ll_empty(&d->directories));

	/* free the memory containing the name */
	if (d->name) heapFree(d->name);

	/* release directory memory */
	d->obj.functions = NULL;
	heapFree(d);
}

/**
 * @brief Deletes the directory
 *
 * @param obj Pointer to the kernel directory object
 * @param mode not used
 */
static void __directoryShutdown(struct object *obj, UNUSED uint32_t mode)
{
	struct directory *d = objectContainer(obj, struct directory, &directoryFunctions);

	if (d->parent)
	{
		/* unlink from the parent directory */
		ll_remove(&d->obj.entry);
		d->parent = NULL;

		/* release object */
		objectRelease(d);
	}
}

/**
 * @brief Renames a kernel directory object
 *
 * @param obj Pointer to the kernel directory object
 * @param buf Pointer to the buffer containing the new name
 * @param length Length of the new name
 * @return Number of bytes written (without null characters) or (-1) on error
 */
static int32_t __directoryWrite(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct directory *d = objectContainer(obj, struct directory, &directoryFunctions);

	/* get rid of nullterminating character at the end of the filename */
	while (length > 0 && !buf[length - 1]) length--;

	if (length && !__isValidFilename(&d->obj, d->parent, (char *)buf, length))
		return -1;

	/* deallocate the old name */
	if (d->name)
		heapFree(d->name);

	/* if there is no new name */
	if (!length)
	{
		d->name = NULL;
		return 0;
	}

	/* allocate new memory for the name */
	d->name = heapAlloc(length + 1);
	if (!d->name) return 0;

	memcpy(d->name, buf, length);
	d->name[length] = 0;
	return length;
}

/**
 * @brief Looks up the name of a kernel directory object
 * @details If the last character written into tbe buffer is 0, then the buffer was
 *			big enough to hold the whole directory name.
 *
 * @param obj Pointer to the kernel directory object
 * @param buf Pointer to the buffer
 * @param length Size of the buffer
 * @return Number of bytes returned
 */
static int32_t __directoryRead(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct directory *d = objectContainer(obj, struct directory, &directoryFunctions);
	uint32_t file_length;
	if (!d->name) return -1;

	file_length = stringLength(d->name) + 1;

	/* limit the number of characters to the string length + 1 */
	if (length > file_length)
		length = file_length;

	memcpy(buf, d->name, length);
	return length;
}

/**
 * @brief Creates a new kernel file object
 *
 * @param parent Pointer to the parent directory
 * @param name Name of the file or NULL if the file doesn't have a name
 * @param nameLength Length of the name
 * @param heapBuffer Optional pointer to some heap memory
 * @param heapSize Size of the heap memory buffer
 *
 * @return Pointer to the kernel file object
 */
struct file *fileCreate(struct directory *parent, char *name, uint32_t nameLength, uint8_t *staticBuffer, uint32_t staticSize)
{
	struct file *f;
	char *buffer = NULL;

	/* allocate some new memory */
	if (!(f = heapAlloc(sizeof(*f))))
		return NULL;

	/* copy the name */
	if (name)
	{
		if (!(buffer = heapAlloc(nameLength + 1)))
		{
			heapFree(f);
			return NULL;
		}
		memcpy(buffer, name, nameLength);
		buffer[nameLength] = 0;
	}

	/* initialize general object info */
	__objectInit(&f->obj, &fileFunctions);
	f->parent	= parent;
	f->name		= buffer;
	ll_init(&f->openedFiles);

	if (staticBuffer)
	{
		f->isHeap	= false;
		f->buffer	= staticBuffer;
		f->size		= staticSize;
	}
	else
	{
		f->isHeap	= true;
		f->buffer	= NULL;
		f->size		= 0;
	}

	/* add this element to the directory */
	if (parent)
	{
		ll_add_tail(&parent->files, &f->obj.entry);
		objectAddRef(f);
	}

	return f;
}

/**
 * @brief Destructor for kernel file objects
 *
 * @param obj Pointer to the kernel file object
 */
static void __fileDestroy(struct object *obj)
{
	struct file *f = objectContainer(obj, struct file, &fileFunctions);

	/* at this point there shouldn't be any parent directory anymore */
	assert(!f->parent);

	/* free the memory containing the name */
	if (f->name) heapFree(f->name);

	/* release heap memory */
	if (f->isHeap && f->buffer)
		heapFree(f->buffer);

	/* release file memory */
	f->obj.functions = NULL;
	heapFree(f);
}

/**
 * @brief Deletes the file
 *
 * @param obj Pointer to the kernel file object
 * @param mode not used
 */
static void __fileShutdown(struct object *obj, UNUSED uint32_t mode)
{
	struct file *f = objectContainer(obj, struct file, &fileFunctions);

	if (f->parent)
	{
		/* unlink from the parent directory */
		ll_remove(&f->obj.entry);
		f->parent = NULL;

		/* release object */
		objectRelease(f);
	}
}

/**
 * @brief Returns the size of the kernel file object
 *
 * @param obj Pointer to the kernel file object
 * @param mode not used
 *
 * @return Number of bytes of the file
 */
static int32_t __fileGetStatus(struct object *obj, UNUSED uint32_t mode)
{
	struct file *f = objectContainer(obj, struct file, &fileFunctions);
	return f->size;
}

/**
 * @brief Renames a kernel file object
 *
 * @param obj Pointer to the kernel file object
 * @param buf Pointer to the buffer containing the new name
 * @param length Length of the new name
 * @return Number of bytes written (without null characters) or (-1) on error
 */
static int32_t __fileWrite(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct file *f = objectContainer(obj, struct file, &fileFunctions);

	/* get rid of nullterminating character at the end of the filename */
	while (length > 0 && !buf[length - 1]) length--;

	if (length && !__isValidFilename(&f->obj, f->parent, (char *)buf, length))
		return -1;

	/* deallocate the old name */
	if (f->name)
		heapFree(f->name);

	/* if there is no new name */
	if (!length)
	{
		f->name = NULL;
		return 0;
	}

	/* allocate new memory for the name */
	f->name = heapAlloc(length + 1);
	if (!f->name) return 0;

	memcpy(f->name, buf, length);
	f->name[length] = 0;
	return length;
}

/**
 * @brief Looks up the name of a kernel file object
 * @details If the last character written into tbe buffer is 0, then the buffer was
 *			big enough to hold the whole directory name.
 *
 * @param obj Pointer to the kernel file object
 * @param buf Pointer to the buffer
 * @param length Size of the buffer
 * @return Number of bytes returned
 */
static int32_t __fileRead(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct file *f = objectContainer(obj, struct file, &fileFunctions);
	uint32_t file_length;
	if (!f->name) return -1;

	file_length = stringLength(f->name) + 1;

	/* limit the number of characters to the string length + 1 */
	if (length > file_length)
		length = file_length;

	memcpy(buf, f->name, length);
	return length;
}

/**
 * @brief Creates a new kernel openedFile object
 *
 * @param file Pointer to the file
 *
 * @return Pointer to the kernel openedFile object
 */
struct openedFile *fileOpen(struct file *file)
{
	struct openedFile *h;
	assert(file);

	/* allocate some new memory */
	if (!(h = heapAlloc(sizeof(*h))))
		return NULL;

	/* initialize general object info */
	__objectInit(&h->obj, &openedFileFunctions);
	h->file		= file;
	h->pos		= 0;

	/* add this element to the file */
	ll_add_tail(&file->openedFiles, &h->obj.entry);
	objectAddRef(file);

	return h;
}

/**
 * @brief Destructor for kernel openedFile objects
 *
 * @param obj Pointer to the kernel openedFile object
 */
static void __openedFileDestroy(struct object *obj)
{
	struct openedFile *h = objectContainer(obj, struct openedFile, &openedFileFunctions);
	struct file *file = h->file;
	assert(file);

	/* unlink from the file */
	ll_remove(&h->obj.entry);

	/* release file memory */
	h->obj.functions = NULL;
	heapFree(h);

	/* decrement the refcount of the parent object */
	objectRelease(file);
}

/**
 * @brief Truncates a file at the given position
 *
 * @param obj Pointer to the kernel openedFile object
 * @param mode not used
 */
static void __openedFileShutdown(struct object *obj, UNUSED uint32_t mode)
{
	struct openedFile *h = objectContainer(obj, struct openedFile, &openedFileFunctions);
	struct file *f = h->file;

	/* only truncating supported so far */
	if (h->pos < f->size)
	{
		/* realloc buffer */
		if (f->isHeap && f->buffer)
		{
			uint8_t *new_buffer = heapReAlloc(f->buffer, h->pos);
			if (new_buffer) f->buffer = new_buffer;
		}

		/* define new size */
		f->size = h->pos;
	}
}

/**
 * @brief Returns the size or position of the kernel openedFile object
 *
 * @param obj Pointer to the kernel openedFile object
 * @param mode If true then this command returns the current position, otherwise
 *			   the size of of the file
 *
 * @return Position or size of the file
 */
static int32_t __openedFileGetStatus(struct object *obj, uint32_t mode)
{
	struct openedFile *h = objectContainer(obj, struct openedFile, &openedFileFunctions);
	return mode ? h->pos : h->file->size;
}

/**
 * @brief Moves the openedFile pointer
 *
 * @param object Pointer to the kernel openedFile object
 * @param result Position in the file
 */
static void __openedFileSignal(struct object *obj, uint32_t result)
{
	struct openedFile *h = objectContainer(obj, struct openedFile, &openedFileFunctions);
	h->pos = result;
}

/**
 * @brief Writes some data into the kernel openedFile object
 * @details This function writes a block of data into the kernel openedFile object.
 *
 * @param obj Pointer to the kernel openedFile object
 * @param buf Pointer to the buffer
 * @param length Number of bytes to write into the file
 * @return Number of bytes written
 */
static int32_t __openedFileWrite(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct openedFile *h = objectContainer(obj, struct openedFile, &openedFileFunctions);
	struct file *f = h->file;

	/* reallocate file memory */
	if (h->pos + length > f->size)
	{
		uint8_t *new_buffer;

		if (!f->isHeap || !f->buffer)
			new_buffer = heapAlloc(h->pos + length);
		else
			new_buffer = heapReAlloc(f->buffer, h->pos + length);

		if (new_buffer)
		{
			/* copy buffer */
			if (!f->isHeap && f->buffer)
				memcpy(new_buffer, f->buffer, f->size);

			/* clear unused buffer space */
			if (h->pos > f->size)
				memset(new_buffer + f->size, 0, h->pos - f->size);

			f->isHeap	= true;
			f->buffer	= new_buffer;
			f->size		= h->pos + length;
		}
		else if (f->isHeap && h->pos < f->size)
		{
			assert(f->size - h->pos < length);
			length = f->size - h->pos;
		}
		else return 0; /* no bytes left */
	}

	/* copy bytes to the buffer */
	memcpy(f->buffer + h->pos, buf, length);
	h->pos += length;

	return length;
}

/**
 * @brief Reads data from a kernel openedFile object into a buffer
 * @details This function reads a block of data from a kernel openedFile object.
 *
 * @param obj Pointer to the kernel openedFile object
 * @param buf Pointer to the buffer
 * @param length Number of bytes to read from the file
 * @return Number of bytes read
 */
static int32_t __openedFileRead(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct openedFile *h = objectContainer(obj, struct openedFile, &openedFileFunctions);
	struct file *f = h->file;
	if (h->pos >= f->size) return -1;

	/* don't read beyond the end of the file */
	if (length > f->size - h->pos)
		length = f->size - h->pos;

	/* copy bytes to the output */
	if (length > 0)
	{
		memcpy(buf, f->buffer + h->pos, length);
		h->pos += length;
	}

	return length;
}

/**
 * @brief Creates a new kernel openedDirectory object
 *
 * @param directory Pointer to the directory
 *
 * @return Pointer to the kernel openedDirectory object
 */
struct openedDirectory *directoryOpen(struct directory *directory)
{
	struct openedDirectory *h;
	assert(directory);

	/* allocate some new memory */
	if (!(h = heapAlloc(sizeof(*h))))
		return NULL;

	/* initialize general object info */
	__objectInit(&h->obj, &openedDirectoryFunctions);
	h->directory	= directory;
	h->pos			= NULL;

	/* add this element to the directory */
	ll_add_tail(&directory->openedDirectories, &h->obj.entry);
	objectAddRef(directory);

	return h;
}

/**
 * @brief Destructor for kernel openedDirectory objects
 *
 * @param obj Pointer to the kernel openedDirectory object
 */
static void __openedDirectoryDestroy(struct object *obj)
{
	struct openedDirectory *h = objectContainer(obj, struct openedDirectory, &openedDirectoryFunctions);
	struct directory *directory = h->directory;
	assert(directory);

	/* unlink from the directory */
	ll_remove(&h->obj.entry);

	/* release current object pointer */
	if (h->pos)
		__objectRelease(h->pos);

	/* release directory memory */
	h->obj.functions = NULL;
	heapFree(h);

	/* decrement the refcount of the parent object */
	objectRelease(directory);
}

/**
 * @brief Enumerate all directories and files in the current directory
 *
 * @param obj Pointer to the kernel openedDirectory object
 * @param buf Pointer to the buffer
 * @param length Number of bytes to read from the file
 * @return Number of bytes read
 */
static int32_t __openedDirectoryRead(struct object *obj, uint8_t *buf, uint32_t length)
{
	struct openedDirectory *h = objectContainer(obj, struct openedDirectory, &openedDirectoryFunctions);
	struct directory *directory = h->directory;

	struct file *f;
	struct directory *d;

	const char *name;
	uint32_t name_length;

	if (!h->pos || h->pos->functions == &fileFunctions)
	{
		if (!h->pos)
			f = LL_ENTRY(directory->files.next, struct file, obj.entry);
		else
		{
			f = objectContainer(h->pos, struct file, &fileFunctions);
			f = LL_ENTRY(f->obj.entry.next, struct file, obj.entry);
		}

		while (&f->obj.entry != &directory->files && !f->name)
			f = LL_ENTRY(f->obj.entry.next, struct file, obj.entry);

		if (&f->obj.entry == &directory->files)
		{
			d = LL_ENTRY(directory->directories.next, struct directory, obj.entry);
			goto enumdir;
		}

		h->pos = &f->obj;
		name = f->name;
	}
	else if (h->pos->functions == &directoryFunctions)
	{
		d = objectContainer(h->pos, struct directory, &directoryFunctions);
		d = LL_ENTRY(d->obj.entry.next, struct directory, obj.entry);
enumdir:
		while (&d->obj.entry != &directory->directories && !d->name)
			d = LL_ENTRY(d->obj.entry.next, struct directory, obj.entry);

		if (&d->obj.entry == &directory->directories)
		{
			h->pos = NULL;
			return -1;
		}

		h->pos = &d->obj;
		name = d->name;
	}
	else assert(0); /* should never happen */

	/* output name */
	name_length = stringLength(name) + 1;

	/* limit the number of characters to the string length + 1 */
	if (length > name_length)
		length = name_length;

	memcpy(buf, name, length);
	return length;
}

static inline bool __tarVerifyChecksum(struct tarHeader *tar)
{
	int8_t *buf = (int8_t *)tar;
	uint32_t i, cksum = 0;

	for (i = 0; i < 148; i++)
		cksum += buf[i];

	/* checksum filled with spaces */
	for (i = 0; i < 8; i++)
		cksum += ' ';

	for (i = 156; i < 512; i++)
		cksum += buf[i];

	return (cksum == stringParseOctal(tar->checksum, sizeof(tar->checksum)));
}

static inline bool __tarIsEOF(struct tarHeader *tar, uint32_t length)
{
	uint8_t *buf = (uint8_t *)tar;

	if (length < sizeof(struct tarHeader)*2)
		return false;

	length = sizeof(struct tarHeader)*2;

	while (length)
	{
		if (*buf)
			return false;
		buf++;
		length--;
	}

	return true;
}

/**
 * @brief Initializes the root file system
 */
void fileSystemInit(void *addr, uint32_t length)
{
	struct tarHeader *tar;

	assert(!fileSystemRoot);

	fileSystemRoot = directoryCreate(NULL, NULL, 0);
	assert(fileSystemRoot);

	/* tar header should fit exactly in one block */
	assert(sizeof(struct tarHeader) == 512);

	/* initialize default layout */
	tar = (struct tarHeader *)addr;
	while (length >= sizeof(struct tarHeader))
	{
		uint32_t size;
		struct file *f;
		char *name, namebuf[100+1+155+1];

		/* detect eof */
		if (__tarIsEOF(tar, length))
			break;

		/* ensure tar header is valid */
		assert(__tarVerifyChecksum(tar));
		size = stringParseOctal(tar->size, sizeof(tar->size));
		assert(size != (uint32_t)-1);
		assert(sizeof(struct tarHeader) + size <= length);
		name = namebuf;

		/* prepend with prefix */
		if (stringIsEqual("ustar", tar->magic, sizeof(tar->magic)) && tar->prefix[0])
		{
			uint32_t prefixlen;
			tar->prefix[sizeof(tar->prefix) - 1] = 0;
			prefixlen = stringLength(tar->prefix);
			memcpy(name, tar->prefix, prefixlen);
			name += prefixlen;
			*name++ = '/';
		}

		/* afterwards add the name and a null to terminate the string */
		memcpy(name, tar->name, sizeof(tar->name));
		name += sizeof(tar->name);
		*name = 0;

		if (!tar->typeflag || tar->typeflag == TAR_TYPE_FILE)
		{

			tar->name[sizeof(tar->name) - 1] = 0;
			f = fileSystemSearchFile(0, namebuf, stringLength(namebuf), true);
			assert(f);

			f->isHeap	= false;
			f->buffer	= (uint8_t *)(tar + 1);;
			f->size		= size;
			objectRelease(f);
		}

		/* round up to next 512 byte boundary */
		size = sizeof(struct tarHeader) + ((size + 511) & ~511);
		if (size > length)
			break;
		tar = (struct tarHeader *)((uint8_t *)tar + size);
		length -= size;
	}
}

/**
 * @brief Checks if a given object is of the type directory and casts it if possible
 * @details Use this functio to safely convert an arbitrary object to a directory
 *			object pointer. If the object doesn't have the right type (or a NULL pointer
 *			is passed), then NULL will be returned.
 *
 * @param obj Arbitrary kernel object
 * @return Pointer to a kernel directory object or NULL
 */
struct directory *fileSystemIsValidDirectory(struct object *obj)
{
	if (!obj) return NULL;

	if (obj->functions == &openedDirectoryFunctions)
	{
		struct openedDirectory *h = objectContainer(obj, struct openedDirectory, &openedDirectoryFunctions);
		if (!h->pos || h->pos->functions != &directoryFunctions) return NULL;
		return objectContainer(h->pos, struct directory, &directoryFunctions);
	}

	if (obj->functions == &directoryFunctions)
		return objectContainer(obj, struct directory, &directoryFunctions);

	return NULL;
}

/**
 * @brief Checks if a given object is of the type file and casts it if possible
 * @details Use this functio to safely convert an arbitrary object to a file
 *			object pointer. If the object doesn't have the right type (or a NULL pointer
 *			is passed), then NULL will be returned.
 *
 * @param obj Arbitrary kernel object
 * @return Pointer to a kernel file object or NULL
 */
struct file *fileSystemIsValidFile(struct object *obj)
{
	if (!obj) return NULL;

	if (obj->functions == &openedDirectoryFunctions)
	{
		struct openedDirectory *h = objectContainer(obj, struct openedDirectory, &openedDirectoryFunctions);
		if (!h->pos || h->pos->functions != &fileFunctions) return NULL;
		return objectContainer(h->pos, struct file, &fileFunctions);
	}

	if (obj->functions == &fileFunctions)
		return objectContainer(obj, struct file, &fileFunctions);

	return NULL;
}

/**
 * @brief Returns a reference to the root node of the file system
 * @details This function returns a reference to the root node of the file system
 *			and increases the reference count. When the reference is not needed
 *			anymore you have to release the object.
 *
 * @return Reference to the root node of the file system
 */
struct directory *fileSystemGetRoot()
{
	/* return reference to the root file system */
	objectAddRef(fileSystemRoot);
	return fileSystemRoot;
}

/**
 * @brief Opens or creates a directory
 * @details This function will search for a given directory in the file system root.
 *			If create is true, then the directory object is created if it doesn't exist yet
 *
 * @param directory Directory in which to search for the specific directory or NULL
 * @param path Path to the directory
 * @param pathLength Length of the directory
 * @param create If true the directory (including parent directories) will be created if it doesn't exist yet
 *
 * @return Pointer to the directory object
 */
struct directory *fileSystemSearchDirectory(struct directory *directory, char *path, uint32_t pathLength, bool create)
{
	struct file *f;
	struct directory *d;
	uint32_t componentLength;

	if (!directory)
		directory = fileSystemRoot;

subdir:
	assert(directory);

	/* skip over leading slashes */
	while (pathLength > 0 && path[0] == '/')
	{
		pathLength--;
		path++;
	}

	/* search for the path delimiter */
	componentLength = 0;
	while (componentLength < pathLength && path[componentLength] != '/') componentLength++;

	/* if the path is empty then return the current directory */
	if (!componentLength)
	{
		objectAddRef(directory);
		return directory;
	}

	/* handle special cases */
	if (stringIsEqual(".", path, componentLength) || stringIsEqual("..", path, componentLength))
	{
		path		+= componentLength;
		pathLength	-= componentLength;

		/* go to the parent directory (if any) */
		if (stringIsEqual("..", path, componentLength) && directory->parent)
			directory = directory->parent;

		goto subdir;
	}

	/* search for subdirectory */
	LL_FOR_EACH(d, &directory->directories, struct directory, obj.entry)
	{
		if (stringIsEqual(d->name, path, componentLength))
		{
			path		+= componentLength;
			pathLength	-= componentLength;
			directory	= d;
			goto subdir;
		}
	}

	/* if we don't want to create it we can return immediately */
	if (!create) return NULL;

	/* ensure name is not used by a file */
	LL_FOR_EACH(f, &directory->files, struct file, obj.entry)
	{
		if (stringIsEqual(f->name, path, componentLength))
			return NULL;
	}

	/* create a new directory (empty) */
	d = directoryCreate(directory, path, componentLength);
	if (!d) return NULL;

	path		+= componentLength;
	pathLength	-= componentLength;
	directory	= d;
	goto subdir;
}


/**
 * @brief Opens or creates a file
 * @details This function will search for a given file in the file system root.
 *			If create is specified the new file object is created if it doesn't exist yet
 *
 * @param directory Directory in which to search for the specific file or NULL
 * @param path Path to the file
 * @param pathLength Length of the filename
 * @param create If true the file (including parent directories) will be created if it doesn't exist yet
 *
 * @return Pointer to the file object
 */
struct file *fileSystemSearchFile(struct directory *directory, char *path, uint32_t pathLength, bool create)
{
	struct file *f;
	struct directory *d;
	uint32_t componentLength;

	componentLength = pathLength;
	while (componentLength > 0 && path[componentLength-1] != '/') componentLength--;

	/* no filename specified */
	if (componentLength >= pathLength)
		return NULL;

	d = fileSystemSearchDirectory(directory, path, componentLength, create);
	if (!d) return NULL;

	path		+= componentLength;
	pathLength	-= componentLength;
	directory	= d;

	/* reserved names cannot be a filename */
	if (stringIsEqual(".", path, pathLength) || stringIsEqual("..", path, pathLength))
		goto err;

	/* search for the file */
	LL_FOR_EACH(f, &directory->files, struct file, obj.entry)
	{
		if (stringIsEqual(f->name, path, pathLength))
		{
			objectAddRef(f);
			objectRelease(directory);
			return f;
		}
	}

	/* if we don't want to create it we can return immediately */
	if (!create) goto err;

	/* ensure the name is not used by a directory */
	LL_FOR_EACH(d, &directory->directories, struct directory, obj.entry)
	{
		if (stringIsEqual(d->name, path, pathLength))
			goto err;
	}

	/* create a new file and return the reference */
	f = fileCreate(directory, path, pathLength, NULL, 0);
	objectRelease(directory);
	return f;

err:
	objectRelease(directory);
	return NULL;
}

/**
 * @}
 */
