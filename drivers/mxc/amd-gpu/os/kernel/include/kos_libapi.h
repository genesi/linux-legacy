/* Copyright (c) 2008-2010, Advanced Micro Devices. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __KOSAPI_H
#define __KOSAPI_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "os_types.h"


//////////////////////////////////////////////////////////////////////////////
//   entrypoint abstraction
//////////////////////////////////////////////////////////////////////////////


#define KOS_DLLEXPORT   extern
#define KOS_DLLIMPORT

//////////////////////////////////////////////////////////////////////////////
//   KOS lib entrypoints
//////////////////////////////////////////////////////////////////////////////
#ifdef __KOSLIB_EXPORTS
#define KOS_API         KOS_DLLEXPORT
#else
#define KOS_API         KOS_DLLIMPORT
#endif // __KOSLIB_EXPORTS

//////////////////////////////////////////////////////////////////////////////
//                             assert API
//////////////////////////////////////////////////////////////////////////////
KOS_API void                    kos_assert_hook(const char* file, int line, int expression);

#if defined(DEBUG) || defined(DBG) || defined (_DBG) || defined (_DEBUG)
#define KOS_ASSERT(expression) //kos_assert_hook(__FILE__, __LINE__, (int)(expression))
#else
#define KOS_ASSERT(expression)
#endif // DEBUG || DBG || _DBG

#if defined(_DEBUG)
#define KOS_MALLOC_DBG(size)    kos_malloc(int size)
#endif // _DEBUG

#define kos_assert(expression)  KOS_ASSERT(expression)
#define kos_malloc_dbg(size)    KOS_MALLOC_DBG(size)

#ifdef UNDER_CE
#define KOS_PAGE_SIZE 0x1000
#endif

typedef enum mutexIndex mutexIndex_t;

//////////////////////////////////////////////////////////////////////////////
//  sync API
//////////////////////////////////////////////////////////////////////////////
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Create a mutex instance.
 *
 *
 * \param   void* name      Name string for the new mutex.
 * \return  Returns a handle to the mutex.
 *//*-------------------------------------------------------------------*/
KOS_API oshandle_t      kos_mutex_create(const char* name);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Get a handle to an already existing mutex.
 *
 *
 * \param   void* name      Name string for the new mutex.
 * \return  Returns a handle to the mutex.
 *//*-------------------------------------------------------------------*/
KOS_API oshandle_t      kos_mutex_open(const char* name);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Free the given mutex.
 *
 *
 * \param   oshandle_t mutexhandle  Handle to the mutex.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_mutex_free(oshandle_t mutexhandle);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Lock the given mutex.
 *
 *
 * \param   oshandle_t mutexhandle  Handle to the mutex.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_mutex_lock(oshandle_t mutexhandle);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Try to lock the given mutex, if already locked returns immediately.
 *
 *
 * \param   oshandle_t mutexhandle  Handle to the mutex.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_mutex_locktry(oshandle_t mutexhandle);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Try to lock the given mutex by waiting for its release. Returns without locking if the
 * mutex is already locked and cannot be acquired within the given period.
 *
 *
 * \param   oshandle_t mutexhandle  Handle to the mutex.
 * \param   int millisecondstowait  Time to wait for the mutex to be available.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_mutex_lockwait(oshandle_t mutexhandle, int millisecondstowait);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Unlock the given mutex.
 *
 *
 * \param   oshandle_t mutexhandle  Handle to the mutex.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_mutex_unlock(oshandle_t mutexhandle);

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Increments (increases by one) the value of the specified 32-bit variable as an atomic operation.
 *
 *
 * \param   int* ptr Pointer to the value to be incremented.
 * \return  Returns the new incremented value.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interlock_incr(int* ptr);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Decrements (decreases by one) the value of the specified 32-bit variable as an atomic operation.
 *
 *
 * \param   int* ptr Pointer to the value to be decremented.
 * \return  Returns the new decremented value.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interlock_decr(int* ptr);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Atomic replacement of a value.
 *
 *
 * \param   int* ptr Pointer to the value to be replaced.
 * \param   int value The new value.
 * \return  Returns the old value.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interlock_xchg(int* ptr, int value);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Perform an atomic compare-and-exchange operation on the specified values. Compares the two specified 32-bit values and exchanges
* with another 32-bit value based on the outcome of the comparison.
 *
 *
 * \param   int* ptr Pointer to the value to be replaced.
 * \param   int value The new value.
 * \param   int compvalue Value to be compared with.
 * \return  Returns the initial value of the first given parameter.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interlock_compxchg(int* ptr, int value, int compvalue);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Atomic addition of two 32-bit values.
 *
 *
 * \param   int* ptr Pointer to the target value.
 * \param   int value Value to be added to the target.
 * \return  Returns the initial value of the target.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interlock_xchgadd(int* ptr, int value);

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Create an event semaphore.
 *
 *
 * \param   int a_manualReset Selection for performing reset manually (or by the system).
 * \return  Returns an handle to the created semaphore.
 *//*-------------------------------------------------------------------*/
