/////////////////////////////////////////////////////////////////////////////
//// INCLUDE FILES

#include "precomp.h"
#pragma hdrstop
#include "iocommon.h"

// Copyright And Configuration Management ----------------------------------
//
//              PassThru Driver Extensions Module - ptextend.c
//
//                  Companion Sample Code for the Article
//
//        "Extending the Microsoft PassThru NDIS Intermediate Driver"
//
//    Copyright (c) 2003 Printing Communications Associates, Inc. (PCAUSA)
//                          http://www.pcausa.com
//
// The right to use this code in your own derivative works is granted so long
// as 1.) your own derivative works include significant modifications of your
// own, 2.) you retain the above copyright notices and this paragraph in its
// entirety within sources derived from this code.
// This product includes software developed by PCAUSA. The name of PCAUSA
// may not be used to endorse or promote products derived from this software
// without specific prior written permission.
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// End ---------------------------------------------------------------------

VOID
PtRefAdapter( PADAPT pAdapt )
{
   NdisInterlockedIncrement( &pAdapt->RefCount );
}

VOID
PtDerefAdapter( PADAPT pAdapt )
{
   if( !pAdapt )
   {
      return;
   }

   if( NdisInterlockedDecrement( &pAdapt->RefCount) == 0 )
   {
      DBGPRINT(( "PtDerefAdapter: Adapter: 0x%8.8X\n", pAdapt ? (ULONG )pAdapt : 0 ));

      //
      //  Free all resources on this adapter structure.
      //
      if (pAdapt->RecvPacketPoolHandle != NULL)
      {
         //
         // Free the packet pool that is used to indicate receives
         //
         NdisFreePacketPool(pAdapt->RecvPacketPoolHandle);

         pAdapt->RecvPacketPoolHandle = NULL;
      }

      if (pAdapt->SendPacketPoolHandle != NULL)
      {

         //
         //  Free the packet pool that is used to send packets below
         //

         NdisFreePacketPool(pAdapt->SendPacketPoolHandle);

         pAdapt->SendPacketPoolHandle = NULL;
      }

// BEGIN_PTEX_FILTER
      //
      // Deinitialize Filter Resources On This Adapter
      //
      FltOnDeinitAdapter( pAdapt );
// END_PTEX_FILTER

      NdisFreeMemory(pAdapt, 0, 0);
   }
}

PADAPT
PtLookupAdapterByName(
   IN PUCHAR   pNameBuffer,
   IN USHORT   NameBufferLength,
   IN BOOLEAN  bUseVirtualName
   )
{
   PADAPT *ppCursor, pAdapt = NULL;

   //
   // Sanity Checks
   //
   if( !pNameBuffer || !NameBufferLength )
   {
      return( NULL );
   }

   //
   // Walk The Adapter List
   // ---------------------
   // Hold the global lock while walking. Otherwise, the adapter list could be altered at any point in
   // the list processing sequence.
   //
   NdisAcquireSpinLock( &GlobalLock );

   for( ppCursor = &pAdaptList; *ppCursor != NULL;
      ppCursor = &(*ppCursor)->Next
      )
   {
      __try
      {
         if( bUseVirtualName )
         {
            //
            // Check For Match Against Virtual Adapter Name
            //
            if( ( (*ppCursor)->DeviceName.Length == NameBufferLength) &&
                  NdisEqualMemory( (*ppCursor)->DeviceName.Buffer, pNameBuffer, NameBufferLength ))
            {
               //
               // Return Pointer To Found Adapter
               //
               pAdapt = (*ppCursor);
               break;
            }
         }
         else
         {
            //
            // Check For Match Against Lower Adapter Name
            //
            if( ( (*ppCursor)->LowerDeviceName.Length == NameBufferLength) &&
                  NdisEqualMemory( (*ppCursor)->LowerDeviceName.Buffer, pNameBuffer, NameBufferLength))
            {
               //
               // Return Pointer To Found Adapter
               //
               pAdapt = (*ppCursor);
               break;
            }
         }
      }
      __except( EXCEPTION_EXECUTE_HANDLER )
      {
         pAdapt = NULL;
         break;
      }
   }

   //
   // Add Reference To Adapter Memory
   // -------------------------------
   // As soon as the spinlock is released (below) and before returning to the caller it is possible
   // for NDIS to unbind the selected adapter from the PassThru protocol. The reference counting scheme
   // insures that the memory pointed to by pAdapt will remain valid until the last call to
   // PtDerefAdapter.
   //
   if( pAdapt )
   {
	   PtRefAdapter( pAdapt );
   }

   NdisReleaseSpinLock( &GlobalLock );

   return( pAdapt );
}

