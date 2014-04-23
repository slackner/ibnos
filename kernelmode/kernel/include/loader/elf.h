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

#ifndef _H_ELF_
#define _H_ELF_

#include <stdint.h>
#include <stdbool.h>

#define ELF_IDENT_MAG0			0
#define ELF_IDENT_MAG1			1
#define ELF_IDENT_MAG2			2
#define ELF_IDENT_MAG3			3
#define ELF_IDENT_CLASS			4
#define ELF_IDENT_DATA			5
#define ELF_IDENT_VERSION		6
#define ELF_IDENT_OSABI			7
#define ELF_IDENT_ABIVERSION	8
#define ELF_IDENT_PAD			9
#define ELF_NIDENT				16

#define ELF_MAG0	 0x7f
#define ELF_MAG1	 'E'
#define ELF_MAG2	 'L'
#define ELF_MAG3	 'F'

#define ELF_CLASS_NONE	0
#define ELF_CLASS_32	1
#define ELF_CLASS_64	2

/* see http://www.sco.com/developers/gabi/2000-07-17/ch4.eheader.html for the other 81 types*/
#define ELF_MACHINE_386 3

#define ELF_DATA_NONE	0
#define ELF_DATA_2LSB	1
#define ELF_DATA_2MSB	2

#define ELF_TYPE_NONE	0
#define ELF_TYPE_REL	1
#define ELF_TYPE_EXEC	2
#define ELF_TYPE_DYN	3
#define ELF_TYPE_CORE	4
#define ELF_TYPE_LOOS	0xfe00
#define ELF_TYPE_HIOS	0xfeff
#define ELF_TYPE_LOPROC	0xff00
#define ELF_TYPE_HIPROC	0xffff

#define ELF_SHN_UNDELF		0
#define ELF_SHN_LORESERVE	0xff00
#define ELF_SHN_LOPROC		0xff00
#define ELF_SHN_HIPROC		0xff1f
#define ELF_SHN_LOOS		0xff20
#define ELF_SHN_HIOS		0xff3f
#define ELF_SHN_ABS			0xfff1
#define ELF_SHN_COMMON		0xfff2
#define ELF_SHN_XINDEX		0xffff
#define ELF_SHN_HIRESERVE	0xffff

#define ELF_STYPE_NULL			0
#define ELF_STYPE_PROGBITS		1
#define ELF_STYPE_SYMTAB		2
#define ELF_STYPE_STRTAB		3
#define ELF_STYPE_RELA			4
#define ELF_STYPE_HASH			5
#define ELF_STYPE_DYNAMIC		6
#define ELF_STYPE_NOTE			7
#define ELF_STYPE_NOBITS		8
#define ELF_STYPE_REL			9
#define ELF_STYPE_SHLIB			10
#define ELF_STYPE_DYNSYM		11
#define ELF_STYPE_INIT_ARRAY	14
#define ELF_STYPE_FINI_ARRAY	15
#define ELF_STYPE_PREINIT_ARRAY	16
#define ELF_STYPE_GROUP			17
#define ELF_STYPE_SYMTAB_SHNDX	18
#define ELF_STYPE_LOOS			0x60000000
#define ELF_STYPE_HIOS			0x6fffffff
#define ELF_STYPE_LOPROC		0x70000000
#define ELF_STYPE_HIPROC		0x7fffffff
#define ELF_STYPE_LOUSER		0x80000000
#define ELF_STYPE_HIUSER		0xffffffff

#define ELF_SFLAGS_WRITE			0x1
#define ELF_SFLAGS_ALLOC			0x2
#define ELF_SFLAGS_EXECINSTR		0x4
#define ELF_SFLAGS_MERGE			0x10
#define ELF_SFLAGS_STRINGS			0x20
#define ELF_SFLAGS_INFO_LINK		0x40
#define ELF_SFLAGS_LINK_ORDER		0x80
#define ELF_SFLAGS_OS_NONCONFORMING	0x100
#define ELF_SFLAGS_GROUP			0x200
#define ELF_SFLAGS_MASKOS			0x0ff00000
#define ELF_SFLAGS_MASKPROC			0xf0000000

#define ELF_PTYPE_NULL		0
#define ELF_PTYPE_LOAD		1
#define ELF_PTYPE_DYNAMIC	2
#define ELF_PTYPE_INTERP	3
#define ELF_PTYPE_NOTE		4
#define ELF_PTYPE_SHLIB		5
#define ELF_PTYPE_PHDR		6
#define ELF_PTYPE_LOOS		0x60000000
#define ELF_PTYPE_HIOS		0x6fffffff
#define ELF_PTYPE_LOPROC	0x70000000
#define ELF_PTYPE_HIPROC	0x7fffffff

struct elfHeader
{
	uint8_t		ident[ELF_NIDENT];
	uint16_t	type;
	uint16_t	machine;
	uint32_t	version;
	uint32_t	entry;
	uint32_t	phoff;
	uint32_t	shoff;
	uint32_t	flags;
	uint16_t	ehsize;
	uint16_t	phentsize;
	uint16_t	phnum;
	uint16_t	shentsize;
	uint16_t	shnum;
	uint16_t	shstrndx;
};


struct elfSectionTable
{
	uint32_t	name;
	uint32_t	type;
	uint32_t	flags;
	uint32_t	addr;
	uint32_t	offset;
	uint32_t	size;
	uint32_t	link;
	uint32_t	info;
	uint32_t	addralign;
	uint32_t	entsize;
};

struct elfSymbolTable
{
	uint32_t	name;
	uint32_t	value;
	uint32_t	size;
	uint8_t		info;
	uint8_t		other;
	uint16_t	shndx;
};

struct selfSymbolInfo
{
	uint16_t boundto;
	uint16_t flags;
};

struct elfReolcationTable
{
	uint32_t	offset;
	uint32_t	info;
};

struct elfReolcationTableAddend
{
	uint32_t	offset;
	uint32_t	info;
	uint32_t	addend;
};

struct elfProgramHeader
{
	uint32_t	type;
	uint32_t	offset;
	uint32_t	vaddr;
	uint32_t	paddr;
	uint32_t	filesz;
	uint32_t	memsz;
	uint32_t	flags;
	uint32_t	align;
};

struct elfDynamicEntry
{
	uint32_t	tag;
	union
	{
		uint32_t val;
		uint32_t ptr;
	} un;
};

#ifdef __KERNEL__

	#include <stdbool.h>
	#include <stdint.h>

	#include <process/process.h>

	bool elfLoadBinary(struct process *p, void *addr, uint32_t length);

#endif

#endif /* _H_ELF_ */