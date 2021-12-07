/*
 * Unit tests for NT synchronization objects
 *
 * Copyright 2020 Zebediah Figura
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/test.h"

static NTSTATUS (WINAPI *pNtAlertThreadByThreadId)( HANDLE );
static NTSTATUS (WINAPI *pNtClose)( HANDLE );
static NTSTATUS (WINAPI *pNtCreateEvent) ( PHANDLE, ACCESS_MASK, const OBJECT_ATTRIBUTES *, EVENT_TYPE, BOOLEAN);
static NTSTATUS (WINAPI *pNtCreateKeyedEvent)( HANDLE *, ACCESS_MASK, const OBJECT_ATTRIBUTES *, ULONG );
static NTSTATUS (WINAPI *pNtCreateMutant)( HANDLE *, ACCESS_MASK, const OBJECT_ATTRIBUTES *, BOOLEAN );
static NTSTATUS (WINAPI *pNtCreateSemaphore)( HANDLE *, ACCESS_MASK, const OBJECT_ATTRIBUTES *, LONG, LONG );
static NTSTATUS (WINAPI *pNtOpenEvent)( HANDLE *, ACCESS_MASK, const OBJECT_ATTRIBUTES * );
static NTSTATUS (WINAPI *pNtOpenKeyedEvent)( HANDLE *, ACCESS_MASK, const OBJECT_ATTRIBUTES * );
static NTSTATUS (WINAPI *pNtPulseEvent)( HANDLE, LONG * );
static NTSTATUS (WINAPI *pNtQueryEvent)( HANDLE, EVENT_INFORMATION_CLASS, void *, ULONG, ULONG * );
static NTSTATUS (WINAPI *pNtQueryMutant)( HANDLE, MUTANT_INFORMATION_CLASS, void *, ULONG, ULONG * );
static NTSTATUS (WINAPI *pNtQuerySemaphore)( HANDLE, SEMAPHORE_INFORMATION_CLASS, void *, ULONG, ULONG * );
static NTSTATUS (WINAPI *pNtQuerySystemTime)( LARGE_INTEGER * );
static NTSTATUS (WINAPI *pNtReleaseKeyedEvent)( HANDLE, const void *, BOOLEAN, const LARGE_INTEGER * );
static NTSTATUS (WINAPI *pNtReleaseMutant)( HANDLE, LONG * );
static NTSTATUS (WINAPI *pNtReleaseSemaphore)( HANDLE, ULONG, ULONG * );
static NTSTATUS (WINAPI *pNtResetEvent)( HANDLE, LONG * );
static NTSTATUS (WINAPI *pNtSetEvent)( HANDLE, LONG * );
static NTSTATUS (WINAPI *pNtWaitForAlertByThreadId)( void *, const LARGE_INTEGER * );
static NTSTATUS (WINAPI *pNtWaitForKeyedEvent)( HANDLE, const void *, BOOLEAN, const LARGE_INTEGER * );
static BOOLEAN  (WINAPI *pRtlAcquireResourceExclusive)( RTL_RWLOCK *, BOOLEAN );
static BOOLEAN  (WINAPI *pRtlAcquireResourceShared)( RTL_RWLOCK *, BOOLEAN );
static void     (WINAPI *pRtlDeleteResource)( RTL_RWLOCK * );
static void     (WINAPI *pRtlInitializeResource)( RTL_RWLOCK * );
static void     (WINAPI *pRtlInitUnicodeString)( UNICODE_STRING *, const WCHAR * );
static void     (WINAPI *pRtlReleaseResource)( RTL_RWLOCK * );
static NTSTATUS (WINAPI *pRtlWaitOnAddress)( const void *, const void *, SIZE_T, const LARGE_INTEGER * );
static void     (WINAPI *pRtlWakeAddressAll)( const void * );
static void     (WINAPI *pRtlWakeAddressSingle)( const void * );

#define KEYEDEVENT_WAIT       0x0001
#define KEYEDEVENT_WAKE       0x0002
#define KEYEDEVENT_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0x0003)

static void test_event(void)
{
    HANDLE event;
    HANDLE event2;
    LONG prev_state = 0xdeadbeef;
    NTSTATUS status;
    UNICODE_STRING str;
    OBJECT_ATTRIBUTES attr;
    EVENT_BASIC_INFORMATION info;

    pRtlInitUnicodeString( &str, L"\\BaseNamedObjects\\testEvent" );
    InitializeObjectAttributes(&attr, &str, 0, 0, NULL);

    status = pNtCreateEvent(&event, GENERIC_ALL, &attr, 2, 0);
    ok( status == STATUS_INVALID_PARAMETER, "NtCreateEvent failed %08x\n", status );

    status = pNtCreateEvent(&event, GENERIC_ALL, &attr, NotificationEvent, 0);
    ok( status == STATUS_SUCCESS, "NtCreateEvent failed %08x\n", status );
    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryEvent(event, EventBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryEvent failed %08x\n", status );
    ok( info.EventType == NotificationEvent && info.EventState == 0,
        "NtQueryEvent failed, expected 0 0, got %d %d\n", info.EventType, info.EventState );
    pNtClose(event);

    status = pNtCreateEvent(&event, GENERIC_ALL, &attr, SynchronizationEvent, 0);
    ok( status == STATUS_SUCCESS, "NtCreateEvent failed %08x\n", status );

    status = pNtPulseEvent(event, &prev_state);
    ok( status == STATUS_SUCCESS, "NtPulseEvent failed %08x\n", status );
    ok( !prev_state, "prev_state = %x\n", prev_state );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryEvent(event, EventBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryEvent failed %08x\n", status );
    ok( info.EventType == SynchronizationEvent && info.EventState == 0,
        "NtQueryEvent failed, expected 1 0, got %d %d\n", info.EventType, info.EventState );

    status = pNtOpenEvent(&event2, GENERIC_ALL, &attr);
    ok( status == STATUS_SUCCESS, "NtOpenEvent failed %08x\n", status );

    pNtClose(event);
    event = event2;

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryEvent(event, EventBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryEvent failed %08x\n", status );
    ok( info.EventType == SynchronizationEvent && info.EventState == 0,
        "NtQueryEvent failed, expected 1 0, got %d %d\n", info.EventType, info.EventState );

    status = pNtSetEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtSetEvent failed: %08x\n", status );
    ok( !prev_state, "prev_state = %x\n", prev_state );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryEvent(event, EventBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryEvent failed %08x\n", status );
    ok( info.EventType == SynchronizationEvent && info.EventState == 1,
        "NtQueryEvent failed, expected 1 1, got %d %d\n", info.EventType, info.EventState );

    status = pNtSetEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtSetEvent failed: %08x\n", status );
    ok( prev_state == 1, "prev_state = %x\n", prev_state );

    status = pNtResetEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtSetEvent failed: %08x\n", status );
    ok( prev_state == 1, "prev_state = %x\n", prev_state );

    status = pNtResetEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtSetEvent failed: %08x\n", status );
    ok( !prev_state, "prev_state = %x\n", prev_state );

    status = pNtPulseEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtPulseEvent failed %08x\n", status );
    ok( !prev_state, "prev_state = %x\n", prev_state );

    status = pNtSetEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtSetEvent failed: %08x\n", status );
    ok( !prev_state, "prev_state = %x\n", prev_state );

    status = pNtPulseEvent( event, &prev_state );
    ok( status == STATUS_SUCCESS, "NtPulseEvent failed %08x\n", status );
    ok( prev_state == 1, "prev_state = %x\n", prev_state );

    pNtClose(event);
}

static const WCHAR keyed_nameW[] = L"\\BaseNamedObjects\\WineTestEvent";

static DWORD WINAPI keyed_event_thread( void *arg )
{
    HANDLE handle;
    NTSTATUS status;
    LARGE_INTEGER timeout;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING str;
    ULONG_PTR i;

    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &str;
    attr.Attributes               = 0;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &str, keyed_nameW );

    status = pNtOpenKeyedEvent( &handle, KEYEDEVENT_ALL_ACCESS, &attr );
    ok( !status, "NtOpenKeyedEvent failed %x\n", status );

    for (i = 0; i < 20; i++)
    {
        if (i & 1)
            status = pNtWaitForKeyedEvent( handle, (void *)(i * 2), 0, NULL );
        else
            status = pNtReleaseKeyedEvent( handle, (void *)(i * 2), 0, NULL );
        ok( status == STATUS_SUCCESS, "%li: failed %x\n", i, status );
        Sleep( 20 - i );
    }

    status = pNtReleaseKeyedEvent( handle, (void *)0x1234, 0, NULL );
    ok( status == STATUS_SUCCESS, "NtReleaseKeyedEvent %x\n", status );

    timeout.QuadPart = -10000;
    status = pNtWaitForKeyedEvent( handle, (void *)0x5678, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)0x9abc, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );

    NtClose( handle );
    return 0;
}

static void test_keyed_events(void)
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING str;
    HANDLE handle, event, thread;
    NTSTATUS status;
    LARGE_INTEGER timeout;
    ULONG_PTR i;

    if (!pNtCreateKeyedEvent)
    {
        win_skip( "Keyed events not supported\n" );
        return;
    }

    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &str;
    attr.Attributes               = 0;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &str, keyed_nameW );

    status = pNtCreateKeyedEvent( &handle, KEYEDEVENT_ALL_ACCESS | SYNCHRONIZE, &attr, 0 );
    ok( !status, "NtCreateKeyedEvent failed %x\n", status );

    status = WaitForSingleObject( handle, 1000 );
    ok( status == 0, "WaitForSingleObject %x\n", status );

    timeout.QuadPart = -100000;
    status = pNtWaitForKeyedEvent( handle, (void *)255, 0, &timeout );
    ok( status == STATUS_INVALID_PARAMETER_1, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)255, 0, &timeout );
    ok( status == STATUS_INVALID_PARAMETER_1, "NtReleaseKeyedEvent %x\n", status );

    status = pNtWaitForKeyedEvent( handle, (void *)254, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)254, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );

    status = pNtWaitForKeyedEvent( handle, NULL, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, NULL, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );

    status = pNtWaitForKeyedEvent( NULL, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT || broken(status == STATUS_INVALID_HANDLE), /* XP/2003 */
        "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( NULL, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT || broken(status == STATUS_INVALID_HANDLE), /* XP/2003 */
        "NtReleaseKeyedEvent %x\n", status );

    status = pNtWaitForKeyedEvent( (HANDLE)0xdeadbeef, (void *)9, 0, &timeout );
    ok( status == STATUS_INVALID_PARAMETER_1, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( (HANDLE)0xdeadbeef, (void *)9, 0, &timeout );
    ok( status == STATUS_INVALID_PARAMETER_1, "NtReleaseKeyedEvent %x\n", status );

    status = pNtWaitForKeyedEvent( (HANDLE)0xdeadbeef, (void *)8, 0, &timeout );
    ok( status == STATUS_INVALID_HANDLE, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( (HANDLE)0xdeadbeef, (void *)8, 0, &timeout );
    ok( status == STATUS_INVALID_HANDLE, "NtReleaseKeyedEvent %x\n", status );

    thread = CreateThread( NULL, 0, keyed_event_thread, 0, 0, NULL );
    for (i = 0; i < 20; i++)
    {
        if (i & 1)
            status = pNtReleaseKeyedEvent( handle, (void *)(i * 2), 0, NULL );
        else
            status = pNtWaitForKeyedEvent( handle, (void *)(i * 2), 0, NULL );
        ok( status == STATUS_SUCCESS, "%li: failed %x\n", i, status );
        Sleep( i );
    }
    status = pNtWaitForKeyedEvent( handle, (void *)0x1234, 0, &timeout );
    ok( status == STATUS_SUCCESS, "NtWaitForKeyedEvent %x\n", status );
    status = pNtWaitForKeyedEvent( handle, (void *)0x5678, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)0x9abc, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );

    ok( WaitForSingleObject( thread, 30000 ) == 0, "wait failed\n" );

    NtClose( handle );

    /* test access rights */

    status = pNtCreateKeyedEvent( &handle, KEYEDEVENT_WAIT, &attr, 0 );
    ok( !status, "NtCreateKeyedEvent failed %x\n", status );
    status = pNtWaitForKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_ACCESS_DENIED, "NtReleaseKeyedEvent %x\n", status );
    NtClose( handle );

    status = pNtCreateKeyedEvent( &handle, KEYEDEVENT_WAKE, &attr, 0 );
    ok( !status, "NtCreateKeyedEvent failed %x\n", status );
    status = pNtWaitForKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_ACCESS_DENIED, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );
    NtClose( handle );

    status = pNtCreateKeyedEvent( &handle, KEYEDEVENT_ALL_ACCESS, &attr, 0 );
    ok( !status, "NtCreateKeyedEvent failed %x\n", status );
    status = WaitForSingleObject( handle, 1000 );
    ok( status == WAIT_FAILED && GetLastError() == ERROR_ACCESS_DENIED,
        "WaitForSingleObject %x err %u\n", status, GetLastError() );
    status = pNtWaitForKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );
    NtClose( handle );

    /* GENERIC_READ gives wait access */
    status = pNtCreateKeyedEvent( &handle, GENERIC_READ, &attr, 0 );
    ok( !status, "NtCreateKeyedEvent failed %x\n", status );
    status = pNtWaitForKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_ACCESS_DENIED, "NtReleaseKeyedEvent %x\n", status );
    NtClose( handle );

    /* GENERIC_WRITE gives wake access */
    status = pNtCreateKeyedEvent( &handle, GENERIC_WRITE, &attr, 0 );
    ok( !status, "NtCreateKeyedEvent failed %x\n", status );
    status = pNtWaitForKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_ACCESS_DENIED, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( handle, (void *)8, 0, &timeout );
    ok( status == STATUS_TIMEOUT, "NtReleaseKeyedEvent %x\n", status );

    /* it's not an event */
    status = pNtPulseEvent( handle, NULL );
    ok( status == STATUS_OBJECT_TYPE_MISMATCH, "NtPulseEvent %x\n", status );

    status = pNtCreateEvent( &event, GENERIC_ALL, &attr, NotificationEvent, FALSE );
    ok( status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_OBJECT_TYPE_MISMATCH /* 7+ */,
        "CreateEvent %x\n", status );

    NtClose( handle );

    status = pNtCreateEvent( &event, GENERIC_ALL, &attr, NotificationEvent, FALSE );
    ok( status == 0, "CreateEvent %x\n", status );
    status = pNtWaitForKeyedEvent( event, (void *)8, 0, &timeout );
    ok( status == STATUS_OBJECT_TYPE_MISMATCH, "NtWaitForKeyedEvent %x\n", status );
    status = pNtReleaseKeyedEvent( event, (void *)8, 0, &timeout );
    ok( status == STATUS_OBJECT_TYPE_MISMATCH, "NtReleaseKeyedEvent %x\n", status );
    NtClose( event );
}