VOID
DevRefOpenContext( POPEN_CONTEXT pOpenContext )
{
   PtRefAdapter( pOpenContext->pAdapt );

   NdisInterlockedIncrement( &pOpenContext->RefCount );
}

VOID
DevDerefOpenContext( POPEN_CONTEXT pOpenContext )
{
   PADAPT pAdapt = NULL;

   if( !pOpenContext )
   {
      return;
   }

   pAdapt = pOpenContext->pAdapt;

   if( NdisInterlockedDecrement( &pOpenContext->RefCount) == 0 )
   {
      DBGPRINT(( "DevDerefOpenContext: Context: 0x%8.8X\n", pOpenContext ? (ULONG )pOpenContext : 0 ));

      NdisFreeSpinLock( &pOpenContext->Lock );

// BEGIN_PTEX_FILTER
      //
      // Deinitialize Filter Resources On This Handle
      //
      FltOnDeinitOpenContext( pOpenContext );
// END_PTEX_FILTER

      NdisFreeMemory(pOpenContext, 0, 0);
   }

   PtDerefAdapter( pAdapt );
}

POPEN_CONTEXT
DevAllocateOpenContext( PADAPT pAdapt )
{
   POPEN_CONTEXT pOpenContext = NULL;

   NdisAllocateMemoryWithTag( &pOpenContext, sizeof( OPEN_CONTEXT ), TAG );

   if( !pOpenContext )
   {
      return( NULL );
   }

   //
   // Initialize The Open Context Structure
   //
   NdisZeroMemory( pOpenContext, sizeof( OPEN_CONTEXT ) );

   NdisAllocateSpinLock( &pOpenContext->Lock );

   NdisInitializeEvent( &pOpenContext->LocalRequest.RequestEvent );

// BEGIN_PTEX_FILTER
   //
   // Initialize Filter Resources On This Handle
   //
   FltOnInitOpenContext( pOpenContext );
// END_PTEX_FILTER

   //
   // Add Initial Reference To Open Context
   // -------------------------------------
   // Note that we already have added an implicit reference to the adapter
   // because of the PtLookupAdapterByName call.
   //
   pOpenContext->RefCount = 1;

   pOpenContext->pAdapt = pAdapt;

   return( pOpenContext );
}

