/*!
 * @author		Nareg Sinenian
 * @file		iSCSIVirtualHBA.h
 * @version		1.0
 * @copyright	(c) 2013-2015 Nareg Sinenian. All rights reserved.
 */

#ifndef __ISCSI_VIRTUAL_HBA_H__
#define __ISCSI_VIRTUAL_HBA_H__

#include <IOKit/IOService.h>
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>
#include <IOKit/scsi/IOSCSIProtocolInterface.h>

#include <libkern/c++/OSArray.h>

#include "iSCSIKernelInterfaceShared.h"
#include "iSCSIPDUKernel.h"
#include "iSCSITypesShared.h"

// BSD Socket Includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>

#define iSCSIVirtualHBA		com_NSinenian_iSCSIVirtualHBA

/*! This class implements the iSCSI virtual host bus adapter (HBA).  The HBA
 *	creates and removes targets and processes SCSI requested by the operating
 *	system. The class maintains state information, including targets and
 *  associated LUNs.  SCSI CDBs that originated from the OS are packaged into
 *  PDUs and then sent over a TCP socket to the specified iSCSI target.
 *	Responses from the target are processes and converted from PDUs to CDBs
 *  and then returned to the OS. */
class iSCSIVirtualHBA : public IOSCSIParallelInterfaceController
{
	OSDeclareDefaultStructors(iSCSIVirtualHBA);

public:
    
    /*! Forward delcaration of an iSCSI session. */
    struct iSCSISession;
    
    /*! Forward delcaration of an iSCSI connection. */
    struct iSCSIConnection;
	
	/////////  FUNCTIONS REQUIRED BY IOSCSIPARALLELINTERFACECONTROLLER  ////////

	/*! Gets the highest logical unit number that the HBA can address.
	 *	@return highest addressable LUN. */
	virtual SCSILogicalUnitNumber ReportHBAHighestLogicalUnitNumber();
	
	/*! Gets whether HBA supports a particular SCSI feature.
	 *	@param theFeature the SCSI feature to check.
	 *	@return true if the specified feature is supported. */
	virtual bool DoesHBASupportSCSIParallelFeature(SCSIParallelFeature theFeature);
	
	/*! Initializes a new SCSI target. After a call to CreateTargetForID(),
	 *	an instance of IOSCSIParallelInterfaceDevice is created.  Upon creation,
	 *	this callback function is invoked.
	 *	@param targetId the target to initialize.
	 *	@return true if the target was successfully initialized. */
	virtual bool InitializeTargetForID(SCSITargetIdentifier targetId);

	virtual SCSIServiceResponse AbortTaskRequest(SCSITargetIdentifier targetId,
                                                 SCSILogicalUnitNumber LUN,
                                                 SCSITaggedTaskIdentifier taggedTaskID);

    virtual SCSIServiceResponse AbortTaskSetRequest(SCSITargetIdentifier targetId,
                                                    SCSILogicalUnitNumber LUN);

    virtual SCSIServiceResponse ClearACARequest(SCSITargetIdentifier targetId,
                                                SCSILogicalUnitNumber LUN);

    virtual SCSIServiceResponse ClearTaskSetRequest(SCSITargetIdentifier targetId,
                                                    SCSILogicalUnitNumber LUN);

    virtual SCSIServiceResponse LogicalUnitResetRequest(SCSITargetIdentifier targetId,
                                                        SCSILogicalUnitNumber LUN);

    virtual SCSIServiceResponse TargetResetRequest(SCSITargetIdentifier targetId);


    /*! Gets the SCSI initiator ID.  This is a random number that is 
     *  generated each time this controller is initialized. */
    virtual SCSIInitiatorIdentifier ReportInitiatorIdentifier();

	/*! Gets the highest SCSI ID that the HBA can address.
	 *	@return highest addressable SCSI device. */
	virtual SCSIDeviceIdentifier ReportHighestSupportedDeviceID();

