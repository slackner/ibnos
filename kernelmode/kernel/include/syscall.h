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

#ifndef _H_SYSCALL_
#define _H_SYSCALL_

#include <stdint.h>
#include <stdbool.h>

/** \addtogroup Syscalls
 *  @{
 */

/** Syscall table */
enum
{

	/**
	 * Schedule another process.
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				Nothing
	 */
	SYSCALL_YIELD,

	/**
	 * Exit current process.
	 * - \b Parameters:
	 *				- Exitcode
	 * - \b Returns:
	 *				Nothing (never returns)
	 */
	SYSCALL_EXIT_PROCESS,

	/**
	 * Exit current thread.
	 * - \b Parameters:
	 *				- Exitcode
	 * - \b Returns:
	 *				Nothing (never returns)
	 */
	SYSCALL_EXIT_THREAD,

	/**
	 * Allocates a new handle which points to the current process.
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				Handle to the current process
	 */
	SYSCALL_GET_CURRENT_PROCESS,

	/**
	 * Allocates a new handle which points to the current thread.
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				Handle to the current thread
	 */
	SYSCALL_GET_CURRENT_THREAD,

	/**
	 * Returns the current kernel timestamp (in milliseconds)
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				Kernel timestamp (lower 32bit)
	 */
	SYSCALL_GET_MONOTONIC_CLOCK,

	/**
	 * Returns information about all running processes in the system
	 * - \b Parameters:
	 *				- Pointer to an array of processInfo structures
	 *				- Number of entries in the array
	 * - \b Returns:
	 *				- Number of processes (can also be greater than the number of entries in the array)
	 */
	SYSCALL_GET_PROCESS_INFO,

	/**
	 * Get thread local storage address.
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				- Pointer to thread local storage
	 */
	SYSCALL_GET_THREADLOCAL_STORAGE_BASE		= 0x100,

	/**
	 * Get thread local storage length in bytes.
	 * - \b Parameters:
	 *				None
	 *
	 * - \b Returns:
	 *				- Length in bytes
	 */
	SYSCALL_GET_THREADLOCAL_STORAGE_LENGTH,

	/**
	 * Allocate virtual pages.
	 * - \b Parameters:
	 *				- Number of pages to allocate
	 * - \b Returns:
	 *				- Pointer to the first page
	 */
	SYSCALL_ALLOCATE_MEMORY						= 0x200,

	/**
	 * Free virtual pages.
	 * - \b Parameters:
	 *				- Pointer to the first page to free
	 *				- Number of pages to free
	 * - \b Returns:
	 *				- True on sucess, otherwise false
	 */
	SYSCALL_RELEASE_MEMORY,

	/**
	 * Fork process.
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				-  0: you are the forked process
	 *				- >0: pid of the client process (i.e. you are the parent process)
	 *				- <0: error
	 */
	SYSCALL_FORK								= 0x300,

	/**
	 * Create a new thread.
	 * - \b Parameters:
	 *				- Pointer to the function which should be started in the new
	 *				thread.
	 *				- (opt) New value of eax
	 *				- (opt) New value of ebx
	 *				- (opt) New value of ecx
	 *				- (opt) New value of edx
	 * - \b Returns:
	 *				- Thread handle
	 */
	SYSCALL_CREATE_THREAD,

	/**
	 * Creates a new event object.
	 * - \b Parameters:
	 *				- True if all process should be woken up, otherwise only one process will wake up
	 * - \b Returns:
	 *				- Event handle
	 */
	SYSCALL_CREATE_EVENT,

	/**
	 * Creates a new semaphore object.
	 * - \b Parameters:
	 *				- Initial value of the semaphore
	 * - \b Returns:
	 *				- Semaphore handle
	 */
	SYSCALL_CREATE_SEMAPHORE,

	/**
	 * Creates a new pipe object.
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				- Pipe handle
	 */
	SYSCALL_CREATE_PIPE,

	/**
	 * Creates a new timer object.
	 * - \b Parameters:
	 *				- True if all process should be woken up, otherwise only one process will wake up
	 * - \b Returns:
	 *				- Timer handle
	 */
	SYSCALL_CREATE_TIMER,

	/**
	 * Duplicates a handle
	 * - \b Parameters:
	 *				- any kernel handle
	 * - \b Returns:
	 *				- new kernel handle
	 */
	 SYSCALL_OBJECT_DUP,