static DWORD WINAPI mutant_thread( void *arg )
{
    MUTANT_BASIC_INFORMATION info;
    NTSTATUS status;
    HANDLE mutant;
    DWORD ret;

    mutant = arg;
    ret = WaitForSingleObject( mutant, 1000 );
    ok( ret == WAIT_OBJECT_0, "WaitForSingleObject failed %08x\n", ret );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryMutant failed %08x\n", status );
    ok( info.CurrentCount == 0, "expected 0, got %d\n", info.CurrentCount );
    ok( info.OwnedByCaller == TRUE, "expected TRUE, got %d\n", info.OwnedByCaller );
    ok( info.AbandonedState == FALSE, "expected FALSE, got %d\n", info.AbandonedState );
    /* abandon mutant */

    return 0;
}

static void test_mutant(void)
{
    MUTANT_BASIC_INFORMATION info;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING str;
    NTSTATUS status;
    HANDLE mutant;
    HANDLE thread;
    DWORD ret;
    ULONG len;
    LONG prev;

    pRtlInitUnicodeString(&str, L"\\BaseNamedObjects\\test_mutant");
    InitializeObjectAttributes(&attr, &str, 0, 0, NULL);
    status = pNtCreateMutant(&mutant, GENERIC_ALL, &attr, TRUE);
    ok( status == STATUS_SUCCESS, "Failed to create Mutant(%08x)\n", status );

    /* bogus */
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, 0, NULL);
    ok( status == STATUS_INFO_LENGTH_MISMATCH,
        "Failed to NtQueryMutant, expected STATUS_INFO_LENGTH_MISMATCH, got %08x\n", status );
    status = pNtQueryMutant(mutant, 0x42, &info, sizeof(info), NULL);
    ok( status == STATUS_INVALID_INFO_CLASS || broken(status == STATUS_NOT_IMPLEMENTED), /* 32-bit on Vista/2k8 */
        "Failed to NtQueryMutant, expected STATUS_INVALID_INFO_CLASS, got %08x\n", status );
    status = pNtQueryMutant((HANDLE)0xdeadbeef, MutantBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_INVALID_HANDLE,
        "Failed to NtQueryMutant, expected STATUS_INVALID_HANDLE, got %08x\n", status );

    /* new */
    len = -1;
    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, sizeof(info), &len);
    ok( status == STATUS_SUCCESS, "NtQueryMutant failed %08x\n", status );
    ok( info.CurrentCount == 0, "expected 0, got %d\n", info.CurrentCount );
    ok( info.OwnedByCaller == TRUE, "expected TRUE, got %d\n", info.OwnedByCaller );
    ok( info.AbandonedState == FALSE, "expected FALSE, got %d\n", info.AbandonedState );
    ok( len == sizeof(info), "got %u\n", len );

    ret = WaitForSingleObject( mutant, 1000 );
    ok( ret == WAIT_OBJECT_0, "WaitForSingleObject failed %08x\n", ret );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryMutant failed %08x\n", status );
    ok( info.CurrentCount == -1, "expected -1, got %d\n", info.CurrentCount );
    ok( info.OwnedByCaller == TRUE, "expected TRUE, got %d\n", info.OwnedByCaller );
    ok( info.AbandonedState == FALSE, "expected FALSE, got %d\n", info.AbandonedState );

    prev = 0xdeadbeef;
    status = pNtReleaseMutant(mutant, &prev);
    ok( status == STATUS_SUCCESS, "NtReleaseMutant failed %08x\n", status );
    ok( prev == -1, "NtReleaseMutant failed, expected -1, got %d\n", prev );

    prev = 0xdeadbeef;
    status = pNtReleaseMutant(mutant, &prev);
    ok( status == STATUS_SUCCESS, "NtReleaseMutant failed %08x\n", status );
    ok( prev == 0, "NtReleaseMutant failed, expected 0, got %d\n", prev );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryMutant failed %08x\n", status );
    ok( info.CurrentCount == 1, "expected 1, got %d\n", info.CurrentCount );
    ok( info.OwnedByCaller == FALSE, "expected FALSE, got %d\n", info.OwnedByCaller );
    ok( info.AbandonedState == FALSE, "expected FALSE, got %d\n", info.AbandonedState );

    /* abandoned */
    thread = CreateThread( NULL, 0, mutant_thread, mutant, 0, NULL );
    ret = WaitForSingleObject( thread, 1000 );
    ok( ret == WAIT_OBJECT_0, "WaitForSingleObject failed %08x\n", ret );
    CloseHandle( thread );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryMutant failed %08x\n", status );
    ok( info.CurrentCount == 1, "expected 0, got %d\n", info.CurrentCount );
    ok( info.OwnedByCaller == FALSE, "expected FALSE, got %d\n", info.OwnedByCaller );
    ok( info.AbandonedState == TRUE, "expected TRUE, got %d\n", info.AbandonedState );

    ret = WaitForSingleObject( mutant, 1000 );
    ok( ret == WAIT_ABANDONED_0, "WaitForSingleObject failed %08x\n", ret );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQueryMutant(mutant, MutantBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQueryMutant failed %08x\n", status );
    ok( info.CurrentCount == 0, "expected 0, got %d\n", info.CurrentCount );
    ok( info.OwnedByCaller == TRUE, "expected TRUE, got %d\n", info.OwnedByCaller );
    ok( info.AbandonedState == FALSE, "expected FALSE, got %d\n", info.AbandonedState );

    NtClose( mutant );
}