NTSTATUS
DevEnumerateBindings(
   IN PDEVICE_OBJECT    pDeviceObject,
   IN PIRP              pIrp
   )
{
   PIO_STACK_LOCATION  pIrpSp;
   NTSTATUS            NtStatus = STATUS_SUCCESS;
   ULONG               BytesReturned = 0;
   PUCHAR              ioBuffer = NULL;
   ULONG               inputBufferLength;
   ULONG               outputBufferLength, Remaining;
   PADAPT              *ppCursor;

   UNREFERENCED_PARAMETER(pDeviceObject);

   pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

   ioBuffer = pIrp->AssociatedIrp.SystemBuffer;
   inputBufferLength  = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
   outputBufferLength = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;
   Remaining = outputBufferLength;

   DBGPRINT(("==>Pt DevEnumerateBindings: FileObject %p\n", pIrpSp->FileObject ));


   //
   // Sanity Check On Length
   //
   if( sizeof( UNICODE_NULL ) > Remaining )
   {
      BytesReturned = 0;
      NtStatus = NDIS_STATUS_BUFFER_OVERFLOW;
      goto CompleteTheIRP;
   }

   //
   // Walk The Adapter List
   //
   NdisAcquireSpinLock( &GlobalLock );

   __try
   {
      //
      // Insert List-Terminating NULL
      //
      *((PWCHAR )ioBuffer) = UNICODE_NULL;

      BytesReturned = sizeof( UNICODE_NULL );
      Remaining -= sizeof( UNICODE_NULL );

      for( ppCursor = &pAdaptList; *ppCursor != NULL;
         ppCursor = &(*ppCursor)->Next
         )
      {
         //
         // Sanity Check On Length
         //
         if( (*ppCursor)->DeviceName.Length + sizeof( UNICODE_NULL) > Remaining )
         {
            BytesReturned = 0;
            NtStatus = NDIS_STATUS_BUFFER_OVERFLOW;
            break;
         }

         //
         // Add The Virtual DeviceName To The Buffer
         // ----------------------------------------
         // This name passed to NdisIMInitializeDeviceInstanceEx.
         //
         NdisMoveMemory(
            ioBuffer,
            (*ppCursor)->DeviceName.Buffer,
            (*ppCursor)->DeviceName.Length
            );

         DBGPRINT(( "Adding VA Name : %d, %d, \042%*.*ws\042\n",
            (*ppCursor)->DeviceName.Length,
            (*ppCursor)->DeviceName.MaximumLength,
            (*ppCursor)->DeviceName.Length/sizeof( wchar_t ),
            (*ppCursor)->DeviceName.Length/sizeof( wchar_t ),
            (*ppCursor)->DeviceName.Buffer
            ));

         //
         // Move Past Virtual DeviceName In Buffer
         //
         Remaining -= (*ppCursor)->DeviceName.Length;
         BytesReturned += (*ppCursor)->DeviceName.Length;
         ioBuffer += (*ppCursor)->DeviceName.Length;

         //
         // Add Name-Terminating NULL
         //
         *((PWCHAR )ioBuffer) = UNICODE_NULL;

         Remaining -= sizeof( UNICODE_NULL );
         BytesReturned += sizeof( UNICODE_NULL );
         ioBuffer += sizeof( UNICODE_NULL );

         //
         // Sanity Check On Length
         //
         if( (*ppCursor)->LowerDeviceName.Length + sizeof( UNICODE_NULL ) > Remaining )
         {
            BytesReturned = 0;
            NtStatus = NDIS_STATUS_BUFFER_OVERFLOW;
            break;
         }

         //
         // Add The Lower DeviceName To The Buffer
         // --------------------------------------
         // This name passed to NdisOpenAdapter.
         //
         NdisMoveMemory(
            ioBuffer,
            (*ppCursor)->LowerDeviceName.Buffer,
            (*ppCursor)->LowerDeviceName.Length
            );

         DBGPRINT(( "Adding LA Name : %d, %d, \042%*.*ws\042\n",
            (*ppCursor)->LowerDeviceName.Length,
            (*ppCursor)->LowerDeviceName.MaximumLength,
            (*ppCursor)->LowerDeviceName.Length/sizeof( wchar_t ),
            (*ppCursor)->LowerDeviceName.Length/sizeof( wchar_t ),
            (*ppCursor)->LowerDeviceName.Buffer
            ));

         //
         // Move Past Lower DeviceName In Buffer
         //
         Remaining -= (*ppCursor)->LowerDeviceName.Length;
         BytesReturned += (*ppCursor)->LowerDeviceName.Length;
         ioBuffer += (*ppCursor)->LowerDeviceName.Length;

         //
         // Add Name-Terminating NULL
         //
         *((PWCHAR )ioBuffer) = UNICODE_NULL;

         Remaining -= sizeof( UNICODE_NULL );
         BytesReturned += sizeof( UNICODE_NULL );
         ioBuffer += sizeof( UNICODE_NULL );

         //
         // Add List-Terminating NULL
         // -------------------------
         // Space is already accomodated for this.
         //
         *((PWCHAR )ioBuffer) = UNICODE_NULL;
      }
   }
   __except( EXCEPTION_EXECUTE_HANDLER )
   {
      BytesReturned = 0;
      NtStatus = STATUS_INVALID_PARAMETER;
   }

   NdisReleaseSpinLock( &GlobalLock );

CompleteTheIRP:
   if (NtStatus != STATUS_PENDING)
   {
      pIrp->IoStatus.Information = BytesReturned;
      pIrp->IoStatus.Status = NtStatus;
      IoCompleteRequest(pIrp, IO_NO_INCREMENT);
   }

   DBGPRINT(("<== Pt DevEnumerateBindings\n"));

   return NtStatus;
}