	/**
	 * Replaces a kernel handle with a new object
	 * - \b Parameters:
	 *			- soure kernel handle
	 *			- destination kernel handle
	 * - \b Returns:
	 *			- new kernel handle
	 */
	SYSCALL_OBJECT_DUP2,

	/**
	 * Checks if a given handle exists.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 * - \b Returns:
	 *				- True if it exists, otherwise false
	 */
	SYSCALL_OBJECT_EXISTS						= 0x400,

	/**
	 * Checks if two handles point to the same kernel object.
	 * - \b Parameters:
	 *				- Handle 1
	 *				- Handle 2
	 * - \b Returns:
	 *				- -1 if both handles are invalid
	 *				- True if both handles are equal, otherwise false
	 */
	SYSCALL_OBJECT_COMPARE,

	/**
	 * Closes a specific handle. The object can still remain if other handles also point to it.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 * - \b Returns:
	 *				- True on success, otherwise false
	 */
	SYSCALL_OBJECT_CLOSE,

	/**
	 * Forces shutdown of a specific kernel object (for example thread termination, closing pipes, ...)
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- (opt) Mode paramter
	 * - \b Returns:
	 *				- True on success, otherwise false
	 */
	SYSCALL_OBJECT_SHUTDOWN,

	/**
	 * Returns an integer representing the internal state of an object. If an object has multiple states
	 * the mode parameter can be used to query each one individually. The exact meaning depends on the
	 * type of the object.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- (opt) Mode parameter
	 * - \b Returns:
	 *				- True on success, otherwise false
	 */
	SYSCALL_OBJECT_GET_STATUS,

	/**
	 * Waits for a specific action related to the kernel object (thread or process termination, semaphore).
	 * The exact meaning depends on the type of the object.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- (opt) Mode parameter
	 * - \b Returns:
	 *				- -1: invalid handle or wakeup because of a critical error (for example object deleted)
	 *				- otherwise: result value (depending on the object)
	 */
	SYSCALL_OBJECT_WAIT,

	/**
	 * Wakes up a specific kernel object. This is only valid for specific objects like threads, semaphores or events.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- Result
	 * - \b Returns:
	 *				- True on success, otherwise false
	 */
	SYSCALL_OBJECT_SIGNAL,

	/**
	 * Writes some data into a kernel object. This is only valid for specific objects like pipes.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- Buffer
	 *				- Length in bytes
	 * - \b Returns:
	 *				- >=0: number of bytes written, 0 means that we have to wait for more data
	 *				-  <0: invalid handle or pointer
	 */
	SYSCALL_OBJECT_WRITE,

	/**
	 * Reads some data from a kernel object. This is only valid for specific objects like pipes.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- Buffer
	 *				- Length in bytes
	 * - \b Returns:
	 *				- >=0: number of bytes read, 0 means that we have to wait for more data
	 *				-  <0: invalid handle or pointer
	 */
	SYSCALL_OBJECT_READ,

	/**
	 * Attaches a second kernel object to the first one. This is only valid for specific objects like events.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- Handle to a second object which should be attached
	 *				- (opt) Mode parameter
	 *				- Identifier
	 * - \b Returns:
	 *				- True on sucess, otherwise false
	 */
	SYSCALL_OBJECT_ATTACH_OBJ,

	/**
	 * Detaches a second kernel object to the first one. This is only valid for specific objects like events.
	 * - \b Parameters:
	 *				- Handle to a kernel object
	 *				- Identifier
	 * - \b Returns:
	 *				- True on sucess, otherwise false
	 */
	SYSCALL_OBJECT_DETACH_OBJ,

	/**
	 * Displays a string on the terminal
	 * - \b Parameters:
	 *				- Buffer
	 *				- Length in bytes
	 * - \b Returns:
	 *				- >=0: number of bytes written
	 *				-  <0: invalid handle or pointer
	 */
	SYSCALL_CONSOLE_WRITE							= 0x500,

	/**
	 * Displays raw color and character codes on the terminal
	 * - \b Parameters:
	 *				- Buffer (uint16_t)
	 *				- Length in characters (half the size of the memory)
	 * - \b Returns:
	 *				- >=0: number of bytes written
	 *				-  <0: invalid handle or pointer
	 */
	SYSCALL_CONSOLE_WRITE_RAW,