    /*! Returns the maximum number of tasks that this virtual HBA can
     *  process at any one time. */
	virtual UInt32 ReportMaximumTaskCount();

    /*! Returns the data size associated with a particular task (0). */
	virtual UInt32 ReportHBASpecificTaskDataSize();

    /*! Returns the device data size (0). */
	virtual UInt32 ReportHBASpecificDeviceDataSize();

	/*! Gets whether the virtual HBA creates and removes targets on its own.
	 *  @return true if HBA creates and remove targtes on its own. */
	virtual bool DoesHBAPerformDeviceManagement();
    
    /*! Gets whether the virtual HBA retrieve sense data for each I/O.
     *  @return true if HBA does its own sensing for each I/O operation. */
//    virtual bool DoesHBAPerformAutoSense();

	/*! Initializes the virtual HBA.
	 *	@return true if controller was successfully initialized. */
	virtual bool InitializeController();

    /*! Frees resources associated with the virtual HBA. */
	virtual void TerminateController();

	/*! Starts controller.
	 *	@return true if the controller was started. */
	virtual bool StartController();

	/*! Stops controller. */
	virtual void StopController();

	/*! Handles hardware interrupts (not used for this virtual HBA). */
	virtual void HandleInterruptRequest();
    
    /*! Handles task timeouts.
     *  @param task the task that timed out. */
    virtual void HandleTimeout(SCSIParallelTaskIdentifier task);
    
    /*! Handles connection timeouts.
     *  @param sessionId the session associated with the timed-out connection.
     *  @param connectionId the connection that timed out. */
    void HandleConnectionTimeout(SID sessionId,CID connectionId);

	/*! Processes a task passed down by SCSI target devices in driver stack.
     *  @param parallelTask the task to process.
     *  @return a response that indicates the processing status of the task. */
	virtual SCSIServiceResponse ProcessParallelTask(SCSIParallelTaskIdentifier parallelTask);
    
    /*! Processes a task immediately. This function may be called from
     *  ProcessParallelTask() to process a task right away or might be called
     *  by our software interrupt source (iSCSIIOEventSource) to process the
     *  next task in a queue. */
    static void BeginTaskOnWorkloopThread(iSCSIVirtualHBA * owner,
                                          iSCSISession * session,
                                          iSCSIConnection * connection,
                                          UInt32 initiatorTaskTag);
    
    /*! Called by our software interrupt source (iSCSIIOEventSource) to let us
     *  know that data has become available for a particular session and
     *  connection - this allows us to continue or complete processing the task.
     *  @param owner an instance of this class.
     *  @param session session associated with connection that received data.
     *  @param connection the connection that received data. */
    static bool ProcessTaskOnWorkloopThread(iSCSIVirtualHBA * owner,
                                             iSCSISession * session,
                                             iSCSIConnection * connection);
    
    /** This function has been overloaded to provide additional task-timing
     *  support for multiple connections.
     *  @param session the session associated with the task.
     *  @param connection the connection associated with the task.
     *  @param parallelRequest the request to complete.
     *  @param completionStatus status of the request.
     *  @param serviceResponse the SCSI service response. */
    void CompleteParallelTask(iSCSISession * session,
                              iSCSIConnection * connection,
                              SCSIParallelTaskIdentifier parallelRequest,
                              SCSITaskStatus completionStatus,
                              SCSIServiceResponse serviceResponse);
        
    
    /////////////////////  FUNCTIONS TO MANIPULATE ISCSI ///////////////////////
    
    /*! Allocates a new iSCSI session and returns a session qualifier ID.
     *  @param targetName the name of the target, or NULL if discovery session.
     *  @param domain the IP domain (e.g., AF_INET or AF_INET6).
     *  @param targetaddress the BSD socket structure used to identify the target.
     *  @param hostaddress the BSD socket structure used to identify the host adapter.
     *  @param sessionId identifier for the new session.
     *  @param connectionId identifier for the new connection.
     *  @return error code indicating result of operation. */
    errno_t CreateSession(const char * targetName,
                          int domain,
                          const struct sockaddr * targetAddress,
                          const struct sockaddr * hostAddress,
                          SID * sessionId,
                          CID * connectionId);
    