NTSTATUS
DevOpenAdapter(
   IN PDEVICE_OBJECT    pDeviceObject,
   IN PIRP              pIrp,
   IN BOOLEAN           bUseVirtualName
   )
{
   PIO_STACK_LOCATION  pIrpSp;
   NTSTATUS            NtStatus = STATUS_SUCCESS;
   ULONG               BytesReturned = 0;
   PUCHAR              pNameBuffer = NULL;
   ULONG               NameBufferLength;
   PADAPT              pAdapt;
   POPEN_CONTEXT       pOpenContext;

   UNREFERENCED_PARAMETER(pDeviceObject);

   pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

   DBGPRINT(("==>Pt DevOpenAdapter: FileObject %p\n", pIrpSp->FileObject));
   
   pNameBuffer = pIrp->AssociatedIrp.SystemBuffer;
   NameBufferLength  = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;

   DBGPRINT(( "   Looking For Name : \042%*.*ws\042\n",
      NameBufferLength/sizeof( wchar_t ),
      NameBufferLength/sizeof( wchar_t ),
      pNameBuffer
      ));

   //
   // Lookup Adapter By Name
   // ----------------------
   // If successful the lookup function has added a ref count to the found ADAPT
   // structure.
   //
   pAdapt = PtLookupAdapterByName( pNameBuffer, (USHORT )NameBufferLength, bUseVirtualName );

   if( !pAdapt )
   {
      DBGPRINT(( "      Adapter Not Found\n" ));
      
      NtStatus = STATUS_OBJECT_NAME_NOT_FOUND;
      goto CompleteTheIRP;
   }

   DBGPRINT(( "      Found Adapter\n" ));

   //
   // Fail Open If Unbind Is In Progress
   //
   NdisAcquireSpinLock(&pAdapt->Lock);
   if( pAdapt->UnbindingInProcess )
   {
      NdisReleaseSpinLock(&pAdapt->Lock);
      DBGPRINT(( "      Unbind In Process\n" ));
      PtDerefAdapter( pAdapt );

      NtStatus = STATUS_INVALID_DEVICE_STATE;
      goto CompleteTheIRP;
   }
   NdisReleaseSpinLock(&pAdapt->Lock);

   if( pAdapt->pOpenContext )
   {
      DBGPRINT(( "      Handle Already Associated(1)\n" ));
      PtDerefAdapter( pAdapt );

      NtStatus = STATUS_DEVICE_BUSY;
      goto CompleteTheIRP;
   }

   pOpenContext = DevAllocateOpenContext( pAdapt );

   if( !pOpenContext )
   {
      DBGPRINT(( "      Unable To Allocate Open Context\n" ));
      PtDerefAdapter( pAdapt );

      NtStatus = STATUS_INSUFFICIENT_RESOURCES;
      goto CompleteTheIRP;
   }

   //
   // Sanity Check For Concurrent Open Race Condition
   // -----------------------------------------------
   // At this point we enforce exclusive access on a per-binding basis.
   //
   // This logic deals with the situation where two concurrent adapter
   // opens could be in progress. We want an atomic mechanism that insures
   // that only one of the opens will be successful.
   //
   // This InterlockedXXX function performs an atomic operation: First it
   // compares pAdapt->pOpenContext with NULL, if they are equal, the function
   // puts pOpenContext into pAdapt->pOpenContext, and return NULL. Otherwise,
   // it return existing pAdapt->pOpenContext without changing anything.
   //
   // NOTE: This implementation is borrowed from the NDISPROT sample from
   // the Windows DDK.
   // 
   
   if ( InterlockedCompareExchangePointer (& (pAdapt->pOpenContext), pOpenContext, NULL) != NULL)
   {
      DBGPRINT(( "      Handle Already Associated(2)\n" ));
      PtDerefAdapter( pAdapt );

      NtStatus = STATUS_DEVICE_BUSY;
      goto CompleteTheIRP;
   }

   //
   // Associate This Handle With The Open Context
   //
   pIrpSp->FileObject->FsContext = pOpenContext;


   //
   // Complete The IRP
   //
CompleteTheIRP:

   pIrp->IoStatus.Information = BytesReturned;
   pIrp->IoStatus.Status = NtStatus;
   IoCompleteRequest(pIrp, IO_NO_INCREMENT);

   DBGPRINT(("<== Pt DevOpenAdapter\n"));
   
   return NtStatus;
}