	/**
	 * Clears the terminal, i.e. removes all text
	 * - \b Parameters:
	 *				None
	 * - \b Returns:
	 *				Nothing
	 */
	SYSCALL_CONSOLE_CLEAR,

	/**
	 * Get she size of the terminal
	 * - \b Parameters:
	 *				- None
	 * - \b Returns:
	 *				- higher 16 bit contain the height and lower 16 bit the width
	 */
	SYSCALL_CONSOLE_GET_SIZE,

	/**
	 * Set color for normal text writes
	 * - \b Parameters:
	 *				- Packed color (foreground & background color)
	 * - \b Returns:
	 *				- Nothing
	 */
	SYSCALL_CONSOLE_SET_COLOR,

	/**
	 * Get the current color of the terminal
	 * - \b Parameters:
	 *				- None
	 * - \b Returns:
	 *				- Packed color (foreground & background color)
	 */
	SYSCALL_CONSOLE_GET_COLOR,

	/**
	 * Set the position of the text cursor
	 * - \b Parameters:
	 *				- X position
	 *				- Y position
	 * - \b Returns:
	 *				- Nothing
	 */
	SYSCALL_CONSOLE_SET_CURSOR,

	/**
	 * Get the current position of the text cursor
	 * - \b Parameters:
	 *				- None
	 * - \b Returns:
	 *				- Packed position (X & Y position)
	 */
	SYSCALL_CONSOLE_GET_CURSOR,

	/**
	 * Set the position of the hardware cursor
	 * - \b Parameters:
	 *				- X position
	 *				- Y position
	 * - \b Returns:
	 *				- Nothing
	 */
	SYSCALL_CONSOLE_SET_HARDWARE_CURSOR,

	/**
	 * Get the current position of the hardware cursor
	 * - \b Parameters:
	 *				- None
	 * - \b Returns:
	 *				- Packed position (X & Y position)
	 */
	SYSCALL_CONSOLE_GET_HARDWARE_CURSOR,

	/**
	 * Set the flags for the console
	 * - \b Parameters:
	 *				- Flags
	 * - \b Returns:
	 *				- Nothing
	 */
	SYSCALL_CONSOLE_SET_FLAGS,

	/**
	 * Get the current flags for the console
	 * - \b Parameters:
	 *				- None
	 * - \b Returns:
	 *				- flags
	 */
	SYSCALL_CONSOLE_GET_FLAGS,

	/**
	 * Searches for a given file, and returns a pointer to the file object
	 * - \b Parameters:
	 *				- Parent directory handle or (-1)
	 *				- Filename buffer
	 *				- Filename length
	 *				- Create nonexistent file
	 *	- \b Returns:
	 *				- Handle to the file object
	 */
	SYSCALL_FILESYSTEM_SEARCH_FILE					= 0x600,

	/**
	 * Searches for a given directory, and returns a pointer to the directory object
	 * - \b Parameters:
	 *				- Parent directory handle or (-1)
	 *				- Directory buffer
	 *				- Directory length
	 *				- Create nonexistent directory
	 *	- \b Returns:
	 *				- Handle to the directory object
	 */
	SYSCALL_FILESYSTEM_SEARCH_DIRECTORY,

	/**
	 * Opens a directory or file.
	 * - \b Parameters:
	 *				- Handle to a directory/file object
	 * - \b Returns:
	 *				- Handle to a opened directory/file object
	 */
	SYSCALL_FILESYSTEM_OPEN,

};