    /*! Releases an iSCSI session, including all connections associated with that
     *  session.  Connections may be active or inactive when this function is
     *  called.
     *  @param sessionId the session qualifier part of the ISID. */
    void ReleaseSession(SID sessionId);
        
    /*! Sets options associated with a particular session.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param options the options to set.
     *  @return error code indicating result of operation. */
    errno_t SetSessionOptions(SID sessionId,
                              iSCSISessionOptions * options);
    
    /*! Gets options associated with a particular session.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param options the options to get.  The user of this function is
     *  responsible for allocating and freeing the options struct.
     *  @return error code indicating result of operation. */
    errno_t GetSessionOptions(SID sessionId,
                              iSCSISessionOptions * options);
        
    /*! Allocates a new iSCSI connection associated with the particular session.
     *  @param sessionId the session to create a new connection for.
     *  @param domain the IP domain (e.g., AF_INET or AF_INET6).
     *  @param targetaddress the BSD socket structure used to identify the target.
     *  @param hostaddress the BSD socket structure used to identify the host adapter.
     *  @param connectionId identifier for the new connection.
     *  @return error code indicating result of operation. */
    errno_t CreateConnection(SID sessionId,
                             int domain,
                             const struct sockaddr * targetAddress,
                             const struct sockaddr * hostAddress,
                             CID * connectionId);
    
    /*! Frees a given iSCSI connection associated with a given session.
     *  The session should be logged out using the appropriate PDUs. */
    void ReleaseConnection(SID sessionId,CID connectionId);
    
    /*! Activates an iSCSI connection, indicating to the kernel that the iSCSI
     *  daemon has negotiated security and operational parameters and that the
     *  connection is in the full-feature phase.
     *  @param sessionId the session to deactivate.
     *  @param connectionId the connection to deactivate.
     *  @return error code indicating result of operation. */
    errno_t ActivateConnection(SID sessionId,CID connectionId);

    /*! Activates all iSCSI connections for the session, indicating to the 
     *  kernel that the iSCSI daemon has negotiated security and operational 
     *  parameters and that the connection is in the full-feature phase.
     *  @param sessionId the session to deactivate.
     *  @param connectionId the connection to deactivate.
     *  @return error code indicating result of operation. */
    errno_t ActivateAllConnections(SID sessionId);

    /*! Deactivates an iSCSI connection so that parameters can be adjusted or
     *  negotiated by the iSCSI daemon.
     *  @param sessionId the session to deactivate.
     *  @return error code indicating result of operation. */
    errno_t DeactivateConnection(SID sessionId,CID connectionId);

    /*! Deactivates all iSCSI connections so that parameters can be adjusted or
     *  negotiated by the iSCSI daemon.
     *  @param sessionId the session to deactivate.
     *  @return error code indicating result of operation. */
    errno_t DeactivateAllConnections(SID sessionId);

    /*! Gets the first connection (the lowest connectionId) for the
     *  specified session.
     *  @param sessionId obtain an connectionId for this session.
     *  @param connectionId the identifier of the connection.
     *  @return error code indicating result of operation. */
    errno_t GetConnection(SID sessionId,CID * connectionId);

    /*! Gets the connection count for the specified session.
     *  @param sessionId obtain the connection count for this session.
     *  @param numConnections the connection count.
     *  @return error code indicating result of operation. */
    errno_t GetNumConnections(SID sessionId,UInt32 * numConnections);
    