VOID
DevRequestComplete(
   IN  PADAPT              pAdapt,
   IN  PNDIS_REQUEST_EX    pLocalRequest,
   IN  NDIS_STATUS         Status
   )
{
   POPEN_CONTEXT  pOpenContext;
   
   DBGPRINT(("<== Pt DevRequestComplete\n"));

   pOpenContext = (POPEN_CONTEXT )pLocalRequest->RequestContext;

   pLocalRequest->RequestStatus = Status;

   NdisSetEvent( &pLocalRequest->RequestEvent );

   DBGPRINT(("<== Pt DevRequestComplete\n"));
}


NTSTATUS
DevQueryInformation(
   IN PDEVICE_OBJECT    pDeviceObject,
   IN PIRP              pIrp
   )
{
   PIO_STACK_LOCATION  pIrpSp;
   NTSTATUS            NtStatus = STATUS_SUCCESS;
   ULONG               BytesReturned = 0;
   PUCHAR              ioBuffer = NULL;
   ULONG               inputBufferLength;
   ULONG               outputBufferLength;
   NDIS_OID            Oid;
   PADAPT              pAdapt;
   POPEN_CONTEXT       pOpenContext;
   PNDIS_REQUEST_EX    pLocalRequest;
   NDIS_STATUS         NdisStatus;

   UNREFERENCED_PARAMETER(pDeviceObject);

   pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

   DBGPRINT(("==>Pt DevQueryInformation: FileObject %p\n", pIrpSp->FileObject));

   ioBuffer = pIrp->AssociatedIrp.SystemBuffer;
   inputBufferLength  = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
   outputBufferLength = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;

   pOpenContext = pIrpSp->FileObject->FsContext;

   if( !pOpenContext )
   {
      DBGPRINT(( "      Invalid Handle\n" ));
      
      NtStatus = STATUS_INVALID_HANDLE;
      goto CompleteTheIRP;
   }


   DBGPRINT(( "      Found Open Context\n" ));

   pAdapt = pOpenContext->pAdapt;

   if( !pAdapt )
   {
      DBGPRINT(( "      Adapter Not Found\n" ));
      
      NtStatus = STATUS_INVALID_HANDLE;
      goto CompleteTheIRP;
   }

   //
   // Sanity Check On Input Buffer/OID
   //
   if( inputBufferLength != sizeof( NDIS_OID ) )
   {
      DBGPRINT(( "      Invalid OID Input Buffer Length\n" ));

      NtStatus = STATUS_INVALID_PARAMETER;
      goto CompleteTheIRP;
   }

   Oid = *(PNDIS_OID )ioBuffer;

   DBGPRINT(( "Query for Information on OID 0x%8.8X\n", Oid ));

   //
   // Fail Open If Unbind Is In Progress
   //
   NdisAcquireSpinLock(&pAdapt->Lock);

   if( pAdapt->UnbindingInProcess )
   {
      NdisReleaseSpinLock(&pAdapt->Lock);
      DBGPRINT(( "      Unbind In Process\n" ));

      NtStatus = STATUS_INVALID_DEVICE_STATE;
      goto CompleteTheIRP;
   }

   //
   // All other queries are failed, if the miniport is not at D0,
   //
   if (pAdapt->MPDeviceState > NdisDeviceStateD0)
   {
      NdisReleaseSpinLock(&pAdapt->Lock);
      DBGPRINT(( "      Invalid Miniport Device State\n" ));

      NtStatus = STATUS_INVALID_DEVICE_STATE;
      goto CompleteTheIRP;
   }

   //
   // This is in the process of powering down the system, always fail the request
   // 
   if (pAdapt->StandingBy == TRUE)
   {
      NdisReleaseSpinLock(&pAdapt->Lock);
      DBGPRINT(( "      Miniport Powering Down\n" ));

      NtStatus = STATUS_INVALID_DEVICE_STATE;
      goto CompleteTheIRP;
   }

   NdisReleaseSpinLock(&pAdapt->Lock);

   //
   // Now (Finally) Make The NDIS Request...
   //

   //
   // May need to add ref counts to adapt and open context. Also, bump
   // a counter of outstanding requests...
   //

   DevRefOpenContext( pOpenContext );

   pLocalRequest = &pOpenContext->LocalRequest;

   pLocalRequest->Request.RequestType = NdisRequestQueryInformation;
   pLocalRequest->Request.DATA.QUERY_INFORMATION.Oid = Oid;
   pLocalRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer = ioBuffer;
   pLocalRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength = outputBufferLength;
   pLocalRequest->Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;
   pLocalRequest->Request.DATA.QUERY_INFORMATION.BytesWritten = 0;

   pLocalRequest->RequestCompleteHandler = DevRequestComplete;
   pLocalRequest->RequestContext = pOpenContext;

   NdisResetEvent( &pLocalRequest->RequestEvent );

   NdisRequest(
      &NdisStatus,
      pAdapt->BindingHandle,
      (PNDIS_REQUEST )pLocalRequest
      );

   if( NdisStatus != NDIS_STATUS_PENDING )
   {
      DevRequestComplete( pAdapt, pLocalRequest, NdisStatus );
   }

   NdisWaitEvent( &pLocalRequest->RequestEvent, 0 );

   NdisStatus = pLocalRequest->RequestStatus;

   if( NdisStatus == NDIS_STATUS_SUCCESS )
   {
      BytesReturned = pLocalRequest->Request.DATA.QUERY_INFORMATION.BytesWritten;

      if( BytesReturned > outputBufferLength )
      {
         BytesReturned = outputBufferLength;
      }

      NtStatus = STATUS_SUCCESS;
   }
   else
   {
      NDIS_STATUS_TO_NT_STATUS( NdisStatus, &NtStatus);
   }

   DevDerefOpenContext( pOpenContext );

   //
   // Complete The IRP
   //
CompleteTheIRP:

   pIrp->IoStatus.Information = BytesReturned;
   pIrp->IoStatus.Status = NtStatus;
   IoCompleteRequest(pIrp, IO_NO_INCREMENT);

   DBGPRINT(("<== Pt DevQueryInformation\n"));
   
   return NtStatus;
}