#ifndef __KERNEL__

	#define IBNOS_SYSCALL_FN(syscall, n0, n1, n2, n3, n4, n5, n6, n7, n8, n, ...) ibnos_syscall##n
	#define ibnos_syscall(syscall, ...) IBNOS_SYSCALL_FN(syscall, ##__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)(syscall, ##__VA_ARGS__)

	static inline int ibnos_syscall0(int syscall)
	{
		int ret;
		asm volatile ("int $0x80" :"=a"(ret) :"a"(syscall));
		return ret;
	}

	static inline int ibnos_syscall1(int syscall, uint32_t value1)
	{
		int ret;
		asm volatile ("int $0x80" :"=a"(ret) : "a"(syscall), "b"(value1));
		return ret;
	}

	static inline int ibnos_syscall2(int syscall, uint32_t value1, uint32_t value2)
	{
		int ret;
		asm volatile ("int $0x80" :"=a"(ret) : "a"(syscall), "b"(value1), "c" (value2));
		return ret;
	}

	static inline int ibnos_syscall3(int syscall, uint32_t value1, uint32_t value2, uint32_t value3)
	{
		int ret;
		asm volatile ("int $0x80" :"=a"(ret) : "a"(syscall), "b"(value1), "c" (value2), "d" (value3));
		return ret;
	}

	static inline int ibnos_syscall4(int syscall, uint32_t value1, uint32_t value2, uint32_t value3, uint32_t value4)
	{
		int ret;
		asm volatile ("int $0x80" :"=a"(ret) : "a"(syscall), "b"(value1), "c" (value2), "d" (value3), "S" (value4));
		return ret;
	}

	static inline int ibnos_syscall5(int syscall, uint32_t value1, uint32_t value2, uint32_t value3, uint32_t value4, uint32_t value5)
	{
		int ret;
		asm volatile ("int $0x80" :"=a"(ret) : "a"(syscall), "b"(value1), "c" (value2), "d" (value3), "S" (value4), "D" (value5));
		return ret;
	}

	static inline void yield()
	{
		ibnos_syscall(SYSCALL_YIELD);
	}
	/* exit is already provided by the libc */

	static inline void exitThread(int exitcode)
	{
		ibnos_syscall(SYSCALL_EXIT_THREAD, exitcode);
	}

	static inline int32_t getCurrentProcess()
	{
		return (int32_t)ibnos_syscall(SYSCALL_GET_CURRENT_PROCESS);
	}

	static inline int32_t getCurrentThread()
	{
		return (int32_t)ibnos_syscall(SYSCALL_GET_CURRENT_THREAD);
	}

	static inline uint32_t getMonotonicClock()
	{

		return ibnos_syscall(SYSCALL_GET_MONOTONIC_CLOCK);
	}

	static inline uint32_t getProcessInfo(void *info, uint32_t count)
	{
		return ibnos_syscall(SYSCALL_GET_PROCESS_INFO, (uint32_t)info, count);
	}

	static inline void *getTLS()
	{
		return (void*)ibnos_syscall(SYSCALL_GET_THREADLOCAL_STORAGE_BASE);
	}

	static inline uint32_t getTLSLength()
	{
		return ibnos_syscall(SYSCALL_GET_THREADLOCAL_STORAGE_LENGTH);
	}

	/* malloc, free and fork are also provided by the libc */

	extern void *_thread_start;
	static inline int32_t createThread(void *func, uint32_t arg0, uint32_t arg1, uint32_t arg2)
	{
		return (int32_t)ibnos_syscall(SYSCALL_CREATE_THREAD, (uint32_t)&_thread_start, (uint32_t)func, arg0, arg1, arg2);
	}

	static inline int32_t createEvent(bool wakeupAll)
	{
		return (int32_t)ibnos_syscall(SYSCALL_CREATE_EVENT, (uint32_t)wakeupAll);
	}

	static inline int32_t createSemaphore(uint32_t value)
	{
		return (int32_t)ibnos_syscall(SYSCALL_CREATE_SEMAPHORE, value);
	}

	static inline int32_t createPipe()
	{
		return (int32_t)ibnos_syscall(SYSCALL_CREATE_PIPE);
	}

	static inline int32_t createTimer(bool wakeupAll)
	{
		return (int32_t)ibnos_syscall(SYSCALL_CREATE_TIMER, (uint32_t)wakeupAll);
	}

	/* dup, dup2 are provided by libc */

	static inline bool objectExists(int32_t handle)
	{
		return ibnos_syscall(SYSCALL_OBJECT_EXISTS, (uint32_t)handle);
	}

	static inline int32_t objectCompare(int32_t handle1, int32_t handle2)
	{
		return (int32_t)ibnos_syscall(SYSCALL_OBJECT_EXISTS, (uint32_t)handle1, (uint32_t)handle2);
	}

	static inline bool objectClose(int32_t handle)
	{
		return ibnos_syscall(SYSCALL_OBJECT_CLOSE, (uint32_t)handle);
	}

	static inline bool objectShutdown(int32_t handle, uint32_t mode)
	{
		return ibnos_syscall(SYSCALL_OBJECT_SHUTDOWN, (uint32_t)handle, mode);
	}

	static inline uint32_t objectGetStatus(int32_t handle, uint32_t mode)
	{
		return ibnos_syscall(SYSCALL_OBJECT_GET_STATUS, (uint32_t)handle, mode);
	}

	static inline int32_t objectWait(int32_t handle, uint32_t mode)
	{
		return (int32_t)ibnos_syscall(SYSCALL_OBJECT_WAIT, (uint32_t)handle, mode);
	}

	static inline bool objectSignal(int32_t handle, uint32_t result)
	{
		return ibnos_syscall(SYSCALL_OBJECT_SIGNAL, (uint32_t)handle, result);
	}

	static inline int32_t objectWrite(int32_t handle, void *buffer, uint32_t length)
	{
		return (int32_t)ibnos_syscall(SYSCALL_OBJECT_WRITE, (uint32_t)handle, (uint32_t)buffer, length);
	}

	static inline int32_t objectRead(int32_t handle, void *buffer, uint32_t length)
	{
		return (int32_t)ibnos_syscall(SYSCALL_OBJECT_READ, (uint32_t)handle, (uint32_t)buffer, length);
	}

	static inline bool objectAttach(int32_t handle, int32_t childHandle, uint32_t mode, uint32_t ident)
	{
		return ibnos_syscall(SYSCALL_OBJECT_ATTACH_OBJ, (uint32_t)handle, (uint32_t)childHandle, mode, ident);
	}

	static inline bool objectDetach(int32_t handle, uint32_t ident)
	{
		return ibnos_syscall(SYSCALL_OBJECT_DETACH_OBJ, (uint32_t)handle, ident);
	}

	static inline int32_t consoleWrite(const char *buffer, uint32_t length)
	{
		return (int32_t)ibnos_syscall(SYSCALL_CONSOLE_WRITE, (uint32_t)buffer, length);
	}

	static inline int32_t consoleWriteRaw(const uint16_t *buffer, uint32_t characters)
	{
		return (int32_t)ibnos_syscall(SYSCALL_CONSOLE_WRITE_RAW, (uint32_t)buffer, characters);
	}

	static inline void consoleClear()
	{
		ibnos_syscall(SYSCALL_CONSOLE_CLEAR);
	}

	static inline uint32_t consoleGetSize()
	{
		return ibnos_syscall(SYSCALL_CONSOLE_GET_SIZE);
	}

	static inline void consoleSetColor(uint32_t value)
	{
		ibnos_syscall(SYSCALL_CONSOLE_SET_COLOR, value);
	}

	static inline uint32_t consoleGetColor()
	{
		return ibnos_syscall(SYSCALL_CONSOLE_GET_COLOR);
	}

	static inline void consoleSetCursor(uint32_t x, uint32_t y)
	{
		ibnos_syscall(SYSCALL_CONSOLE_SET_CURSOR, x, y);
	}

	static inline uint32_t consoleGetCursor()
	{
		return ibnos_syscall(SYSCALL_CONSOLE_GET_CURSOR);
	}

	static inline void consoleSetHardwareCursor(uint32_t x, uint32_t y)
	{
		ibnos_syscall(SYSCALL_CONSOLE_SET_HARDWARE_CURSOR, x, y);
	}

	static inline uint32_t consoleGetHardwareCursor()
	{
		return ibnos_syscall(SYSCALL_CONSOLE_GET_HARDWARE_CURSOR);
	}

	static inline void consoleSetFlags(uint32_t flags)
	{
		ibnos_syscall(SYSCALL_CONSOLE_SET_FLAGS, flags);
	}

	static inline uint32_t consoleGetFlags()
	{
		return ibnos_syscall(SYSCALL_CONSOLE_GET_FLAGS);
	}

	static inline int32_t filesystemSearchFile(int32_t handle, const char *path, uint32_t length, bool create)
	{
		return ibnos_syscall(SYSCALL_FILESYSTEM_SEARCH_FILE, (uint32_t)handle, (uint32_t)path, length, create);
	}

	static inline int32_t filesystemSearchDirectory(int32_t handle, const char *path, uint32_t length, bool create)
	{
		return ibnos_syscall(SYSCALL_FILESYSTEM_SEARCH_DIRECTORY, (uint32_t)handle, (uint32_t)path, length, create);
	}

	static inline int32_t filesystemOpen(int32_t handle)
	{
		return ibnos_syscall(SYSCALL_FILESYSTEM_OPEN, (uint32_t)handle);
	}

#endif
/**
 *  @}
 */
#endif /* _H_SYSCALL_ */