KOS_API oshandle_t      kos_event_create(int a_manualReset);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Destroy an event semaphore.
 *
 *
 * \param   oshandle_t a_event Handle to the semaphore.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_event_destroy(oshandle_t a_event);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Signal an event semaphore.
 *
 *
 * \param   oshandle_t a_event Handle to the semaphore.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_event_signal(oshandle_t a_event);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Reset an event semaphore.
 *
 *
 * \param   oshandle_t a_event Handle to the semaphore.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_event_reset(oshandle_t a_event);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Wait for an event semaphore to be freed and acquire it.
 *
 *
 * \param   oshandle_t a_event Handle to the semaphore.
 * \param   int a_milliSeconds Time to wait.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_event_wait(oshandle_t a_event, int a_milliSeconds);


//////////////////////////////////////////////////////////////////////////////
//  interrupt handler API
//////////////////////////////////////////////////////////////////////////////
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Enable an interrupt with specified id.
 *
 *
 * \param   int interrupt   Identification number for the interrupt.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interrupt_enable(int interrupt);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Disable an interrupt with specified id.
 *
 *
 * \param   int interrupt   Identification number for the interrupt.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interrupt_disable(int interrupt);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Set the callback function for an interrupt.
 *
 *
 * \param   int interrupt   Identification number for the interrupt.
 * \param   void* handler   Pointer to the callback function.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interrupt_setcallback(int interrupt, void* handler);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Remove a callback function from an interrupt.
 *
 *
 * \param   int interrupt   Identification number for the interrupt.
 * \param   void* handler   Pointer to the callback function.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_interrupt_clearcallback(int interrupt, void* handler);


//////////////////////////////////////////////////////////////////////////////
//  thread and process API
//////////////////////////////////////////////////////////////////////////////
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Allocate an entry from the thread local storage table.
 *
 *
 * \return  Index of the reserved entry.
 *//*-------------------------------------------------------------------*/
KOS_API unsigned int    kos_tls_alloc(void);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Free an entry from the thread local storage table.
 *
 *
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_tls_free(unsigned int tlsindex);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Read the value of an entry in the thread local storage table.
 *
 *
 * \param   unsigned int tlsindex   Index of the entry.
 * \return  Returns the value of the entry.
 *//*-------------------------------------------------------------------*/
KOS_API void*           kos_tls_read(unsigned int tlsindex);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Write a value to an entry in the thread local storage table.
 *
 *
 * \param   unsigned int tlsindex   Index of the entry.
 * \param   void* tlsvalue          Value to be written.
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_tls_write(unsigned int tlsindex, void* tlsvalue);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Get the id of the current process.
 *
 *
 * \return  Returns the process id.
 *//*-------------------------------------------------------------------*/
KOS_API unsigned int    kos_process_getid(void);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Get the id of the current caller process.
 *
 *
 * \return  Returns the caller process id.
 *//*-------------------------------------------------------------------*/
KOS_API unsigned int    kos_callerprocess_getid(void);

//////////////////////////////////////////////////////////////////////////////
//  timing API
//////////////////////////////////////////////////////////////////////////////
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Get the current time as a timestamp.
 *
 *
 * \return  Returns the timestamp.
 *//*-------------------------------------------------------------------*/
KOS_API unsigned int    kos_timestamp(void);


//////////////////////////////////////////////////////////////////////////////
//  libary API
//////////////////////////////////////////////////////////////////////////////
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Map the given library (not required an all OS'es).
 *
 *
 * \param   char* libraryname   The name string of the lib.
 * \return  Returns a handle for the lib.
 *//*-------------------------------------------------------------------*/
KOS_API oshandle_t      kos_lib_map(char* libraryname);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Unmap the given library.
 *
 * \param   oshandle_t libhandle Handle to the lib.
 * \return  Returns an error code incase of an error.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_lib_unmap(oshandle_t libhandle);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Get the address of a lib.
 *
 * \param   oshandle_t libhandle Handle to the lib.
 * \return  Returns a pointer to the lib.
 *//*-------------------------------------------------------------------*/
KOS_API void*           kos_lib_getaddr(oshandle_t libhandle, char* procname);


//////////////////////////////////////////////////////////////////////////////
//  query API
//////////////////////////////////////////////////////////////////////////////
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Sync block start
 *
 * \param   void
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_syncblock_start(void);
/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Sync block end
 *
 * \param   void
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_syncblock_end(void);

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Sync block start with argument
 *
 * \param   void
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int kos_syncblock_start_ex( mutexIndex_t a_index );

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Sync block start with argument
 *
 * \param   void
 * \return  Returns NULL if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int kos_syncblock_end_ex( mutexIndex_t a_index );

//////////////////////////////////////////////////////////////////////////////
//  file API
//////////////////////////////////////////////////////////////////////////////

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Opens a file
 *
 * \param   const char* filename    Name of the file to open.
 * \param   const char* mode        Mode used for file opening. See fopen.
 * \return  Returns file handle or NULL if error.
 *//*-------------------------------------------------------------------*/
KOS_API oshandle_t      kos_fopen(const char* filename, const char* mode);

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Writes to a file
 *
 * \param   oshandle_t file     Handle of the file to write to.
 * \param   const char* format  Format string. See fprintf.
 * \return  Returns the number of bytes written
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_fprintf(oshandle_t file, const char* format, ...);

/*-------------------------------------------------------------------*//*!
 * \external
 * \brief   Closes a file
 *
 * \param   oshandle_t file     Handle of the file to close.
 * \return  Returns zero if no error, otherwise an error code.
 *//*-------------------------------------------------------------------*/
KOS_API int             kos_fclose(oshandle_t file);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif  // __KOSAPI_H