NTSTATUS
DevOpen(
    IN PDEVICE_OBJECT    pDeviceObject,
    IN PIRP              pIrp
    )
/*++

Routine Description:

    This is the dispatch routine for handling IRP_MJ_CREATE.
    We simply succeed this.

Arguments:

    pDeviceObject - Pointer to the device object.

    pIrp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION  pIrpSp;
    NTSTATUS            NtStatus = STATUS_SUCCESS;
	
    UNREFERENCED_PARAMETER(pDeviceObject);
    
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	
    pIrpSp->FileObject->FsContext = NULL;
    pIrpSp->FileObject->FsContext2 = NULL;
	
    DBGPRINT(("==>Pt DevOpen: FileObject %p\n", pIrpSp->FileObject));
	
    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = NtStatus;
	
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	
    DBGPRINT(("<== Pt DevOpen\n"));
	
    return NtStatus;
}


VOID
DevOnUnbindAdapter( POPEN_CONTEXT pOpenContext )
{
   PADAPT pAdapt = NULL;

   if( !pOpenContext )
   {
      return;
   }

   DBGPRINT(("==>Pt DevOnUnbindAdapter: Context %p\n", pOpenContext ));

   //
   // Set Flag That Will Cause Future I/O To Fail
   //
   pOpenContext->bAdapterClosed = TRUE;

   //
   // Wait For Pending NDIS Operations To Complete
   //

   //
   // Cancel Pending User-Mode I/O Operations
   //

   DBGPRINT(("<== Pt DevOnUnbindAdapter\n"));
}


NTSTATUS
DevCleanup(
    IN PDEVICE_OBJECT    pDeviceObject,
    IN PIRP              pIrp
    )
/*++

Routine Description:

    This is the dispatch routine for handling IRP_MJ_CLEANUP.
    Cancel and complete currently queued IRPs that are associated with
    the FileObject member of the driver's IO_STACK_LOCATION.

Arguments:

    pDeviceObject - Pointer to the device object.

    pIrp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION  pIrpSp;
    NTSTATUS            NtStatus = STATUS_SUCCESS;
    POPEN_CONTEXT       pOpenContext;
	
    UNREFERENCED_PARAMETER(pDeviceObject);
    
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pOpenContext = pIrpSp->FileObject->FsContext;
	
    DBGPRINT(("==>Pt DevCleanup: Context %p\n", (pIrpSp->FileObject)->FsContext ));
	
    if( pOpenContext )
    {
    }

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = NtStatus;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	
    DBGPRINT(("<== Pt DevCleanup\n"));
	
    return NtStatus;
} 


NTSTATUS
DevClose(
    IN PDEVICE_OBJECT    pDeviceObject,
    IN PIRP              pIrp
    )
/*++

Routine Description:

    This is the dispatch routine for handling IRP_MJ_CLOSE.
    Undo that which was done in IRP_MJ_CREATE.

Arguments:

    pDeviceObject - Pointer to the device object.

    pIrp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION  pIrpSp;
    NTSTATUS            NtStatus = STATUS_SUCCESS;
    POPEN_CONTEXT       pOpenContext;
	
    UNREFERENCED_PARAMETER(pDeviceObject);
    
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pOpenContext = pIrpSp->FileObject->FsContext;
	
    DBGPRINT(("==>Pt DevClose: Context %p\n", (pIrpSp->FileObject)->FsContext ));

    //
    // Undo IRP_MJ_CREATE Operations
    //
    pIrpSp->FileObject->FsContext = NULL;
    pIrpSp->FileObject->FsContext2 = NULL;
	
    if( pOpenContext )
    {
      if( pOpenContext->pAdapt )
      {
         NdisAcquireSpinLock(&(pOpenContext->pAdapt)->Lock);
         (pOpenContext->pAdapt)->pOpenContext = NULL;
         NdisReleaseSpinLock(&(pOpenContext->pAdapt)->Lock);
      }

      DevDerefOpenContext( pOpenContext );
    }
	
    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = NtStatus;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	
    DBGPRINT(("<== Pt DevClose\n"));
	
    return NtStatus;
} 


NTSTATUS
DevIoControl(
    IN PDEVICE_OBJECT    pDeviceObject,
    IN PIRP              pIrp
    )
/*++

Routine Description:

    This is the dispatch routine for handling device ioctl requests.

Arguments:

    pDeviceObject - Pointer to the device object.

    pIrp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION  pIrpSp;
    NTSTATUS            NtStatus = STATUS_SUCCESS;
    ULONG               BytesReturned = 0;
    ULONG               FunctionCode;
    PUCHAR              ioBuffer = NULL;
    ULONG               inputBufferLength;
    ULONG               outputBufferLength;
    
    UNREFERENCED_PARAMETER(pDeviceObject);
    
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    
    ioBuffer = pIrp->AssociatedIrp.SystemBuffer;
    inputBufferLength  = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    
    FunctionCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;
    
    DBGPRINT(("==>Pt DevIoControl: Context %p\n", (pIrpSp->FileObject)->FsContext ));
    
    switch (FunctionCode)
    {
        case IOCTL_PTUSERIO_ENUMERATE:
         return( DevEnumerateBindings(
                  pDeviceObject,
                  pIrp
                  )
               );

        case IOCTL_PTUSERIO_OPEN_ADAPTER:
         return( DevOpenAdapter(
                  pDeviceObject,
                  pIrp,
                  FALSE        // Is Lower Adapter
                  )
               );

        case IOCTL_PTUSERIO_QUERY_INFORMATION:
         return( DevQueryInformation(
                  pDeviceObject,
                  pIrp
                  )
               );

        case IOCTL_PTUSERIO_SET_INFORMATION:
            NtStatus = STATUS_NOT_SUPPORTED;
            break;

        default:
         //
         // Pass Unrecognized IOCTL's To Filter
         //
         return( FltDevIoControl(
                  pDeviceObject,
                  pIrp
                  )
               );
    }
    
    if (NtStatus != STATUS_PENDING)
    {
        pIrp->IoStatus.Information = BytesReturned;
        pIrp->IoStatus.Status = NtStatus;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    }
    
    DBGPRINT(("<== Pt DevIoControl\n"));
    
    return NtStatus;
} 



