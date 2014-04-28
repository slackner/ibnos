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

#ifndef _H_FILESYSTEM_
#define _H_FILESYSTEM_

#ifdef __KERNEL__

	struct directory;
	struct file;
	struct openedFile;

	#include <stdint.h>
	#include <stdbool.h>

	#include <process/object.h>
	#include <util/list.h>

	struct directory
	{
		struct object obj;
		struct directory *parent;
		char *name;

		struct linkedList openedDirectories;

		struct linkedList files;
		struct linkedList directories;
	};

	struct file
	{
		struct object obj;
		struct directory *parent;
		char *name;

		struct linkedList openedFiles;

		bool isHeap;
		uint8_t *buffer;
		uint32_t size;
	};

	struct openedDirectory
	{
		struct object obj;
		struct directory *directory;

		struct object *pos;
	};

	struct openedFile
	{
		struct object obj;
		struct file *file;

		uint32_t pos;
	};

	struct directory *directoryCreate(struct directory *parent, char *name, uint32_t nameLength);
	struct file *fileCreate(struct directory *parent, char *name, uint32_t nameLength, uint8_t *staticBuffer, uint32_t staticSize);

	struct openedFile *fileOpen(struct file *file);
	struct openedDirectory *directoryOpen(struct directory *directory);

	void fileSystemInit();

	struct directory *fileSystemIsValidDirectory(struct object *obj);
	struct file *fileSystemIsValidFile(struct object *obj);

	struct directory *fileSystemGetRoot();

	struct directory *fileSystemSearchDirectory(struct directory *directory, char *path, uint32_t pathLength, bool create);
	struct file *fileSystemSearchFile(struct directory *directory, char *path, uint32_t pathLength, bool create);

#endif

#endif /* _H_FILESYSTEM_ */