static void test_semaphore(void)
{
    SEMAPHORE_BASIC_INFORMATION info;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING str;
    NTSTATUS status;
    HANDLE semaphore;
    ULONG prev;
    ULONG len;
    DWORD ret;

    pRtlInitUnicodeString(&str, L"\\BaseNamedObjects\\test_semaphore");
    InitializeObjectAttributes(&attr, &str, 0, 0, NULL);

    status = pNtCreateSemaphore(&semaphore, GENERIC_ALL, &attr, 2, 1);
    ok( status == STATUS_INVALID_PARAMETER, "Failed to create Semaphore(%08x)\n", status );
    status = pNtCreateSemaphore(&semaphore, GENERIC_ALL, &attr, 1, 2);
    ok( status == STATUS_SUCCESS, "Failed to create Semaphore(%08x)\n", status );

    /* bogus */
    status = pNtQuerySemaphore(semaphore, SemaphoreBasicInformation, &info, 0, NULL);
    ok( status == STATUS_INFO_LENGTH_MISMATCH,
        "Failed to NtQuerySemaphore, expected STATUS_INFO_LENGTH_MISMATCH, got %08x\n", status );
    status = pNtQuerySemaphore(semaphore, 0x42, &info, sizeof(info), NULL);
    ok( status == STATUS_INVALID_INFO_CLASS,
        "Failed to NtQuerySemaphore, expected STATUS_INVALID_INFO_CLASS, got %08x\n", status );
    status = pNtQuerySemaphore((HANDLE)0xdeadbeef, SemaphoreBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_INVALID_HANDLE,
        "Failed to NtQuerySemaphore, expected STATUS_INVALID_HANDLE, got %08x\n", status );

    len = -1;
    memset(&info, 0xcc, sizeof(info));
    status = pNtQuerySemaphore(semaphore, SemaphoreBasicInformation, &info, sizeof(info), &len);
    ok( status == STATUS_SUCCESS, "NtQuerySemaphore failed %08x\n", status );
    ok( info.CurrentCount == 1, "expected 1, got %d\n", info.CurrentCount );
    ok( info.MaximumCount == 2, "expected 2, got %d\n", info.MaximumCount );
    ok( len == sizeof(info), "got %u\n", len );

    ret = WaitForSingleObject( semaphore, 1000 );
    ok( ret == WAIT_OBJECT_0, "WaitForSingleObject failed %08x\n", ret );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQuerySemaphore(semaphore, SemaphoreBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQuerySemaphore failed %08x\n", status );
    ok( info.CurrentCount == 0, "expected 0, got %d\n", info.CurrentCount );
    ok( info.MaximumCount == 2, "expected 2, got %d\n", info.MaximumCount );

    prev = 0xdeadbeef;
    status = pNtReleaseSemaphore(semaphore, 3, &prev);
    ok( status == STATUS_SEMAPHORE_LIMIT_EXCEEDED, "NtReleaseSemaphore failed %08x\n", status );
    ok( prev == 0xdeadbeef, "NtReleaseSemaphore failed, expected 0xdeadbeef, got %d\n", prev );

    prev = 0xdeadbeef;
    status = pNtReleaseSemaphore(semaphore, 1, &prev);
    ok( status == STATUS_SUCCESS, "NtReleaseSemaphore failed %08x\n", status );
    ok( prev == 0, "NtReleaseSemaphore failed, expected 0, got %d\n", prev );

    prev = 0xdeadbeef;
    status = pNtReleaseSemaphore(semaphore, 1, &prev);
    ok( status == STATUS_SUCCESS, "NtReleaseSemaphore failed %08x\n", status );
    ok( prev == 1, "NtReleaseSemaphore failed, expected 1, got %d\n", prev );

    prev = 0xdeadbeef;
    status = pNtReleaseSemaphore(semaphore, 1, &prev);
    ok( status == STATUS_SEMAPHORE_LIMIT_EXCEEDED, "NtReleaseSemaphore failed %08x\n", status );
    ok( prev == 0xdeadbeef, "NtReleaseSemaphore failed, expected 0xdeadbeef, got %d\n", prev );

    memset(&info, 0xcc, sizeof(info));
    status = pNtQuerySemaphore(semaphore, SemaphoreBasicInformation, &info, sizeof(info), NULL);
    ok( status == STATUS_SUCCESS, "NtQuerySemaphore failed %08x\n", status );
    ok( info.CurrentCount == 2, "expected 2, got %d\n", info.CurrentCount );
    ok( info.MaximumCount == 2, "expected 2, got %d\n", info.MaximumCount );

    NtClose( semaphore );
}

static void test_wait_on_address(void)
{
    SIZE_T size;
    NTSTATUS status;
    LARGE_INTEGER start, end, timeout;
    DWORD elapsed;
    LONG64 address, compare;

    if (!pRtlWaitOnAddress)
    {
        win_skip("RtlWaitOnAddress not supported, skipping test\n");
        return;
    }

    if (0) /* crash on Windows */
    {
        pRtlWaitOnAddress(&address, NULL, 8, NULL);
        pRtlWaitOnAddress(NULL, &compare, 8, NULL);
        pRtlWaitOnAddress(NULL, NULL, 8, NULL);
    }

    /* don't crash */
    pRtlWakeAddressSingle(NULL);
    pRtlWakeAddressAll(NULL);

    /* invalid values */
    address = 0;
    compare = 0;
    status = pRtlWaitOnAddress(&address, &compare, 5, NULL);
    ok(status == STATUS_INVALID_PARAMETER, "got %x\n", status);

    /* values match */
    address = 0;
    compare = 0;
    pNtQuerySystemTime(&start);
    timeout.QuadPart = start.QuadPart + 100 * 10000;
    status = pRtlWaitOnAddress(&address, &compare, 8, &timeout);
    pNtQuerySystemTime(&end);
    ok(status == STATUS_TIMEOUT, "got 0x%08x\n", status);
    elapsed = (end.QuadPart - start.QuadPart) / 10000;
    ok(90 <= elapsed && elapsed <= 900, "timed out in %u ms\n", elapsed);
    ok(address == 0, "got %s\n", wine_dbgstr_longlong(address));
    ok(compare == 0, "got %s\n", wine_dbgstr_longlong(compare));

    /* different address size */
    for (size = 1; size <= 4; size <<= 1)
    {
        compare = ~0;
        compare <<= size * 8;

        pNtQuerySystemTime(&start);
        timeout.QuadPart = -100 * 10000;
        status = pRtlWaitOnAddress(&address, &compare, size, &timeout);
        pNtQuerySystemTime(&end);
        ok(status == STATUS_TIMEOUT, "got 0x%08x\n", status);
        elapsed = (end.QuadPart - start.QuadPart) / 10000;
        ok(90 <= elapsed && elapsed <= 900, "timed out in %u ms\n", elapsed);

        status = pRtlWaitOnAddress(&address, &compare, size << 1, &timeout);
        ok(!status, "got 0x%08x\n", status);
    }
    address = 0;
    compare = 1;
    status = pRtlWaitOnAddress(&address, &compare, 8, NULL);
    ok(!status, "got 0x%08x\n", status);

    /* no waiters */
    address = 0;
    pRtlWakeAddressSingle(&address);
    ok(address == 0, "got %s\n", wine_dbgstr_longlong(address));
    pRtlWakeAddressAll(&address);
    ok(address == 0, "got %s\n", wine_dbgstr_longlong(address));
}

static HANDLE thread_ready, thread_done;

static DWORD WINAPI resource_shared_thread(void *arg)
{
    RTL_RWLOCK *resource = arg;
    BOOLEAN ret;

    ret = pRtlAcquireResourceShared(resource, TRUE);
    ok(ret == TRUE, "got %u\n", ret);

    SetEvent(thread_ready);
    ok(!WaitForSingleObject(thread_done, 1000), "wait failed\n");
    pRtlReleaseResource(resource);
    return 0;
}

static DWORD WINAPI resource_exclusive_thread(void *arg)
{
    RTL_RWLOCK *resource = arg;
    BOOLEAN ret;

    ret = pRtlAcquireResourceExclusive(resource, TRUE);
    ok(ret == TRUE, "got %u\n", ret);

    SetEvent(thread_ready);
    ok(!WaitForSingleObject(thread_done, 1000), "wait failed\n");
    pRtlReleaseResource(resource);
    return 0;
}

static void test_resource(void)
{
    HANDLE thread, thread2;
    RTL_RWLOCK resource;
    BOOLEAN ret;

    pRtlInitializeResource(&resource);
    thread_ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread_done = CreateEventA(NULL, FALSE, FALSE, NULL);

    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == FALSE, "got %u\n", ret);
    pRtlReleaseResource(&resource);
    pRtlReleaseResource(&resource);

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);
    pRtlReleaseResource(&resource);
    pRtlReleaseResource(&resource);

    /* Do not acquire the resource ourselves, but spawn a shared thread holding it. */

    thread = CreateThread(NULL, 0, resource_shared_thread, &resource, 0, NULL);
    ok(!WaitForSingleObject(thread_ready, 1000), "wait failed\n");

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == FALSE, "got %u\n", ret);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);

    SetEvent(thread_done);
    ok(!WaitForSingleObject(thread, 1000), "wait failed\n");
    CloseHandle(thread);

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);

    /* Acquire the resource as exclusive, and then spawn a shared thread. */

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    thread = CreateThread(NULL, 0, resource_shared_thread, &resource, 0, NULL);
    ok(WaitForSingleObject(thread_ready, 100) == WAIT_TIMEOUT, "expected timeout\n");

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);

    pRtlReleaseResource(&resource);
    ok(!WaitForSingleObject(thread_ready, 1000), "wait failed\n");
    SetEvent(thread_done);
    ok(!WaitForSingleObject(thread, 1000), "wait failed\n");
    CloseHandle(thread);

    /* Acquire the resource as shared, and then spawn an exclusive thread. */

    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    thread = CreateThread(NULL, 0, resource_exclusive_thread, &resource, 0, NULL);
    ok(WaitForSingleObject(thread_ready, 100) == WAIT_TIMEOUT, "expected timeout\n");

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == FALSE, "got %u\n", ret);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);

    pRtlReleaseResource(&resource);
    ok(!WaitForSingleObject(thread_ready, 1000), "wait failed\n");
    SetEvent(thread_done);
    ok(!WaitForSingleObject(thread, 1000), "wait failed\n");
    CloseHandle(thread);

    /* Spawn a shared and then exclusive waiter. */
    thread = CreateThread(NULL, 0, resource_shared_thread, &resource, 0, NULL);
    ok(!WaitForSingleObject(thread_ready, 1000), "wait failed\n");
    thread2 = CreateThread(NULL, 0, resource_exclusive_thread, &resource, 0, NULL);
    ok(WaitForSingleObject(thread_ready, 100) == WAIT_TIMEOUT, "expected timeout\n");

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == FALSE, "got %u\n", ret);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);

    SetEvent(thread_done);
    ok(!WaitForSingleObject(thread, 1000), "wait failed\n");
    CloseHandle(thread);

    ok(!WaitForSingleObject(thread_ready, 1000), "wait failed\n");
    SetEvent(thread_done);
    ok(!WaitForSingleObject(thread2, 1000), "wait failed\n");
    CloseHandle(thread2);

    ret = pRtlAcquireResourceExclusive(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);
    ret = pRtlAcquireResourceShared(&resource, FALSE);
    ok(ret == TRUE, "got %u\n", ret);
    pRtlReleaseResource(&resource);

    CloseHandle(thread_ready);
    CloseHandle(thread_done);
    pRtlDeleteResource(&resource);
}