    /*! Sends data over a kernel socket associated with iSCSI.  If the specified
     *  data segment length is not a multiple of 4-bytes, padding bytes will be
     *  added to the data segment of the PDU per RF3720 specification.
     *  This function will automatically calculate the data segment length
     *  field of the PDU and place it in the header using the correct byte order.
     *  It will also assign a command sequence number and expected status sequence
     *  number using values from the session and connection objects to the PDU
     *  header in the correct (network) byte order.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param bhs the basic header segment to send.
     *  @param ahs the additional header segments, if any
     *  @param data the data segment to send.
     *  @param length the byte size of the data segment
     *  @return error code indicating result of operation. */
    errno_t SendPDU(iSCSISession * session,
                    iSCSIConnection * connection,
                    iSCSIPDUInitiatorBHS * bhs,
                    iSCSIPDU::iSCSIPDUCommonAHS * ahs,
                    void * data,
                    size_t length);

    /*! Gets whether a PDU is available for receiption on a particular
     *  connection.
     *  @param the connection to check.
     *  @return true if a PDU is available, false otherwise. */
    static bool isPDUAvailable(iSCSIConnection * connection);
    
    /*! Receives a basic header segment over a kernel socket.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param bhs the basic header segment received.
     *  @param flags optional flags to be passed onto sock_recv.
     *  @return error code indicating result of operation. */
    errno_t RecvPDUHeader(iSCSISession * session,
                          iSCSIConnection * connection,
                          iSCSIPDUTargetBHS * bhs,
                          int flags);
    
    /*! Receives a data segment over a kernel socket.  If the specified length is
     *  not a multiple of 4-bytes, the padding bytes will be discarded per
     *  RF3720 specification (all data segment are multiples of 4 bytes).
     *  multiple of 4-bytes per RFC3720.  For length
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param data the data received.
     *  @param length the length of the data buffer.
     *  @param flags optional flags to be passed onto sock_recv.
     *  @return error code indicating result of operation. */
    errno_t RecvPDUData(iSCSISession * session,
                        iSCSIConnection * connection,
                        void * data,
                        size_t length,
                        int flags);
    
    /*! Wrapper around SendPDU for user-space calls.
     *  Sends data over a kernel socket associated with iSCSI.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param bhs the basic header segment to send.
     *  @param data the data segment to send.
     *  @param length the byte size of the data segment
     *  @return error code indicating result of operation. */
    errno_t SendPDUUser(SID sessionId,
                        CID connectionId,
                        iSCSIPDUInitiatorBHS * bhs,
                        void * data,
                        size_t dataLength);
    
    /*! Wrapper around RecvPDUHeader for user-space calls.
     *  Receives a basic header segment over a kernel socket.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param bhs the basic header segment received.
     *  @return error code indicating result of operation. */
    errno_t RecvPDUHeaderUser(SID sessionId,
                              CID connectionId,
                              iSCSIPDUTargetBHS * bhs);
    
    /*! Wrapper around RecvPDUData for user-space calls.
     *  Receives a data segment over a kernel socket.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param data the data received.
     *  @param length the length of the data buffer.
     *  @return error code indicating result of operation. */
    errno_t RecvPDUDataUser(SID sessionId,
                            CID connectionId,
                            void * data,
                            size_t length);
    
    /*! Sets options associated with a particular connection.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param options the options to set.
     *  @return error code indicating result of operation. */
    errno_t SetConnectionOptions(SID sessionId,
                                 CID connectionId,
                                 iSCSIConnectionOptions * options);
    
    /*! Gets options associated with a particular connection.
     *  @param sessionId the qualifier part of the ISID (see RFC3720).
     *  @param connectionId the connection associated with the session.
     *  @param options the options to get.  The user of this function is
     *  responsible for allocating and freeing the options struct.
     *  @return error code indicating result of operation. */
    errno_t GetConnectionOptions(SID sessionId,
                                 CID connectionId,
                                 iSCSIConnectionOptions * options);
    
    /*! Looks up the session identifier associated with a particular target name.
     *  @param targetName the IQN name of the target (e.q., iqn.2015-01.com.example)
     *  @param sessionId the session identifier.
     *  @return error code indicating result of operation. */
    errno_t GetSessionIdFromTargetName(const char * targetName,SID * sessionId);
    
    /*! Looks up the connection identifier associated with a particular connection address.
     *  @param sessionId the session identifier.
     *  @param address the name used when adding the connection (e.g., IP or DNS).
     *  @param connectionId the associated connection identifier.
     *  @return error code indicating result of operation. */
    errno_t GetConnectionIdFromName(SID sessionId,
                                    const char * address,
                                    CID * connectionId);
    
    /*! Gets an array of session identifiers for each session.
     *  @param sessionIds an array of session identifiers.
     *  @param sessionCount number of session identifiers.
     *  @return error code indicating result of operation. */
    errno_t GetSessionIds(UInt16 ** sessionIds,UInt16 * sessionCount);

    /*! Gets an array of connection identifiers for each session.
     *  @param sessionId session identifier.
     *  @param connectionIds an array of connection identifiers for the session.
     *  @param connectionCount number of connection identifiers.
     *  @return error code indicating result of operation. */
    errno_t GetConnectionIds(SID sessionId,
                             UInt32 ** connectionIds,
                             UInt32 * connectionCount);
                                              
private:
    
    /*! Process an incoming task management response PDU.
     *  @param session the session associated with the task mgmt response.
     *  @param connection the connection associated with the task mgmt response.
     *  @param bhs the basic header segment of the task mgmt response. */
    void ProcessTaskMgmtRsp(iSCSISession * session,
                            iSCSIConnection * connection,
                            iSCSIPDU::iSCSIPDUTaskMgmtRspBHS * bhs);

    /*! Process an incoming NOP in PDU.  This can be either a simple response
     *  to a NOP in initiated by the target, or a NOP in response to a previous
     *  NOP out that was sent by this initiator.  For the latter, the original
     *  NOP was sent with a timestamp at that time and it should bounce back to
     *  us allowing us to measure the latency of the connection.
     *  @param session the session associated with the NOP in.
     *  @param connection the connection associated with the NOP in.
     *  @param bhs the basic header segment of the NOP in. */
    void ProcessNOPIn(iSCSISession * session,
                      iSCSIConnection * connection,
                      iSCSIPDU::iSCSIPDUNOPInBHS * bhs);
    
    
    /*! Process an incoming SCSI response PDU.
     *  @param session the session associated with the SCSI response.
     *  @param connection the connection associated with the SCSI response.
     *  @param bhs the basic header segment of the SCSI response. */
    void ProcessSCSIResponse(iSCSISession * session,
                             iSCSIConnection * connection,
                             iSCSIPDU::iSCSIPDUSCSIRspBHS * bhs);

    /*! Process an incoming data PDU.
     *  @param session the session associated with the data PDU.
     *  @param connection the connection associated with the data PDU.
     *  @param bhs the basic header segment of the data PDU. */
    void ProcessDataIn(iSCSISession * session,
                       iSCSIConnection * connection,
                       iSCSIPDU::iSCSIPDUDataInBHS * bhs);
    
    /*! Process an incoming asynchronous message PDU.
     *  @param session the session associated with the async PDU.
     *  @param connection the connection associated with the async PDU.
     *  @param bhs the basic header segment of the async PDU. */
    void ProcessAsyncMsg(iSCSISession * session,
                         iSCSIConnection * connection,
                         iSCSIPDU::iSCSIPDUAsyncMsgBHS * bhs);