static DWORD WINAPI tid_alert_thread( void *arg )
{
    NTSTATUS ret;

    ret = pNtAlertThreadByThreadId( arg );
    ok(!ret, "got %#x\n", ret);

    ret = pNtWaitForAlertByThreadId( (void *)0x123, NULL );
    ok(ret == STATUS_ALERTED, "got %#x\n", ret);

    return 0;
}

static void test_tid_alert( char **argv )
{
    LARGE_INTEGER timeout = {{0}};
    char cmdline[MAX_PATH];
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi;
    HANDLE thread;
    NTSTATUS ret;
    DWORD tid;

    if (!pNtWaitForAlertByThreadId)
    {
        win_skip("NtWaitForAlertByThreadId is not available\n");
        return;
    }

    ret = pNtWaitForAlertByThreadId( (void *)0x123, &timeout );
    ok(ret == STATUS_TIMEOUT, "got %#x\n", ret);

    ret = pNtAlertThreadByThreadId( 0 );
    ok(ret == STATUS_INVALID_CID, "got %#x\n", ret);

    ret = pNtAlertThreadByThreadId( (HANDLE)0xdeadbeef );
    ok(ret == STATUS_INVALID_CID, "got %#x\n", ret);

    ret = pNtAlertThreadByThreadId( (HANDLE)(DWORD_PTR)GetCurrentThreadId() );
    ok(!ret, "got %#x\n", ret);

    ret = pNtAlertThreadByThreadId( (HANDLE)(DWORD_PTR)GetCurrentThreadId() );
    ok(!ret, "got %#x\n", ret);

    ret = pNtWaitForAlertByThreadId( (void *)0x123, &timeout );
    ok(ret == STATUS_ALERTED, "got %#x\n", ret);

    ret = pNtWaitForAlertByThreadId( (void *)0x123, &timeout );
    ok(ret == STATUS_TIMEOUT, "got %#x\n", ret);

    ret = pNtWaitForAlertByThreadId( (void *)0x321, &timeout );
    ok(ret == STATUS_TIMEOUT, "got %#x\n", ret);

    thread = CreateThread( NULL, 0, tid_alert_thread, (HANDLE)(DWORD_PTR)GetCurrentThreadId(), 0, &tid );
    timeout.QuadPart = -1000 * 10000;
    ret = pNtWaitForAlertByThreadId( (void *)0x123, &timeout );
    ok(ret == STATUS_ALERTED, "got %#x\n", ret);

    ret = WaitForSingleObject( thread, 100 );
    ok(ret == WAIT_TIMEOUT, "got %d\n", ret);
    ret = pNtAlertThreadByThreadId( (HANDLE)(DWORD_PTR)tid );
    ok(!ret, "got %#x\n", ret);

    ret = WaitForSingleObject( thread, 1000 );
    ok(!ret, "got %d\n", ret);

    ret = pNtAlertThreadByThreadId( (HANDLE)(DWORD_PTR)tid );
    ok(!ret, "got %#x\n", ret);

    CloseHandle(thread);

    sprintf( cmdline, "%s %s subprocess", argv[0], argv[1] );
    ret = CreateProcessA( NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
    ok(ret, "failed to create process, error %u\n", GetLastError());
    ret = pNtAlertThreadByThreadId( (HANDLE)(DWORD_PTR)pi.dwThreadId );
    todo_wine ok(ret == STATUS_ACCESS_DENIED, "got %#x\n", ret);
    ok(!WaitForSingleObject( pi.hProcess, 1000 ), "wait failed\n");
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}

static HANDLE test_close_io_completion_port_ready, test_close_io_completion_test_ready;
static HANDLE test_close_io_completion_port;

static DWORD WINAPI test_close_io_completion_thread(void *param)
{
    FILE_IO_COMPLETION_INFORMATION info;
    IO_STATUS_BLOCK iosb;
    ULONG_PTR key, value;
    NTSTATUS status;
    ULONG count;
    DWORD ret;

    ret = WaitForSingleObject( test_close_io_completion_port_ready, INFINITE );
    ok( ret == WAIT_OBJECT_0, "Got unexpected ret %#x.\n", ret );
    SetEvent( test_close_io_completion_test_ready );
    status = NtRemoveIoCompletion( test_close_io_completion_port, &key, &value, &iosb, NULL );
    if (status == STATUS_INVALID_HANDLE)
        skip( "Handle closed before wait started.\n" );
    else
        ok( status == STATUS_ABANDONED_WAIT_0, "Got unexpected status %#x.\n", status );

    ret = WaitForSingleObject( test_close_io_completion_port_ready, INFINITE );
    ok( ret == WAIT_OBJECT_0, "Got unexpected ret %#x.\n", ret );
    SetEvent( test_close_io_completion_test_ready );
    count = 0xdeadbeef;
    status = NtRemoveIoCompletionEx( test_close_io_completion_port, &info, 1, &count, NULL, FALSE );
    ok( count == 1, "Got unexpected count %u.\n", count );
    if (status == STATUS_INVALID_HANDLE)
        skip( "Handle closed before wait started.\n" );
    else
        ok( status == STATUS_ABANDONED_WAIT_0, "Got unexpected status %#x.\n", status );

    return 0;
}

static void test_close_io_completion(void)
{
    NTSTATUS status;
    unsigned int i;
    HANDLE thread;
    DWORD ret;

    test_close_io_completion_port_ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    test_close_io_completion_test_ready = CreateEventA(NULL, FALSE, FALSE, NULL);

    thread = CreateThread( NULL, 0, test_close_io_completion_thread, NULL, 0, NULL );
    ok( !!thread, "Failed to create thread, error %u.\n", GetLastError() );

    for (i = 0; i < 2; ++i)
    {
        status = NtCreateIoCompletion( &test_close_io_completion_port, IO_COMPLETION_ALL_ACCESS, NULL, 0 );
        ok( !status, "Got unexpected status %#x.\n", status );
        ret = SignalObjectAndWait( test_close_io_completion_port_ready, test_close_io_completion_test_ready,
                                   INFINITE, FALSE );
        ok( ret == WAIT_OBJECT_0, "Got unexpected ret %#x.\n", ret );
        Sleep(10);
        status = pNtClose( test_close_io_completion_port );
        ok( !status, "Got unexpected status %#x.\n", status );
    }

    WaitForSingleObject( thread, INFINITE );
    CloseHandle( thread );
}

START_TEST(sync)
{
    HMODULE module = GetModuleHandleA("ntdll.dll");
    char **argv;
    int argc;

    argc = winetest_get_mainargs( &argv );

    if (argc > 2) return;

    pNtAlertThreadByThreadId        = (void *)GetProcAddress(module, "NtAlertThreadByThreadId");
    pNtClose                        = (void *)GetProcAddress(module, "NtClose");
    pNtCreateEvent                  = (void *)GetProcAddress(module, "NtCreateEvent");
    pNtCreateKeyedEvent             = (void *)GetProcAddress(module, "NtCreateKeyedEvent");
    pNtCreateMutant                 = (void *)GetProcAddress(module, "NtCreateMutant");
    pNtCreateSemaphore              = (void *)GetProcAddress(module, "NtCreateSemaphore");
    pNtOpenEvent                    = (void *)GetProcAddress(module, "NtOpenEvent");
    pNtOpenKeyedEvent               = (void *)GetProcAddress(module, "NtOpenKeyedEvent");
    pNtPulseEvent                   = (void *)GetProcAddress(module, "NtPulseEvent");
    pNtQueryEvent                   = (void *)GetProcAddress(module, "NtQueryEvent");
    pNtQueryMutant                  = (void *)GetProcAddress(module, "NtQueryMutant");
    pNtQuerySemaphore               = (void *)GetProcAddress(module, "NtQuerySemaphore");
    pNtQuerySystemTime              = (void *)GetProcAddress(module, "NtQuerySystemTime");
    pNtReleaseKeyedEvent            = (void *)GetProcAddress(module, "NtReleaseKeyedEvent");
    pNtReleaseMutant                = (void *)GetProcAddress(module, "NtReleaseMutant");
    pNtReleaseSemaphore             = (void *)GetProcAddress(module, "NtReleaseSemaphore");
    pNtResetEvent                   = (void *)GetProcAddress(module, "NtResetEvent");
    pNtSetEvent                     = (void *)GetProcAddress(module, "NtSetEvent");
    pNtWaitForAlertByThreadId       = (void *)GetProcAddress(module, "NtWaitForAlertByThreadId");
    pNtWaitForKeyedEvent            = (void *)GetProcAddress(module, "NtWaitForKeyedEvent");
    pRtlAcquireResourceExclusive    = (void *)GetProcAddress(module, "RtlAcquireResourceExclusive");
    pRtlAcquireResourceShared       = (void *)GetProcAddress(module, "RtlAcquireResourceShared");
    pRtlDeleteResource              = (void *)GetProcAddress(module, "RtlDeleteResource");
    pRtlInitializeResource          = (void *)GetProcAddress(module, "RtlInitializeResource");
    pRtlInitUnicodeString           = (void *)GetProcAddress(module, "RtlInitUnicodeString");
    pRtlReleaseResource             = (void *)GetProcAddress(module, "RtlReleaseResource");
    pRtlWaitOnAddress               = (void *)GetProcAddress(module, "RtlWaitOnAddress");
    pRtlWakeAddressAll              = (void *)GetProcAddress(module, "RtlWakeAddressAll");
    pRtlWakeAddressSingle           = (void *)GetProcAddress(module, "RtlWakeAddressSingle");

    test_wait_on_address();
    test_event();
    test_mutant();
    test_semaphore();
    test_keyed_events();
    test_resource();
    test_tid_alert( argv );
    test_close_io_completion();
}