    /*! Process an incoming R2T PDU.
     *  @param session the session associated with the R2T PDU.
     *  @param connection the connection associated with the R2T PDU.
     *  @param bhs the basic header segment of the R2T PDU. */
    void ProcessR2T(iSCSISession * session,
                    iSCSIConnection * connection,
                    iSCSIPDU::iSCSIPDUR2TBHS * bhs);
    
    
    /*! Process an incoming reject PDU.
     *  @param session the session associated with the reject PDU.
     *  @param connection the connection associated with the reject PDU.
     *  @param bhs the basic header segment of the reject PDU. */
    void ProcessReject(iSCSISession * session,
                       iSCSIConnection * connection,
                       iSCSIPDU::iSCSIPDURejectBHS * bhs);

    
    /*! Adjusts the timeouts associated with a particular connection.  This
     *  function uses a NOP out PDU to measure the latency of particular
     *  iSCSI connection. This is achieved by generating and sending 
     *  a PDU with the current timestamp which is then echoed back by the
     *  target. The response PDU is processed by ProcessNOPIn().
     *  @param session the session to tune.
     *  @param connection the connection to tune. */
    void MeasureConnectionLatency(iSCSISession * session,
                                  iSCSIConnection * connection);
	
    /*! Maximum allowable sessions. */
    static const UInt16 kMaxSessions;
    
    /*! Maximum allowable connections per session. */
    static const UInt16 kMaxConnectionsPerSession;
    
    /*! Highest LUN supported by the virtual HBA. */
    static const SCSILogicalUnitNumber kHighestLun;
    
    /*! Highest SCSI device ID supported by the HBA. */
    static const SCSIDeviceIdentifier kHighestSupportedDeviceId;
    
    /*! Maximum number of SCSI tasks the HBA can handle. */
    static const UInt32 kMaxTaskCount;
    
    /*! Number of PDUs that are transmitted before we calculate an average speed
     *  for the connection. */
    static const UInt32 kNumBytesPerAvgBW;
    
    /*! Used as part of the iSCSI layer intiator task tag to specify the 
     *  type of task. */
    enum InitiatorTaskTagCodes {
        
        /*! Used as part of the iSCSI task tag for all SCSI tasks. */
        kInitiatorTaskTagSCSITask = 0,
    
        /*! Used as part of the iSCSI task tag for latency measurement task. */
        kInitiatorTaskTagLatency = 1,
    
        /*! Used as part of the iSCSI task tag for all task management operations. */
        kInitiatorTaskTagTaskMgmt = 2
    };
    
    /*! Creates the iSCSI layer's initiator task tag for a PDU using the task
     *  code, LUN, and the SCSI layer's task identifier. */
    inline UInt32 BuildInitiatorTaskTag(InitiatorTaskTagCodes taskCode,
                                        SCSILogicalUnitNumber LUN,
                                        SCSITaggedTaskIdentifier taskId)
    {
        // The task tag is constructed using the HBA controller task ID, the
        // LUN and a taskCode that maps to differnet *types* of iSCSI tasks
        return ( ((UInt8)taskCode)<<24 | ((UInt8)LUN)<<16 | (UInt16)taskId );
    }
    
    inline InitiatorTaskTagCodes ParseInitiatorTaskTagForID(UInt32 initiatorTaskTag)
    {
        return (InitiatorTaskTagCodes)((initiatorTaskTag>>24) & 0xFF);
    }
    
    /*! Helper function.  Sends a burst of data out PDUs, either as a response
     *  to an R2T from the target or as unsolicited data. */
 //   void SendDataOutBurst(IOMemoryDescriptor )
/*
    inline UInt32 ParseInitiatorTaskTag(UInt32 initiatorTaskTag,
                                        SCSILogicalUnitNumber & LUN,
                                        SCSITaggedTaskIdentifier taskId)
  */
    /*! Initiator ID of the virtual HBA.  This value is auto-generated upon 
     *  initialization of the initiator and the 24 least significant bits
     *  are used to form part of the iSCSI ISID. */
    SCSIInitiatorIdentifier kInitiatorId;
	
	/*! Lookup table that maps iSCSI sessions to ISID qualifiers
     *  (session qualifier IDs). */
    iSCSISession * * sessionList;
    
    /*! Lookup table mapping target names (IQN names) to session identifiers. */
    OSDictionary * targetList;
};


#endif /* defined(__iSCSI_Initiator__iSCSIInitiatorVirtualHBA__) */


